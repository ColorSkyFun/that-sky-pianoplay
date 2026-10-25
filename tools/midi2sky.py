#!/usr/bin/env python3
# midi2sky.py -- MIDI -> Sky 15-key ensemble lua (for piano-rpc mod, P.sky format)
#
# Usage:
#   python tools\midi2sky.py input.mid -o songs\xxx.lua --name "Title" \
#          --ins P.PI,P.PI,P.CB [--speed 1.0] [--max-notes 4000]
#
# - Parses note on/off pairs per (track, channel), applies the tempo map.
# - Transposes to C major (best white-key fit), snaps black keys to the
#   nearest white key, octave-folds anything outside C4..C6.
# - Splits voices by (track, channel); each voice becomes a P.sky event
#   table; instruments are assigned by the --ins list (index by voice order
#   sorted from LOWEST average pitch to HIGHEST; extra voices reuse the
#   last entry). Voices with 'P.CB'/'P.CE'/'P.VI'/'P.FL'/'P.OC'/'P.BU'/
#   'P.SAX'/'P.MA' get real note durations (sustained), others get none.
# ASCII-only source on purpose (console codepage safety).

import argparse, struct, sys, io, os

WHITE = {0: 0, 2: 1, 4: 2, 5: 3, 7: 4, 9: 5, 11: 6}  # pitch class -> diatonic degree
SUSTAINED = {'P.CB', 'P.CE', 'P.VI', 'P.FL', 'P.OC', 'P.BU', 'P.SAX', 'P.MA'}

def read_varlen(d, p):
    v = 0
    while True:
        b = d[p]; p += 1
        v = (v << 7) | (b & 0x7F)
        if not (b & 0x80):
            return v, p

def parse_midi(path):
    data = open(path, 'rb').read()
    if data[:4] != b'MThd':
        raise SystemExit('not a MIDI file (no MThd)')
    hlen = struct.unpack('>I', data[4:8])[0]
    fmt, ntrk, division = struct.unpack('>HHH', data[8:14])
    pos = 8 + hlen
    tracks = []
    while pos + 8 <= len(data):
        tag = data[pos:pos+4]; ln = struct.unpack('>I', data[pos+4:pos+8])[0]
        if tag == b'MTrk':
            tracks.append((pos + 8, ln))
        pos += 8 + ln
    return data, tracks, division

def track_notes(data, start, ln):
    """Return (notes, tempos): notes = list of (start_tick, end_tick, pitch, vel, ch)."""
    p = start; end = start + ln
    tick = 0; running = None
    notes = []; tempos = []
    stack = {}
    while p < end:
        dt, p = read_varlen(data, p)
        tick += dt
        b = data[p]
        if b & 0x80:
            status = b; p += 1
            if status < 0xF0:
                running = status
        else:
            status = running
        if status == 0xFF:
            meta = data[p]; p += 1
            mln, p = read_varlen(data, p)
            payload = data[p:p+mln]; p += mln
            if meta == 0x51 and len(payload) == 3:
                tempos.append((tick, (payload[0] << 16) | (payload[1] << 8) | payload[2]))
        elif status in (0xF0, 0xF7):
            mln, p = read_varlen(data, p); p += mln
        else:
            typ = status & 0xF0; ch = status & 0x0F
            if typ == 0x90 and p < end and data[p+1] > 0:
                pitch, vel = data[p], data[p+1]; p += 2
                stack.setdefault((ch, pitch), []).append((tick, vel))
            elif typ == 0x80 or (typ == 0x90 and p < end and data[p+1] == 0):
                pitch = data[p]; p += 2
                lst = stack.get((ch, pitch))
                if lst:
                    st, vel = lst.pop(0)
                    notes.append((st, tick, pitch, vel, ch))
            elif typ in (0xA0, 0xB0, 0xE0):
                p += 2
            elif typ in (0xC0, 0xD0):
                p += 1
            else:
                p += 1
    return notes, tempos

def build_tempo_map(tempos, division):
    """Global tempo list sorted by tick -> (tick, us_per_quarter)."""
    m = sorted(tempos) or [(0, 500000)]
    # merge duplicate ticks (last wins)
    out = []
    for t, v in m:
        if out and out[-1][0] == t:
            out[-1] = (t, v)
        else:
            out.append((t, v))
    if out[0][0] != 0:
        out.insert(0, (0, 500000))
    return out

def tick_to_ms(tick, tmap, division):
    ms = 0.0
    for i in range(len(tmap)):
        t0, us = tmap[i]
        t1 = tmap[i+1][0] if i + 1 < len(tmap) else None
        if t1 is not None and tick >= t1:
            ms += (t1 - t0) * us / division / 1000.0
        else:
            ms += (tick - t0) * us / division / 1000.0
            return ms
    return ms

def snap_to_index(pitch):
    """midi pitch -> diatonic index (C4=60 -> 0), snapping black keys."""
    pc = pitch % 12
    if pc in WHITE:
        deg = WHITE[pc]
    else:
        down = (pc - 1) % 12
        up = (pc + 1) % 12
        deg = WHITE[down] if down in WHITE else WHITE[up]  # both neighbors of a black key are white
    octave = pitch // 12 - 5  # C4 (60) -> 0
    return octave * 7 + deg

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('midi')
    ap.add_argument('-o', '--out', required=True)
    ap.add_argument('--name', default=None)
    ap.add_argument('--ins', default='P.CB,P.PI,P.PI')
    ap.add_argument('--speed', type=float, default=1.0)
    ap.add_argument('--max-notes', type=int, default=4000)
    ap.add_argument('--chord-ms', type=int, default=15)
    ap.add_argument('--split', type=int, default=60,
                    help='split a voice spanning both sides of this midi pitch (default 60=C4)')
    ap.add_argument('--splits', default='',
                    help='comma list of pitch thresholds for multi-band split, e.g. "48,60,72"')
    ap.add_argument('--compress', action='store_true',
                    help='one uniform C4..C6 octave wrap for ALL voices (keeps inter-voice '
                         'register relationships; melody inside the range is untouched)')
    ap.add_argument('--lift', default='',
                    help='per-voice diatonic octave lift before the wrap, e.g. "1:+1,3:-1" '
                         '(voice indices in low->high order; +1 = one octave up)')
    ap.add_argument('--remap-pads', default='',
                    help='voice indices to re-map onto distinct keys by drum-hit frequency '
                         '(for TR909/Handpan pad instruments, e.g. "0")')
    ap.add_argument('--drum-drop', type=int, default=0,
                    help='with --remap-pads: drop the N most frequent kit pieces '
                         '(1 = remove the hi-hat wall)')
    ap.add_argument('--nosplit', action='store_true', help='disable voice splitting')
    ap.add_argument('--prune', type=int, default=0,
                    help='max notes per second per voice (0=off); keeps loudest')
    ap.add_argument('--local-tracks', default='',
                    help='comma list of track indices rendered local-only (no RPC), e.g. "2,3,8"')
    ap.add_argument('--local-chans', default='',
                    help='comma list of 0-based channel indices rendered local-only (no RPC)')
    ap.add_argument('--drop-tracks', default='',
                    help='comma list of track indices to discard entirely (e.g. duplicate cannons)')
    ap.add_argument('--dedupe', action='store_true', default=True,
                    help='drop a voice whose (time,pitch) stream duplicates an earlier one')
    args = ap.parse_args()

    data, tracks, division = parse_midi(args.midi)
    all_notes = []   # (start_ms, dur_ms, pitch, vel, track, ch)
    tmap = build_tempo_map(sum((track_notes(data, s, l)[1] for s, l in tracks), []), division)

    total_raw = 0
    for ti, (s, l) in enumerate(tracks):
        notes, _ = track_notes(data, s, l)
        if not notes:
            continue
        for st, en, pitch, vel, ch in notes:
            if en <= st:
                en = st + 1
            ms0 = tick_to_ms(st, tmap, division)
            ms1 = tick_to_ms(en, tmap, division)
            all_notes.append((ms0, ms1 - ms0, pitch, vel, ti, ch))
            total_raw += 1
    if not all_notes:
        raise SystemExit('no notes found')
    drop_tracks = set()
    for x in args.drop_tracks.split(','):
        x = x.strip()
        if x:
            drop_tracks.add(int(x))
    if drop_tracks:
        before = len(all_notes)
        all_notes = [n for n in all_notes if n[4] not in drop_tracks]
        print('dropped tracks %s: %d -> %d notes' % (sorted(drop_tracks), before, len(all_notes)))
    all_notes.sort(key=lambda n: n[0])

    # transpose: maximize white-key coverage. GM percussion (channel 10, idx 9)
    # is EXCLUDED from the vote — its atonal hits flatten the histogram and
    # used to pick the wrong rotation, snapping scale notes onto blacks and
    # darkening major pieces into a faux-minor sound.
    pitched = [n for n in all_notes if n[5] != 9]
    best_t, best_hits = 0, -1
    for t in range(-6, 6):
        hits = sum(1 for n in pitched if (n[2] + t) % 12 in WHITE)
        if hits > best_hits:
            best_hits, best_t = hits, t
    snapped = sum(1 for n in pitched if (n[2] + best_t) % 12 not in WHITE)

    # voices by (track, ch); wide voices split at --split pitch (default C4)
    local_tracks = set()
    for x in args.local_tracks.split(','):
        x = x.strip()
        if x:
            local_tracks.add(int(x))
    local_chans = set()
    for x in args.local_chans.split(','):
        x = x.strip()
        if x:
            local_chans.add(int(x))
    raw_voices = {}
    for ms0, dur, pitch, vel, ti, ch in all_notes:
        raw_voices.setdefault((ti, ch), []).append((ms0, dur, pitch + best_t, vel))

    # dedupe identical voices (black midis often double the cannon track)
    if args.dedupe:
        seen_streams = []
        drop = []
        for k in sorted(raw_voices):
            ns = raw_voices[k]
            stream = frozenset((round(n[0]), n[2]) for n in ns)
            if any(stream == s for s in seen_streams):
                print('dedupe: track%d ch%d duplicates an earlier voice, dropped' % (k[0], k[1] + 1))
                drop.append(k)
            else:
                seen_streams.append(stream)
        for k in drop:
            del raw_voices[k]

    # density pruning: per voice, per 100ms bucket, keep loudest
    if args.prune > 0:
        per_bucket = max(1, round(args.prune / 10.0))
        for k, ns in raw_voices.items():
            if len(ns) <= args.prune:
                continue
            buckets = {}
            for n in ns:
                buckets.setdefault(int(n[0] // 100), []).append(n)
            kept = []
            dropped = 0
            for b, items in buckets.items():
                if len(items) > per_bucket:
                    items.sort(key=lambda n: -n[3])
                    dropped += len(items) - per_bucket
                    items = items[:per_bucket]
                kept += items
            kept.sort(key=lambda n: n[0])
            raw_voices[k] = kept

    voices = {}
    split_list = []
    if args.splits:
        split_list = sorted(int(x) for x in args.splits.split(',') if x.strip())
    for k, ns in raw_voices.items():
        if args.nosplit:
            voices[(k[0], k[1], '')] = ns
            continue
        if split_list:
            # multi-band split: notes bucketed by pitch thresholds
            bands = [[] for _ in range(len(split_list) + 1)]
            for n in ns:
                bi = 0
                while bi < len(split_list) and n[2] >= split_list[bi]:
                    bi += 1
                bands[bi].append(n)
            # merge tiny bands (<2%) into their biggest neighbour
            total_v = float(len(ns))
            merged = []
            for bi, band in enumerate(bands):
                if band and len(band) >= 0.02 * total_v:
                    merged.append(band)
                elif band:
                    if merged:
                        merged[-1] += band
                    else:
                        merged.append(band)
            for bi, band in enumerate(merged):
                if band:
                    voices[(k[0], k[1], 'B%d' % bi)] = band
            continue
        lows = [n for n in ns if n[2] < args.split]
        highs = [n for n in ns if n[2] >= args.split]
        if lows and highs and len(lows) >= 0.15 * len(ns) and len(highs) >= 0.15 * len(ns):
            voices[(k[0], k[1], 'LOW')] = lows
            voices[(k[0], k[1], 'HIGH')] = highs
        else:
            voices[(k[0], k[1], '')] = ns
    vkeys = sorted(voices, key=lambda k: sum(n[2] for n in voices[k]) / len(voices[k]))
    ins_list = [x.strip() for x in args.ins.split(',')]

    lifts = {}  # voice index (low->high order) -> diatonic octaves
    for spec in (args.lift or '').split(','):
        spec = spec.strip()
        if spec:
            vi_s, oct_s = spec.split(':')
            lifts[int(vi_s)] = int(oct_s)

    remap_pads = set()
    for spec in (args.remap_pads or '').split(','):
        spec = spec.strip()
        if spec:
            remap_pads.add(int(spec))

    print('transpose %+d semitones (%d/%d white before snapping %d)' % (best_t, best_hits, total_raw, snapped))
    print('voices (low->high):')
    out_blocks = []
    grand_total = 0
    for vi, k in enumerate(vkeys):
        ns = voices[k]
        lift_semis = lifts.get(vi, 0) * 12
        if args.compress:
            # Uniform mapping for every voice: snap to white keys, then wrap
            # octaves into the 15-key window by the SAME rule everywhere.
            # Notes already inside C4..C6 (incl. the whole melody line here)
            # are left untouched -> no contour distortion. --lift raises a
            # voice by whole octaves BEFORE the wrap: anything below C5 moves
            # into the top register, the rest keeps its place.
            idxs = [snap_to_index(p + lift_semis) for _, _, p, _ in ns]
            best_shift = lift_semis
            folded = 0
            for j in range(len(idxs)):
                while idxs[j] > 14:
                    idxs[j] -= 7; folded += 1
                while idxs[j] < 0:
                    idxs[j] += 7; folded += 1
        else:
            # per-voice octave fit: the piece is diatonic, so only +/- whole octaves
            # keep it on white keys; pick the one with fewest out-of-range folds
            # (tie-break: mean index closest to the middle of the 0..14 keyboard)
            best_shift, best_score = 0, None
            for oct_shift in (-24, -12, 0, 12, 24):
                idxs = [snap_to_index(p + oct_shift + lift_semis) for _, _, p, _ in ns]
                folds = sum(1 for i in idxs if i < 0 or i > 14)
                mean = sum(min(14, max(0, i)) for i in idxs) / len(idxs)
                score = (folds, abs(mean - 7))
                if best_score is None or score < best_score:
                    best_score, best_shift = score, oct_shift
            idxs = [snap_to_index(p + best_shift + lift_semis) for _, _, p, _ in ns]
            folded = 0
            for j in range(len(idxs)):
                while idxs[j] < 0:
                    idxs[j] += 7; folded += 1
                while idxs[j] > 14:
                    idxs[j] -= 7; folded += 1
        if vi in remap_pads:
            # Drum-kit voice on a pad instrument (TR909/Handpan = 8 pads): the
            # octave wrap collapses GM kit pitches onto 1-2 keys, which sounds
            # like no drums at all. Re-map each distinct drum pitch to its OWN
            # key, ordered by hit frequency (busiest drum -> key 0), capped at
            # 8 pads (type-2 instruments). --drum-drop N removes the N most
            # frequent kit pieces first (e.g. the hi-hat wall).
            from collections import Counter as _C
            freq = _C(n[2] for n in ns)
            if args.drum_drop > 0:
                drop = {p for p, _ in freq.most_common(args.drum_drop)}
                ns = [n for n in ns if n[2] not in drop]
                for p in drop:
                    freq.pop(p, None)
            rank = {p: i for i, (p, _) in enumerate(freq.most_common(8))}
            idxs = [rank.get(n[2], 0) for n in ns]
            folded = 0
            best_shift = 'pads'
        events = {}  # time -> [keys]
        durs = {}    # time -> dur (max of chord notes)
        for (ms0, dur, _, _), idx in zip(ns, idxs):
            t = int(round(ms0 / args.speed))
            events.setdefault(t, []).append(idx)
            durs[t] = max(durs.get(t, 0), int(round(dur / args.speed)))
        evs = sorted(events.items())
        compact = []
        note_count = 0
        for t, keys in evs:
            seen = []
            for kk in keys:
                if kk not in seen:
                    seen.append(kk)
            compact.append((t, seen, durs[t]))
            note_count += len(seen)
        ins = ins_list[vi] if vi < len(ins_list) else ins_list[-1]
        use_dur = ins in SUSTAINED
        grand_total += note_count
        avg = sum(n[2] for n in ns) / len(ns)
        print('  voice %d track%d ch%d%s: %d notes, %d events, avg midi-pitch %.1f, oct %s, folded %d -> %s%s'
              % (vi, k[0], k[1] + 1, k[2], note_count, len(compact), avg, best_shift, folded, ins,
                 ' (sustained, real durations)' if use_dur else ''))
        if use_dur:
            lines = ['{{{0},{1},{2}}}'.format(t, d, ','.join(str(x) for x in ks))
                     for t, ks, d in compact]
            fn = 'P.skyD'
        else:
            lines = ['{{{0},{1}}}'.format(t, ','.join(str(x) for x in ks))
                     for t, ks, d in compact]
            fn = 'P.sky'
        is_local = k[0] in local_tracks or k[1] in local_chans
        out_blocks.append((ins, fn, lines, is_local))

    print('total sky notes: %d (raw %d)' % (grand_total, total_raw))
    if grand_total > args.max_notes:
        print('WARNING: exceeds --max-notes %d' % args.max_notes)

    name = args.name or os.path.splitext(os.path.basename(args.midi))[0]
    dur_total = max(n[0] for n in all_notes) / args.speed / 1000.0
    out = io.StringIO()
    out.write('-- "%s" -- converted from MIDI by tools/midi2sky.py\n' % name)
    out.write('-- source: %s | transpose %+d | %d notes | ~%.1fs | speed=%.2f\n'
              % (os.path.basename(args.midi), best_t, grand_total, dur_total, args.speed))
    out.write('-- needs songs/lib.lua first\n')
    out.write('piano.clear()\n')
    for ins, fn, lines, is_local in out_blocks:
        out.write('do local E={\n')
        out.write(',\n'.join(lines))
        opt = 'ins=%s' % ins
        if is_local:
            opt += ', noRpc=true'  # 'local' is a Lua keyword -- never emit it
        out.write('\n} %s(E, {%s}) end\n' % (fn, opt))
    out.write('print("queued %d notes")\n' % grand_total)
    with open(args.out, 'w', encoding='utf-8', newline='\n') as f:
        f.write(out.getvalue())
    print('written:', args.out)

if __name__ == '__main__':
    main()

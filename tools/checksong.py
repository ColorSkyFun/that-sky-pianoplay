import re, sys
sys.stdout.reconfigure(encoding='utf-8')
path = sys.argv[1] if len(sys.argv) > 1 else 'songs/cornfield_midi.lua'
text = open(path, encoding='utf-8').read()
blocks = re.findall(r'do local E=\{(.*?)\}\s*(P\.skyD?)\(E,\s*\{ins=(P\.\w+)[^}]*\}', text, re.S)
print('blocks:', len(blocks))
ok = True
for bi, (body, fn, ins) in enumerate(blocks):
    evs = re.findall(r'\{([\d,.]+)\}', body)
    keys_hist = {}
    tmin, tmax = None, 0
    bad = 0
    ndur = 0
    for ev in evs:
        nums = [int(x) for x in ev.split(',') if x != '']
        t = nums[0]
        rest = nums[1:]
        if fn == 'P.skyD':
            dur = rest[0]; ks = rest[1:]
            ndur += 1
            if dur <= 0 or dur > 10000:
                bad += 1
        else:
            ks = rest
        for k in ks:
            if k < 0 or k > 14:
                bad += 1
            keys_hist[k] = keys_hist.get(k, 0) + 1
        tmin = t if tmin is None else min(tmin, t)
        tmax = max(tmax, t)
    top = sorted(keys_hist.items(), key=lambda kv: -kv[1])[:6]
    print('block %d %s %s: %d events, t=[%d..%d], keys(top)=%s, bad=%d'
          % (bi, fn, ins, len(evs), tmin or 0, tmax, top, bad))
    ok = ok and bad == 0
print('ALL OK' if ok else 'PROBLEMS FOUND')

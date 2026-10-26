// NotePlayer.cpp — PlaySound + note-time + broadcast gates + sustain handles.
#include "NotePlayer.h"

#include "GameState.h"
#include "Instrument.h"
#include "Offsets.h"
#include "Rpc.h"
#include "Sigs.h"
#include "Scheduler.h"

namespace piano {

const char *PlayResultName(int result) {
  switch (result) {
    case kPlayOk: return "ok";
    case kPlayBadNote: return "note out of range";
    case kPlayNoGame: return "no game pointer";
    case kPlayNoInstrument: return "no instrument (equip the piano)";
    case kPlayNotReady: return "not in world yet";
    default: return "?";
  }
}

int PlayNote(int note, int typeOverride, int subOverride, int durMs, bool noRpc) {
  if (g_game == nullptr || g_gameBase == nullptr || g_funcs.playSound == nullptr) {
    return kPlayNoGame;
  }
  if (!IsReadable(g_game, kGameReadableSize)) {
    return kPlayNoGame;
  }

  const InstrumentStatus st = GetInstrumentStatus();
  if (!st.valid) {
    return kPlayNoInstrument;
  }
  const int customType = typeOverride >= 0 ? typeOverride : g_customType.load();
  const int customSub = subOverride >= 0 ? subOverride : g_customSub.load();
  const bool custom = customType >= 0 || customSub >= 0;
  const int noteLimit = custom ? kNoteMax : (st.numNotes - 1);
  if (note < kNoteMin || note > noteLimit) {
    return kPlayBadNote;
  }
  if (!st.inWorld) {
    return kPlayNotReady;
  }
  const auto *ins = static_cast<const unsigned char *>(st.instrument);
  const unsigned localPid =
      g_data.localPlayerId != nullptr ? *g_data.localPlayerId : 0u;
  const bool useOverrides = customType >= 0 || customSub >= 0;
  int effType = customType >= 0 ? (customType & 0xFF) : st.type;
  int effSub = customSub >= 0 ? (customSub & 0xFF) : st.pitch;
  unsigned char altType = st.altType;
  // Unknown instrument layout: prefer the voice the game itself broadcast on a
  // manual key press (see CaptureNativeVoice).
  if (!useOverrides && !g_off->instrLayoutVerified && g_nativeKnown.load()) {
    effType = static_cast<int>(g_nativeType.load());
    effSub = static_cast<int>(g_nativeSub.load());
    altType = static_cast<unsigned char>(g_nativeAlt.load());
  }
  // Sustaining instruments (violin/cello/flute/...) loop until stopped, so
  // they always get a sustain handle plus a scheduled note-off; decaying
  // instruments only do when an explicit duration is requested.
  const bool wantsSustain = durMs > 0 || IsSustainingInstrument(effType, effSub);
  const unsigned char sustain = wantsSustain ? 1 : static_cast<unsigned char>(st.sustain);
  const unsigned stopDelay =
      wantsSustain ? (durMs > 0 ? static_cast<unsigned>(durMs) : kDefaultSustainMs) : 0;

  static bool firstPlay = true;
  if (firstPlay) {
    firstPlay = false;
    const char *name = InstrumentName(static_cast<unsigned char>(effType),
                                      static_cast<unsigned char>(effSub));
    const unsigned localGlobal = g_data.localPlayerId != nullptr ? *g_data.localPlayerId : 0u;
    Log("[piano-rpc] first native note: note=%d instrument=%s (type=%d sub=%d alt=%u) "
        "computedPid=%u localPidGlobal=%u",
        note, name ? name : "?", effType, effSub, static_cast<unsigned>(altType),
        st.playerId, localGlobal);
  }

  // Same sustain polyphony cap as PianoButton::OnStart.
  if (sustain == 1 && g_funcs.numSustain != nullptr) {
    void *bank = *reinterpret_cast<void *const *>(ins + g_off->instrBank);
    if (IsReadable(bank, 0x420)) {
      auto numSustain = reinterpret_cast<NumSustainFn>(g_funcs.numSustain);
      if (numSustain(bank) > 4) {
        return kPlayOk;
      }
    }
  }

  // Local sound through the game's own note player. This path never touches
  // the avatar pose controller, so the instrument UI does not need to be open
  // (unlike PianoButton::OnStart, whose animation section dereferences a NULL
  // pose controller when the UI is closed).
  char soundRef[16] = {};
  auto playSound = reinterpret_cast<PlaySoundFn>(g_funcs.playSound);
  playSound(soundRef, g_game, static_cast<unsigned>(note), sustain & 1, localPid,
            static_cast<unsigned char>(effType), static_cast<unsigned>(effSub), 0, 1.0f, 1.0f);

  // Opt-in native play animation. The pose controller only exists while the
  // instrument UI is open; guarded so it silently no-ops otherwise.
  if (g_animEnabled.load() && g_funcs.playAnim != nullptr) {
    void *pose = *reinterpret_cast<void *const *>(ins + g_off->instrPose);
    if (IsReadable(pose, 0x200)) {
      char animRet[16] = {};
      auto playAnim = reinterpret_cast<PlayAnimFn>(g_funcs.playAnim);
      // V1 takes a result buffer first; V2 takes the game pointer.
      playAnim(g_off->animTakesGame ? g_game : static_cast<void *>(animRet), note, st.instrument,
               static_cast<unsigned char>(effType), static_cast<unsigned>(effSub));
    }
  }

  // Schedule the note-off for sustained voices (local stop + stop RPC), no
  // matter whether the broadcast gates below suppress the note-on RPC.
  if (stopDelay > 0) {
    QueueStop(note, stopDelay, noRpc);
  }

  // Note-time bookkeeping, the same call OnStart makes right before the RPC.
  if (g_funcs.noteTime != nullptr) {
    auto noteTime = reinterpret_cast<NoteTimeFn>(g_funcs.noteTime);
    noteTime(g_game, note);
  }

  // Broadcast gates, identical to OnStart.
  if (g_off->muteGateEnabled
      && *reinterpret_cast<const unsigned *>(ins + g_off->instrMuteGate) != 0) {
    return kPlayOk;
  }
  bool sheetGate = false;
  void *sheetMgr = *reinterpret_cast<void **>(static_cast<char *>(g_game) + g_off->sheetMgr);
  if (IsReadable(sheetMgr, g_off->sheetInner + 8)) {
    void *inner = *reinterpret_cast<void **>(static_cast<char *>(sheetMgr) + g_off->sheetInner);
    if (IsReadable(inner, g_off->sheetGate + 4)) {
      sheetGate = *reinterpret_cast<const int *>(static_cast<char *>(inner) + g_off->sheetGate) > 0;
    }
  }
  if (sheetGate) {
    return kPlayOk;
  }

  if (!noRpc) SendNoteRpc(static_cast<unsigned>(note), static_cast<unsigned char>(effType),
              static_cast<unsigned char>(effSub), altType, st.playerId, sustain);
  return kPlayOk;
}

void StopNote(int note, bool noRpc) {
  if (g_game == nullptr || g_gameBase == nullptr) {
    return;
  }
  const InstrumentStatus st = GetInstrumentStatus();
  if (!st.valid) {
    return;
  }
  if (g_funcs.stopSustain != nullptr) {
    void *bank = *reinterpret_cast<void **>(static_cast<char *>(st.instrument) + g_off->instrBank);
    if (IsReadable(bank, 0x420)) {
      auto stopSustain = reinterpret_cast<StopSustainFn>(g_funcs.stopSustain);
      stopSustain(bank, static_cast<unsigned>(note), 1);
    }
  }
  if (!noRpc) SendStopRpc(static_cast<unsigned>(note), st.playerId);
}

int StopAllSustained() {
  if (g_game == nullptr || g_gameBase == nullptr) {
    return 0;
  }
  const InstrumentStatus st = GetInstrumentStatus();
  if (!st.valid) {
    return 0;
  }
  auto isPlaying = reinterpret_cast<IsSustainPlayingFn>(g_funcs.isSustainPlaying);
  int stopped = 0;
  if (isPlaying != nullptr) {
    void *bank = *reinterpret_cast<void **>(static_cast<char *>(st.instrument) + g_off->instrBank);
    if (!IsReadable(bank, 0x420)) {
      return 0;
    }
    for (int note = kNoteMin; note <= kNoteMax; ++note) {
      if (isPlaying(bank, static_cast<unsigned>(note)) != 0) {
        StopNote(note);
        ++stopped;
      }
    }
    return stopped;
  }
  // Builds without an IsSustainNotePlaying entry point: stop every slot. A
  // voice that is not sounding ignores the stop, so this is safe.
  for (int note = kNoteMin; note <= kNoteMax; ++note) {
    StopNote(note);
    ++stopped;
  }
  return stopped;
}

}  // namespace piano

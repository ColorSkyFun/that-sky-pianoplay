// NotePlayer.cpp — PlaySound + note-time + broadcast gates + sustain handles.
#include "NotePlayer.h"

#include "GameState.h"
#include "Instrument.h"
#include "Offsets.h"
#include "Rpc.h"
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
  if (g_game == nullptr || g_gameBase == nullptr) {
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
  const auto base = reinterpret_cast<std::uintptr_t>(g_gameBase);
  const auto *ins = static_cast<const unsigned char *>(st.instrument);
  const unsigned invalidPid =
      *reinterpret_cast<const unsigned *>(base + kRvaInvalidPlayerId);
  const int effType = customType >= 0 ? (customType & 0xFF) : st.type;
  const int effSub = customSub >= 0 ? (customSub & 0xFF) : st.pitch;
  // Sustaining instruments (violin/cello/flute/...) loop until stopped, so
  // they always get a sustain handle plus a scheduled note-off; decaying
  // instruments only do when an explicit duration is requested.
  const bool wantsSustain = durMs > 0 || IsSustainingInstrument(effType, effSub);
  const unsigned char sustain = wantsSustain ? 1 : ins[kInstrSustain];
  const unsigned stopDelay =
      wantsSustain ? (durMs > 0 ? static_cast<unsigned>(durMs) : kDefaultSustainMs) : 0;

  static bool firstPlay = true;
  if (firstPlay) {
    firstPlay = false;
    const char *name = InstrumentName(static_cast<unsigned char>(effType),
                                      static_cast<unsigned char>(effSub));
    Log("[piano-rpc] first native note: note=%d instrument=%s (type=%d sub=%d) pid=%u",
        note, name ? name : "?", effType, effSub, st.playerId);
  }

  // Same sustain polyphony cap as PianoButton::OnStart.
  if (sustain == 1) {
    void *bank = *reinterpret_cast<void *const *>(ins + kInstrBank);
    if (IsReadable(bank, 0x420)) {
      auto numSustain = reinterpret_cast<NumSustainFn>(base + kRvaNumSustain);
      if (numSustain(bank) > 4) {
        return kPlayOk;
      }
    }
  }

  // Local sound through the game's own note player (sub_1405F7730). This path
  // never touches the avatar pose controller, so the instrument UI does not
  // need to be open (unlike PianoButton::OnStart, whose animation section
  // dereferences *(instrument+0xA8) == NULL when the UI is closed).
  char soundRef[16] = {};
  auto playSound = reinterpret_cast<PlaySoundFn>(base + kRvaPlaySound);
  playSound(soundRef, g_game, static_cast<unsigned>(note), sustain & 1, invalidPid,
            static_cast<unsigned char>(effType), static_cast<unsigned>(effSub), 0, 1.0f, 1.0f);

  // Opt-in native play animation (PianoButton::PlayAnimation). The pose
  // controller only exists while the instrument UI is open; guarded so it
  // silently no-ops otherwise instead of crashing like OnStart did.
  if (g_animEnabled.load()) {
    void *pose = *reinterpret_cast<void *const *>(ins + kInstrPoseController);
    if (IsReadable(pose, 0x200)) {
      char animRet[16] = {};
      auto playAnim = reinterpret_cast<PlayAnimFn>(base + kRvaPlayAnim);
      playAnim(animRet, note, st.instrument, static_cast<unsigned char>(effType),
               static_cast<unsigned>(effSub));
    }
  }

  // Schedule the note-off for sustained voices (local stop + stop RPC), no
  // matter whether the broadcast gates below suppress the note-on RPC.
  if (stopDelay > 0) {
    QueueStop(note, stopDelay, noRpc);
  }

  // Note-time bookkeeping, the same call OnStart makes right before the RPC.
  auto noteTime = reinterpret_cast<NoteTimeFn>(base + kRvaNoteTimeFn);
  noteTime(g_game, note);

  // Broadcast gates, identical to OnStart.
  if (*reinterpret_cast<const unsigned *>(ins + kInstrMuteGate) != 0) {
    return kPlayOk;
  }
  bool sheetGate = false;
  void *sheetMgr = *reinterpret_cast<void **>(static_cast<char *>(g_game) + kGameSheetMgrOffset);
  if (IsReadable(sheetMgr, 0x48)) {
    void *inner = *reinterpret_cast<void **>(static_cast<char *>(sheetMgr) + 64);
    if (IsReadable(inner, 75572)) {
      sheetGate = *reinterpret_cast<const int *>(static_cast<char *>(inner) + 75568) > 0;
    }
  }
  if (sheetGate) {
    return kPlayOk;
  }

  if (!noRpc) SendNoteRpc(static_cast<unsigned>(note), static_cast<unsigned char>(effType),
              static_cast<unsigned char>(effSub), ins[kInstrType2], st.playerId, sustain);
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
  void *bank = *reinterpret_cast<void **>(static_cast<char *>(st.instrument) + kInstrBank);
  if (IsReadable(bank, 0x420)) {
    auto stopSustain = reinterpret_cast<StopSustainFn>(
        reinterpret_cast<std::uintptr_t>(g_gameBase) + kRvaStopSustain);
    stopSustain(bank, static_cast<unsigned>(note), 1);
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
  void *bank = *reinterpret_cast<void **>(static_cast<char *>(st.instrument) + kInstrBank);
  if (!IsReadable(bank, 0x420)) {
    return 0;
  }
  auto isPlaying = reinterpret_cast<IsSustainPlayingFn>(
      reinterpret_cast<std::uintptr_t>(g_gameBase) + kRvaIsSustainPlaying);
  int stopped = 0;
  for (int note = kNoteMin; note <= kNoteMax; ++note) {
    if (isPlaying(bank, static_cast<unsigned>(note)) != 0) {
      StopNote(note);
      ++stopped;
    }
  }
  return stopped;
}

}  // namespace piano

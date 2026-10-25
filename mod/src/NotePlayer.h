// NotePlayer.h — the native note-on/note-off path (audio + RPC, no pose).
#pragma once

#include "Common.h"

namespace piano {

enum PlayResult {
  kPlayOk = 0,
  kPlayBadNote,
  kPlayNoGame,
  kPlayNoInstrument,
  kPlayNotReady,
};

const char *PlayResultName(int result);

// Reproduces PianoButton::OnStart's audio + RPC steps without the avatar pose
// section: the game's own note sound player (sub_1405F7730) plus a
// byte-identical PianoButtonRpc. durMs > 0 stops sustaining voices after that
// delay (they also get a default 500ms auto-stop); typeOverride/subOverride
// replace the equipped instrument when >= 0. noRpc = local sound only (for
// extremely dense "black midi" walls that must not flood the network).
int PlayNote(int note, int typeOverride = -1, int subOverride = -1, int durMs = 0,
             bool noRpc = false);

// Releases one sustained note, mirroring PianoButton::StopNote: local stop
// through the bank plus the note-off RPC (skipped when noRpc).
void StopNote(int note, bool noRpc = false);

// Panic stop: releases every sustained voice that is still ringing.
// Returns the number of voices that were actually ringing.
int StopAllSustained();

}  // namespace piano

// Instrument.h — equipped-instrument state snapshot and timbre helpers.
#pragma once

#include "Common.h"
#include "Offsets.h"

namespace piano {

struct InstrumentStatus {
  void *instrument = nullptr;
  int numNotes = 0;
  bool inWorld = false;      // instrument+68108 < 0 (PlaySound's own avatar gate)
  bool sustain = false;      // instrument+79 == 1 (held-note instrument, piano = 0)
  unsigned char type = 0;    // instrument+76
  unsigned char pitch = 0;   // instrument+78
  unsigned playerId = 0;
  bool valid = false;        // instrument object readable
};

// Reads the equipped instrument through the game's own accessor and caches
// every field the mod needs. valid=false when any hop is unreadable.
InstrumentStatus GetInstrumentStatus();

// Instruments whose sound events loop until stopped (they need note-off /
// StopSustainNote; piano-family and percussion decay on their own).
bool IsSustainingInstrument(int type, int sub);

// sub_140EC55D0(type, sub) -> display name; nullptr when unknown/no game.
const char *InstrumentName(unsigned char type, unsigned char sub);

}  // namespace piano

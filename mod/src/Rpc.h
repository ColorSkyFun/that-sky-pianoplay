// Rpc.h — PianoButtonRpc construction + broadcast via the game's WriteRpc.
//
// Layout (0x48 bytes, CHAIN_ANALYSIS.md §5):
//   +0x00 vtable(0x141E6FCD0)  +0x08 metaId  +0x10 -> +0x18 self-ref
//   +0x18 rpc-chain global      +0x20 u16 0x100     +0x28 playerId  +0x2C note
//   +0x30 type  +0x31 sub  +0x32 altType  +0x38 timestamp double
//   +0x40/0x41 sustain  +0x42 isStop
#pragma once

#include "Common.h"

namespace piano {

// Note-on: sustain bytes at +0x40/+0x41 mirror PianoButton::OnStart.
bool SendNoteRpc(unsigned note, unsigned char type, unsigned char sub, unsigned char altType,
                 unsigned playerId, unsigned char sustain);

// Note-off: +0x40 = 1, +0x42 = 1 (isStop), type/sub stay 0.
bool SendStopRpc(unsigned note, unsigned playerId);

}  // namespace piano

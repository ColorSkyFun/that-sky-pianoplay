// Sigs.h — signature/RVA resolution of every game function the mod calls,
// for every supported build.
//
// V1 entries are HTModLoader E8 signatures: the byte pattern is a `call rel32`
// site (E8 kept concrete, rel32 wildcarded) and the loader computes the callee,
// so nothing is hardcoded. V2 is resolved by the RVA table alone (the build was
// mapped by hand, see docs/BUILD_VERSIONS.md).
#pragma once

#include "Offsets.h"

namespace piano {

// Resolved callee addresses. nullptr when the build has no such entry point
// (the callers then skip that step instead of crashing).
struct GameFuncs {
  void *writeRpc = nullptr;
  void *playSound = nullptr;
  void *playAnim = nullptr;
  void *noteTime = nullptr;
  void *getInstrument = nullptr;
  void *instrumentName = nullptr;
  void *numSustain = nullptr;
  void *stopSustain = nullptr;
  void *isSustainPlaying = nullptr;
};

extern GameFuncs g_funcs;

// Resolved data addresses (.rdata/.data globals the RPC path needs). Found in
// V1 through a signature on the instruction that references them; V2 uses the
// profile RVAs directly.
struct GameData {
  void *rpcVtable = nullptr;       // PianoButtonRpc vtable
  void *rpcChain = nullptr;        // rpc+0x18 chain global
  void **metaGlobal = nullptr;     // holder of a pointer; +56 = metaId
  const unsigned *localPlayerId = nullptr;  // the game's "local player" dword
};

extern GameData g_data;

// Fills g_funcs / g_data for the selected build. SelectOffsets must run first.
int ResolveGameFunctions();
int ResolveGameData();

}  // namespace piano

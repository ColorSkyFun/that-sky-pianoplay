// Rpc.cpp — builds the 0x48-byte PianoButtonRpc and hands it to WriteRpc.
#include "Rpc.h"

#include "GameState.h"
#include "Offsets.h"

namespace piano {

namespace {

// Fills the shared header fields; type/sub/sustain/stop differ per call.
unsigned char *BuildRpcHeader(unsigned char rpc[0x48]) {
  *reinterpret_cast<void **>(rpc + 0x00) =
      static_cast<char *>(g_gameBase) + kRvaPianoButtonRpcVtable;
  const void *metaGlobal = *reinterpret_cast<const void *const *>(
      reinterpret_cast<std::uintptr_t>(g_gameBase) + kRvaMetaGlobal);
  if (IsReadable(metaGlobal, 64)) {
    *reinterpret_cast<unsigned *>(rpc + 0x08) =
        *reinterpret_cast<const unsigned *>(static_cast<const char *>(metaGlobal) + 56);
  }
  *reinterpret_cast<std::uintptr_t *>(rpc + 0x10) =
      reinterpret_cast<std::uintptr_t>(rpc + 0x18);
  *reinterpret_cast<void **>(rpc + 0x18) =
      static_cast<char *>(g_gameBase) + kRvaRpcChain;
  *reinterpret_cast<unsigned short *>(rpc + 0x20) = 0x100;
  return rpc;
}

bool Dispatch(unsigned char rpc[0x48]) {
  char *net = ResolveNetModule();
  if (net == nullptr) {
    return false;
  }
  auto writeRpc = reinterpret_cast<WriteRpcFn>(
      reinterpret_cast<std::uintptr_t>(g_gameBase) + kRvaWriteRpc);
  writeRpc(net, rpc, net + 0x38);
  return true;
}

}  // namespace

bool SendNoteRpc(unsigned note, unsigned char type, unsigned char sub, unsigned char altType,
                 unsigned playerId, unsigned char sustain) {
  if (g_gameBase == nullptr) {
    return false;
  }
  unsigned char rpc[0x48] = {};
  BuildRpcHeader(rpc);
  *reinterpret_cast<unsigned *>(rpc + 0x28) = playerId;
  *reinterpret_cast<unsigned *>(rpc + 0x2C) = note;
  rpc[0x30] = type;
  rpc[0x31] = sub;
  rpc[0x32] = altType;
  // +0x33/+0x34 stay 0 (no karaoke feedback), +0x42 stays 0 (note-on).
  *reinterpret_cast<double *>(rpc + 0x38) = GameTimestamp();
  rpc[0x40] = sustain;
  rpc[0x41] = sustain;
  return Dispatch(rpc);
}

bool SendStopRpc(unsigned note, unsigned playerId) {
  if (g_gameBase == nullptr) {
    return false;
  }
  unsigned char rpc[0x48] = {};
  BuildRpcHeader(rpc);
  *reinterpret_cast<unsigned *>(rpc + 0x28) = playerId;
  *reinterpret_cast<unsigned *>(rpc + 0x2C) = note;
  // Note-off payload: type/sub stay 0, +0x40 = 1, +0x42 = 1 (isStop).
  rpc[0x40] = 1;
  rpc[0x42] = 1;
  return Dispatch(rpc);
}

}  // namespace piano

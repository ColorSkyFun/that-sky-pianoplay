// Rpc.cpp — builds the 0x48-byte PianoButtonRpc and hands it to WriteRpc.
#include "Rpc.h"

#include "GameState.h"
#include "Offsets.h"
#include "Sigs.h"

namespace piano {

namespace {

int g_headerLogs = 0;
int g_hexLogs = 0;

void LogRpcHex(const char *tag, const unsigned char *rpc) {
  static const char kDigits[] = "0123456789ABCDEF";
  char hex[3 * 0x48 + 1];
  for (int i = 0; i < 0x48; ++i) {
    hex[i * 3] = kDigits[rpc[i] >> 4];
    hex[i * 3 + 1] = kDigits[rpc[i] & 0x0F];
    hex[i * 3 + 2] = ' ';
  }
  hex[3 * 0x48 - 1] = 0;
  Log("[piano-rpc] %s rpc[48]: %s", tag, hex);
}

// Fills the shared header fields; type/sub/sustain/stop differ per call.
unsigned char *BuildRpcHeader(unsigned char rpc[0x48]) {
  *reinterpret_cast<void **>(rpc + 0x00) = g_data.rpcVtable;
  const void *metaGlobal =
      g_data.metaGlobal != nullptr ? *g_data.metaGlobal : nullptr;
  unsigned metaId = 0;
  if (IsReadable(metaGlobal, 64)) {
    metaId = *reinterpret_cast<const unsigned *>(static_cast<const char *>(metaGlobal) + 56);
    *reinterpret_cast<unsigned *>(rpc + 0x08) = metaId;
  }
  *reinterpret_cast<std::uintptr_t *>(rpc + 0x10) =
      reinterpret_cast<std::uintptr_t>(rpc + 0x18);
  *reinterpret_cast<void **>(rpc + 0x18) = g_data.rpcChain;
  *reinterpret_cast<unsigned short *>(rpc + 0x20) = 0x100;
  if (g_headerLogs < 2) {
    ++g_headerLogs;
    Log("[piano-rpc] rpc header: vtable=%p metaGlobal=%p metaObj=%p metaId=%u chain=%p ts=%.6f",
        g_data.rpcVtable, static_cast<const void *>(g_data.metaGlobal), metaGlobal, metaId,
        g_data.rpcChain, GameTimestamp());
  }
  return rpc;
}

bool Dispatch(unsigned char rpc[0x48]) {
  char *net = ResolveNetModule();
  if (net == nullptr) {
    Log("[piano-rpc] rpc drop: net module unresolved (game=%p)", g_game);
    return false;
  }
  auto writeRpc = reinterpret_cast<WriteRpcFn>(g_funcs.writeRpc);
  g_inModDispatch = true;
  writeRpc(net, rpc, net + 0x38);
  g_inModDispatch = false;
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
  if (g_hexLogs < 6) {
    ++g_hexLogs;
    LogRpcHex("note-on", rpc);
  }
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

// Hooks.cpp — detours: Game::Update drives the mod, WriteRpc is observe-only.
#include "Hooks.h"

#include <atomic>

#include "GameState.h"
#include "LuaRuntime.h"
#include "Offsets.h"
#include "Scheduler.h"
#include "Sigs.h"

namespace piano {

namespace {

std::atomic<void *> g_originGameUpdateRaw{nullptr};
std::atomic<void *> g_originWriteRpcRaw{nullptr};

// Captures the Game pointer, keeps the lua binding alive, drains the
// scheduler, then calls the original update.
void GameUpdateDetour(void *game) {
  if (game != nullptr) {
    g_game = game;
    TryBindLua(game);
  }
  TickQueue();
  auto origin = reinterpret_cast<GameUpdateFn>(g_originGameUpdateRaw.load(std::memory_order_acquire));
  if (origin != nullptr) {
    origin(game);
  }
}

// Observer only: confirm that a note we requested really went out as a
// PianoButtonRpc (local audio + broadcast happened).
void WriteRpcDetour(void *netModule, void *rpc, void *stream) {
  if (rpc != nullptr && g_data.rpcVtable != nullptr && IsReadable(rpc, 0x48)
      && *reinterpret_cast<void **>(rpc) == g_data.rpcVtable) {
    const auto *bytes = static_cast<const char *>(rpc);
    // A packet the game itself sent comes from a real key press, so it carries
    // the equipped instrument's voice — cache it for builds whose instrument
    // layout we could not map.
    if (!g_inModDispatch) {
      CaptureNativeVoice(static_cast<unsigned char>(bytes[0x30]),
                         static_cast<unsigned char>(bytes[0x31]),
                         static_cast<unsigned char>(bytes[0x32]));
    }
    // Layout (CHAIN_ANALYSIS.md §5): +0x28 playerId, +0x2C note, +0x30 type,
    // +0x31 subtype, +0x38 timestamp (double), +0x40 sustain, +0x42 isStop.
    Log("[piano-rpc] PianoButtonRpc out [%s]: note=%u type=%u.%u pid=%u sustain=%u stop=%u",
        g_inModDispatch ? "mod" : "game",
        *reinterpret_cast<const unsigned *>(bytes + 0x2C),
        static_cast<unsigned>(static_cast<unsigned char>(bytes[0x30])),
        static_cast<unsigned>(static_cast<unsigned char>(bytes[0x31])),
        *reinterpret_cast<const unsigned *>(bytes + 0x28),
        static_cast<unsigned>(static_cast<unsigned char>(bytes[0x40])),
        static_cast<unsigned>(static_cast<unsigned char>(bytes[0x42])));
    static std::atomic<int> dumps{0};
    if (dumps.fetch_add(1) < 24) {
      static const char kDigits[] = "0123456789ABCDEF";
      char hex[3 * 0x48 + 1];
      for (int i = 0; i < 0x48; ++i) {
        hex[i * 3] = kDigits[static_cast<unsigned char>(bytes[i]) >> 4];
        hex[i * 3 + 1] = kDigits[static_cast<unsigned char>(bytes[i]) & 0x0F];
        hex[i * 3 + 2] = ' ';
      }
      hex[3 * 0x48 - 1] = 0;
      Log("[piano-rpc] rpc[48] %s: %s", g_inModDispatch ? "mod" : "game", hex);
    }
  }
  auto origin = reinterpret_cast<WriteRpcFn>(g_originWriteRpcRaw.load(std::memory_order_acquire));
  if (origin != nullptr) {
    origin(netModule, rpc, stream);
  }
}

const HTAsmSig kSigGameUpdate = {
    "55 41 56 56 57 53 48 81 EC 70 02 00 00 48 8D AC 24 80 00 00 00 48 83 E4 E0 8B 81 70 18 00 "
    "00 39 05",
    HTSigScanType_Direct,
    0,
};

bool CreateHook(void *target, void *detour, std::atomic<void *> *originOut, const char *name) {
  void *origin = nullptr;
  HTStatus status = HTAsmHookCreateRaw(g_mod, target, detour, &origin);
  if (status != HT_SUCCESS) {
    Log("[piano-rpc] hook %s failed: %d", name, status);
    return false;
  }
  originOut->store(origin, std::memory_order_release);
  status = HTAsmHookEnable(g_mod, target);
  if (status != HT_SUCCESS) {
    Log("[piano-rpc] enable %s failed: %d", name, status);
    return false;
  }
  Log("[piano-rpc] hook %s at %p", name, target);
  return true;
}

}  // namespace

bool InstallHooks() {
  const auto base = reinterpret_cast<std::uintptr_t>(g_gameBase);

  void *updateTarget = nullptr;
  if (g_version != GameVersion::kV2) {
    updateTarget = HTSigScan(&kSigGameUpdate);
  }
  if (updateTarget == nullptr) {
    updateTarget = reinterpret_cast<void *>(base + g_off->rvaGameUpdate);
    Log("[piano-rpc] Game::Update by rva: %p", updateTarget);
  } else {
    Log("[piano-rpc] Game::Update found by sig: %p", updateTarget);
  }

  if (!CreateHook(updateTarget,
                  reinterpret_cast<void *>(&GameUpdateDetour),
                  &g_originGameUpdateRaw,
                  "Game::Update")) {
    return false;
  }

  // Observer only — no behaviour depends on it.
  auto *writeRpcTarget = reinterpret_cast<void *>(base + g_off->rvaWriteRpc);
  CreateHook(writeRpcTarget,
             reinterpret_cast<void *>(&WriteRpcDetour),
             &g_originWriteRpcRaw,
             "WriteRpc");
  return true;
}

}  // namespace piano

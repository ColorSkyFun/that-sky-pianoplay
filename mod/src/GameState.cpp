// GameState.cpp — global bindings + net/clock resolution.
#include "GameState.h"

#include "Offsets.h"

namespace piano {

void *g_gameBase = nullptr;
void *g_game = nullptr;

thread_local bool g_inModDispatch = false;

std::atomic<bool> g_nativeKnown{false};
std::atomic<unsigned> g_nativeType{0};
std::atomic<unsigned> g_nativeSub{0};
std::atomic<unsigned> g_nativeAlt{0};

void CaptureNativeVoice(unsigned char type, unsigned char sub, unsigned char alt) {
  if (type > 5 || sub > 15) {
    return;
  }
  const bool first = !g_nativeKnown.load();
  g_nativeType.store(type);
  g_nativeSub.store(sub);
  g_nativeAlt.store(alt);
  g_nativeKnown.store(true);
  if (first) {
    Log("[piano-rpc] equipped voice from a manual key press: type=%u sub=%u alt=%u",
        static_cast<unsigned>(type), static_cast<unsigned>(sub), static_cast<unsigned>(alt));
  }
}

std::atomic<int> g_customType{-1};
std::atomic<int> g_customSub{-1};
std::atomic<bool> g_animEnabled{false};

char *ResolveNetModule() {
  if (g_game == nullptr) {
    return nullptr;
  }
  void *client = *reinterpret_cast<void **>(static_cast<char *>(g_game) + g_off->netClient);
  if (!IsReadable(client, 0x40)) {
    return nullptr;
  }
  char *net = static_cast<char *>(*reinterpret_cast<void **>(static_cast<char *>(client) + 0x38));
  if (!IsReadable(net, 0x40)) {
    return nullptr;
  }
  return net;
}

double GameTimestamp() {
  if (g_game == nullptr) {
    return 0.0;
  }
  void *clockObj = *reinterpret_cast<void **>(static_cast<char *>(g_game) + g_off->gameClock);
  if (!IsReadable(clockObj, 0x100)) {
    return 0.0;
  }
  const auto *clock = static_cast<const char *>(clockObj);
  const std::size_t slot =
      *reinterpret_cast<const float *>(clock + 0x28) != 0.0f ? g_off->clockSlotA : g_off->clockSlotB;
  return *reinterpret_cast<const double *>(clock + slot);
}

}  // namespace piano

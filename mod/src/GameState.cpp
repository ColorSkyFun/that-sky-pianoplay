// GameState.cpp — global bindings + net/clock resolution.
#include "GameState.h"

#include "Offsets.h"

namespace piano {

void *g_gameBase = nullptr;
void *g_game = nullptr;

std::atomic<int> g_customType{-1};
std::atomic<int> g_customSub{-1};
std::atomic<bool> g_animEnabled{false};

char *ResolveNetModule() {
  if (g_game == nullptr) {
    return nullptr;
  }
  void *client = *reinterpret_cast<void **>(static_cast<char *>(g_game) + kGameNetClientOffset);
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
  void *clockObj = *reinterpret_cast<void **>(static_cast<char *>(g_game) + kGameClockOffset);
  if (!IsReadable(clockObj, 0x100)) {
    return 0.0;
  }
  const auto *clock = static_cast<const char *>(clockObj);
  const std::size_t slot = *reinterpret_cast<const float *>(clock + 0x28) != 0.0f ? 0xE0 : 0xF0;
  return *reinterpret_cast<const double *>(clock + slot);
}

}  // namespace piano

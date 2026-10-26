// Common.cpp — logging + memory-safety implementation.
#include "Common.h"

namespace piano {

HMODULE g_mod = nullptr;

namespace {

constexpr std::uintptr_t kMaxUserAddress = 0x00007FFFFFFFFFFFULL;

void AppendToLogFile(const char *text) {
  static char logPath[MAX_PATH] = {};
  if (logPath[0] == '\0') {
    char modulePath[MAX_PATH] = {};
    if (GetModuleFileNameA(g_mod, modulePath, MAX_PATH) == 0) {
      return;
    }
    char *slash = strrchr(modulePath, '\\');
    if (slash == nullptr) {
      return;
    }
    *(slash + 1) = '\0';
    snprintf(logPath, sizeof(logPath), "%spiano-rpc.log", modulePath);
  }
  FILE *fp = fopen(logPath, "a");
  if (fp == nullptr) {
    return;
  }
  fprintf(fp, "[%llu] %s\n", static_cast<unsigned long long>(GetTickCount64()), text);
  fclose(fp);
}

}  // namespace

void Log(const char *format, ...) {
  va_list args;
  va_start(args, format);
  HTTellTextV(format, args);
  va_end(args);

  char text[512] = {};
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  AppendToLogFile(text);
}

bool IsReadable(const void *ptr, std::size_t size) {
  if (ptr == nullptr) {
    return false;
  }
  const auto start = reinterpret_cast<std::uintptr_t>(ptr);
  if (start < 0x10000 || start > kMaxUserAddress) {
    return false;
  }
  if (size == 0) {
    return true;
  }
  const auto end = start + size;
  if (end < start) {
    return false;
  }
  std::uintptr_t cursor = start;
  while (cursor < end) {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(cursor), &info, sizeof(info)) != sizeof(info)) {
      return false;
    }
    if (info.State != MEM_COMMIT) {
      return false;
    }
    if ((info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
      return false;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    const auto next = base + info.RegionSize;
    if (next <= cursor) {
      return false;
    }
    cursor = next;
  }
  return true;
}

}  // namespace piano

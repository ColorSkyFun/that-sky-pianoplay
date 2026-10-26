// Common.h — shared basics: logging and safe-memory helpers.
#pragma once

#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "htmodloader/htmod.h"
#include "htmodloader/htmodloader.h"

namespace piano {

// This DLL's module handle (assigned in DllMain).
extern HMODULE g_mod;

// Logs to the HTModLoader console and to piano-rpc.log next to the DLL.
void Log(const char *format, ...);

// True when [ptr, ptr+size) is committed and readable user memory.
bool IsReadable(const void *ptr, std::size_t size);

}  // namespace piano

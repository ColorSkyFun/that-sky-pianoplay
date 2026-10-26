// Hooks.h — Game::Update and WriteRpc detours + installation.
#pragma once

#include "Common.h"

namespace piano {

// Scans Game::Update (signature first, RVA fallback) and hooks it plus the
// WriteRpc observer. Returns false when the Game::Update hook fails.
bool InstallHooks();

}  // namespace piano

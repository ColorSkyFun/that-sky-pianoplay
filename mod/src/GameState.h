// GameState.h — global game bindings and small game-state helpers.
#pragma once

#include <atomic>

#include "Common.h"

namespace piano {

// Sky.exe base address (set once in HTModOnInit).
extern void *g_gameBase;
// Game instance, captured from the Game::Update detour every tick.
extern void *g_game;

// Custom instrument overrides, -1 = follow the equipped instrument.
// They replace button+904 (instrumentType) and button+924 (subtype), which
// drive the local sound event, the RPC bytes and the remote playback.
extern std::atomic<int> g_customType;
extern std::atomic<int> g_customSub;

// When true, PlayNote also runs the game's native play animation
// (PianoButton::PlayAnimation) — safe-guarded on the pose controller, so it
// silently no-ops while the instrument UI is closed. Default false.
extern std::atomic<bool> g_animEnabled;

// Resolve the net module used by WriteRpc: *(*(game+0x1C8)+0x38).
// Returns nullptr when any hop is unreadable.
char *ResolveNetModule();

// Game clock double — the rpc+0x38 timestamp source:
// *(game+0x1B0), then +0xE0 or +0xF0 selected by the float at +0x28.
double GameTimestamp();

}  // namespace piano

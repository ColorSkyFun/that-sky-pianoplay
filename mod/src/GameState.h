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

// True while the mod itself is inside WriteRpc (used to label log lines).
extern thread_local bool g_inModDispatch;

// The equipped instrument's real voice, captured from a game-originated
// PianoButtonRpc (i.e. the player pressed a key). Builds whose instrument
// layout is not fully mapped use this instead of guessing offsets.
extern std::atomic<bool> g_nativeKnown;
extern std::atomic<unsigned> g_nativeType;
extern std::atomic<unsigned> g_nativeSub;
extern std::atomic<unsigned> g_nativeAlt;
void CaptureNativeVoice(unsigned char type, unsigned char sub, unsigned char alt);

// Resolve the net module used by WriteRpc: *(*(game+netClient)+0x38).
// Returns nullptr when any hop is unreadable.
char *ResolveNetModule();

// Game clock double — the rpc+0x38 timestamp source:
// *(game+0x1B0), then +0xE0 or +0xF0 selected by the float at +0x28.
double GameTimestamp();

}  // namespace piano

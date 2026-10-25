// dllmain.cpp — HTModLoader lifecycle entry points only.
//
// Module map (see each header for details):
//   Common.h/.cpp     logging + safe-memory helpers
//   Offsets.h         every Sky RVA / struct offset / native typedef
//   GameState.*       g_game, g_gameBase, instrument overrides, net/clock
//   Instrument.*      equipped-instrument snapshot + timbre helpers
//   Rpc.*             PianoButtonRpc build + WriteRpc broadcast (on/off)
//   NotePlayer.*      PlayNote / StopNote / StopAllSustained (audio + RPC)
//   Scheduler.*       pending-note queues drained on the game thread
//   LuaRuntime.*      sky-lua.dll API, state binding, piano.* registration
//   Hooks.*           Game::Update + WriteRpc detours
//
// piano.play(note) reproduces PianoButton::OnStart's audio + RPC steps without
// the avatar pose section: the game's own note sound player (sub_1405F7730)
// plus a byte-identical PianoButtonRpc via WriteRpc. No instrument UI needed;
// remote players still hear the note and see their local play animation.
// See CHAIN_ANALYSIS.md.
//
// Private/local research only.

#include "Common.h"
#include "GameState.h"
#include "Hooks.h"
#include "Scheduler.h"

using namespace piano;

extern "C" __declspec(dllexport) HTStatus HTMLAPI HTModOnInit(void *) {
  if (g_mod == nullptr) {
    g_mod = HTGetModuleHandle(nullptr);
  }
  if (g_mod == nullptr) {
    return HT_FAIL;
  }

  HTGameStatus gameStatus{};
  HTGetGameStatus(&gameStatus);
  g_gameBase = gameStatus.baseAddr;
  if (g_gameBase == nullptr) {
    Log("[piano-rpc] game base not found");
    return HT_FAIL;
  }
  Log("[piano-rpc] game base=%p pid=%lu", g_gameBase, gameStatus.pid);

  if (!InstallHooks()) {
    return HT_FAIL;
  }

  Log("[piano-rpc] ready: piano.play(note) calls the game's own note path");
  return HT_SUCCESS;
}

extern "C" __declspec(dllexport) HTStatus HTMLAPI HTModOnEnable(void *) {
  return HT_SUCCESS;
}

extern "C" __declspec(dllexport) HTStatus HTMLAPI HTModOnDisable(void *) {
  ClearQueue();
  return HT_SUCCESS;
}

extern "C" __declspec(dllexport) HTStatus HTMLAPI HTModOnDeinit(void *) {
  ClearQueue();
  return HT_SUCCESS;
}

extern "C" __declspec(dllexport) void HTMLAPI HTModRenderGui(float, void *) {
  // No GUI.
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_mod = hModule;
  }
  return TRUE;
}

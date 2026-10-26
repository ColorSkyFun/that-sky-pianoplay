// LuaRuntime.h — sky-lua.dll API loading, state binding and piano.* bindings.
#pragma once

#include "Common.h"

struct lua_State;
typedef int (*lua_CFunction)(lua_State *);

namespace piano {

// Lua 5.2 C API subset resolved from sky-lua.dll.
struct LuaApi {
  void (*pushcclosure)(lua_State *, lua_CFunction, int) = nullptr;
  void (*setglobal)(lua_State *, const char *) = nullptr;
  void (*createtable)(lua_State *, int, int) = nullptr;
  void (*setfield)(lua_State *, int, const char *) = nullptr;
  void (*pushinteger)(lua_State *, long long) = nullptr;
  void (*pushboolean)(lua_State *, int) = nullptr;
  long long (*tointegerx)(lua_State *, int, int *) = nullptr;
  int (*toboolean)(lua_State *, int) = nullptr;
  void (*getglobal)(lua_State *, const char *) = nullptr;
  void (*settop)(lua_State *, int) = nullptr;
  int (*type)(lua_State *, int) = nullptr;
  int (*gettop)(lua_State *) = nullptr;
  void (*pushstring)(lua_State *, const char *) = nullptr;
  void (*pushnumber)(lua_State *, double) = nullptr;
  const char *(*tolstring)(lua_State *, int, std::size_t *) = nullptr;
  double (*tonumberx)(lua_State *, int, int *) = nullptr;
  int (*pcallk)(lua_State *, int, int, int, void *, void *) = nullptr;
  int (*L_loadstring)(lua_State *, const char *) = nullptr;
  void (*rawseti)(lua_State *, int, int) = nullptr;
};

extern LuaApi g_luaApi;
extern lua_State *g_lua;  // game lua state (game+0x20), re-validated periodically

// Resolve the Lua exports from sky-lua.dll (idempotent).
bool LoadLuaApi();

// Play a local sound event through the injected __piano_emit helper.
void EmitLocal(const char *sound, int menuPitch, double volume);

// Find and bind the game's lua_State; re-registers piano.* if it was lost.
void TryBindLua(void *game);

// Create the piano.* table on L and inject the helper functions.
void RegisterLua(lua_State *L);

}  // namespace piano

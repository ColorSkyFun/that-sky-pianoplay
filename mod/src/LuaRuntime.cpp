// LuaRuntime.cpp — piano.* bindings, helper injection and state binding.
#include "LuaRuntime.h"

#include "GameState.h"
#include "Instrument.h"
#include "NotePlayer.h"
#include "Offsets.h"
#include "Scheduler.h"

namespace piano {

LuaApi g_luaApi;
lua_State *g_lua = nullptr;

// --- Argument helpers ---------------------------------------------------------

namespace {

bool GetIntArg(lua_State *L, int index, int *out) {
  int isnum = 0;
  const long long value = g_luaApi.tointegerx(L, index, &isnum);
  if (isnum == 0) {
    return false;
  }
  *out = static_cast<int>(value);
  return true;
}

// --- piano.* implementations ---------------------------------------------------

// piano.play(note[, type[, sub[, durMs]]]) — native note-on; durMs stops
// sustaining voices after that delay (default auto-stop 500ms).
int LuaPlay(lua_State *L) {
  int note = 0;
  if (!GetIntArg(L, 1, &note)) {
    g_luaApi.pushboolean(L, 0);
    return 1;
  }
  int typeOv = -1;
  int subOv = -1;
  int durMs = 0;
  (void)GetIntArg(L, 2, &typeOv);
  (void)GetIntArg(L, 3, &subOv);
  (void)GetIntArg(L, 4, &durMs);
  const int result = PlayNote(note, typeOv, subOv, durMs);
  static int failLogs = 0;
  if (result != kPlayOk && failLogs < 10) {
    ++failLogs;
    Log("[piano-rpc] piano.play(%d) failed: %s", note, PlayResultName(result));
  }
  g_luaApi.pushboolean(L, result == kPlayOk ? 1 : 0);
  return 1;
}

// piano.playAfter(note, ms[, type[, sub[, durMs[, noRpc]]]])
// noRpc = 1 时只发本地声音不发广播(黑乐谱音墙专用, 避免刷爆网络)
int LuaPlayAfter(lua_State *L) {
  int note = 0;
  int delay = 0;
  if (!GetIntArg(L, 1, &note) || !GetIntArg(L, 2, &delay)) {
    g_luaApi.pushboolean(L, 0);
    return 1;
  }
  int typeOv = -1;
  int subOv = -1;
  int durMs = 0;
  (void)GetIntArg(L, 3, &typeOv);
  (void)GetIntArg(L, 4, &subOv);
  (void)GetIntArg(L, 5, &durMs);
  // Read arg 6 via tointegerx, NOT lua_toboolean: toboolean on a missing
  // index is UB and this release Lua reads garbage above the stack top
  // (that silently noRpc'd every 5-arg call). tointegerx reports isnum=0.
  int noRpc = 0;
  {
    int isnum = 0;
    const long long v6 = g_luaApi.tointegerx(L, 6, &isnum);
    noRpc = (isnum != 0 && v6 != 0) ? 1 : 0;
  }
  QueueNote(note, delay < 0 ? 0u : static_cast<unsigned>(delay), typeOv, subOv, durMs,
            noRpc != 0);
  g_luaApi.pushboolean(L, 1);
  return 1;
}

// piano.setInstrument([type [, sub]]) — 全局音色覆盖; 传 -1/不传恢复已装备乐器.
// 例如: Flute=(3,1) Saxophone=(3,7) Cello=(1,2) Harp=(0,0) Piano=(0,1)
int LuaSetInstrument(lua_State *L) {
  const int prevType = g_customType.load();
  const int prevSub = g_customSub.load();
  int typeOv = -1;
  int subOv = -1;
  const int gotType = GetIntArg(L, 1, &typeOv);
  const int gotSub = GetIntArg(L, 2, &subOv);
  if (gotType) {
    g_customType.store(typeOv >= 0 ? typeOv : -1);
  }
  if (gotSub) {
    g_customSub.store(subOv >= 0 ? subOv : -1);
  }
  Log("[piano-rpc] setInstrument: type=%d sub=%d (prev %d,%d)",
      g_customType.load(), g_customSub.load(), prevType, prevSub);
  g_luaApi.pushinteger(L, prevType);
  g_luaApi.pushinteger(L, prevSub);
  return 2;
}

// piano.getInstrument() -> 装备type, 装备sub, 覆盖type, 覆盖sub
int LuaGetInstrument(lua_State *L) {
  const InstrumentStatus st = GetInstrumentStatus();
  g_luaApi.pushinteger(L, st.valid ? st.type : -1);
  g_luaApi.pushinteger(L, st.valid ? st.pitch : -1);
  g_luaApi.pushinteger(L, g_customType.load());
  g_luaApi.pushinteger(L, g_customSub.load());
  return 4;
}

// piano.listInstruments() -> {{type=,sub=,name=}, ...} 全部可用音色
int LuaListInstruments(lua_State *L) {
  g_luaApi.createtable(L, 16, 0);
  if (g_gameBase == nullptr || g_luaApi.rawseti == nullptr) {
    return 1;
  }
  const char *seen[64] = {};
  int seenCount = 0;
  int n = 0;
  for (int type = 0; type <= 5; ++type) {
    for (int sub = 0; sub <= 14; ++sub) {
      const char *name = InstrumentName(static_cast<unsigned char>(type),
                                        static_cast<unsigned char>(sub));
      if (name == nullptr) {
        continue;
      }
      bool dup = false;
      for (int i = 0; i < seenCount; ++i) {
        if (strcmp(seen[i], name) == 0) {
          dup = true;
          break;
        }
      }
      if (dup || seenCount >= 64) {
        continue;
      }
      seen[seenCount++] = name;
      g_luaApi.createtable(L, 0, 3);
      g_luaApi.pushinteger(L, type);
      g_luaApi.setfield(L, -2, "type");
      g_luaApi.pushinteger(L, sub);
      g_luaApi.setfield(L, -2, "sub");
      g_luaApi.pushstring(L, name);
      g_luaApi.setfield(L, -2, "name");
      g_luaApi.rawseti(L, -2, ++n);
    }
  }
  return 1;
}

// piano.clear() — panic stop: flush the pending queue AND release ringing
// sustained voices (local + note-off RPC). Returns the number of voices.
int LuaClear(lua_State *L) {
  ClearQueue();
  const int stopped = StopAllSustained();
  Log("[piano-rpc] clear: pending playback flushed, %d sustained voice(s) released", stopped);
  g_luaApi.pushinteger(L, stopped);
  return 1;
}

// piano.ready() -> game bound, instrument valid, in world, numNotes, pid
int LuaReady(lua_State *L) {
  const InstrumentStatus st = GetInstrumentStatus();
  g_luaApi.pushboolean(L, g_game != nullptr ? 1 : 0);
  g_luaApi.pushboolean(L, st.valid ? 1 : 0);
  g_luaApi.pushboolean(L, st.inWorld ? 1 : 0);  // true = 可弹(在世界内即可, 无需开界面)
  g_luaApi.pushinteger(L, st.numNotes);
  g_luaApi.pushinteger(L, st.playerId);
  return 5;
}

// piano.setAnim([bool]) — 开关本地弹琴动画(默认关)。
// 开着时每个音都会走游戏原生 PlayAnimation;乐器界面没开时姿态控制器
// 不存在,会自动跳过(不崩)。返回旧值。
int LuaSetAnim(lua_State *L) {
  const bool prev = g_animEnabled.load();
  int want = 0;
  if (GetIntArg(L, 1, &want)) {
    g_animEnabled.store(want != 0);
  }
  Log("[piano-rpc] setAnim: %d (prev %d)", g_animEnabled.load() ? 1 : 0, prev ? 1 : 0);
  g_luaApi.pushboolean(L, prev ? 1 : 0);
  return 1;
}

// piano.animReady() -> 姿态控制器是否就绪(=乐器界面已打开, 动画会生效)
int LuaAnimReady(lua_State *L) {
  const InstrumentStatus st = GetInstrumentStatus();
  bool ready = false;
  if (st.valid) {
    const void *pose = *reinterpret_cast<const void *const *>(
        static_cast<const char *>(st.instrument) + kInstrPoseController);
    ready = IsReadable(pose, 0x200);
  }
  g_luaApi.pushboolean(L, ready ? 1 : 0);
  return 1;
}

// piano.instrumentName(type, noteIndex) -> 乐器名（游戏自己的映射表）
int LuaInstrumentName(lua_State *L) {
  int type = 0;
  int noteIndex = 0;
  GetIntArg(L, 1, &type);
  GetIntArg(L, 2, &noteIndex);

  const char *name = "Piano";
  const char *resolved = InstrumentName(static_cast<unsigned char>(type),
                                        static_cast<unsigned char>(noteIndex));
  if (resolved != nullptr) {
    name = resolved;
  }
  g_luaApi.pushstring(L, name);
  return 1;
}

// piano.note(note, soundName, menuPitch, volume) -> 本地音（任意乐器）+ 同步 RPC
int LuaNote(lua_State *L) {
  int note = 0;
  if (!GetIntArg(L, 1, &note)) {
    g_luaApi.pushboolean(L, 0);
    return 1;
  }

  std::size_t len = 0;
  const char *sound = g_luaApi.tolstring(L, 2, &len);
  if (sound == nullptr) {
    sound = "PianoLocal";
  }

  int menuPitch = note;
  GetIntArg(L, 3, &menuPitch);

  double volume = 1.0;
  int isnum = 0;
  const double v = g_luaApi.tonumberx(L, 4, &isnum);
  if (isnum != 0) {
    volume = v;
  }

  // 本地音：调注入的 __piano_emit（内部建 SoundEmitter）。
  EmitLocal(sound, menuPitch, volume);

  // 同步：RPC
  const int result = PlayNote(note);
  g_luaApi.pushboolean(L, result == kPlayOk ? 1 : 0);
  return 1;
}

// piano.noteAfter(note, soundName, menuPitch, volume, delayMs) -> 定时本地音 + RPC
int LuaNoteAfter(lua_State *L) {
  int note = 0;
  if (!GetIntArg(L, 1, &note) || note < kNoteMin || note > kNoteMax) {
    g_luaApi.pushboolean(L, 0);
    return 1;
  }

  std::size_t len = 0;
  const char *sound = g_luaApi.tolstring(L, 2, &len);
  if (sound == nullptr) {
    sound = "PianoLocal";
  }

  int menuPitch = note;
  GetIntArg(L, 3, &menuPitch);

  double volume = 1.0;
  int isnum = 0;
  const double v = g_luaApi.tonumberx(L, 4, &isnum);
  if (isnum != 0) {
    volume = v;
  }

  int delay = 0;
  if (!GetIntArg(L, 5, &delay)) {
    g_luaApi.pushboolean(L, 0);
    return 1;
  }

  QueueNoteFull(note, delay < 0 ? 0u : static_cast<unsigned>(delay), sound, menuPitch, volume);
  g_luaApi.pushboolean(L, 1);
  return 1;
}

// Injected into the game Lua state so piano.note can play local audio.
constexpr char kEmitHelper[] =
    "__piano_emit = function(soundName, menuPitch, volume)\n"
    "  local e = game:objectFactory():CreateObject('SoundEmitter')\n"
    "  if e then\n"
    "    e:soundName(soundName); e:pitch(1.0); e:volume(volume)\n"
    "    e:param1('menuPitch'); e:value1(menuPitch)\n"
    "    e:param2('PlayCount'); e:value2(1)\n"
    "    e:autoPlay(true); e:SetEnabled(true)\n"
    "  end\n"
    "end\n"
    "__piano_activate = function()\n"
    "  if __piano_activated then return end\n"
    "  __piano_activated = true\n"
    "  local ok, err = pcall(function()\n"
    "    local e = game:eventBarn():AddEventByMetaName('OpenInstrumentMenu')\n"
    "    if not e then error('no OpenInstrumentMenu event') end\n"
    "    e:useEquippedInstrument(true)\n"
    "    e:localOnly(false)\n"
    "    e:localAudio(false)\n"
    "    e:Start()\n"
    "  end)\n"
    "  if not ok then\n"
    "    if sle and sle.log then sle.log('[piano] activate err: '..tostring(err)) else print(err) end\n"
    "  end\n"
    "end\n";

void RegisterField(lua_State *L, const char *name, lua_CFunction fn) {
  g_luaApi.pushcclosure(L, fn, 0);
  g_luaApi.setfield(L, -2, name);
}

// --- sky-lua.dll export loading -------------------------------------------------

template <typename T>
T GetLuaExport(HMODULE mod, const char *name) {
  return reinterpret_cast<T>(reinterpret_cast<void *>(GetProcAddress(mod, name)));
}

// lua_State layout (Lua 5.2), verified against sky-lua.dll's own build.
constexpr std::size_t kLuaThreadTag = 8;  // LUA_TTHREAD
constexpr std::size_t kLuaStateTagOffset = 0x08;
constexpr std::size_t kLuaStateTopOffset = 0x10;
constexpr std::size_t kLuaStateCiOffset = 0x20;

bool IsLuaState(const void *ptr) {
  if (!IsReadable(ptr, 0x30)) {
    return false;
  }
  const char *base = static_cast<const char *>(ptr);
  if (static_cast<unsigned char>(base[kLuaStateTagOffset]) != kLuaThreadTag) {
    return false;
  }
  const void *top = *reinterpret_cast<const void *const *>(base + kLuaStateTopOffset);
  const void *ci = *reinterpret_cast<const void *const *>(base + kLuaStateCiOffset);
  return IsReadable(top, 0x10) && IsReadable(ci, 0x10);
}

constexpr int kLuaTableTag = 5;  // LUA_TTABLE

bool HasPianoTable(lua_State *L) {
  g_luaApi.getglobal(L, "piano");
  const bool ok = g_luaApi.type(L, -1) == kLuaTableTag;
  g_luaApi.settop(L, -2);
  return ok;
}

}  // namespace

// --- Public entry points --------------------------------------------------------

bool LoadLuaApi() {
  if (g_luaApi.setglobal != nullptr) {
    return true;
  }
  HMODULE luaMod = GetModuleHandleW(L"sky-lua.dll");
  if (luaMod == nullptr) {
    return false;
  }
  g_luaApi.pushcclosure = GetLuaExport<decltype(g_luaApi.pushcclosure)>(luaMod, "lua_pushcclosure");
  g_luaApi.setglobal = GetLuaExport<decltype(g_luaApi.setglobal)>(luaMod, "lua_setglobal");
  g_luaApi.createtable = GetLuaExport<decltype(g_luaApi.createtable)>(luaMod, "lua_createtable");
  g_luaApi.setfield = GetLuaExport<decltype(g_luaApi.setfield)>(luaMod, "lua_setfield");
  g_luaApi.pushinteger = GetLuaExport<decltype(g_luaApi.pushinteger)>(luaMod, "lua_pushinteger");
  g_luaApi.pushboolean = GetLuaExport<decltype(g_luaApi.pushboolean)>(luaMod, "lua_pushboolean");
  g_luaApi.tointegerx = GetLuaExport<decltype(g_luaApi.tointegerx)>(luaMod, "lua_tointegerx");
  g_luaApi.toboolean = GetLuaExport<decltype(g_luaApi.toboolean)>(luaMod, "lua_toboolean");
  g_luaApi.getglobal = GetLuaExport<decltype(g_luaApi.getglobal)>(luaMod, "lua_getglobal");
  g_luaApi.settop = GetLuaExport<decltype(g_luaApi.settop)>(luaMod, "lua_settop");
  g_luaApi.type = GetLuaExport<decltype(g_luaApi.type)>(luaMod, "lua_type");
  g_luaApi.gettop = GetLuaExport<decltype(g_luaApi.gettop)>(luaMod, "lua_gettop");
  g_luaApi.pushstring = GetLuaExport<decltype(g_luaApi.pushstring)>(luaMod, "lua_pushstring");
  g_luaApi.pushnumber = GetLuaExport<decltype(g_luaApi.pushnumber)>(luaMod, "lua_pushnumber");
  g_luaApi.tolstring = GetLuaExport<decltype(g_luaApi.tolstring)>(luaMod, "lua_tolstring");
  g_luaApi.tonumberx = GetLuaExport<decltype(g_luaApi.tonumberx)>(luaMod, "lua_tonumberx");
  g_luaApi.pcallk = GetLuaExport<decltype(g_luaApi.pcallk)>(luaMod, "lua_pcallk");
  g_luaApi.L_loadstring = GetLuaExport<decltype(g_luaApi.L_loadstring)>(luaMod, "luaL_loadstring");
  g_luaApi.rawseti = GetLuaExport<decltype(g_luaApi.rawseti)>(luaMod, "lua_rawseti");

  if (g_luaApi.pushcclosure == nullptr || g_luaApi.setglobal == nullptr
      || g_luaApi.createtable == nullptr || g_luaApi.setfield == nullptr
      || g_luaApi.pushinteger == nullptr || g_luaApi.pushboolean == nullptr
      || g_luaApi.tointegerx == nullptr || g_luaApi.toboolean == nullptr
      || g_luaApi.getglobal == nullptr
      || g_luaApi.settop == nullptr || g_luaApi.type == nullptr
      || g_luaApi.gettop == nullptr || g_luaApi.pushstring == nullptr
      || g_luaApi.pushnumber == nullptr || g_luaApi.tolstring == nullptr
      || g_luaApi.tonumberx == nullptr || g_luaApi.pcallk == nullptr
      || g_luaApi.L_loadstring == nullptr) {
    Log("[piano-rpc] sky-lua.dll found but lua exports are missing");
    return false;
  }
  return true;
}

void EmitLocal(const char *sound, int menuPitch, double volume) {
  if (g_lua == nullptr) {
    return;
  }
  g_luaApi.getglobal(g_lua, "__piano_emit");
  g_luaApi.pushstring(g_lua, sound);
  g_luaApi.pushinteger(g_lua, menuPitch);
  g_luaApi.pushnumber(g_lua, volume);
  g_luaApi.pcallk(g_lua, 3, 0, 0, nullptr, nullptr);
}

void RegisterLua(lua_State *L) {
  g_luaApi.createtable(L, 0, 10);
  RegisterField(L, "play", &LuaPlay);
  RegisterField(L, "playAfter", &LuaPlayAfter);
  RegisterField(L, "clear", &LuaClear);
  RegisterField(L, "ready", &LuaReady);
  RegisterField(L, "note", &LuaNote);
  RegisterField(L, "noteAfter", &LuaNoteAfter);
  RegisterField(L, "instrumentName", &LuaInstrumentName);
  RegisterField(L, "setInstrument", &LuaSetInstrument);
  RegisterField(L, "getInstrument", &LuaGetInstrument);
  RegisterField(L, "listInstruments", &LuaListInstruments);
  RegisterField(L, "setAnim", &LuaSetAnim);
  RegisterField(L, "animReady", &LuaAnimReady);
  g_luaApi.pushinteger(L, kNoteMax);
  g_luaApi.setfield(L, -2, "maxNote");
  g_luaApi.setglobal(L, "piano");

  g_luaApi.pushcclosure(L, &LuaPlay, 0);
  g_luaApi.setglobal(L, "PianoPlay");

  // Inject the local-sound helper.
  if (g_luaApi.L_loadstring(L, kEmitHelper) == 0) {
    g_luaApi.pcallk(L, 0, 0, 0, nullptr, nullptr);
  }

  Log("[piano-rpc] registered piano.* (play/note/instrumentName) for lua state %p", L);
}

void TryBindLua(void *game) {
  if (game == nullptr) {
    return;
  }
  if (!LoadLuaApi()) {
    return;
  }
  if (g_lua != nullptr) {
    static int healthTick = 0;
    if (++healthTick < 300) {
      return;
    }
    healthTick = 0;
    if (IsLuaState(g_lua) && HasPianoTable(g_lua)) {
      return;
    }
    Log("[piano-rpc] lua state or piano table lost, rebinding");
    g_lua = nullptr;
  }

  void *candidate =
      *reinterpret_cast<void **>(static_cast<char *>(game) + kGameLuaStateOffset);
  static bool loggedRaw = false;
  if (!loggedRaw) {
    loggedRaw = true;
    Log("[piano-rpc] game=%p +0x20=%p", game, candidate);
  }
  if (!IsLuaState(candidate)) {
    return;
  }
  g_lua = static_cast<lua_State *>(candidate);
  RegisterLua(g_lua);
}

}  // namespace piano

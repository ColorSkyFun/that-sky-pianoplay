# Piano RPC

HTModLoader mod for Sky **0.35.1.423272** that exposes the game's own
piano-note path to Lua: `piano.play(note)` plays the note locally through
`PianoButton::PlaySound` (sub_1405F7730) and broadcasts a byte-identical
`PianoButtonRpc` through `WriteRpc` (sub_1401BC3F0) — **without** the avatar
pose section of `PianoButton::OnStart`, so the instrument UI does not need to
be open. Remote players hear the note and see their local play animation.

Private/local research only. Do not use it in multiplayer.

## Lua API

Registered as the global `piano` table once the game Lua state is found
(needs `sky-lua` loaded). See `songs/lib.lua` for a friendly wrapper.

| Lua | 说明 |
|---|---|
| `piano.play(note[, type[, sub[, durMs]]])` | 立即弹一音 (0..14)；`type/sub` 覆盖音色；`durMs` 控制延音乐器时值 |
| `piano.playAfter(note, ms[, type[, sub[, durMs]]])` | 定时弹（游戏线程派发） |
| `piano.clear()` | 急停：清空待播队列 + 掐掉正在响的延音（本地 + note-off RPC 远端同步），返回掐掉的音数 |
| `piano.setInstrument([type[, sub]])` | 全局音色覆盖，-1/不传恢复装备乐器 |
| `piano.getInstrument()` | 装备 type/sub + 当前覆盖 |
| `piano.listInstruments()` | 全部音色 `{{type=,sub=,name=},...}` |
| `piano.instrumentName(type, sub)` | 游戏自己的音色名映射 |
| `piano.ready()` | `game绑定, 乐器有效, 在世界内, 键数, pid` |
| `piano.note / piano.noteAfter` | 本地 SoundEmitter 任意音效 + 同步 RPC |
| `piano.maxNote` | `14` |

```lua
piano.play(7)                       -- C5
piano.play(7, 3, 1, 800)            -- 长笛 C5, 响 800ms
piano.clear()                       -- 急停
```

延音乐器（提琴/管乐/小提琴/极光）不给 `durMs` 会在 500ms 后自动 note-off。
同时延音上限 4（游戏的 5 音上限，超了会静默丢音）。

## Source layout

```text
src/
├─ dllmain.cpp     HTModLoader 生命周期入口（仅此而已）
├─ Common.h/.cpp   日志（控制台 + piano-rpc.log）与安全内存检查
├─ Offsets.h       全部 RVA / 结构体偏移 / 原生函数签名（0.35.1.423272）
├─ GameState.*     g_game / g_gameBase / 音色覆盖 / net 模块与时钟解析
├─ Instrument.*    已装备乐器快照（字段见 CHAIN_ANALYSIS.md §1）
├─ Rpc.*           PianoButtonRpc 0x48 字节构建 + WriteRpc 广播（note-on/off）
├─ NotePlayer.*    PlayNote / StopNote / StopAllSustained（音频 + RPC + 延音槽）
├─ Scheduler.*     延迟队列，游戏线程 Tick 派发
├─ LuaRuntime.*    sky-lua.dll API 加载、lua_State 绑定、piano.* 注册
└─ Hooks.*         Game::Update 挂钩（驱动一切）+ WriteRpc 观察者（仅日志）
songs/
├─ lib.lua         P 库：乐器/和弦表、P.at、P.rows 拍位制整谱播放器
├─ jianpu948.lua   简谱APP截图转译（钢琴主旋律 + 提琴伴奏）
└─ histheme.lua    His Theme 增强版多乐器合奏
```

## What it does

- **All nine game functions are located by E8 call-site signatures**
  (`src/Sigs.cpp`), not hardcoded RVAs. Each pattern is a `call rel32` site: the
  `E8` opcode stays concrete, the displacement is wildcarded, and HTModLoader
  computes the callee itself. RVAs in `Offsets.h` are only a logged fallback;
  startup prints `signatures resolved: N/9`.
- Hooks `Game::Update` (sig + RVA fallback): captures `Game*`, keeps the lua
  binding healthy (rebinds every 300 ticks if lost), drains the note queue.
- Hooks `WriteRpc` as an observer only — logs every outgoing `PianoButtonRpc`.
- A note play = PlaySound (local audio, no pose controller → no crash with UI
  closed) + note-time bookkeeping + the game's own broadcast gates
  (mute gate, sheet gate) + hand-built `PianoButtonRpc`.
- Sustained notes get a bank handle (`SetSustainNote`) plus a scheduled
  note-off (local stop + stop RPC), mirroring `PianoButton::StopNote`.

## Build

```sh
make SDK_ROOT=../../html-sdk
# 或者一条命令:
g++ -shared -O2 -std=c++17 -I../../html-sdk/sdk/include -o build/piano-rpc.dll \
    src/*.cpp ../../html-sdk/sdk/lib/htmodloader.lib -static -s
```

注意 `src/*.cpp` 展开顺序无关紧要，但如果 shell 不支持通配则按 Makefile 的
SRCS 列表传全。

## Install

```text
<GameDir>/htmodloader/mods/piano-rpc/
├─ piano-rpc.dll
└─ manifest.json
```

歌曲脚本不需要安装，进游戏后在 sky-lua 控制台粘贴 `songs/lib.lua` 再粘贴
任意歌曲文件即可。

## Important

- RVAs are for game version **0.35.1.423272** only. A game update will move
  them — re-check `Offsets.h` against the IDB and update CHAIN_ANALYSIS.md.
- 音高表是 C 大调全白键两八度（C4..C6 = 键 0..14），没有黑键；RPC 里没有
  音高字段，变调只能用音级平移。
- No anti-cheat bypass is included or intended.

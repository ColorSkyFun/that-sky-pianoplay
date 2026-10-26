# Piano RPC — 交接文档（给下一个 agent）

> **⚠️ 2026-10-25 全链路静态分析已完成，见 [`CHAIN_ANALYSIS.md`](CHAIN_ANALYSIS.md)。**
> 其中修正了本文的三处关键误判：
> 1. `sub_1405F8300` 不是"发送函数"，是 `PianoButton::StopNote`（note-off）；**note-on 的 RPC 构造+广播在 `sub_1405F7B70`（PianoButton::OnStart）里**。
> 2. `instrument+80` 音色库的 `+320+16*note` 槽是"当前延音句柄注册表"（`sub_14165E2C0` = SetSustainNote 写入），**不是懒加载的音符音色** —— `noteSlot==0` 是正常状态。"注入 OpenInstrumentMenu 后没加载音色"是误判，菜单当时已完整可用。
> 3. 音符按钮 vtable（`0x141E6FD00`）在 Instrument 构造（`sub_14060FA80`，随 AvatarManager 构造 `sub_140F4BB30` 内联执行）时就写好，**不是按键时才填**；该 vtable 除 slot5(+0x28)=OnStart 外其余槽位全是同一个桩 `sub_1400199B0`（"函数指针全 0"观感的来源）。
> 4. RPC 布局修正：`+0x38` 是 **double 型 pitch**，`+0x40/+0x41` sustain、`+0x42` isStop，`+0x34` 是 karaoke 字段（非 pitch）。
> 新的注入建议（静态推导）：**直接调 `sub_1405F7B70`（PianoButton::OnStart），一次调用 = 本地音+动画+RPC 广播，与手动按键同路径**，详见 CHAIN_ANALYSIS.md §9。

## 目标

在《Sky 光遇》(Sky: Children of the Light) 里，用 Lua 弹钢琴音符，**同步给其他玩家**，并且能**自定义乐器音色 / 音调 / 音符**。

用户明确要求：**不要**热键、**不要** autoexec 脚本、**不要**纯 RPC 直发（用户拒绝了）、**不要**纯本地 SoundEmitter。用户要的是走游戏自己的「按按钮 → 发 RPC → 本地播放」整条原生路径。

---

## 环境

- 游戏目录：`E:\SteamLibrary\steamapps\common\Sky Children of the Light`
- 游戏版本：**0.35.1.423272**（日志 `Sky.log` 第 36 行 `Version: 0.35.1.423272`）
- 工程目录：`D:\Workspace\ThatSkyProject\meta dump 0.33.5.390271`
- Mod 加载器：HTModLoader，mods 目录 `htmodloader/mods/`，mods：`piano-rpc`（本项目）、`skylua`（SkyLuaEngine，Lua 引擎）、`skylog`、`ssl_skip`
- **IDA MCP**（SSE，`scripts/ida_mcp.py --port <port> <tool> <json>`）：
  - `13337` = x86 0.35.1（无符号）
  - `13338` = ARM64 ELF（老版本，**有符号**，可用 `py_eval` 查符号名）
- 编译：MinGW g++（**无 make**，手动两条命令）：
  ```sh
  g++ -std=c++17 -O2 -Wall -Wextra -I../html-sdk/sdk/include -c src/dllmain.cpp -o build/dllmain.o
  g++ build/dllmain.o -o piano-rpc.dll ../html-sdk/sdk/lib/htmodloader.lib -static -s -shared
  ```
  装到 `<GameDir>\htmodloader\mods\piano-rpc\piano-rpc.dll`（重启游戏生效）。
- sky-lua 是预编译 `sky-lua.dll`，导出完整 Lua 5.2 C API（`lua_pushcclosure`/`lua_setglobal`/`lua_getglobal`/`lua_pcallk`/`luaL_loadstring`/`lua_tointegerx` 等）。游戏自己的 `lua_State` 在 `Game + 0x20`（见下）。

---

## 当前 mod 状态（`native/piano-rpc/src/dllmain.cpp`）

- hook `Game::Update`（RVA `0x116EC90`，用签名）→ 拿 `Game*`、每帧 tick 队列、注册 Lua。
- hook `WriteRpc`（`0x1BC3F0`）→ 纯观察者，看到真实 `PianoButtonRpc` 时打日志。
- 用 sky-lua.dll 导出的 Lua API 把 `piano.*` 注册进游戏 Lua 状态（已验证可用）。
- **注意：当前 `PlayNote` 是一版「只扫描、不发送」的诊断代码**（扫描音符按钮偏移，不真正发 RPC），因为上一版调 `sub_140615400` 会闪退。

### 已暴露给 Lua 的 API

```lua
piano.play(note)                  -- note 0..36
piano.playAfter(note, ms)         -- 游戏线程队列
piano.note(note, soundName, menuPitch, volume)  -- 本地 SoundEmitter + RPC
piano.instrumentName(type, noteIndex)           -- 返回乐器名（调 sub_140EC55D0）
piano.clear() / piano.ready()
```

本地音色：注入的 Lua helper `__piano_emit`（`game:objectFactory():CreateObject("SoundEmitter")` + `menuPitch`），已验证能出声、能自定义音色。

---

## 关键 RVA（0.35.1.423272，base 0x140000000）

| 符号 | RVA | 说明 |
|---|---|---|
| `Game::Update` | `0x116EC90` | hook 拿 Game；签名 `55 41 56 56 57 53 48 81 EC 70 02 00 00 48 8D AC 24 80 00 00 00 48 83 E4 E0 8B 81 70 18 00 00 39 05` |
| `Game::UpdateSync` | `0x116FDF0` | **sky-lua 已经 hook 了它**，别再 hook；它 a1 的 `+0x20` 是 lua_State |
| `sub_141763B00` | `0x1763B00` | 拿乐器对象：返回 `avatarMgr+0x30`；签名 `void*(avatarMgr, u8)`，内部检查 `*(avatarMgr+68156)&8` |
| `sub_1405F8300` | `0x5F8300` | **PianoButton::StopNote（note-off）**：停延音 + 发 note-off RPC（+0x40=1, +0x42=1）。⚠️ 旧注"发送函数"是误判，note-on 的 RPC 在 `sub_1405F7B70`（OnStart）里 |
| `sub_140615400` | `0x615400` | **note-on 处理器**（`NoteAction::Execute`），签名 `int(a1=NoteAction, a2=flags)` |
| `sub_1406153E0` | `0x6153E0` | NoteAction 拷贝构造，vtable `unk_141E70E90` |
| `sub_14060B0C0` | `0x60B0C0` | 音序器 tick，**不是**干净的音符入口（slot 用 `~note<<10` 算成野指针） |
| `sub_1405F9E20` | `0x5F9E20` | 批量：对 16 个 slot 调 `sub_1405F8300(instrument+96+1024*i)` |
| `sub_1405F94E0` | `0x5F94E0` | 乐器 reset/onopen（引用 `"tutorial_instrument_layout"`） |
| `sub_140604E50` / `sub_140607830` | `0x604E50` / `0x607830` | **音符按钮处理器**（引用了 NoteAction vtable `0x141E70E90`），还没啃透 |
| `PianoButton::PlaySound` | `0x5F7730` | ARM 里的 `PlaySound(Game*, note, PlayerId, InstrumentType, uint, double)`，x86 对应 `sub_1405F7730`（9 参数 + 栈上 float） |
| `PianoButton::PlayAnimation` | `0x5F7930` | 同上，动画 |
| `sub_1405F8C00` | `0x5F8C00` | 姿势播放，`switch(a4)` = InstrumentType → 姿势名 |
| `sub_1405F88A0` | `0x5F88A0` | 声音播放，`switch(instrumentType, note)` → `"XxxLocal"/"XxxRemote"` |
| `sub_140EC55D0` | `0xEC55D0` | **instrumentType→乐器名映射**，返回 `"Piano"/"Flute"/"Cello"/...` |
| `WriteRpc` | `0x1BC3F0` | `sub_1401BC3F0`，`(netModule, rpc, netModule+0x38)` |
| `GetMetaClassById` | `0x086990` | `sub_140086990` |

Game 内部偏移：
- `Game+0x20` = `lua_State*`
- `Game+0x1C8` = 网络客户端对象；`*(客户端+0x38)` = net module
- `Game+0x318` = avatar 管理器
- `Game+0x740` = instrument holder
- `avatarMgr+0x30` = 乐器对象（75KB）
- **音符按钮/slot**：`instrument + 96 + 1024*i`（note 在 `+896`=`0x380`，active 在 `+944`=`0x3B0`）
- 音色库 `*(instrument+80)`，16 个音符槽在 `+320 + 16*note`
- `playerId = *(u32*)(instrument+47200) ^ (u32)(instrument+47200)`
- `byte_142548352`（`0x2548352`）= 乐器激活标志

---

## PianoButtonRpc 布局（0x48 字节，来自 Serialize `0x1405F79C0`）

```
+0x00 vtable      = base + 0x1E6FCD0
+0x08 metaId      = 326  (读 *(u32*)(*(void**)(base+0x2548418) + 0x38))
+0x0C = 0
+0x10 = base + 0x5FD300
+0x18 = base + 0x1E3CDE0
+0x20 = base + 0x1510100
+0x28 playerId
+0x2C note        (0..36)
+0x30 instrumentType (u8, 0..15)
+0x34 pitch       (int, -5000..5000)
+0x38 double
+0x40 = 1, +0x41 = 1, +0x42 = 0
```

RPC 直发（已验证字节级正确）：`WriteRpc(net, &rpc, net+0x38)`，其中 `net = *(void**)(*(void**)(game+0x1C8) + 0x38)`。metaId=326、playerId=0xA121 都已和真实发送对过。

---

## 已经验证能用的

1. **Lua 绑定**：`piano.*` 成功注册进 sky-lua 的 Lua 状态（日志 `registered piano.* ...`）。
2. **本地音色**：`SoundEmitter + menuPitch` 能出声、能自定义乐器（40+ 个 `*Local` 事件，如 `PianoLocal`/`FluteLocal`/`CelloLocal`）。
3. **RPC 直发**：字节级正确的 `PianoButtonRpc` + WriteRpc，构造有效（metaId=326 能被 `GetMetaClassById` 查到、playerId 和真实发送一致）。**但没有回显**（RPC 只广播给别人，自己听不到），且远端是否收到未验证（用户没有第二个客户端）。
4. **游戏手动按键**：手动按键会正常 `sent PianoButtonRpc note=N`（证明游戏自己的 note-on 路径是对的）。

---

## 已经试过但失败 / 崩溃的

| 尝试 | 结果 |
|---|---|
| 强制 `byte_142548352` 等标志位 = 1，再调 `sub_14060B0C0` | **闪退**（`sub_14060B0C0` 的 `~note<<10` 算出野 slot） |
| 直接调 `sub_14060B0C0(instrument, game, note)` | 不发（门控 `activeFlag=0`），且 slot 计算是错的 |
| 调 `sub_140615400`，slot = `instrument+96+1024*note` | **闪退**（`Sky.exe+0x61547D`，note 按钮 vtable 为 null，读 `*(vtable+40)` 崩） |
| 注入 `OpenInstrumentMenu` 事件（`useEquippedInstrument(true)`） | 打开了钢琴 UI，但**没有加载音符音色**（`noteSlot=0`） |

---

## 核心卡点

> **⚠️ 本节结论已被 2026-10-25 的全链路静态分析推翻大半**（两条"懒初始化卡点"均不成立），详见 [`CHAIN_ANALYSIS.md`](CHAIN_ANALYSIS.md) §8。以下保留原文仅作历史记录。

「按按钮 → note-on → 本地播放 + 发 RPC」这条路，需要两样东西，而它们都是**懒初始化、且耦合在 UI/输入系统里**：

1. **音符按钮的 vtable**：`*(instrument+96)` = `0x10b99600`（堆上对象），扫描时函数指针全 0，**按键时才填充**。
2. **音符音色库**：`*(instrument+80)` 的 16 个音符槽（`+320+16*note`），**按键时才懒加载**（`noteSlot=0`）。

音符按钮在 `sub_140607830` 的循环里被确认：`_R13 = instrument+96 + (v56<<10)`，每帧对每个按钮调 `sub_1405F81A0`（这是 UI 视觉更新，不是 note-on）。

note-on 真正入口是 `sub_140615400`（NoteAction::Execute），它由**按钮按下**触发，而按钮按下处理器是 `sub_140604E50` / `sub_140607830`（两者都引用了 NoteAction vtable `0x141E70E90`）。

ARM（有符号）里干净入口是 `PianoButton::OnStart(noteButton, game)`（vtable[8]，读 note/playerId/instrumentType → `PlaySound` + `PlayAnimation` + 发 RPC）。

---

## 下一步（给新 agent 的三个方向）

**A. 继续啃按钮按下处理器**：啃 `sub_140604E50`（更大，0x29dc 字节）和 `sub_140607830`，找到「按键 → 构造 NoteAction + noteDesc + note 按钮 → 调 `sub_140615400`」的完整流程，以及 note 按钮 vtable / 音色库是怎么初始化的。这是用户明确要的方向，但很深、很脆。

**B. 用游戏原生 `PianoButton::PlaySound`**（`sub_1405F7730`，带 InstrumentType）+ RPC 直发。本地音色走游戏自己的函数（不是 SoundEmitter hack），同步走字节级 RPC。需要钉死 `sub_1405F7730` 的签名（9 参数 + 2 个栈上 float）。

**C. 务实的可靠组合**：RPC 直发（字节级正确，已通）+ SoundEmitter 本地（已通）。这是目前唯一「不崩、可复现」的方案，但用户明确拒绝过纯 RPC 直发和纯本地。

---

## 关键文件

- `native/piano-rpc/src/dllmain.cpp` — 唯一源文件
- `native/piano-rpc/manifest.json` — mod 清单
- `scripts/ida_mcp.py` — IDA MCP 客户端
- `scripts/dump_crash.py` — 解析 crashpad minidump（找闪退点用）
- `scripts/find_sig.py` — 扫描 Sky.exe 字节签名
- `scripts/pe_exports.py` — 列 PE 导出
- `scripts/*.json` — 各种 decompile 缓存（out_*.json）

---

## 日志

- mod 日志：`<GameDir>\htmodloader\mods\piano-rpc\piano-rpc.log`（`AppendToLogFile` 写文件）
- 崩溃 dump：`<GameDir>\crash_reports\reports\*.dmp`（用 `dump_crash.py` 解析）
- 游戏日志：`<GameDir>\Sky.log`

---

## 提醒

- 用户偏好中文回复，要直接可运行的代码。
- 只在本机/私有环境用，不要多人。
- `make` 不可用，手动编译。
- 游戏版本更新后所有 RVA 会变，需重新定位。
- sky-lua 只在启动时扫一次 `scripts/autoexec/*.lua`，改脚本要重启。
- 换 DLL 要完全退出游戏进程（不是关菜单）。

# that-sky-pianoplay

光遇 (Sky: Children of the Light) 钢琴/乐器自动演奏项目 —— 从静态逆向分析到
MIDI 直转多乐器合奏的完整工具链。**仅供本机/私有环境研究,不要在多人环境滥用。**

## 项目结构

```text
that-sky-pianoplay/
├─ build.ps1          一键: 编译 mod + 部署到游戏 + 重启 (工具已内建)
├─ mod/               HTModLoader C++ mod (piano-rpc.dll)
│  ├─ src/            9 个模块: Common/Offsets/GameState/Instrument/Rpc/
│  │                  NotePlayer/Scheduler/LuaRuntime/Hooks + dllmain
│  ├─ docs/LUA_API.md piano.* Lua API 完整文档
│  ├─ CHAIN_ANALYSIS.md  乐器→RPC 全链路逆向笔记 (0.35.1.423272)
│  ├─ Makefile / manifest.json / README.md
├─ docs/             INSTRUMENTS_AND_THEORY.md 乐器图鉴+15键乐理手册
├─ songs/             游戏内粘贴的 Lua 谱
│  ├─ lib.lua         P 库 (必贴): 乐器表/和弦/P.at/P.sky/P.skyD/P.rows
│  ├─ moshimo.lua / black_cornfield.lua / histheme.lua / cornfield_ensemble.lua
│  ├─ sky_*.lua       Sky Studio 社区谱转换产物
│  └─ instrument_zoo.lua  33 种音色试听
├─ tools/             转谱工具 (内建, 用法见 tools/README.md)
│  ├─ midi2sky.py     MIDI -> 15键多乐器合奏 (主力)
│  ├─ sky2lua.ps1     Sky Studio 社区谱 -> Lua
│  ├─ fetchsheets.ps1 从 StageGuard 共享谱库批量下载+转换
│  ├─ checksong.py / luacheck.py   产物校验
└─ midi/              本项目歌曲的 MIDI 源文件
```

## 快速上手

### 构建 mod (一次性)

```powershell
powershell -File build.ps1
# 编译 + 部署到 E:\SteamLibrary\...\htmodloader\mods\piano-rpc\ + 重启游戏
# 只编译: build.ps1 -NoDeploy ; 不重启: -NoLaunch
```

依赖: MinGW g++ (scoop install mingw)、`..\html-sdk\sdk`(已就位)。

### 弹歌 (每次进游戏)

1. 控制台粘贴 `songs\lib.lua` (P 库)
2. 粘贴任意 `songs\*.lua` 歌曲文件
3. `piano.clear()` 随时急停; `piano.play(7)` 试音

API 文档: `mod\docs\LUA_API.md`

### 转新歌 (工具链)

```powershell
# MIDI -> 合奏 (自动分声部/选八度/配乐器/黑键吸附/C大调/C4~C6压缩)
python tools\midi2sky.py 歌.mid -o songs\歌.lua --name "歌名" --compress `
    --ins "P.CB,P.TR,P.HA,...,P.FL" --local-chans "9"

# Sky Studio 社区谱 (175+首: StageGuard/SkyAutoPlayerScript shared_sheets)
powershell -File tools\sky2lua.ps1 谱.txt piano

# 批量拉共享谱
powershell -File tools\fetchsheets.ps1
```

## 核心概念速记

- **15 键 = C4~C6 全白键**, 无黑键; RPC 无音高字段, 变调只能音级平移
- **延音乐器**(提琴/管乐/极光)需要 durMs, 否则 500ms 自动停; 并发 ≤4
- **noRpc 第 6 参**: 本地音墙不发广播 (黑乐谱/密集织体专用), `P.sky(..., {noRpc=true})`
- 延音句柄、RPC 布局、全部 RVA: 见 `mod\CHAIN_ANALYSIS.md` 与 `mod\src\Offsets.h`

## 已验证曲目

| 歌 | 来源 | 亮点 |
|---|---|---|
| もしも命が描けたら (YOASOBI) | midi/ | 21 声部, 长笛主奏, C 大调压缩 |
| Cornfield Chase BLACK | midi/ | 黑乐谱 128k 音符剪枝, 本地音墙 + 10 乐器 |
| Cornfield Chase 合奏版 | qinpu123 双钢琴大谱 | 四层编制 (识谱转译) |
| 夜空中最亮的星 等 7 首 | StageGuard 共享谱 | sky2lua 直转 |

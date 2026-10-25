# songs/ — 弹奏脚本

用法：进游戏后先把 `lib.lua` 整段粘贴执行（定义全局 `P` 库），再粘贴任意歌曲文件。

| 文件 | 曲目 | 时长 | 编制 |
|---|---|---|---|
| `lib.lua` | 共用库（乐器表 / 和弦表 / `P.at` / `P.sky` / `P.rows`） | — | — |
| `jianpu948.lua` | 简谱APP截图转译（24行95小节） | ≈3:18 | 钢琴主旋律 + 大提琴和弦 + 低音提琴 |
| `histheme.lua` | His Theme (Undertale) 增强版 | ≈2:28 | 小提琴/长笛/竖琴/大提琴/低音提琴/钢片琴 |
| `sky_夜空中最亮的星.lua` | Sky Studio 社区谱(转换器生成) | 70s | 钢琴(带和弦) |

## Sky Studio 社区谱直读(推荐找谱方式)

社区最大的谱子生态就是 Sky Studio 格式(`.txt` JSON),直接转换即可用:

```powershell
# 175+ 首共享谱: github.com/StageGuard/SkyAutoPlayerScript 的 shared_sheets/
powershell -File tools\sky2lua.ps1 下载的谱.txt [乐器名]
# 乐器名: piano(默认)/ap/violin/cello/flute/harp/glock/bass
# 生成 songs\sky_<曲名>.lua,游戏里贴完 lib.lua 再贴它
```

## MIDI 直转(任意歌曲)

```powershell
python tools\midi2sky.py 谱.mid -o songs\xxx.lua --name "曲名" --ins "P.CB,P.PI,P.PI,P.VI"
```

自动: 解析全部音轨/通道 → 按 (轨,通道) 分声部 → 横跨音域的声部在 C4 切分 →
每个声部自动选八度塞进 15 键 → 黑音就近吸附白键 → `--ins` 按声部**从低到高**配乐器
(延音乐器自动带真实时值, 用 P.skyD)。示例 `cornfield_midi.lua`。
`tools\checksong.py 生成的.lua` 可校验键位范围/时值合法性。

游戏内也可以直接粘乐谱 JSON 原文:

```lua
P.skyJson([[{"name":..,"songNotes":[{"time":0,"key":"2Key0"},..]}]], {ins=P.PI})
```

`"NKeyM"` 的 **M 就是 0..14 键号**(N 是编辑器页码,忽略),同 time 为和弦;
`speed=2` 快一倍,`dur=800` 配延音乐器(P.VI/P.CE/P.FL)用。
MIDI 谱可先用网页工具 dh-114/midi-to-sky-studio 转成 SkyStudio 格式再走上面流程。

## P 库速查

```lua
P.at(ms, note, P.PI[, durMs])   -- 定时弹一音
P.deg("1'")                     -- 唱名转键号 (1'=C5=7)
P.strum(t, {0,2,4}, P.PI)       -- 三音琶奏
P.sky(E, opts)                  -- Sky Studio 事件表 {t,键...} 播放
P.skyJson(json, opts)           -- Sky Studio JSON 原文直接弹
P.rows(ROWS, opts)              -- 拍位制整谱播放(见 jianpu948.lua 头部注释)
piano.clear()                   -- 急停: 清队列 + 掐掉正在响的延音(远端同步)
```

## 新增歌曲

1. **有 Sky Studio 谱(最优)**：`tools\sky2lua.ps1` 一键转换。
2. 简谱截图转译：按 `"唱名@拍位"` 填 `ROWS`（每小节一个表，拍位=4/4内起始拍），
   最后 `P.rows(ROWS, {Q=...})`。
3. 自由编配：参考 `histheme.lua`，用 `P.at` + 乐器表逐声部排。
   延音乐器（提琴/管乐/VI/MA）长音记得给 `durMs`，否则 500ms 自动停。
   同时延音 ≤4（mod 上限 5 会丢音）。


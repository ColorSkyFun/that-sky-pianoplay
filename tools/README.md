# tools/ — 转谱工具链

全部内建, 无外部服务依赖 (除下载谱子时访问 GitHub)。统一从项目根目录运行。

## midi2sky.py — MIDI → 15 键多乐器合奏 (主力)

```powershell
python tools\midi2sky.py <歌.mid> -o songs\<歌>.lua --name "曲名" [选项]
```

| 选项 | 作用 |
|---|---|
| `--compress` | 全曲统一压缩到 C4~C6 (保留声部相对音区, 主旋律不失真) — **推荐必加** |
| `--ins "P.CB,P.PI,..."` | 按声部**从低到高**配乐器; 延音乐器自动带真实时值 (走 P.skyD) |
| `--local-chans "9,8"` | 指定 GM 通道 (0 基) 只发本地声音不发 RPC (鼓/音墙) |
| `--local-tracks "2,3"` | 指定 MIDI 轨 (0 基) 本地化 (黑乐谱音墙) |
| `--drop-tracks "10"` | 整轨丢弃 (重复音炮) |
| `--prune 30` | 每声部每秒最多 30 音 (保最响), 0=关 |
| `--splits "48,60,72"` | 按音高阈值多段切声部 (默认单阈值 C4 自动切) |
| `--nosplit` | 关闭切分 |
| `--semitones N` *(规划)* | 手动指定平移; 目前自动选 (已排除鼓轨干扰) |

自动行为: 按 (轨,通道) 分声部 → 宽音域声部在 C4 切分 → 黑键就近吸附白键 →
调性检测排除 GM 鼓轨后选最佳 C 大调旋转。

## sky2lua.ps1 — Sky Studio 社区谱 → Lua

```powershell
powershell -File tools\sky2lua.ps1 <谱.txt> [piano/ap/violin/cello/flute/harp/glock/bass]
# 生成 songs\sky_<曲名>.lua
```

谱源: `https://github.com/StageGuard/SkyAutoPlayerScript` 的 `shared_sheets/`
(175+ 首, JSON 格式, `"NKeyM"` 的 M 即 0..14 键号)。

## fetchsheets.ps1 — 批量下载共享谱

编辑 `sheets\fetchlist.txt` (一行一个文件名), 然后:

```powershell
powershell -File tools\fetchsheets.ps1
# 下载到 tools\sheets\ 并逐个转成 songs\sky_*.lua
```

## checksong.py / luacheck.py — 产物校验

```powershell
python tools\checksong.py songs\<歌>.lua   # 键位 0..14 / 时值合法性 / 分块统计
python tools\luacheck.py                   # lib.lua 关键字误用/括号配平 (防 local= 这类坑)
```

## 依赖

- Python 3.10+ (标准库, 无 pip 依赖)
- PowerShell 5.1+ (Windows 自带)
- g++ 仅 build.ps1 需要

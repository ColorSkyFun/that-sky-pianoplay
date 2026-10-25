# piano-rpc Lua API 文档

`piano-rpc.dll` 加载后会自动在**游戏自己的 Lua 状态**里注册全局表 `piano`
(需要 sky-lua 已加载;一般进游戏几秒后自动就绪)。所有函数直接操作游戏原生
弹琴链路:本地出声 + 广播 `PianoButtonRpc`,**远端玩家能听到**。

> 仅供本机/私有环境研究使用,不要多人环境滥用。

---

## 快速开始

```lua
piano.ready()            -- 检查状态(见下)
piano.play(7)            -- 立即弹 C5
piano.playAfter(9, 500)  -- 500ms 后弹 E5
piano.clear()            -- 急停:清掉所有排队的音 + 掐掉正在响的延音
```

弹歌前建议先 `piano.clear()` 顶掉上一首没播完的。

---

## 键位(0..14)

15 键 = **C 大调全白键两个八度**,没有黑键。

| 键 | 音 | 唱名 | 键 | 音 | 唱名 |
|---|---|---|---|---|---|
| 0 | C4 | 1 | 8 | D5 | 2' |
| 1 | D4 | 2 | 9 | E5 | 3' |
| 2 | E4 | 3 | 10 | F5 | 4' |
| 3 | F4 | 4 | 11 | G5 | 5' |
| 4 | G4 | 5 | 12 | A5 | 6' |
| 5 | A4 | 6 | 13 | B5 | 7' |
| 6 | B4 | 7 | 14 | C6 | 1'' |
| 7 | C5 | 1' | | | |

RPC 里没有音高字段,**变调只能按音级平移**(±N 后对 0..14 取范围),半音变调
无法同步到远端。

---

## piano.* 完整参考

### piano.play(note[, type[, sub[, durMs]]]) → bool

立即弹一个音。这是核心函数,内部做四件事(全部走游戏原生路径):

1. `PianoButton::PlaySound`(sub_1405F7730)—— 本地出声;
2. note-time 记账(和真实按键一致);
3. 过游戏的广播闸门(静音闸 / 乐谱闸);
4. 手工构建 0x48 字节 `PianoButtonRpc` 交给 `WriteRpc` 广播。

- `note`: 0..14(默认上限为当前乐器键数-1)
- `type, sub`: 临时音色覆盖(见乐器表);不传则用全局覆盖(setInstrument)或装备乐器
- `durMs`: 延音乐器的发音时长(毫秒)。**延音乐器不给也会 500ms 自动停**
- 返回 `false` 表示失败(原因看 piano-rpc.log:没进世界/没装乐器/键越界)

```lua
piano.play(7)                -- 装备乐器弹 C5
piano.play(7, 3, 1, 800)     -- 长笛 C5, 响 800ms
piano.play(0, 1, 2)          -- 大提琴 C(音色低八度)
```

### piano.playAfter(note, ms[, type[, sub[, durMs]]]) → bool

定时版:入队,游戏线程到点派发。排歌就是批量调它。

```lua
for i,n in ipairs({0,2,4,7}) do piano.playAfter(n, (i-1)*400) end
```

### piano.clear() → number

急停。清空待播队列 + 遍历 15 个延音槽,正在响的逐个本地停止并广播
note-off RPC(**远端一起停**)。返回掐掉的延音数。
钢琴类自然衰减音掐不掉(没有延音句柄),余音 ≤1s 属正常。

### piano.setInstrument([type[, sub]]) → prevType, prevSub

全局音色覆盖;之后所有不带 type/sub 的 play 都用它。
传 -1 或不传恢复装备乐器。

```lua
piano.setInstrument(0, 11)   -- 全部按小提琴发声
piano.setInstrument(-1)      -- 恢复
```

### piano.getInstrument() → type, sub, overrideType, overrideSub

装备乐器的 type/sub + 当前覆盖值(未覆盖为 -1)。

### piano.listInstruments() → { {type=,sub=,name=}, ... }

全部可用音色(游戏自己的映射表)。

### piano.instrumentName(type, sub) → string

单个音色名。

### piano.setAnim([bool]) → prev

**弹琴动画开关,默认关**。打开后每个音额外调用游戏原生
`PianoButton::PlayAnimation`(和真实按键同一份代码,按乐器选姿态:
钢琴 MusicPoseHuge、大提琴 CelloPose、贝斯 BassPose...)。

- 动画依赖姿态控制器(`instrument+0xA8`),它**只在乐器界面打开时存在**;
  界面没开时 mod 会自动跳过动画(不会像早期那样崩溃),声音照常。
- 所以要看到动作:先打开乐器界面(或执行注入的 `__piano_activate()`),
  再 `piano.setAnim(true)`。

```lua
__piano_activate()       -- 或手动打开乐器界面
piano.setAnim(true)
piano.play(7)            -- 有人形弹琴动作
```

### piano.animReady() → bool

姿态控制器是否就绪(=乐器界面已打开)。`true` 时开 setAnim 才有动作。

### piano.ready() → gameOk, instrumentOk, inWorld, numNotes, pid

- `gameOk`: Game 指针已捕获
- `instrumentOk`: 已装备乐器
- `inWorld`: 已进世界(**弹奏只要求这个,不需要开乐器界面**)
- `numNotes`: 键数(15/8/4)
- `pid`: 自己的玩家 id(RPC 里的 playerId)

### piano.note(note[, soundName[, menuPitch[, volume]]]) → bool
### piano.noteAfter(note, soundName, menuPitch, volume, delayMs) → bool

本地 SoundEmitter 播**任意音效名**(不限于乐器音色)+ 同步一个 note RPC。
menuPitch 控制音高映射,volume 0..1。

### piano.maxNote

常量 `14`。

---

## 延音乐器与时值规则

以下乐器声音是循环事件,不停就一直响(提琴/管乐类):

| 类别 | type.sub | 乐器 |
|---|---|---|
| type 1 | 0/1/2 | Contrabass / 4thAnnivBass / **Cello** |
| type 3 | 1..10 | **Flute**/Quena/**Bugle**/Ocarina/GoldBugle/AP18Ocarina/**Saxophone**/4thAnnivArp/Harmonica/AP29Piccolo |
| type 0 | 9 / 11 | MicAurora / **Violin** |

规则:

- `durMs > 0`:到点自动 note-off(本地停 + 远端停)。
- 不给 `durMs`:**500ms 自动停**(防止大提琴无限响)。
- **同时延音 ≤ 4**(游戏原生 5 音上限,mod 按同样规则超限丢音),编配时控制
  提琴/管乐的并发声部数。
- 钢琴/竖琴/钢片琴/鼓类自然衰减,无上述限制。

常用音色速查:`{0,1}`钢琴 `{0,2}`APPiano `{0,0}`竖琴 `{0,4}`钢片琴
`{0,11}`小提琴 `{1,2}`大提琴 `{1,0}`低音提琴 `{3,1}`长笛 `{3,3}`小号
`{3,7}`萨克斯 `{3,4}`奥卡里那。

---

## P 库(songs/lib.lua)

进游戏先粘贴执行 `songs/lib.lua`,得到全局 `P`:

```lua
P.PI  -- {0,1} 乐器速查表: PI/AP/HA/GL/VI/MA/CE/CB/FL/OC/BU/SAX
P.CH  -- 和弦表: C/F/G/Am/Dm/Em (m根音 b五度 t三和弦, 全是键号)

P.at(ms, note, P.PI[, durMs])     -- 定时弹一音
P.deg("1'")                       -- 唱名转键号 -> 7
P.strum(t, {0,2,4}, P.PI)         -- 三音琶奏
P.rows(ROWS, opts)                -- 拍位制整谱播放
```

`P.rows` 的 ROWS 格式(简谱 APP 截图转译的标准格式):

```lua
-- 每行 = 若干小节; 每小节 = {"唱名@拍位", ...}; 拍位 = 4/4 小节内起始拍
-- 时值 = 到同小节下一音的距离; 十六分用 2.75/3.25 这类拍位
local ROWS = {
  {"1'@0","7@1","1'@2","7@3"},          -- 4 个四分音符
  {"1'@0","7@0.5","1'@1","7@1.5"},      -- 4 个八分音符
  {},                                    -- 空小节(休止)
}
P.rows(ROWS, {
  Q=520,                                 -- 四分音符毫秒(≈115bpm)
  mel=P.PI,                              -- 旋律乐器(默认钢琴)
  rot={"Am","F","C","G"},                -- 每行 4 小节的和弦轮换
  rotClose={"G","Am","F","C"},           -- closeFrom 起的结尾段轮换
  closeFrom=20,                          -- 结尾段从第几行开始
  lastChord={"G","G","C"},               -- 最后一行的和弦
  chords=true, bass=true,                -- 可关掉大提琴和弦/低音提琴
})
```

弱起小节(第一个音 ≥ 第 2 拍)和空小节自动跳过伴奏。伴奏编制固定为:
大提琴三和弦延音(每小节)+ 低音提琴根五交替(每小节两拍各一音)。

---

## 常见问题

**弹了没声音 / ready 返回 false?** 按顺序检查:① `piano.ready()` 第 1 个值
(sky-lua/注入是否就绪);② 第 2 个值(有没有**装备**乐器,不用打开界面但
身上要背着);③ 第 3 个值(进世界没有)。

**远端听不到?** 游戏原生闸门生效中:乐器静音闸(instrument+47248)、
乐谱界面打开时静音。另外确认不在"离线/故事模式"。

**音不对/想要黑键?** 15 键只有白键。需要半音变化只能换调重排。

**崩溃?** mod 对所有游戏指针做了可读性检查,理论上不再需要"打开乐器
界面"防崩(早期崩溃是姿态控制器空指针,现在只有显式开 setAnim 才会碰
姿态路径,且有护栏)。

**换曲?** 新曲代码开头加 `piano.clear()`,旧的立即停。

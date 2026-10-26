# 支持的游戏版本（多版本偏移表）

一个 `piano-rpc.dll` 同时支持下列客户端。`HTModOnInit` 会先读模块 PE 头里的
`SizeOfImage` / `TimeDateStamp` 选 profile，再解析地址；后续所有指针/偏移都走
`g_off`，所以两套版本可以共用同一份代码。

| profile | 识别特征 | 备注 |
| --- | --- | --- |
| V1 `0.35.1.423272` | SizeOfImage `0x3171000`，timestamp `0x6ACFD5EF` | 旧 Steam 版，函数全部走 `Sigs.cpp` 的 E8 签名 |
| V2 `new dump` | SizeOfImage `0x843D000`，timestamp `0x6ADB03C9` | 新版（无符号 dump）；按下面的 RVA 表直接解析 |

PE 头读不到或两个特征都不匹配时按 V1 处理（保持旧行为）。

## V2 地址表（相对模块基址）

全部来自 `Sky_dump.exe`（imagebase `0x7FF613520000`，`.text` RVA `0x1000`，
`.pdata` 在 dump 里已损坏，不能当函数表用）。

### 入口

| 用途 | RVA | 证据 |
| --- | --- | --- |
| `Game::Update` | `0x14CE980` | 与 V1 同形：`mov eax,[rcx+0x1C80]; cmp [rip+g],eax` 帧计数缓存（V1 是 `[rcx+0x1870]`） |
| `WriteRpc` | `0x2B8DD0` | OnStart 里 `rcx=[game+0x1E8]→[+0x38]`、`rdx=rpc`、`r8=net+0x38`；体内按 `[obj+0x38]` 查 map，与 V1 一致 |
| `PlaySound` | `0x7C9620` | 本地音；写 `[clock+0xE8/0xF8]` 时间戳到 `[avatar+0x9F9A]`；RPC 接收端也调它 |
| `PlayAnimation` | `0x7CAC00` | 引用 `MusicPoseHuge`（RVA `0x379AB8F`）等姿态名 |
| `NoteTime` | `0x7C88B0` | OnStart 里 `(game, note)` 两参调用，紧邻 RPC 构造 |
| `GetInstrument` | `0x1B331D0` | `(avatarMgr, 1) → instrument`，OnStart/PlayAnimation 都这么调 |
| `StopSustainNote` | `0x1A26C60` | `(bank, note, 1)`；`note>15` 走全槽分支，槽位 `+0x140`/步长 `0x10`，与 V1 相同 |
| `NumSustainNotesPlaying` | `0x1A271A0` | 数 `[bank+0x140+16*i]` 两 qword 非空且 `vtable[+0x18]` 为真 |
| `InstrumentName` | `0x11CD020` | 与 V1 同形：`cmp cl,4` → 相对偏移跳转表（`0x2388368`）→ 按 `type<<8\|sub` 查各 type 的名称表（type0 `0x2388270`，首项 `Piano`） |
| `IsSustainNotePlaying` | `0x1A27130` | 与 V1 `0x165E7F0` 逐句同形：`cmp edx,0xF` → 槽 `[bank+0x140+16*note]` → `vtable[+8]` 校验 → `vtable[+0x18]` |

### RPC 数据

| 用途 | RVA | 证据 |
| --- | --- | --- |
| `PianoButtonRpc` vtable | `0x22EC510` | OnStart 里 `lea` 写 `rpc+0x00`；`vtable[1]=0x7C9350` 就是读取 `+0x2C/+0x30/+0x40` 的接收端 `Run` |
| `rpc+0x18` chain 对象 | `0x22AFAE0` | OnStart 里 `lea rcx` 写 `rpc+0x18`，`rpc+0x10=&rpc+0x18` |
| meta global | `0x436B100` | OnStart 读 `[global]` 再取 `+0x38` 写 `rpc+0x08`（metaId） |
| 本机玩家 id | `0x436ADA0` | OnStart 用它与按钮 `+0x504` 比较判断"本地"，也作为 PlaySound 的 pid |

RPC 载荷布局与 V1 **完全一致**（`+0x28 playerId / +0x2C note / +0x30 type /
+0x31 sub / +0x32 altType / +0x38 double 时间戳 / +0x40,+0x41 sustain /
+0x42 isStop`，对象 `0x48` 字节，`+0x20 = 0x100`），由 OnStart 的构造代码逐条核对。

### RPC 序列化字段（vtable slot 3 = `0x7C9910`）

序列化函数只读这些字段，**其余字节（含 `+0x43`）不参与打包**：

| 偏移 | 类型 | 含义 |
| --- | --- | --- |
| `+0x28` | u32 | playerId（专用编码器 `0x117E800`） |
| `+0x2C` | u32 | note（范围 0x24） |
| `+0x30` / `+0x31` / `+0x32` | u8 & 0xF | type / sub / altType |
| `+0x33` | u8 | karaoke（范围 6） |
| `+0x34` | i32 | 夹到 `[-5000, 5000]` |
| `+0x38` | double | 时间戳 |
| `+0x41` / `+0x42` / `+0x40` | u8 | sustain / isStop / sustain |

实测抓包：手动按键的 `rpc[48]` 与本 mod 生成的包除 note/type/sub/时间戳外逐字节相同；
`+0x20..0x27` 的 `BF 87 CE 00` 和 `+0x43` 的 `3F` 是 OnStart 栈帧里的残留数据，不影响发送。

### 乐器 type/sub 的正确来源（新版，未完全打通）

新版 `GetInstrument`（`0x1B331D0`）返回的是 `avatarMgr + 0x30`（内联子对象），
旧的 `+0x4C/+0x4E` 字段在这个版本里读出来是 0/0，而钢琴实际是 `type=0 sub=1`。

静态追到的布局：

- `PianoButton` 类大小 **0x580**（`0x7E0430` 析构里 `mov edx,0x580`），vtable 为
  **`0x22EC540`**（slot 5 = OnStart `0x7C9A30`）。
- 乐器控件对象由 `0x7E0840` 构造：**16 个 PianoButton 在 `控件+0x70`，步长 0x580**
  （`add rbx,0x580` / `cmp rbx,0x5800`），槽内 `+0x500 note / +0x504 pid /
  +0x508 type / +0x509 altType / +0x51C sub / +0x530 sustain`。
- 这个控件是 **独立堆对象，大小 0x19FC0**：`0x1261AC0` 的注册表构造器里
  `mov edx,0x19FC0` 分配、memset、调用 `0x7E0840`，然后挂到注册表的 `+0x40` 和 `+0x2D0`。
  这个注册表 **不是** `*(game+0x330)`（那个才是 `GetInstrument` 用的 avatarMgr，
  其 `+0x30` 是内联的仪器状态，含 `+0x90 bank / +0xB8E0 pid / +0x10AA0 loaded`）。
  所以从 mod 拿到的 instrument 指针没法直接走到按钮控件。

因此从 mod 拿到的 instrument 指针没法直接走到按钮控件。**不再做运行时扫描**
（扫不到，而且每个音符探测内存会卡顿），改为：

1. 未证实布局的版本直接默认钢琴音色 `type=0 sub=1`（正是手动按钢琴键广播的值）。
2. 兜底：`WriteRpc` detour 捕获**游戏自己发出的** PianoButtonRpc（手动按键），
   把 `+0x30/+0x31/+0x32` 缓存下来，之后所有不带覆盖参数的 `piano.play` 都用它。
3. V1（0.35.1）仍用已验证的 `+0x4C/+0x4E` 字段，行为不变。

> 下一步若要彻底打通：找注册表对象的持有者（`0x1261AC0` 无直接 E8 调用，可能是工厂/注册表），
> 拿到注册表后 `[registry+0x40]` 就是按钮控件。

### 结构偏移

| 字段 | V1 | V2 |
| --- | --- | --- |
| Game→netClient | `0x1C8` | `0x1E8` |
| Game→clock | `0x1B0` | `0x1D0` |
| clock double 槽 | `0xE0` / `0xF0` | `0xE8` / `0xF8` |
| Game→avatarMgr | `0x318` | `0x330` |
| Game→sheetMgr → 内层 → gate | `0x740` / `+64` / `+75568` | `0x7C8` / `+0x40` / `+0x19FB0` |
| instrument→bank | `+0x50` | `+0x90` |
| instrument 玩家 id XOR | `+0xB800` | `+0xB8E0` |
| instrument 已进世界标志 | `+0x10A0C` | `+0x10AA0` |
| instrument type/pitch/sustain/numNotes | `76/78/79/75128` | **沿用 V1，未证实**（已排除"整体平移 +0x40"的猜测：乐器代码区没有任何结构体 `+0x8C..+0x8F` 访问） |
| instrument 静音门 | `+0xB890` | 未证实，V2 关闭该检查 |
| instrument 姿态控制器 | `+0xA8` | 沿用 V1，未证实（动画默认关闭） |

V2 的 `instrLayoutVerified = false`：`numNotes` 固定按 15 键、`sustain` 取 0，
避免误读导致卡音或音域被截断；type/pitch 仍读取但带范围校验（type≤4、pitch≤14）。

## 下一个版本怎么再适配

```bash
# 1) 看段表 / 确认 PE 完整
python tools/newbuild.py --exe Sky_dump.exe info
# 2) 找字符串引用（姿态名、RPC 名等）
python newbuild.py --exe Sky_dump.exe calls 0x7C9620
python newbuild.py --exe Sky_dump.exe find 66 C7 45 ?? 00 01
# 3) 反汇编 / 对照旧版结构
python newbuild.py --exe Sky_dump.exe dis 0x7C9A30 0x7C9E6B
```

定位到新地址后，在 `Sigs.cpp` 里加一个 `kOffsetsV3`，在 `DetectGameVersion`
加上它的 `SizeOfImage`/`TimeDateStamp` 即可；V1 的签名不会受影响。

> V2 的 dump 里 `.pdata` 是坏的（67584 条只有 9 条落在 `.text`），`.rdata` 里的
> 字符串完整、PE 段表完整，所以按"字符串引用 + `CC` 填充找函数边界"定位。

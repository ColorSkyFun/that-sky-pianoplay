# 乐器链路完整静态分析(打开 → 按键 → RPC → 播放)

> 版本:Sky 0.35.1.423272 x64(base 0x140000000),交叉验证 ARM 0.15.1(带符号)。
> 本文档为纯静态分析结论,所有 x86 结论均与 ARM 符号版逐一对照过。
> 本文同时修正了 `HANDOFF.md` 中的三处误判,见 §8。

---

## 0. 总览:一次按键的完整生命周期

```
[装备乐器道具 AvatarProp]
        │  tgui 按钮 "useEquippedInstrument"
        ▼
[OpenInstrumentMenu 事件] ──► 菜单系统(tgui)打开乐器 UI
        │
        ▼
Instrument::OnOpen (sub_1405F94E0, vtable slot 6)
  · 设 opening 标志、重置音符运行态
  · 播放 "menuon"、avatar 进入持琴姿势
  · "tutorial_instrument_layout" 教学提示(首次)
        │  每帧
        ▼
InstrumentMenu::m_DrawNoteButtons (sub_140607830 / sub_140604E50)
  · for i in 0..numNotes-1:  button = instrument+96+1024*i
  · 画 tgui 音符按钮 (GameButtonBinding = note+50)
  · 构造 NoteDesc(0x38B) + NoteAction{vt 0x141E70E90}
  · sub_140609530 注册到按钮的 "OnButtonUpdate" 回调
        │  玩家按下音符按钮(tgui 状态变化)
        ▼
NoteAction::Execute (sub_140615400, NoteAction vtable slot 2)
  · flags(a2+40)&4 → note-on:  把 desc 写进 PianoButton,然后
        button->vtable[5](button, game)   ← 唯一实装虚函数
  · flags(a2+41)&1 → note-off: sub_1405F8300 = PianoButton::StopNote
        │
        ▼
PianoButton::OnStart (sub_1405F7B70)   ★ 一站式完成全部三件事 ★
  ├─ PianoButton::PlaySound (sub_1405F7730)     本地出声
  │     └─ sub_1405F88A0 → "PianoLocal"/"PianoRemote" 事件名
  │           └─ sub_140EC5C70 = SoundManager 3D 播放
  ├─ sub_1405F8C00                              持琴姿势/动画
  └─ 仅当 playerId == InvalidPlayerId(本地按键):
        构造 PianoButtonRpc + WriteRpc (sub_1401BC3F0) 广播
        │  网络
        ▼
[远端] PianoButtonRpc::Run (sub_1405F74E0)
  · IsSourceValid → GetAvatarByPlayerId
  · isStop → 停延音;否则 → PlaySound+PlayAnimation(远程 avatar)
    (或 AvatarKaraoke::OnTap sub_1416F2140 卡拉OK模拟)
```

---

## 1. 对象模型与关键偏移

### Game
| 偏移 | 含义 | 证据 |
|---|---|---|
| +0x1C8 | 网络客户端;`*(client+0x38)` = net module(WriteRpc 第 3 参) | WriteRpc 调用点 `*(game[57]+56)` |
| +0x138 (a2[274]) | SoundManager(音效统计 `+264300 += 2`) | PlaySound |
| +0x150 (a2[276]) | SoundManager(实际出声) | sub_1405F88A0 a2 |
| +0x18 (a2[54])→+0x1B0 | pitch 源:`*(double*)(X+0xE0 / +0xF0)`(`+0x28` float 选择) | OnStart 构造 RPC 时 |
| +0x138 (a2[99]) 附近 | avatar 管理器入口(sub_141763B00 参数) | 全部调用 |
| +0x2C8 (a2[275])→+0x4894 | SheetMusicBarn/karaoke(`+18588` 评分器、`+18596` 开关、`+18632` 门控) | OnStart/Run |
| +0x898 (a2+2200) | SheetMusicBarn,存入 instrument+75576 | Instrument vtable slot1 (sub_1405FD070) |
| +0x8A0 (a2+2208) | SoundManager("menuon") | OnOpen |

### Instrument(由 sub_141763B00(avatarMgr,1) 返回;构造函数 sub_14060FA80,被 AvatarManager 构造 sub_140F4BB30 内联调用)
| 偏移 | 含义 |
|---|---|
| +0x00 | vtable `0x141E70700`:slot1=sub_1405FD070(存 SheetMusicBarn 到 +75576)、slot6=**OnOpen sub_1405F94E0**、slot7=**StopAllNotes sub_1405F9E20**(对 16 按钮逐个调 StopNote) |
| +0x18 (24) | 变换矩阵指针 |
| +0x20 (32) | AvatarPose*(sub_141694DB0 姿势播放 / sub_141691360 SetState) |
| +0x4C (76) | 乐器变体(u8):`==2`→8 键,`==4`→4 键,否则 15 键(决定 numNotes) |
| +0x4D (77) | 变体 2(u8,进 RPC +0x32 的那个 +905 的源) |
| +0x4E (78) | pitch(u8,源 of +924) |
| +0x4F (79) | **sustain/延音乐器标志**(≠"激活";钢琴=0 单发,长笛类=1 可按住)。Execute 复制到 button+944 决定单发/延音行为,StopNote 只对 ==1 的乐器做事。实测:进世界持钢琴时恒为 0 |
| +0x50 (80) | **AvatarSound / 延音注册表**:`bank+320+16*note`(note≤15)存 16 字节 SoundRef —— 当前该音符正在响的延音句柄,**没有 sustain 在响时就是 0,这是正常的,不是"音色没加载"** |
| +0x60 (96) | **PianoButton[16]**,每个 1024 字节。**生命周期:平时(含菜单开着)整体置零,真实按键时才被 placement-construct 到 `instrument+96+1024*i`(`sub_140611740`:基类构造 `sub_14060F5C0` + vtable + 默认字段),然后填音符字段 → Execute → OnStart**。2026-10-25 双重实测确认(开菜单后 vtable 仍为 0;`sub_140611670`=堆版工厂、`sub_140611740`=placement 版) |
| +16480 | 第二按钮数组:64 × 912 字节,vtable `0x141E707A0` |
| +47200 | playerId 异或源:`*(u32*)(instr+47200) ^ (u32)(instr+47200)` |
| +47216 / +47248 / +68108 / +68112 | Run/PlaySound 的门控(静音/屏蔽/avatar 加载状态,char 符号位) |
| +74880 | 乐谱移调模式(Execute 里 `desc+0x18==1 && 此标志==1` → 用 desc+0x10 的替换音符) |
| +74884 | 当前按住的音符(u8) |
| +75128 | **numNotes**(15/8/4,m_DrawNoteButtons 每帧刷新) |
| +75132 | 模式计数(karaoke 开时置 5) |
| +75497 | opening 标志 |
| +75576 | SheetMusicBarn*(slot1 初始化写入) |

### PianoButton(instrument+96+1024*i,1024 字节)
| 偏移 | 含义 |
|---|---|
| +0x00 | vtable `0x141E6FD00`:**slot5(+0x28)= OnStart(sub_1405F7B70)是唯一实装**,其余槽位全部指向同一个桩 sub_1400199B0 —— 这就是之前"函数指针全 0/全相同"观感的来源 |
| +896 (0x380) | note(u32) |
| +900 (0x384) | playerId(u32;Invalid = dword_142548138) |
| +904 | 本地 instrumentType(u8,源 instr+76) |
| +905 | 变体 2(u8,源 instr+77) |
| +906 | 远端回放用 instrumentType(u8) |
| +924 | pitch(dword,低字节进 RPC+0x31;源 instr+78) |
| +928 | 远端 pitch |
| +944 (0x3B0) | **active/sustain**(u8;0=单发,1=延音;源 instr+79;同时是复音上限判断条件) |
| +948 | 状态机(0/2/3;OnStart 里 2→3) |
| +960/968 | vector\<float\>(视觉) |
| +1001 | karaoke feedback 激活 |
| +1016 | karaoke 按住标志 |
| +1020 | 1.0f |

构造:Instrument 构造函数(sub_14060FA80)内联循环 16 次调用 sub_14060F5C0(基类构造)+ 写 vtable/note=0/playerId=Invalid/active=0 等。**即按钮 vtable 在 AvatarManager 构造时就已有效,不存在"按键才填 vtable"**。

---

## 2. 打开乐器

1. 装备乐器道具(`AvatarProp::IsPropAnInstrument`,ARM 符号),tgui 界面元素绑定字符串 **`"useEquippedInstrument"`**(0x141fdbcb5)→ 触发 **`OpenInstrumentMenu`** 事件(字符串 0x141fcb4df;另有 `"OpenInstrumentMenuRPC"` 0x142067517,社交乐器场景用)。上一版 mod 注入该事件能打开 UI,与此一致。
2. 菜单系统调用 **Instrument::OnOpen = sub_1405F94E0**(Instrument vtable slot6,无直接 xref,纯虚调用):
   - `instrument+75497 = 1`(opening)
   - 重置 8 × 2048 步长的音符运行态块
   - `avatar+168 → +194 = 1、+280 = 0`(进入乐器模式)
   - SoundManager(game+2208)播放 `"menuon"`
   - `sub_141694DB0(instrument+32, …, 1)` 持琴姿势,`sub_141691360(instrument+32, 0)` 置姿态
   - 首次显示 `"tutorial_instrument_layout"` 教学提示
   - `+75008=513、+75104=2`,最后调 `sub_1405F99E0`(收尾,读取 +76/+78)
3. 每帧 UI 刷新:**`InstrumentMenu::m_DrawNoteButtons = sub_140607830`**(普通弹奏模式;`sub_140604E50` 是另一套更大的乐谱/卡拉OK模式绘制,同样构造 NoteAction):
   - `numNotes = instrument+75128`(默认 15;`instr+76==2` 或 `instr+74880≠0` → 8;`==4` → 4)
   - 循环 `button = instrument+96+1024*i`:
     - `sub_1405F81A0` 按钮视觉缓动(位置 +0x3F0、缩放 +0x3EC,每帧)
     - `sub_14060DB70`(= tguiPrefabs::DrawNoteButton)绘制,属性 **`GameButtonBinding = note+50`**
     - `sub_14019FD50` 分配 **NoteDesc(0x38 字节)** 并填:
       `+0x00` float(缩放)、`+0x08` Game*、`+0x10` 乐谱替换音符、`+0x18` flag、`+0x20` note(u8)、`+0x28` note(u32)、`+0x30` **PianoButton\***
     - 栈上拼 **NoteAction**:`{+0x00 vtable=0x141E70E90, +0x08 desc, +0x10 instrument}`
     - `sub_140609530` → `sub_140730430(..., "OnButtonUpdate", ...)` 把 NoteAction 注册为该 tgui 按钮的回调

---

## 3. 点击按钮 → NoteAction::Execute

NoteAction vtable `0x141E70E90`:slot0/1=sub_1406153E0(拷贝构造),**slot2(+0x10)= Execute(sub_140615400)**,由 tgui OnButtonUpdate 分发,第二参是输入状态块:

- `flags(a2+40) & 4` → 本帧**按下**(note-on)
- `flags(a2+41) & 1` → **释放**(note-off,还要求 `instrument+79==1`)

**note-on 分支**(sub_140615400):
```c
button = desc->button(+0x30);          // = instrument+96+1024*note
button+944 = instr+79;   // active/sustain
button+924 = instr+78;   // pitch
button+904 = instr+76;   // instrType(本地)
button+905 = instr+77;
button+896 = desc+0x28;  // note
if (desc+0x18==1 && instr+74880==1) button+896 = desc+0x10;  // 乐谱移调
button->vtable[5](button, desc->game);   // ★ PianoButton::OnStart ★
if (instr+79==1) instr+74884 = desc+0x20;  // held note
```
上次直调本函数崩溃(0x61547D)正是因为虚调用 `*(vtable+40)` 时 button 无效/字段未填。

**note-off 分支**:`sub_1405F8300(button, game)` = **PianoButton::StopNote**(见 §6)。

---

## 4. PianoButton::OnStart(sub_1405F7B70)—— 发送与播放的核心

```
入参:(a1 = PianoButton*, a2 = Game*)
1. instrType/pitch 选择:playerId(+900)==Invalid → 用 +904/+924,否则 +906/+928
2. v12 = sub_141763B00(game+avatarMgr字段, 1)   // 本地 instrument
3. 复音限制:active(+944)==1 且 NumSustainNotesPlaying(bank=v12+80) > 4 → 直接跳到结尾
4. PlaySound(sub_1405F7730)(retBuf, game, note, active&1, playerId, instrType, pitch, …, 1.0f)
5. 动画对象:playerId==Invalid → 本地;否则 sub_141769C00(avatarMgr, playerId) 远程 avatar
   (远程还要 sub_1416AED60 可见性检查 + sub_141691360)
   sub_1405F8C00(…, note, avatar, instrType, pitch)   // 姿势动画
6. 仅 playerId == InvalidPlayerId(本地按键)时构造并广播 RPC:
   sub_140954DC0(game+…); sub_1405F69B0(game, note)
   卡拉OK评分 sub_140AE1010(SheetMusicBarn+18588, out, button+905, note, …)(需 +18596==1)
   门控:instr+47248 == 0 且 *(game+232→+64→+75568) <= 0
   rpc.playerId = *(u32*)(v12+47200) ^ (u32)(v12+47200)
   WriteRpc(net = *(game[57]+56), &rpc, net+0x38)      // sub_1401BC3F0
7. button+900 = Invalid;sub_14121E730(button)  // Event::Stop
```

---

## 5. PianoButtonRpc 布局(0.35.1 修正版,总长 0x48)

```
+0x00  vtable  = base+0x1E6FCD0
+0x08  metaId  = *(u32*)(*(base+0x2548418) + 0x38) = 326
+0x10  → +0x18 自引用链,+0x18 = base+0x1E3CDE0
+0x20  u16 = 0x100
+0x28  playerId (u32)
+0x2C  note (u32, 0..14 旋律)
+0x30  instrumentType (u8)          ← button+904
+0x31  pitch 低字节 (u8)            ← button+924
+0x32  变体2 (u8)                   ← button+905
+0x33  karaoke feedback (u8)
+0x34  karaoke 附加 (u32)
+0x38  时间戳/sync double（实测 ~2.689e9 且随真实时间递增，来自 game+0x1B0 对象 +0xE0/+0xF0；旧注"pitch"有误）
+0x40  sustain (u8)                 ← note-on 时 = button+944
+0x41  sustain (u8)                 ← 同上
+0x42  isStop (u8)                  ← note-off RPC 置 1
```
- **note-on**(OnStart 发):+0x2C..+0x41 填音符参数,+0x42=0
- **note-off**(StopNote 发):+0x30..+0x3F 全 0,仅 +0x40=1、+0x42=1
- 接收端 Run 按 `+40=playerId、+44=note、+48=instrType、+49=pitch、+66=isStop` 读取,与此一致。
- 旧 HANDOFF 里"+0x34 pitch(int -5000..5000)"实际是 karaoke 字段;真正的音调是 **+0x38 的 double**。

## 5b. WriteRpc

`sub_1401BC3F0(net, &rpc, net+0x38)`,其中 `net = *(u64*)(*(u64*)(game+0x1C8) + 0x38)`(ARM 里对应 `NetRpcClient::Broadcast`,game+432 网络对象 +56)。RPC 只广播、自己不回放 —— 本地声音早在 OnStart 第 4 步播过了,这是设计行为而非 bug。

---

## 6. note-off:PianoButton::StopNote(sub_1405F8300)

**这不是"发送函数"**(旧 HANDOFF 误判),它是松键处理:
```
button+1020 = 1.0f; v5 = 本地 instrument
state(+948) 2→3; button+940 = 1.0f
if (active(+944)==1):                       // 只有延音乐器需要停音
    if (IsSustainNotePlaying(bank, note))    // sub_14165E7F0
        StopSustainNote(bank, note, 0)       // sub_14165E330
        若乐器无延音姿势: sub_141694DB0(instrument+32, …) 松键姿势/释放音
        WriteRpc(note-off: 仅 playerId/note 有效, +0x40=1, +0x42=1)
```
单发音符(active=0)松键不做任何事(声音本身是一次性的)。

### AvatarSound(bank = instrument+80 / avatar+80)函数表
| 函数 | 语义 |
|---|---|
| `sub_14165E2C0(bank, note, &ref)` | **SetSustainNote**:把 PlaySound 返回的 16 字节 SoundRef 存入 `bank+320+16*note`,顶掉旧的 |
| `sub_14165E7F0(bank, note)` | **IsSustainNotePlaying**(note≤15 且槽内句柄有效) |
| `sub_14165E330(bank, note, flag)` | **StopSustainNote** |
| `sub_14165E860(bank)` | **NumSustainNotesPlaying**(OnStart 复音上限 >4 即 5 个) |

---

## 7. 真正出声:PlaySound 链

`sub_1405F7730(retBuf, game, note, sustain, playerId, instrType, pitch, …, 1.0f)`:
1. playerId==Invalid → 本地 instrument(sub_141763B00),否则远程 avatar(sub_141769C00);取 `+24` 坐标、`+80` bank
2. 门控:avatar 加载态(+68108)、sub_141766D40
3. 远程做**距离衰减**(avatar 间距离 vs sub_140EC5C40 可听范围),超范围不出声
4. `sub_1405F88A0(retBuf, SoundManager(game+0x150), note, avatarTransform, isLocal, instrType, pitch, …)`:
   - 旋律乐器(instrType≠2/4):note 必须 <15;音高 = 基础音高表(sub_140912770,按 note 索引)+ 移调;
     **事件名 = `sprintf("%s%s", sub_140EC55D0(instrType, subType), isLocal ? "Local" : "Remote")`** → `"PianoLocal"`/`"PianoRemote"`/`"FluteLocal"`/…
   - instrType==2(打击):按 subType|0x200 查表 → `APBell1/APBell2/HandPan/Dundun/GoldHandPan/GoldDundun/TR909/Percussion` + Local/Remote
   - instrType==4:`Cymbals` / `FortuneDrum` + Local/Remote
   - → `sub_140EC5C70 = SoundManager::PlaySound3D(事件名, 音高, avatar坐标, 音量)`(WWise 事件)
5. sustain 时 `SetSustainNote(bank, note, retBuf的SoundRef)`
6. 统计 `*(game+0x138对象 + 264300) += 2`

## 7b. 接收端:PianoButtonRpc::Run(sub_1405F74E0)

```
if (!sub_140E6C8C0(game, rpc+16, rpc+40)) return;   // NetRpc::IsSourceValid
if (SheetMusicBarn+18632 > 0) return;
avatar = sub_141769C00(avatarMgr, rpc->playerId, 0); if (!avatar) return;
本地instrument = sub_141763B00(avatarMgr, 1);
sub_1405F69B0(game, note); sub_140956EB0(game, avatar);
if (rpc+66 == 1)  // isStop
    { sub_14165E330(avatar+80, note, 1); return; }   // 停对方延音
门控:avatar+47216/+47248/+68112、sub_140B12A10、byte_142548458、每帧限 6 个(dword_14254845C)
if (ShouldSimulate(sub_1416F1DC0) && avatar+184→+8)
    AvatarKaraoke::OnTap(sub_1416F2140)(karaoke, game, rpc)      // 卡拉OK联动
else
    PlaySound(... rpc->playerId ...) + PlayAnimation(sub_1405F7930)(...)  // 直接播远程音
```

---

## 8. 对旧 HANDOFF 的修正(重要)

1. **`sub_1405F8300` ≠ 发送函数**。它是 `PianoButton::StopNote`(note-off)。note-on 的 RPC 构造+广播在 **`sub_1405F7B70`(PianoButton::OnStart)** 里 —— 手动按键时 WriteRpc 观察到的 PianoButtonRpc 来自 OnStart,不是 sub_1405F8300。
2. **`bank+320+16*note` 不是"懒加载的音色库"**,是"当前延音句柄注册表"(SetSustainNote 写入)。`noteSlot==0` 只表示该音符没有 sustain 在响。因此"注入 OpenInstrumentMenu 后 noteSlot=0 = 音色没加载"是**误判**,当时菜单应已完整可用,直接按键即可发声。
3. **按钮 vtable 的生命周期**(结论经过两次修正,以实测为准):Instrument 构造(`sub_14060FA80`)时确实初始化过 16 个按钮,但**运行期按钮数组平时被置零,真实按键时才由 `sub_140611740` placement-construct 重建** —— 所以"按键时才填充"的原始观察是对的。UI 打开并不会构造按钮(实测:开菜单后 vtable 仍为 NULL)。mod 侧解法:调 OnStart 前先自己 placement-construct(`sub_140611740(0, button)`)。该 vtable 除 slot5(OnStart)外其余槽位全是桩 sub_1400199B0。
4. **RPC 布局修正**:+0x38 是 double 型 pitch;+0x40/+0x41 sustain、+0x42 isStop;+0x34 是 karaoke 字段(不是 pitch)。
5. **"RPC 没有回显"是设计行为**:OnStart 只在 `playerId==InvalidPlayerId`(本地)时广播;接收端 Run 只处理"别人的 playerId"。自己听到的是 OnStart 里同步调用的本地 PlaySound。
6. 直调 `sub_140615400` 崩溃根因:虚调用 `button->vtable[5]` 需要**完整初始化的 PianoButton**(note/playerId/instrType/pitch/active)加合法 desc。绕过办法见 §9。

---

## 9. 对 mod 的直接可注入点(静态推导,按接近原生排序)

> **2026-10-25 最终落地形态**:mod 实际采用 **方案 D(拆解 OnStart)** —— `sub_1405F7730`(PlaySound)做本地音 + 按 §5 布局手工构造 PianoButtonRpc + WriteRpc,复刻 OnStart 的复音上限/门禁/时间戳,**跳过姿态段**。好处:`instrument+0xA8`(姿势控制器)只在开界面时非空,跳过姿态即无需开界面、无崩溃风险;远程玩家依然会看到你角色的弹奏动画(那由远端 `Run → PlayAnimation` 驱动)。已实测:250+ RPC、自定义音色、关界面弹奏全部正常。方案 A(OnStart 全路径)仅在需要本地动画时使用,且必须满足其两个前提。

**D. 拆解 OnStart(最终采用,无姿态、无界面要求)**
```
PlaySound(ret, game, note, sustain&1, InvalidPid, type, sub, 0, 1.0f, 1.0f)   // 本地音
sub_1405F69B0(game, note)                                                     // 计时簿记
门禁: instr+47248==0 且 *(*(game+0x740)+64)+75568 <= 0
RPC: §5 布局逐字段构造(时间戳 double 取 *(game+0x1B0 → +0xE0/+0xF0))
WriteRpc(net, &rpc, net+0x38), net = *(*(game+0x1C8)+0x38)
```

**A. 直接调 `PianoButton::OnStart`(sub_1405F7B70)—— 与手动按键 100% 同路径(已实测可用)**
```cpp
// 前提(缺一不可,均经实测):
// 1) 游戏内已装备并【打开】乐器 UI —— instrument+0xA8(姿势控制器)只有开界面时非 NULL,
//    否则 OnStart 的动画段在 sub_14168F310 读 NULL+0xC1 直接崩(2026-10-25 crash dump 实证)
// 2) 按钮需 placement-construct:调 sub_141611740(0, button) —— 按钮数组平时是置零的
void* instr = sub_141763B00(avatarMgr, 1);
uint8_t* btn = (uint8_t*)instr + 96 + 1024 * note;
sub_141611740(nullptr, btn);               // 真实按键就是这么构造的
*(uint32_t*)(btn + 896) = note;            // note
*(uint32_t*)(btn + 900) = dword_142548138; // playerId = Invalid
*(uint8_t*)(btn + 904) = 自定义type;        // 音色: 同时决定本地音/RPC/远程/动画
*(uint8_t*)(btn + 905) = *(uint8_t*)(instr + 77);
*(uint32_t*)(btn + 924) = 自定义sub;       // 子音色
*(uint8_t*)(btn + 944) = *(uint8_t*)(instr + 79);  // sustain(照抄装备值)
sub_1405F7B70(btn, game);                  // 本地音 + 动画 + RPC 广播
```

**B. 模拟 tgui 事件**:构造 NoteDesc+NoteAction 并以 `flags(a2+40)|=4` 调 `sub_140615400` —— 与 A 等价但更繁琐(还得伪造输入状态块)。

**C. 继续直发 RPC**:注意按 §5 修正字段(+0x38 double pitch、+0x40/+0x42 语义),note-on 时 `+0x40 = +0x41 = active`,`+0x42 = 0`。

---

## 10. 关键地址速查(x86 0.35.1,RVA = 去掉 0x140000000)

| 符号(ARM 对照) | 地址 | 作用 |
|---|---|---|
| AvatarManager 构造(内联 Instrument 构造) | sub_140F4BB30 | Instrument/按钮出生地 |
| Instrument::构造 | sub_14060FA80 | 16×PianoButton 初始化(写 vtable 0x141E6FD00) |
| Instrument vtable | 0x141E70700 | slot6=OnOpen, slot7=StopAllNotes |
| Instrument::OnOpen | sub_1405F94E0 | 开菜单时乐器侧初始化 |
| Instrument::StopAllNotes | sub_1405F9E20 | 16 按钮逐个 StopNote |
| InstrumentMenu::m_DrawNoteButtons | sub_140607830 | 每帧画音符按钮+构造 NoteAction |
| (乐谱/卡拉OK模式绘制) | sub_140604E50 | 同上,另一模式 |
| NoteAction vtable | 0x141E70E90 | slot2=Execute |
| NoteAction::Execute | sub_140615400 | 按下/释放分发 |
| PianoButton vtable | 0x141E6FD00 | slot5=OnStart(唯一实装) |
| **PianoButton::OnStart** | **sub_1405F7B70** | 本地音+动画+RPC 广播 |
| PianoButton::StopNote | sub_1405F8300 | note-off + note-off RPC |
| PianoButton::PlaySound | sub_1405F7730 | 出声入口 |
| 声音事件选择器 | sub_1405F88A0 | "%s%s" → PianoLocal/Remote 等 |
| SoundManager::PlaySound3D | sub_140EC5C70 | 真正发声(WWise) |
| 乐器名映射 | sub_140EC55D0 | instrType → "Piano"/"Flute"/… |
| 姿势动画 | sub_1405F8C00 / sub_1405F7930 | 动画/姿势 |
| AvatarSound::SetSustainNote | sub_14165E2C0 | bank+320+16*note 写句柄 |
| AvatarSound::IsSustainNotePlaying | sub_14165E7F0 | 槽位检查 |
| AvatarSound::StopSustainNote | sub_14165E330 | 停延音 |
| AvatarSound::NumSustainNotesPlaying | sub_14165E860 | 复音计数 |
| **PianoButtonRpc::Run** | **sub_1405F74E0** | 接收端 |
| AvatarKaraoke::OnTap | sub_1416F2140 | 卡拉OK联动(远程) |
| AvatarKaraoke::ShouldSimulate | sub_1416F1DC0 | 是否走模拟 |
| GetInstrument(本地) | sub_141763B00 | avatarMgr,1 → instrument |
| GetAvatarByPlayerId | sub_141769C00 | 远程 avatar/instrument |
| WriteRpc / NetRpcClient::Broadcast | sub_1401BC3F0 | (net, &rpc, net+0x38) |
| InvalidPlayerId | dword_142548138 | 全局 |
| PianoButtonRpc metaId 源 | qword_142548418 | +56 → 326 |
| 字符串 | 0x141fcb4df / 0x141fdbcb5 / 0x142067517 | OpenInstrumentMenu / useEquippedInstrument / OpenInstrumentMenuRPC |

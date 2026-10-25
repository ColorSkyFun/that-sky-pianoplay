-- ═══ Cornfield Chase (星际穿越) · 双钢琴合奏谱转译 · 1=C 3/4 全白键 ═══
-- 谱源: qinpu123.com/qiyue/gangqin/681831.html (1P/2P 合奏大谱, C大调)
-- 编制: 钢琴=1P固定音型  小提琴=1P主旋律  竖琴=2P琶音(B段起)  低音提琴=2P低音
-- 结构: 引子(8) → A 旋律浮现(16) → B 声部互换(24) → C 高潮(40) → 尾声(24) ≈3分40秒
piano.clear()
local Q = 165                -- 四分音符ms (♩≈182); 想更快改150, 更慢改185
local BAR = Q * 3            -- 3/4
local ST = Q / 4             -- 十六分音符 = 固定音型的步进
local at = P.at

-- ── 固定音型引擎 (钢琴): 每小节12个十六分 root-5th-3rd'-5th ──
-- 音级: r=根音键号, 模式 = r, r+4, r+7, r+4 (循环)
local function ostinato(t0, r, bars, ins)
  ins = ins or P.PI
  for b = 0, bars - 1 do
    local t = t0 + b * BAR
    local pat = {r, r + 4, r + 7, r + 4}
    for s = 0, 11 do
      at(t + s * ST, pat[(s % 4) + 1], ins)
    end
  end
end

-- ── 低音 (低音提琴): 每小节根音全音符延音 ──
local function bass(t0, roots, bars, ins)
  ins = ins or P.CB
  for i = 1, bars do
    local r = roots[((i - 1) % #roots) + 1]
    at(t0 + (i - 1) * BAR, r, ins, BAR * 0.92)
  end
end

-- 和弦根音键号: C=0 Am=5(A4,提琴音色低八度) F=3 G=4
local RC, RAM, RF, RG = 0, 5, 3, 4

-- ═══ 引子 (m1-8): 音型(C-G-E-G) + 低音踏板 ═══
ostinato(0, RC, 8)
bass(0, {RC, RG}, 8)

-- ═══ A 段 (m9-24): 音型升八度(C5-E5-G5), 旋律隐伏, C→Am→F→G ═══
local A0 = 8 * BAR
ostinato(A0, RC + 7, 16)
bass(A0, {RC, RAM, RF, RG}, 16)
-- 小提琴点出旋律骨干 (长音, 每两小节一句)
local AMEL = {
  {0, 7, 4}, {4, 9, 2}, {6, 11, 2},          -- C5→E5→G5 上行
  {9, 12, 2}, {11, 11, 2}, {13, 9, 2},       -- A5→G5→E5
  {16, 7, 4}, {20, 9, 2}, {22, 11, 2},       -- 反复上行
  {25, 12, 2}, {27, 11, 2}, {29, 14, 2},     -- 冲高 A5→G5→C6
}
for _, m in ipairs(AMEL) do
  at(A0 + m[1] * Q, m[2], P.VI, m[3] * Q * 0.9)
end

-- ═══ B 段 (m25-48): 声部互换 — 旋律交小提琴长音, 竖琴接琶音 ═══
local B0 = 24 * BAR
bass(B0, {RF, RG, RAM, RC}, 24)
for b = 0, 23 do                                             -- 竖琴: 每拍一个琶音音, 三和弦轮转
  local ch = {{3,5,7},{4,6,8},{5,7,9},{0,2,4}}[(b % 4) + 1]  -- F G Am C
  local t = B0 + b * BAR
  at(t, ch[1], P.HA); at(t + Q, ch[2], P.HA); at(t + 2*Q, ch[3], P.HA)
end
local BMEL = {
  {0, 9, 3}, {3, 7, 3}, {6, 9, 3}, {9, 11, 3},
  {12, 12, 3}, {15, 11, 3}, {18, 9, 3}, {21, 7, 3},
  {24, 9, 3}, {27, 11, 3}, {30, 12, 3}, {33, 14, 3},
  {36, 12, 3}, {39, 11, 3}, {42, 9, 3}, {45, 7, 3},
}
for _, m in ipairs(BMEL) do
  at(B0 + m[1] * Q, m[2], P.VI, m[3] * Q * 0.9)
end
-- 钢琴: B段前半稀疏音型回声(每小节第一拍一组)
for b = 0, 11 do
  local t = B0 + b * BAR
  at(t, 0, P.PI); at(t + ST, 4, P.PI); at(t + 2*ST, 7, P.PI); at(t + 3*ST, 4, P.PI)
end

-- ═══ C 段高潮 (m49-88): 满编制, 音型加密, 旋律顶点 ═══
local C0 = 48 * BAR
ostinato(C0, RC, 20)                       -- 钢琴回低八度密集音型
bass(C0, {RC, RF, RG, RAM}, 40)            -- 低音加厚循环
for b = 0, 39 do                           -- 竖琴十六分琶音全速
  local ch = {{0,2,4},{3,5,7},{4,6,8},{5,7,9}}[(b % 4) + 1]
  local t = C0 + b * BAR
  local idx = 1
  for s = 0, 11 do
    at(t + s * ST, ch[idx], P.HA)
    idx = idx % 3 + 1
  end
end
local CMEL = {
  {0, 9, 3}, {3, 12, 3}, {6, 11, 3}, {9, 9, 3},
  {12, 7, 3}, {15, 9, 3}, {18, 11, 3}, {21, 12, 3},
  {24, 14, 3}, {27, 12, 3}, {30, 11, 3}, {33, 9, 3},
  {36, 11, 3}, {39, 12, 3}, {42, 14, 3}, {45, 14, 3},   -- 顶点双 A5... (C6=14)
  {48, 12, 3}, {51, 11, 3}, {54, 9, 3}, {57, 7, 3},
  {60, 9, 3}, {63, 11, 3}, {66, 12, 3}, {69, 14, 3},
  {72, 12, 4}, {76, 11, 3}, {79, 9, 3}, {82, 7, 3},
  {85, 9, 4}, {89, 7, 3}, {92, 9, 3}, {95, 11, 3},
  {98, 12, 4}, {102, 11, 3}, {105, 9, 3}, {108, 7, 4},
}
for _, m in ipairs(CMEL) do
  at(C0 + m[1] * Q, m[2], P.VI, m[3] * Q * 0.9)
end
-- 高潮小号点缀 (最强处, 每四小节一击)
for b = 4, 36, 4 do
  at(C0 + b * BAR, 7, P.BU, BAR * 0.8)
end

-- ═══ 尾声 (m89-112): 渐弱收场 ═══
local E0 = 88 * BAR
bass(E0, {RC, RG}, 24, P.CB)
for b = 0, 11 do                           -- 音型渐稀: 前半每小节, 后半隔小节
  if b % 2 == 0 then
    ostinato(E0 + b * BAR, RC, 1)
  end
end
local EMEL = {
  {0, 9, 4}, {4, 7, 4}, {8, 9, 4}, {12, 11, 4},
  {16, 9, 6}, {22, 7, 4}, {26, 9, 4}, {30, 7, 6},
}
for _, m in ipairs(EMEL) do
  at(E0 + m[1] * Q, m[2], P.VI, m[3] * Q * 0.85)
end
-- 终止: C 大和弦全员长音, 钢片琴点亮
local T0 = 104 * BAR
at(T0, 0, P.CB, BAR * 3); at(T0, 2, P.CE, BAR * 2.6); at(T0, 4, P.CE, BAR * 2.2)
at(T0, 7, P.VI, BAR * 2.5); at(T0, 11, P.GL); at(T0 + Q, 14, P.GL)

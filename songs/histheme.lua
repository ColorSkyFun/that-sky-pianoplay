-- ═══ His Theme (Undertale) · 增强版合奏 · 原谱1=B 转 1=C · 90BPM ═══
-- A小提琴叙事 → A'加长笛竖琴 → B流动段 → C高潮 → 尾声全奏 ≈2分28秒
-- 需要 lib.lua
piano.clear()
local Q=650
local BAR=Q*4
local CELL=Q*2     -- 流动段循环单元 = 2拍(八分+八分+四分)
local at=P.at
local TRIAD={C={0,2,4},Fs={3,4,7},G={4,6,8},Am={5,7,9}}  -- F用sus2开放和声
local ROOT={C=0,Fs=3,G=4,Am=5}
local TOP={C=7,Fs=7,G=11,Am=9}
local K=function(d,hi) return (d-1)+(hi==1 and 7 or 0) end

-- A段旋律 {拍,唱名,高八度,拍长}
local AMEL={
 {0,5,1,2},{2,2,1,2},{4,1,1,2},{6,5,0,2},{8,7,0,2},{10,7,0,2},{12,1,1,4},
 {16,5,1,2},{18,5,1,2},{20,1,1,2},{22,5,0,2},{24,7,0,2},{26,7,0,2},{28,1,1,4},
 {32,5,1,2},{34,2,1,2},{36,1,1,2},{38,5,0,2},{40,7,0,2},{42,7,0,2},{44,1,1,4},
 {48,1,0,2},{50,5,0,2},{52,1,1,2},{54,3,1,2},{56,2,1,2},{58,1,1,2},{60,2,1,4}}
local ACH={"Fs","Fs","G","G","Am","Am","C","C","Fs","Fs","G","G","C","C","G","C"}

-- A1 (1-16小节): 小提琴 + 钢琴分解 + 大提琴
for _,m in ipairs(AMEL) do at(m[1]*Q,K(m[2],m[3]),P.VI,math.floor(m[4]*Q*0.92)) end
for b=0,15 do
  local tr=TRIAD[ACH[b+1]]
  for i=0,3 do at(b*BAR+i*Q,({tr[1],tr[3],tr[2],tr[3]})[i+1],P.PI) end
  at(b*BAR,ROOT[ACH[b+1]],P.CE,BAR*0.88)
end
for _,bb in ipairs({3,7,11}) do at(bb*BAR+2*Q,7,P.GL) end

-- A2 (17-32小节): +长笛三度 +竖琴琶音
local O2=BAR*16
for _,m in ipairs(AMEL) do
  at(O2+m[1]*Q,K(m[2],m[3]),P.VI,math.floor(m[4]*Q*0.92))
  at(O2+m[1]*Q,K(m[2],m[3])-2,P.FL,math.floor(m[4]*Q*0.8))
end
for b=0,15 do
  local tr=TRIAD[ACH[b+1]]
  P.strum(O2+b*BAR,tr,P.PI); P.strum(O2+b*BAR+2*Q,{tr[2],tr[3],tr[3]},P.PI)
  at(O2+b*BAR,tr[1],P.HA); at(O2+b*BAR+Q,tr[2],P.HA)
  at(O2+b*BAR+2*Q,tr[3],P.HA); at(O2+b*BAR+3*Q,tr[1]+7,P.HA)
  at(O2+b*BAR,ROOT[ACH[b+1]],P.CE,BAR*0.88)
end
for _,bb in ipairs({4,6}) do at(O2+bb*BAR+2*Q,11,P.GL); at(O2+bb*BAR+3*Q,9,P.GL) end

-- B 流动段 (32-40小节): 每2拍一个 5'5'X 循环,一小节两圈
local BX ={8,7,11,11,6,7,8,11,6,7,11,9,7,8,8,11}
local BCH={"G","Fs","C","C","G","Fs","G","C","G","Fs","C","Am","Fs","G","G","C"}
local O3=BAR*32
for c=0,15 do
  local t=O3+c*CELL local x=BX[c+1] local ch=BCH[c+1] local tr=TRIAD[ch]
  at(t,11,P.HA); at(t+Q*0.5,11,P.HA); at(t+Q,x,P.HA)     -- da-da-DUM
  at(t+Q,x,P.VI,math.floor(Q*0.85))
  if c%2==0 then P.strum(t,tr,P.PI) else at(t,tr[2],P.PI); at(t,tr[3],P.PI) end
  at(t,ROOT[ch],P.CE,Q*1.2)
  if c%2==0 then at(t,ROOT[ch],P.CB,Q*3.8) end
  if x+7<=14 then at(t+Q,x+7,P.GL) end
end

-- C 流动段高潮 (40-48小节): +APPiano +长笛
local CX ={6,7,11,11,6,7,8,11,6,7,4,9,7,8,8,11}   -- 第11格=装饰音5'5'5
local CCH={"G","Fs","C","C","G","Fs","G","C","G","Fs","C","Am","Fs","G","G","C"}
local O4=BAR*40
for c=0,15 do
  local t=O4+c*CELL local x=CX[c+1] local ch=CCH[c+1] local tr=TRIAD[ch] local r=ROOT[ch]
  at(t,11,P.HA); at(t+Q*0.5,11,P.HA); at(t+Q,x,P.HA)
  at(t+Q,x,P.VI,math.floor(Q*0.85))
  at(t+Q,x,P.AP)
  if c%2==0 then P.strum(t,tr,P.PI); at(t,TOP[ch],P.FL,Q*1.6)
  else at(t,tr[2],P.PI); at(t,tr[3],P.PI) end
  at(t,r,P.CE,Q*1.2)
  if c%2==0 then at(t,r,P.CB,Q*3.8) end
  if x+7<=14 then at(t+Q,x+7,P.GL) end
end

-- 尾声 (48-56小节): A主题全奏 + 终止和弦
local O5=BAR*48
local FMEL={
 {0,5,1,2},{2,2,1,2},{4,1,1,2},{6,5,0,2},{8,7,0,2},{10,7,0,2},{12,1,1,4},
 {16,1,0,2},{18,5,0,2},{20,1,1,2},{22,3,1,2},{24,2,1,2},{26,1,1,2},{28,1,1,4}}
local FCH={"C","C","G","C","Fs","Fs","G","C"}
for _,m in ipairs(FMEL) do
  at(O5+m[1]*Q,K(m[2],m[3]),P.VI,math.floor(m[4]*Q*0.92))
  at(O5+m[1]*Q,K(m[2],m[3])-2,P.FL,math.floor(m[4]*Q*0.8))
  at(O5+m[1]*Q,K(m[2],m[3]),P.AP)
end
for b=0,7 do
  local tr=TRIAD[FCH[b+1]] local r=ROOT[FCH[b+1]]
  P.strum(O5+b*BAR,tr,P.PI,30); P.strum(O5+b*BAR+2*Q,{tr[2],tr[3],tr[3]},P.PI,30)
  at(O5+b*BAR,r,P.CE,BAR*0.88); at(O5+b*BAR,r,P.CB,BAR*0.88)
  at(O5+b*BAR,tr[1],P.HA); at(O5+b*BAR+Q,tr[3],P.HA)
  at(O5+b*BAR+2*Q,tr[1]+7,P.HA); at(O5+b*BAR+3*Q,tr[3],P.HA)
end
for _,bb in ipairs({0,3,7}) do at(O5+bb*BAR,11,P.GL) end
local T0=O5+7*BAR
at(T0,0,P.CE,BAR); at(T0,0,P.CB,BAR); at(T0,7,P.VI,BAR); at(T0,9,P.FL,BAR)
P.strum(T0,{0,2,4},P.PI,60); P.strum(T0+Q*0.5,{7,9,11},P.PI,60)
at(T0,7,P.AP); at(T0+Q*0.5,9,P.AP); at(T0+Q,11,P.AP); at(T0+Q*1.5,14,P.AP)
at(T0+Q*2,14,P.GL); at(T0+Q*2.5,11,P.GL)

-- ═══ 乐器动物园 · 全音色试听 ≈1分钟 ═══
-- 每种乐器弹一句上行琶音 0-2-4-7 (C4-E4-G4-C5), 控制台按顺序报名字
-- 延音乐器(提琴/管乐/极光)给 700ms 时值; 打击类同键不同音色
piano.clear()
local ZOO = {
  {0,0,"竖琴 Harp"},        {0,1,"钢琴 Piano"},       {0,2,"APPiano"},
  {0,3,"吉他 Guitar"},      {0,4,"尤克里里"},         {0,5,"钢片琴 Glock"},
  {0,6,"光之吉他"},         {0,7,"琵琶"},             {0,8,"卡林巴 Kalimba"},
  {0,9,"极光 MicAurora"},   {0,10,"琶音铃"},          {0,11,"小提琴 Violin"},
  {0,12,"四周年主音"},      {0,13,"金钢琴"},
  {1,0,"低音提琴"},         {1,1,"四周年贝斯"},       {1,2,"大提琴 Cello"},
  {2,1,"AP铃1"},            {2,2,"AP铃2"},            {2,3,"手碟 Handpan"},
  {2,4,"墩墩鼓"},           {2,5,"金手碟"},           {2,6,"金墩墩"},         {2,7,"TR909"},
  {3,1,"长笛 Flute"},       {3,2,"盖那笛"},           {3,3,"小号 Bugle"},
  {3,4,"陶笛 Ocarina"},     {3,5,"金小号"},           {3,6,"AP18陶笛"},
  {3,7,"萨克斯"},           {3,8,"四周年琶音"},       {3,9,"口琴"},           {3,10,"AP29短笛"},
  {4,0,"财富鼓"},           {4,1,"镲片 Cymbals"},
}
local SUST = { [0]=true,  [9]=true,  [11]=true }   -- type0: 极光/小提琴
local function sustaining(t, s)
  return t == 1 or t == 3 or (t == 0 and SUST[s])
end
local t, gap, step = 0, 1900, 260
for i, ins in ipairs(ZOO) do
  print(string.format("%2d. %s (type%d sub%d)", i, ins[3], ins[1], ins[2]))
  local dur = sustaining(ins[1], ins[2]) and 700 or 0
  local motif = {0, 2, 4, 7}
  for j, k in ipairs(motif) do
    piano.playAfter(k, t + (j - 1) * step, ins[1], ins[2], dur)
  end
  piano.playAfter(9, t + 4 * step, ins[1], ins[2], dur)  -- 收一个 C6 点头音
  t = t + gap
end
print(("共 %d 种, 总时长 %.0f 秒 — 听中哪个记编号, 歌曲里用 {type,sub} 或改 P.sky 的 ins"):format(#ZOO, t / 1000))

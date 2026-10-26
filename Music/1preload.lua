-- ═══ P 库 · 每次进游戏后先执行这一段 ═══
-- 之后各个歌曲文件直接粘贴即可 (文件之间相互独立, 都只依赖这里的 P)
P = {}

-- 乐器表 {type, sub} —— 名称见 piano.listInstruments()
P.PI = { 0, 1 } -- Piano       钢琴
P.AP = { 0, 2 } -- APPiano     APP钢琴
P.HA = { 0, 0 } -- Harp        竖琴
P.GL = { 0, 5 } -- Glock       钢片琴 (sub 4 是尤克里里)
P.LG = { 0, 6 } -- LightGuitar 光之吉他
P.KA = { 0, 8 } -- Kalimba     卡林巴
P.AB = { 0, 10 } -- ArpeggiatorBell 琶音铃
P.VI = { 0, 11 } -- Violin      小提琴(延音)
P.MA = { 0, 9 } -- MicAurora   极光(延音)
P.CE = { 1, 2 } -- Cello       大提琴(延音,音色低八度)
P.CB = { 1, 0 } -- Contrabass  低音提琴(延音,最低)
P.FL = { 3, 1 } -- Flute       长笛(延音)
P.OC = { 3, 4 } -- Ocarina     奥卡里那笛(延音)
P.BU = { 3, 3 } -- Bugle       小号(延音)
P.SAX = { 3, 7 } -- Saxophone   萨克斯(延音)
P.HP = { 2, 3 } -- Handpan     手碟
P.TR = { 2, 7 } -- TR909       鼓机

-- 常用和弦(15键键号): m=根音 b=五度 t=三和弦
P.CH = {
  C = { m = 4, b = 0, t = { 0, 2, 4 } },
  F = { m = 3, b = 0, t = { 3, 5, 7 } },
  G = { m = 4, b = 1, t = { 4, 6, 8 } },
  Am = { m = 5, b = 2, t = { 5, 7, 9 } },
  Dm = { m = 1, b = 5, t = { 1, 3, 5 } },
  Em = { m = 2, b = 6, t = { 2, 4, 6 } },
  G7 = { m = 4, b = 1, t = { 4, 6, 8 } },
}

-- 基础: t毫秒后弹 n(键号0-14), ins=P.PI 之类, dur=延音毫秒(延音乐器用)
function P.at(t, n, ins, dur) piano.playAfter(n, t, ins[1], ins[2], dur or 0) end

-- 简谱唱名 -> 键号: "1'"=C5, "5"=G4, "6"=A4 (低八度","不建议, 低于C4键不存在)
function P.deg(s) return tonumber(s:match("%d")) - 1 + (s:find("'") and 7 or 0) end

-- 三音琶奏: t时刻起, 每隔gap毫秒弹和弦一音
function P.strum(t, tr, ins, gap, dur)
  for i = 1, 3 do P.at(t + (i - 1) * (gap or 35), tr[i], ins, dur) end
end

-- ═══ Sky Studio 社区共享乐谱直读 ═══
-- 格式(StageGuard/SkyAutoPlayerScript 共享谱库等通用):
--   [{"name":..,"bpm":..,"songNotes":[{"time":毫秒,"key":"2Key0"},...]}]
-- "NKeyM" 的 M 就是 0..14 键号(N是编辑器页码,忽略);同 time = 和弦。
-- 谱源: github.com/StageGuard/SkyAutoPlayerScript 的 shared_sheets/ (175+首)
--
-- P.skyJson(整段JSON字符串, opts) -- 粘贴乐谱原文直接弹
-- P.sky(事件表, opts)             -- {{毫秒,键,键...},{...}} (转换器生成)
-- opts: ins=乐器(默认钢琴) speed=速度倍率(2=快一倍) dur=延音ms t0=起始延迟ms
--       noRpc=true 只发本地声音不发RPC(黑乐谱音墙; 需要mod支持第6参)
function P.sky(evs, opts)
  opts = opts or {}
  local ins = opts.ins or P.PI
  local speed = opts.speed or 1
  local gap = opts.chordGap or 12
  local start = opts.t0 or 0
  local norpc = (opts.noRpc or opts["local"]) and 1 or 0
  local count = 0
  for _, ev in ipairs(evs) do
    local t = start + math.floor(ev[1] / speed)
    for j = 2, #ev do
      piano.playAfter(ev[j], t + (j - 2) * gap, ins[1], ins[2], opts.dur or 0, norpc)
      count = count + 1
    end
  end
  return count
end

function P.skyJson(txt, opts)
  local evs = {}
  local lastT, cur
  for tm, key in string.gmatch(txt, '"time":(%d+),"key":"[0-9]-Key(%d+)"') do
    local t, k = tonumber(tm), tonumber(key)
    if t == lastT and cur then
      cur[#cur + 1] = k
    else
      cur = { t, k }
      evs[#evs + 1] = cur
      lastT = t
    end
  end
  return P.sky(evs, opts)
end

-- P.skyD(事件表, opts) -- 延音乐器专用: ev = {毫秒, 时值毫秒, 键, 键...}
-- 每个音按 MIDI 原始时值发音(到点自动 note-off), 适合提琴/管乐旋律声部
function P.skyD(evs, opts)
  opts = opts or {}
  local ins = opts.ins or P.VI
  local speed = opts.speed or 1
  local start = opts.t0 or 0
  local norpc = (opts.noRpc or opts["local"]) and 1 or 0
  local count = 0
  for _, ev in ipairs(evs) do
    local t = start + math.floor(ev[1] / speed)
    local d = math.floor((ev[2] or 500) / speed)
    for j = 3, #ev do
      piano.playAfter(ev[j], t + (j - 3) * 12, ins[1], ins[2], d, norpc)
      count = count + 1
    end
  end
  return count
end

-- ═══ 拍位制整谱播放(简谱APP截图转译格式) ═══
--   ROWS = { {小节1,小节2,...}, ... }  每小节 = {"1'@0","7@1",...} ("唱名@拍位")
--   时值 = 到同小节下一音的距离, 弱起(<2拍)/空小节自动跳过伴奏
-- opts: Q=四分音符ms(默认520)  mel=旋律乐器(默认钢琴)  rot/rotClose=和弦轮换
--       closeFrom=结尾段起始行  lastChord=最后一行和弦  chords/bass=false可关
function P.rows(ROWS, opts)
  opts = opts or {}
  local Q = opts.Q or 520
  local BAR = Q * 4
  local mel = opts.mel or P.PI
  local rot = opts.rot or { "Am", "F", "C", "G" }
  local rotClose = opts.rotClose or { "G", "Am", "F", "C" }
  local t = 0
  for ri, row in ipairs(ROWS) do
    for bi, bar in ipairs(row) do
      local name
      if opts.chordRow and opts.chordRow[ri] then
        name = opts.chordRow[ri][bi]
      elseif ri == #ROWS and opts.lastChord then
        name = opts.lastChord[bi]
      elseif ri >= (opts.closeFrom or #ROWS) then
        name = rotClose[bi]
      else
        name = rot[bi]
      end
      for _, tok in ipairs(bar) do
        local p, off = tok:match("([^@]+)@([%d%.]+)")
        P.at(t + tonumber(off) * Q, P.deg(p), mel)
      end
      if #bar >= 2 and tonumber(bar[1]:match("@([%d%.]+)")) < 2 then
        local c = P.CH[name]
        if c then
          if opts.chords ~= false then for j = 1, 3 do P.at(t + (j - 1) * 45, c.t[j], P.CE, Q * 3.6) end end
          if opts.bass ~= false then
            P.at(t, c.m, P.CB, Q * 1.7)
            P.at(t + 2 * Q, c.b, P.CB, Q * 1.7)
          end
        end
      end
      t = t + BAR
    end
  end
  return t
end

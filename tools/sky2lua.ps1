# sky2lua.ps1 — Sky Studio 乐谱(.txt JSON) -> songs/ 下的 lua 歌曲文件
# 用法: powershell -File tools\sky2lua.ps1 谱子.txt [乐器名]
#   乐器名: piano(默认)/ap/violin/cello/flute/harp/glock
# 生成: songs\sky_<曲名>.lua, 游戏里先贴 lib.lua 再贴生成的文件

param(
  [Parameter(Mandatory = $true)][string]$Sheet,
  [string]$Instrument = "piano"
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [Text.Encoding]::UTF8

$insMap = @{
  piano = "P.PI"; ap = "P.AP"; violin = "P.VI"; cello = "P.CE"
  flute = "P.FL"; harp = "P.HA"; glock = "P.GL"; bass = "P.CB"
}
if (-not $insMap.ContainsKey($Instrument)) { throw "未知乐器: $Instrument" }

$raw = Get-Content $Sheet -Raw -Encoding UTF8
$j = ConvertFrom-Json $raw
$s = if ($j -is [array]) { $j[0] } else { $j }

$notes = @($s.songNotes)
Write-Host ("曲名: {0}  bpm={1}  pitchLevel={2}  音符={3}" -f $s.name, $s.bpm, $s.pitchLevel, $notes.Count)

# 同 time 合并为和弦; "NKeyM" 的 M 即 0..14 键号
$events = New-Object System.Collections.Generic.List[string]
$lastT = -1; $line = $null
foreach ($n in $notes) {
  if ($n.key -notmatch '^(?:\d)?Key(\d+)$') { continue }
  $k = [int]$Matches[1]
  if ($n.time -eq $lastT -and $line) {
    $line += ",$k"
  } else {
    if ($line) { $events.Add($line + "}") }
    $line = ("{{{0},{1}" -f $n.time, $k)
    $lastT = $n.time
  }
}
if ($line) { $events.Add($line + "}") }
Write-Host ("事件(含和弦): " + $events.Count)

$safe = ($s.name -replace '[\\/:*?"<>|]', '_')
$out = Join-Path $PSScriptRoot "..\songs\sky_$safe.lua"
$header = @"
-- 《$($s.name)》 Sky Studio 社区谱自动转换
-- 原谱: $Sheet  bpm=$($s.bpm)  音符$($notes.Count)个  pitchLevel=$($s.pitchLevel)
-- 需要 lib.lua; 可改 P.sky 第三参: {ins=P.VI, speed=2(快一倍), dur=800}
piano.clear()
local E={
"@
$body = ($events -join ",
")
$footer = @"
}
local n = P.sky(E, {ins=$($insMap[$Instrument])})
print("queued " .. n .. " notes")
"@
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($out, ($header + $body + $footer), $utf8NoBom)
Write-Host ("已生成: " + (Resolve-Path $out))

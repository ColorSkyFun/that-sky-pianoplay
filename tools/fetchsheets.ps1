# fetchsheets.ps1 - download shared sheets listed in sheets\fetchlist.txt
# and convert each to songs\sky_<name>.lua via sky2lua.ps1
[Console]::OutputEncoding = [Text.Encoding]::UTF8
$base = 'https://raw.githubusercontent.com/StageGuard/SkyAutoPlayerScript/master/shared_sheets/'
$dir = Join-Path $PSScriptRoot 'sheets'
$names = Get-Content (Join-Path $dir 'fetchlist.txt') -Encoding UTF8 | Where-Object { $_.Trim() -ne '' }
foreach ($n in $names) {
  $n = $n.Trim()
  $enc = [Uri]::EscapeDataString($n)
  $out = Join-Path $dir $n
  try {
    Invoke-WebRequest -UseBasicParsing -OutFile $out ($base + $enc)
    Write-Host ("OK  " + $n)
    & (Join-Path $PSScriptRoot 'sky2lua.ps1') $out piano
  } catch {
    Write-Host ("FAIL " + $n + " : " + $_.Exception.Message)
  }
}

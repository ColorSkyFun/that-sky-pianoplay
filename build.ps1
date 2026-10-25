# build.ps1 — 一键: 编译 mod -> 部署到游戏 -> (可选)重启游戏
# 用法:
#   powershell -File build.ps1              # 编译 + 部署 + 重启游戏
#   powershell -File build.ps1 -NoDeploy    # 只编译
#   powershell -File build.ps1 -NoLaunch    # 编译+部署, 不自动启动游戏
param(
  [switch]$NoDeploy,
  [switch]$NoLaunch
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$mod  = Join-Path $root 'mod'
$sdk  = Join-Path $root '..\html-sdk\sdk'
$out  = Join-Path $mod 'build'
New-Item -ItemType Directory -Force -Path $out | Out-Null

if (-not (Test-Path (Join-Path $sdk 'include\htmodloader\htmod.h'))) {
  throw "SDK not found at $sdk -- expected native\html-sdk\sdk"
}
$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gpp) {
  # scoop shims sometimes live outside PATH for non-interactive shells
  $gpp = Get-ChildItem "$env:USERPROFILE\scoop\apps\mingw\current\bin\g++.exe" -ErrorAction SilentlyContinue
}
if (-not $gpp) { throw 'g++ not found (install mingw via scoop)' }
$compiler = if ($gpp -is [System.Management.Automation.CommandInfo]) { $gpp.Source } else { $gpp.FullName }

$src = Get-ChildItem (Join-Path $mod 'src\*.cpp') | ForEach-Object { $_.FullName }
& $compiler -shared -O2 -std=c++17 -Wall -I (Join-Path $sdk 'include') `
  -o (Join-Path $out 'piano-rpc.dll') $src (Join-Path $sdk 'lib\htmodloader.lib') -static -s
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
Write-Host 'BUILD_OK ->' (Join-Path $out 'piano-rpc.dll')

if ($NoDeploy) { return }

$dst = 'E:\SteamLibrary\steamapps\common\Sky Children of the Light\htmodloader\mods\piano-rpc\piano-rpc.dll'
if (-not (Test-Path (Split-Path $dst))) { throw "game mod dir not found: $(Split-Path $dst)" }
Stop-Process -Name Sky -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 3
Copy-Item (Join-Path $out 'piano-rpc.dll') $dst -Force
Write-Host 'DEPLOY_OK ->' $dst

if (-not $NoLaunch) {
  Start-Process 'steam://rungameid/2325290'
  Write-Host 'GAME_LAUNCHING'
}

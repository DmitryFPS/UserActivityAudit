#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Dist = Join-Path $RepoRoot 'build\dist'
$Native = Join-Path $RepoRoot 'build\native'
$WixBin = "${env:ProgramFiles(x86)}\WiX Toolset v3.14\bin"
if (-not (Test-Path "$WixBin\candle.exe")) {
    throw "WiX not found at $WixBin. Install WiX Toolset v3.14."
}

New-Item -ItemType Directory -Force -Path $Dist | Out-Null
Copy-Item "$Native\UserAuditSvc\Release\UserAudit.exe" $Dist -Force
Copy-Item "$Native\UserAuditWatchdog\Release\UserAuditWatchdog.exe" $Dist -Force
Copy-Item "$Native\UserAuditAdmin\Release\UserAuditAdmin.exe" $Dist -Force
Copy-Item "$Native\UserAuditKeygen\Release\UserAuditKeygen.exe" $Dist -Force
Copy-Item (Join-Path $RepoRoot 'installer\config.example.json') $Dist -Force
Copy-Item (Join-Path $RepoRoot 'installer\deploy.ps1') $Dist -Force

$Wxs = Join-Path $RepoRoot 'installer\wix\Product.wxs'
$OutMsi = Join-Path $RepoRoot 'build\UserAuditSetup.msi'
$WixObj = Join-Path $RepoRoot 'build\wix\Product.wixobj'
New-Item -ItemType Directory -Force -Path (Split-Path $WixObj) | Out-Null

$DistAbs = (Resolve-Path $Dist).Path
& "$WixBin\candle.exe" "-dSourceDir=$DistAbs" -out $WixObj $Wxs
if ($LASTEXITCODE -ne 0) { throw 'candle failed' }
& "$WixBin\light.exe" -out $OutMsi $WixObj -sice:ICE57 -sice:ICE59 -sice:ICE60 -sice:ICE61
if ($LASTEXITCODE -ne 0) { throw 'light failed' }

Write-Host "MSI: $OutMsi"

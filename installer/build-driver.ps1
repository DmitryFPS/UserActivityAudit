#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$MsBuild = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
$Project = Join-Path $RepoRoot "native\UserAuditFilter\UserAuditFilter.vcxproj"
$OutSys = Join-Path $RepoRoot "build\native\UserAuditFilter\Release\UserAuditFilter.sys"

if (-not (Test-Path $MsBuild)) {
    throw "MSBuild not found: $MsBuild"
}

$header = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include\*\km\fltkernel.h" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $header) {
    throw "WDK kernel headers not found. Install WDK 10.0.26100+."
}

Write-Host "Building UserAuditFilter.sys..."
& $MsBuild $Project /p:Configuration=Release /p:Platform=x64 /m
if ($LASTEXITCODE -ne 0) { throw "Driver build failed." }
if (-not (Test-Path $OutSys)) {
    throw "Expected output not found: $OutSys"
}

Write-Host "OK: $OutSys ($((Get-Item $OutSys).Length) bytes)"

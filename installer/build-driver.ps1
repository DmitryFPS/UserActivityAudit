#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$MsBuild = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
$Project = Join-Path $RepoRoot "native\UserAuditFilter\UserAuditFilter.vcxproj"
$OutSys = Join-Path $RepoRoot "build\native\UserAuditFilter\Release\UserAuditFilter.sys"
$ToolsetDir = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Microsoft\VC\v170\Platforms\x64\PlatformToolsets\WindowsKernelModeDriver10.0"

if (-not (Test-Path $MsBuild)) {
    throw "MSBuild not found: $MsBuild"
}

$header = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Include\*\km\fltkernel.h" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $header) {
    throw "WDK kernel headers not found. Install WDK 10.0.26100+."
}

if (-not (Test-Path (Join-Path $ToolsetDir 'Toolset.props'))) {
    Write-Host 'WDK VS toolset missing — running setup-wdk-toolset.ps1 (admin)...'
    & (Join-Path $PSScriptRoot 'setup-wdk-toolset.ps1')
}

# Keep VS toolset in sync with repo stubs (no admin if writable).
$ToolsetSrc = Join-Path $PSScriptRoot 'wdk-toolset\x64\WindowsKernelModeDriver10.0'
Copy-Item (Join-Path $ToolsetSrc 'Toolset.props') (Join-Path $ToolsetDir 'Toolset.props') -Force -ErrorAction SilentlyContinue
Copy-Item (Join-Path $ToolsetSrc 'Toolset.targets') (Join-Path $ToolsetDir 'Toolset.targets') -Force -ErrorAction SilentlyContinue

$pf86 = ${env:ProgramFiles(x86)}
$wdkInclude = Get-ChildItem (Join-Path $pf86 'Windows Kits\10\Include') -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName 'km\fltkernel.h') } |
    Sort-Object Name -Descending |
    Select-Object -First 1
if (-not $wdkInclude) {
    throw 'WDK kernel headers not found.'
}
$wdkRoot = Join-Path $pf86 'Windows Kits\10\'
$wdkBuild = $wdkInclude.Name

Write-Host "Building UserAuditFilter.sys (WDK $wdkBuild)..."
& $MsBuild $Project /p:Configuration=Release /p:Platform=x64 /m
if ($LASTEXITCODE -ne 0) { throw "Driver build failed." }
if (-not (Test-Path $OutSys)) {
    throw "Expected output not found: $OutSys"
}

Write-Host "OK: $OutSys ($((Get-Item $OutSys).Length) bytes)"

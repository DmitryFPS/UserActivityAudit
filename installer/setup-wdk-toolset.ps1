#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Installs WDK PlatformToolset into VS Build Tools (fixes MSB8020).
#>
$ErrorActionPreference = 'Stop'

$pf86 = ${env:ProgramFiles(x86)}
$VsRoot = Join-Path $pf86 'Microsoft Visual Studio\2022\BuildTools'
$VcPlatforms = Join-Path $VsRoot 'MSBuild\Microsoft\VC\v170\Platforms\x64'
$ToolsetDst = Join-Path $VcPlatforms 'PlatformToolsets\WindowsKernelModeDriver10.0'
$ImportDstDir = Join-Path $VcPlatforms 'ImportAfter'
$ToolsetSrc = Join-Path $PSScriptRoot 'wdk-toolset\x64\WindowsKernelModeDriver10.0'

if (-not (Test-Path $VsRoot)) {
    throw "Visual Studio 2022 Build Tools not found: $VsRoot"
}

$wdkInclude = Get-ChildItem (Join-Path $pf86 'Windows Kits\10\Include') -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName 'km\fltkernel.h') } |
    Sort-Object Name -Descending |
    Select-Object -First 1
if (-not $wdkInclude) {
    throw 'WDK kernel headers not found. Install WDK 10.0.26100+.'
}

$wdkBuild = Join-Path $pf86 "Windows Kits\10\build\$($wdkInclude.Name)"
$importSrc = Join-Path $wdkBuild 'x64\ImportAfter\WDK.x64.WindowsKernelModeDriver.Platform.props'
if (-not (Test-Path $importSrc)) {
    throw "WDK platform import not found: $importSrc"
}

New-Item -ItemType Directory -Force -Path $ToolsetDst | Out-Null
New-Item -ItemType Directory -Force -Path $ImportDstDir | Out-Null

Copy-Item (Join-Path $ToolsetSrc 'Toolset.props') (Join-Path $ToolsetDst 'Toolset.props') -Force
Copy-Item (Join-Path $ToolsetSrc 'Toolset.targets') (Join-Path $ToolsetDst 'Toolset.targets') -Force
Copy-Item $importSrc (Join-Path $ImportDstDir 'WDK.x64.WindowsKernelModeDriver.Platform.props') -Force

Write-Host "[OK] WDK toolset installed:"
Write-Host "  $ToolsetDst"
Write-Host "  $(Join-Path $ImportDstDir 'WDK.x64.WindowsKernelModeDriver.Platform.props')"

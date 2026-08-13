#Requires -Version 5.1
<#
.SYNOPSIS
    Production build: native Release, admin publish, MSI, pilot dist/ package.

.PARAMETER SkipNative
    Skip cmake native build.

.PARAMETER SkipDriver
    Skip UserAuditFilter.sys build attempt.

.PARAMETER Sign
    Run sign.ps1 after build (requires EV cert in store or PFX path).

.EXAMPLE
    .\installer\build-dist.ps1
#>
[CmdletBinding()]
param(
    [switch] $SkipNative,
    [switch] $SkipDriver,
    [switch] $Sign,
    [string] $SignPfx = '',
    [string] $SignPassword = ''
)

$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Version = (Get-Content (Join-Path $RepoRoot 'VERSION') -Raw).Trim()
$DistRoot = Join-Path $RepoRoot "dist\UserActivityAudit-$Version"
$BuildNative = Join-Path $RepoRoot 'build\native'

function Write-Step([string] $Message) {
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Ensure-Cmake {
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        $fallback = 'C:\Program Files\CMake\bin\cmake.exe'
        if (Test-Path $fallback) { $env:Path = "C:\Program Files\CMake\bin;$env:Path" }
        else { throw 'cmake not found' }
    }
}

Write-Step "UserActivityAudit production build v$Version"

if (-not $SkipNative) {
    Write-Step 'Native Release + unit tests'
    Ensure-Cmake
    & cmake -S (Join-Path $RepoRoot 'native') -B $BuildNative -G 'Visual Studio 17 2022' -A x64 | Out-Null
    & cmake --build $BuildNative --config Release
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed' }
    & ctest --test-dir $BuildNative -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Unit tests failed' }
}

Write-Step 'Admin Dashboard publish'
& dotnet publish (Join-Path $RepoRoot 'admin\UserAudit.Dashboard\UserAudit.Dashboard.csproj') `
    -c Release -r win-x64 --self-contained false -o (Join-Path $RepoRoot 'build\publish\Dashboard')
if ($LASTEXITCODE -ne 0) { throw 'Dashboard publish failed' }

Write-Step 'SmokeImport + TamperVerifyTest'
& dotnet build (Join-Path $RepoRoot 'installer\SmokeImport\SmokeImport.csproj') -c Release | Out-Null
& dotnet build (Join-Path $RepoRoot 'installer\TamperVerifyTest\TamperVerifyTest.csproj') -c Release | Out-Null

Write-Step 'MSI'
& (Join-Path $RepoRoot 'installer\build-msi.ps1')

$DriverSys = Join-Path $BuildNative 'UserAuditFilter\Release\UserAuditFilter.sys'
if (-not $SkipDriver) {
    Write-Step 'Minifilter driver (optional)'
    try {
        & (Join-Path $RepoRoot 'installer\build-driver.ps1')
    } catch {
        Write-Warning "Driver build skipped: $($_.Exception.Message)"
    }
}

Write-Step "Assembling dist: $DistRoot"
if (Test-Path $DistRoot) { Remove-Item $DistRoot -Recurse -Force }
New-Item -ItemType Directory -Force -Path $DistRoot | Out-Null

$agentDir = Join-Path $DistRoot 'Agent'
$adminDir = Join-Path $DistRoot 'Admin'
$toolsDir = Join-Path $DistRoot 'Tools'
$docsDir = Join-Path $DistRoot 'Docs'
foreach ($d in @($agentDir, $adminDir, $toolsDir, $docsDir)) { New-Item -ItemType Directory -Force -Path $d | Out-Null }

Copy-Item (Join-Path $RepoRoot 'build\UserAuditSetup.msi') $agentDir -Force
Copy-Item (Join-Path $RepoRoot 'installer\deploy.ps1') $agentDir -Force
Copy-Item (Join-Path $RepoRoot 'installer\config.example.json') $agentDir -Force
Copy-Item (Join-Path $BuildNative 'UserAuditSvc\Release\UserAudit.exe') $agentDir -Force
Copy-Item (Join-Path $BuildNative 'UserAuditWatchdog\Release\UserAuditWatchdog.exe') $agentDir -Force
Copy-Item (Join-Path $BuildNative 'UserAuditAdmin\Release\UserAuditAdmin.exe') $agentDir -Force
Copy-Item (Join-Path $BuildNative 'UserAuditKeygen\Release\UserAuditKeygen.exe') $toolsDir -Force

Copy-Item (Join-Path $RepoRoot 'build\publish\Dashboard\*') $adminDir -Recurse -Force
Copy-Item (Join-Path $RepoRoot 'installer\SmokeImport\bin\Release\net10.0\*') $toolsDir -Force -ErrorAction SilentlyContinue
Copy-Item (Join-Path $RepoRoot 'installer\TamperVerifyTest\bin\Release\net10.0\*') $toolsDir -Force -ErrorAction SilentlyContinue
Copy-Item (Join-Path $RepoRoot 'installer\soak-test.ps1') $toolsDir -Force
Copy-Item (Join-Path $RepoRoot 'installer\verify-reboot.ps1') $toolsDir -Force
Copy-Item (Join-Path $RepoRoot 'installer\verify-soak.ps1') $toolsDir -Force
Copy-Item (Join-Path $RepoRoot 'installer\verify-driver.ps1') $toolsDir -Force
Copy-Item (Join-Path $RepoRoot 'installer\verify-dist.ps1') $toolsDir -Force

if (Test-Path $DriverSys) {
    New-Item -ItemType Directory -Force -Path (Join-Path $agentDir 'Driver') | Out-Null
    Copy-Item $DriverSys (Join-Path $agentDir 'Driver') -Force
    Copy-Item (Join-Path $RepoRoot 'native\UserAuditFilter\UserAuditFilter.inf') (Join-Path $agentDir 'Driver') -Force
}

$docFiles = @(
    'docs\InstallGuide.md', 'docs\AdminGuide.md', 'docs\SecurityModel.md',
    'docs\STANDALONE.md', 'docs\DEPLOY.md', 'docs\TESTING.md', 'docs\DRIVER.md',
    'docs\DRIVER-BUILD.md', 'docs\SIGNING-CHECKLIST.md', 'docs\PRODUCTION.md',
    'RELEASE_NOTES.md', 'docs\QA-PERFORMANCE.md'
)
foreach ($rel in $docFiles) {
    $src = Join-Path $RepoRoot $rel
    if (Test-Path $src) { Copy-Item $src $docsDir -Force }
}

@"
UserActivityAudit $Version — release package
Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm')

Agent/
  UserAuditSetup.msi     Silent install (recommended)
  deploy.ps1             Script deploy + smoke checks
  UserAudit.exe          Service binary
  UserAuditAdmin.exe     IT USB stop/uninstall
  config.example.json

Admin/
  UserAudit.Dashboard.exe + deps   Log analyzer (run as admin)

Tools/
  UserAuditKeygen.exe    IT USB key ceremony (offline)
  soak-test.ps1          24h monitoring
  SmokeImport.exe        QA import check

Docs/
  InstallGuide.md, AdminGuide.md, SecurityModel.md

Install: msiexec /i Agent\UserAuditSetup.msi /quiet
Analyze: Admin\UserAudit.Dashboard.exe (same PC, admin)
"@ | Set-Content (Join-Path $DistRoot 'README.txt') -Encoding UTF8

if ($Sign) {
    Write-Step 'Code signing'
    $signArgs = @{ DistRoot = $DistRoot }
    if ($SignPfx) { $signArgs.Pfx = $SignPfx; $signArgs.Password = $SignPassword }
    & (Join-Path $RepoRoot 'installer\sign.ps1') @signArgs
}

$zip = Join-Path $RepoRoot "dist\UserActivityAudit-$Version-win-x64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $DistRoot -DestinationPath $zip -Force

Write-Step 'Done'
Write-Host "  Dist:  $DistRoot"
Write-Host "  ZIP:   $zip"
Write-Host "  MSI:   $(Join-Path $agentDir 'UserAuditSetup.msi')"

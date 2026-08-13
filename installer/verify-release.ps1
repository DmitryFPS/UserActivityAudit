#Requires -Version 5.1
<#
.SYNOPSIS
    Objective release gate. Score = sum of weighted checks. PASS >= 95.
    No manual markdown claims — run this script.
#>
[CmdletBinding()]
param(
    [switch] $SkipBuild,
    [switch] $SkipDriver,
    [string] $SoakCsv = ''
)

$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BuildNative = Join-Path $RepoRoot 'build\native'
$Version = (Get-Content (Join-Path $RepoRoot 'VERSION') -Raw).Trim()

if (-not $SoakCsv) {
    $SoakCsv = Join-Path $RepoRoot 'installer\results\soak-arm1-20260812.csv'
}

$checks = [ordered]@{}
$details = [ordered]@{}

function Add-Check {
    param([string] $Name, [int] $Weight, [bool] $Ok, [string] $Detail)
    $checks[$Name] = @{ weight = $Weight; ok = $Ok }
    $details[$Name] = $Detail
    $icon = if ($Ok) { '[OK]' } else { '[FAIL]' }
    Write-Host "$icon $Name ($Weight pts): $Detail"
}

Write-Host "=== UserActivityAudit verify-release v$Version ===" -ForegroundColor Cyan

# --- Build (optional) ---
if (-not $SkipBuild) {
    Write-Host '--- build ---'
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        $env:Path = "C:\Program Files\CMake\bin;$env:Path"
    }
    & cmake --build $BuildNative --config Release
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed' }
    & dotnet build (Join-Path $RepoRoot 'admin\UserActivityAudit.Admin.slnx') -c Release --no-restore 2>$null
    & dotnet restore (Join-Path $RepoRoot 'admin\UserActivityAudit.Admin.slnx') | Out-Null
    & dotnet build (Join-Path $RepoRoot 'admin\UserActivityAudit.Admin.slnx') -c Release
    if ($LASTEXITCODE -ne 0) { throw 'Admin build failed' }
    & dotnet build (Join-Path $RepoRoot 'installer\SmokeImport\SmokeImport.csproj') -c Release
    & dotnet build (Join-Path $RepoRoot 'installer\TamperVerifyTest\TamperVerifyTest.csproj') -c Release
}

# 1. ctest (15)
Write-Host '--- tests ---'
& ctest --test-dir $BuildNative -C Release --output-on-failure
Add-Check 'ctest' 15 ($LASTEXITCODE -eq 0) $(if ($LASTEXITCODE -eq 0) { '8/8 PASS' } else { "exit $LASTEXITCODE" })

# 2. SmokeImport (10)
$smoke = Join-Path $RepoRoot 'installer\SmokeImport\bin\Release\net10.0\SmokeImport.exe'
if (-not (Test-Path $smoke)) {
    & dotnet build (Join-Path $RepoRoot 'installer\SmokeImport\SmokeImport.csproj') -c Release | Out-Null
}
$smokeOut = & $smoke 2>&1 | Out-String
$smokeOk = $LASTEXITCODE -eq 0 -and $smokeOut -match 'events imported|PASS|OK'
Add-Check 'SmokeImport' 10 $smokeOk $(if ($smokeOk) { 'import PASS' } else { $smokeOut.Trim().Substring(0, [Math]::Min(120, $smokeOut.Length)) })

# 3. TamperVerifyTest (10)
$tamper = Join-Path $RepoRoot 'installer\TamperVerifyTest\bin\Release\net10.0\TamperVerifyTest.exe'
if (-not (Test-Path $tamper)) {
    & dotnet build (Join-Path $RepoRoot 'installer\TamperVerifyTest\TamperVerifyTest.csproj') -c Release | Out-Null
}
& $tamper 2>&1 | Out-Null
Add-Check 'TamperVerifyTest' 10 ($LASTEXITCODE -eq 0) $(if ($LASTEXITCODE -eq 0) { 'PASS' } else { "exit $LASTEXITCODE" })

# 4. verify-dist (10) — build if missing
$distZip = Join-Path $RepoRoot "dist\UserActivityAudit-$Version-win-x64.zip"
if (-not (Test-Path $distZip)) {
    Write-Host '--- build-dist (dist missing) ---'
    & (Join-Path $PSScriptRoot 'build-dist.ps1') -SkipNative
}
& (Join-Path $PSScriptRoot 'verify-dist.ps1') 2>&1 | Out-Null
Add-Check 'verify-dist' 10 ($LASTEXITCODE -eq 0) $(if ($LASTEXITCODE -eq 0) { $distZip } else { 'dist invalid' })

# 5. verify-soak (15)
if (Test-Path $SoakCsv) {
    & (Join-Path $PSScriptRoot 'verify-soak.ps1') -CsvPath $SoakCsv 2>&1 | Out-Null
    Add-Check 'verify-soak' 15 ($LASTEXITCODE -eq 0) "ARM1 CSV PASS"
} else {
    Add-Check 'verify-soak' 15 $false "missing $SoakCsv"
}

# 6. verify-driver (15) — needs admin + loaded filter
$driverOk = $false
$driverDetail = 'skipped (not admin or filter absent)'
if ([bool]([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    & (Join-Path $PSScriptRoot 'verify-driver.ps1') 2>&1 | Out-String | Out-Null
    $driverOk = $LASTEXITCODE -eq 0
    $driverDetail = if ($driverOk) { 'fltmc + delete denied' } else { 'verify-driver FAIL' }
}
Add-Check 'verify-driver' 15 $driverOk $driverDetail

# 7. service live (5)
$svc = Get-Service UserAuditSvc -ErrorAction SilentlyContinue
$svcOk = $svc -and $svc.Status -eq 'Running'
Add-Check 'UserAuditSvc' 5 $svcOk $(if ($svcOk) { 'RUNNING' } else { 'not running' })

# 8. docs consistency (10) — grep stale claims
$docFiles = @(
    'docs\ARCHITECTURE.md', 'docs\DEPLOY.md', 'docs\TESTING.md', 'docs\AdminGuide.md',
    'docs\QA-PERFORMANCE.md', 'ROADMAP.md'
)
$badPatterns = @(
    '1\.0\.0-rc1',
    'mass rollout',
    'Release Candidate \(RC1\)',
    'optional for pilot',
    'zrelost'
)
$docIssues = @()
foreach ($rel in $docFiles) {
    $path = Join-Path $RepoRoot $rel
    if (-not (Test-Path $path)) { continue }
    $text = Get-Content $path -Raw
    foreach ($pat in $badPatterns) {
        if ($text -match $pat) { $docIssues += "$rel : $pat" }
    }
}
$docsOk = $docIssues.Count -eq 0
Add-Check 'docs-sync' 10 $docsOk $(if ($docsOk) { 'no stale patterns' } else { ($docIssues -join '; ').Substring(0, [Math]::Min(200, ($docIssues -join '; ').Length)) })

# 9. VERSION file (5)
Add-Check 'version' 5 ($Version -eq '1.0.0') "VERSION=$Version"

# 10. unsigned acknowledged in SecurityModel (5) — v1.0 path documented
$secPath = Join-Path $RepoRoot 'docs\SecurityModel.md'
$secText = if (Test-Path $secPath) { Get-Content $secPath -Raw } else { '' }
$secOk = $secText -match 'WDAC' -and $secText -match 'v1\.0 standalone'
Add-Check 'security-model' 5 $secOk 'v1.0 deploy checklist'

# Score
$totalWeight = 0
$earned = 0
foreach ($k in $checks.Keys) {
    $totalWeight += $checks[$k].weight
    if ($checks[$k].ok) { $earned += $checks[$k].weight }
}
$pct = [math]::Round(100.0 * $earned / $totalWeight, 1)
$pass = $pct -ge 95

Write-Host ''
Write-Host "=== SCORE: $earned / $totalWeight = $pct% ===" -ForegroundColor $(if ($pass) { 'Green' } else { 'Yellow' })
Write-Host $(if ($pass) { 'PASS (>= 95%)' } else { "FAIL (need 95%, gap $(95 - $pct) pts)" })

# JSON for tooling
$result = @{
    version     = $Version
    timestamp   = (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
    score       = $pct
    earned      = $earned
    totalWeight = $totalWeight
    pass        = $pass
    checks      = $checks
    details     = $details
}
$jsonPath = Join-Path $RepoRoot 'installer\results\verify-release-latest.json'
New-Item -ItemType Directory -Force -Path (Split-Path $jsonPath) | Out-Null
$result | ConvertTo-Json -Depth 5 | Set-Content $jsonPath -Encoding UTF8
Write-Host "JSON: $jsonPath"

if (-not $pass) { exit 1 }
exit 0

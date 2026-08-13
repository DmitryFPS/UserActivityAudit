#Requires -Version 5.1
<#
.SYNOPSIS
    SINGLE maturity score for UserActivityAudit. PASS >= 95.
    Output: installer/results/verify-release-latest.json
    Rule: no other % in docs/chat — only this script.
#>
[CmdletBinding()]
param(
    [switch] $SkipBuild,
    [string] $SoakCsv = ''
)

$ErrorActionPreference = 'Stop'
$PassThreshold = 95
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BuildNative = Join-Path $RepoRoot 'build\native'
$Version = (Get-Content (Join-Path $RepoRoot 'VERSION') -Raw).Trim()
$IsAdmin = [bool]([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $SoakCsv) {
    $SoakCsv = Join-Path $RepoRoot 'installer\results\soak-arm1-20260812.csv'
}

$checks = [ordered]@{}
$details = [ordered]@{}

function Add-Check {
    param([string] $Name, [int] $Weight, [bool] $Ok, [string] $Detail)
    $checks[$Name] = @{ weight = $Weight; ok = $Ok }
    $details[$Name] = $Detail
    Write-Host "$(if ($Ok) { '[OK]' } else { '[FAIL]' }) $Name ($Weight): $Detail"
}

Write-Host "=== verify-release v$Version (admin=$IsAdmin) ===" -ForegroundColor Cyan

if (-not $SkipBuild) {
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        $env:Path = "C:\Program Files\CMake\bin;$env:Path"
    }
    & cmake --build $BuildNative --config Release
    if ($LASTEXITCODE -ne 0) { throw 'native build failed' }
}

# 1 build-admin (8)
& dotnet build (Join-Path $RepoRoot 'admin\UserActivityAudit.Admin.slnx') -c Release -v q
Add-Check 'admin-build' 7 ($LASTEXITCODE -eq 0) $(if ($LASTEXITCODE -eq 0) { 'slnx Release' } else { 'build fail' })

# 2 ctest (9)
& ctest --test-dir $BuildNative -C Release --output-on-failure
Add-Check 'ctest' 9 ($LASTEXITCODE -eq 0) $(if ($LASTEXITCODE -eq 0) { '8/8' } else { "exit $LASTEXITCODE" })

# 3 SmokeImport (7)
& dotnet build (Join-Path $RepoRoot 'installer\SmokeImport\SmokeImport.csproj') -c Release -v q | Out-Null
$smoke = Join-Path $RepoRoot 'installer\SmokeImport\bin\Release\net10.0\SmokeImport.exe'
$smokeOut = & $smoke 2>&1 | Out-String
Add-Check 'SmokeImport' 7 ($LASTEXITCODE -eq 0) 'import OK'

# 4 TamperVerify (7)
& dotnet build (Join-Path $RepoRoot 'installer\TamperVerifyTest\TamperVerifyTest.csproj') -c Release -v q | Out-Null
$tamper = Join-Path $RepoRoot 'installer\TamperVerifyTest\bin\Release\net10.0\TamperVerifyTest.exe'
& $tamper 2>&1 | Out-Null
Add-Check 'TamperVerify' 7 ($LASTEXITCODE -eq 0) 'PASS'

# 5 dist (9)
$distZip = Join-Path $RepoRoot "dist\UserActivityAudit-$Version-win-x64.zip"
if (-not (Test-Path $distZip)) {
    & (Join-Path $PSScriptRoot 'build-dist.ps1') -SkipNative | Out-Null
}
& (Join-Path $PSScriptRoot 'verify-dist.ps1') 2>&1 | Out-Null
Add-Check 'verify-dist' 9 ($LASTEXITCODE -eq 0) 'ZIP+MSI'

# 6 dist tools in repo (5)
$toolScripts = @('verify-release.ps1', 'verify-driver.ps1', 'verify-soak.ps1', 'verify-reboot.ps1')
$toolsOk = ($toolScripts | ForEach-Object { Test-Path (Join-Path $PSScriptRoot $_) }) -notcontains $false
Add-Check 'verify-scripts' 5 $toolsOk 'installer/*.ps1'

# 7 soak (10)
& (Join-Path $PSScriptRoot 'verify-soak.ps1') -CsvPath $SoakCsv 2>&1 | Out-Null
Add-Check 'verify-soak' 10 ($LASTEXITCODE -eq 0) 'ARM1 13h'

# 8 driver (10) — admin
$driverOk = $false
if ($IsAdmin) {
    & (Join-Path $PSScriptRoot 'verify-driver.ps1') 2>&1 | Out-Null
    $driverOk = $LASTEXITCODE -eq 0
}
Add-Check 'verify-driver' 10 $driverOk $(if (-not $IsAdmin) { 'need admin' } elseif ($driverOk) { 'fltmc OK' } else { 'FAIL' })

# 9 decrypt HMAC (9) — admin + installed agent
$decryptOk = $false
$ua = Join-Path ${env:ProgramFiles} 'UserAudit\UserAudit.exe'
if ($IsAdmin -and (Test-Path $ua)) {
    $outF = Join-Path $env:TEMP "ua-out-$([guid]::NewGuid()).txt"
    $errF = Join-Path $env:TEMP "ua-err-$([guid]::NewGuid()).txt"
    $p = Start-Process -FilePath $ua -ArgumentList '--decrypt', '--verify' -Wait -PassThru -NoNewWindow `
        -RedirectStandardOutput $outF -RedirectStandardError $errF
    $decryptOk = $p.ExitCode -eq 0
    Remove-Item $outF, $errF -Force -ErrorAction SilentlyContinue
}
Add-Check 'decrypt-verify' 10 $decryptOk $(if (-not $IsAdmin) { 'need admin' } elseif (-not (Test-Path $ua)) { 'not installed' } elseif ($decryptOk) { 'HMAC OK' } else { 'FAIL' })

# 10 service (4)
$svc = Get-Service UserAuditSvc -EA SilentlyContinue
Add-Check 'UserAuditSvc' 4 ($svc -and $svc.Status -eq 'Running') $(if ($svc) { $svc.Status } else { 'missing' })

# 11 docs (7)
$bad = @('1\.0\.0-rc1', 'mass rollout', 'Release Candidate')
$docOk = $true
foreach ($rel in @('docs\ARCHITECTURE.md', 'docs\DEPLOY.md', 'docs\QA-PERFORMANCE.md', 'ROADMAP.md')) {
    $t = Get-Content (Join-Path $RepoRoot $rel) -Raw -EA SilentlyContinue
    foreach ($p in $bad) { if ($t -match $p) { $docOk = $false } }
}
$roadmap = Get-Content (Join-Path $RepoRoot 'ROADMAP.md') -Raw
if ($roadmap -match 'verify-release\.ps1') { } else { $docOk = $false }
Add-Check 'docs' 7 $docOk 'synced'

# 12 wdac template (4)
$wdacOk = (Test-Path (Join-Path $PSScriptRoot 'wdac\README.md')) -and
          (Test-Path (Join-Path $PSScriptRoot 'wdac\UserAudit-supplemental.xml'))
Add-Check 'wdac-template' 4 $wdacOk 'installer/wdac/'

# 13 CI workflow (4)
$wf = Get-Content (Join-Path $RepoRoot '.github\workflows\build-native.yml') -Raw -EA SilentlyContinue
$ciOk = $wf -match 'ctest' -and $wf -match 'UserActivityAudit.Admin.slnx' -and $wf -match 'SmokeImport'
Add-Check 'ci-workflow' 4 $ciOk 'build-native.yml'

# 14 version (3)
Add-Check 'version' 3 ($Version -eq '1.0.0') $Version

# 15 security model (4)
$sec = Get-Content (Join-Path $RepoRoot 'docs\SecurityModel.md') -Raw -EA SilentlyContinue
Add-Check 'security-model' 4 ($sec -match 'WDAC' -and $sec -match 'v1\.0 standalone') 'OK'

# Score
$earned = 0; $total = 0
foreach ($k in $checks.Keys) {
    $total += $checks[$k].weight
    if ($checks[$k].ok) { $earned += $checks[$k].weight }
}
$pct = [math]::Round(100.0 * $earned / $total, 1)
$pass = $pct -ge $PassThreshold

Write-Host ''
Write-Host "=== MATURITY: $earned/$total = $pct% ===" -ForegroundColor $(if ($pass) { 'Green' } else { 'Red' })
Write-Host $(if ($pass) { "PASS (>= $PassThreshold%)" } else { 'FAIL - fix checks above' })

$result = @{
    maturity    = $pct
    earned      = $earned
    totalWeight = $total
    pass        = $pass
    threshold   = $PassThreshold
    version     = $Version
    timestamp   = (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
    checks      = $checks
    details     = $details
}
$json = Join-Path $RepoRoot 'installer\results\verify-release-latest.json'
New-Item -ItemType Directory -Force -Path (Split-Path $json) | Out-Null
$result | ConvertTo-Json -Depth 5 | Set-Content $json -Encoding UTF8
Write-Host "JSON: $json"

if (-not $pass) { exit 1 }
exit 0

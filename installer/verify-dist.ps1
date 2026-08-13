#Requires -Version 5.1
<#
.SYNOPSIS
    Validates dist/ package after build-dist.ps1.
#>
$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Version = (Get-Content (Join-Path $RepoRoot 'VERSION') -Raw).Trim()
$DistRoot = Join-Path $RepoRoot "dist\UserActivityAudit-$Version"
$Zip = Join-Path $RepoRoot "dist\UserActivityAudit-$Version-win-x64.zip"

$required = @(
    (Join-Path $DistRoot 'Agent\UserAuditSetup.msi'),
    (Join-Path $DistRoot 'Agent\UserAudit.exe'),
    (Join-Path $DistRoot 'Agent\deploy.ps1'),
    (Join-Path $DistRoot 'Admin\UserAudit.Dashboard.exe'),
    (Join-Path $DistRoot 'Tools\UserAuditKeygen.exe'),
    (Join-Path $DistRoot 'Tools\verify-reboot.ps1'),
    (Join-Path $DistRoot 'Docs\InstallGuide.md'),
    (Join-Path $DistRoot 'README.txt')
)

Write-Host '=== verify-dist ==='
$ok = $true
foreach ($path in $required) {
    if (Test-Path $path) {
        Write-Host "[OK] $path"
    } else {
        Write-Host "[FAIL] missing: $path"
        $ok = $false
    }
}

if (Test-Path $Zip) {
    $mb = [math]::Round((Get-Item $Zip).Length / 1MB, 1)
    Write-Host "[OK] ZIP $Zip ($mb MB)"
} else {
    Write-Host "[FAIL] missing ZIP: $Zip"
    $ok = $false
}

if (-not $ok) { exit 1 }
Write-Host '=== PASS ==='

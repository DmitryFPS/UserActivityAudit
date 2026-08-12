#Requires -Version 5.1
<#
.SYNOPSIS
    Post-reboot acceptance check for UserAuditSvc.
#>
$ErrorActionPreference = 'Continue'
$ok = $true

Write-Host '=== UserActivityAudit reboot verification ==='

$q = sc.exe query UserAuditSvc 2>&1 | Out-String
if ($q -match 'RUNNING') {
    Write-Host '[OK] Service RUNNING'
} else {
    Write-Host '[FAIL] Service not RUNNING'
    Write-Host $q
    $ok = $false
}

$logDir = Join-Path $env:ProgramData 'UserAudit\logs'
$enc = Get-ChildItem $logDir -Filter '*.jsonl.enc' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($enc) {
    Write-Host "[OK] Log file: $($enc.Name) ($($enc.Length) bytes)"
} else {
    Write-Host '[FAIL] No .jsonl.enc in logs'
    $ok = $false
}

$agent = Join-Path ${env:ProgramFiles} 'UserAudit\UserAudit.exe'
if (Test-Path $agent) {
    & $agent --decrypt --verify *> $null
    if ($LASTEXITCODE -eq 0) {
        Write-Host '[OK] --decrypt --verify exit 0'
    } else {
        Write-Host "[FAIL] --decrypt --verify exit $LASTEXITCODE"
        $ok = $false
    }
} else {
    Write-Host '[WARN] UserAudit.exe not in Program Files'
}

$proc = Get-Process UserAudit -ErrorAction SilentlyContinue | Sort-Object WorkingSet64 -Descending | Select-Object -First 1
if ($proc) {
    $mb = [math]::Round($proc.WorkingSet64 / 1MB, 1)
    Write-Host "[OK] RAM main PID: $mb MB"
    if ($mb -gt 15) { Write-Host '[WARN] RAM above 15 MB Low target' }
}

if ($ok) {
    Write-Host '=== PASS ==='
    exit 0
}
Write-Host '=== FAIL ==='
exit 1

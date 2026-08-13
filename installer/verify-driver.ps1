#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Verifies UserAuditFilter: loaded + blocks delete of protected logs.
#>
$ErrorActionPreference = 'Continue'
$ok = $true

Write-Host '=== UserAuditFilter verification ==='

$flt = fltmc filters 2>&1 | Out-String
if ($flt -match 'UserAudit') {
    Write-Host '[OK] Minifilter listed in fltmc'
} else {
    Write-Host '[FAIL] UserAuditFilter not in fltmc filters'
    $ok = $false
}

$agent = Join-Path ${env:ProgramFiles} 'UserAudit\UserAudit.exe'
if (Test-Path $agent) {
    Write-Host '[INFO] Restart UserAuditSvc after driver install to connect filter port'
}

$logDir = Join-Path $env:ProgramData 'UserAudit\logs'
$sample = Get-ChildItem $logDir -Filter '*.jsonl.enc' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($sample) {
    $target = Join-Path $env:TEMP ('useraudit-delete-test-' + [guid]::NewGuid().ToString() + '.enc')
    Copy-Item $sample.FullName $target -Force
    Remove-Item $target -Force -ErrorAction SilentlyContinue
    if (Test-Path $target) {
        Write-Host '[FAIL] Could delete copy in TEMP (expected if outside UserAudit path)'
    }

    try {
        Remove-Item $sample.FullName -Force -ErrorAction Stop
        Write-Host '[FAIL] Deleted protected log — driver not blocking'
        $ok = $false
    } catch {
        Write-Host "[OK] Delete denied on $($sample.Name)"
    }
} else {
    Write-Host '[WARN] No .jsonl.enc to test delete'
}

if ($ok) {
    Write-Host '=== PASS ==='
    exit 0
}
Write-Host '=== FAIL ==='
exit 1

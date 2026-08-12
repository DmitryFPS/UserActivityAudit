#Requires -Version 5.1
<#
.SYNOPSIS
    24-hour soak monitor for UserAuditSvc (phase 10 QA).

.PARAMETER Hours
    Duration in hours (default 24).

.PARAMETER IntervalSeconds
    Sample interval (default 300 = 5 min).

.PARAMETER OutputCsv
    Path to CSV log file.
#>
[CmdletBinding()]
param(
    [double] $Hours = 24,
    [int] $IntervalSeconds = 300,
    [string] $OutputCsv = ''
)

$ErrorActionPreference = 'Continue'
if (-not $OutputCsv) {
    $OutputCsv = Join-Path $PSScriptRoot ("soak-{0:yyyyMMdd-HHmmss}.csv" -f (Get-Date))
}

$deadline = (Get-Date).AddHours($Hours)
"timestamp,service_state,process_count,ram_mb_max,cpu_pct,log_bytes,decrypt_ok" | Out-File $OutputCsv -Encoding ascii
Write-Host "Soak test started until $deadline"
Write-Host "Log: $OutputCsv"

$agent = Join-Path ${env:ProgramFiles} 'UserAudit\UserAudit.exe'
if (-not (Test-Path $agent)) {
    $agent = Join-Path (Split-Path $PSScriptRoot -Parent) 'build\native\UserAuditSvc\Release\UserAudit.exe'
}

while ((Get-Date) -lt $deadline) {
    $ts = Get-Date -Format 'o'
    $svcState = 'missing'
    sc.exe query UserAuditSvc 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $q = sc.exe query UserAuditSvc 2>&1 | Out-String
        if ($q -match 'RUNNING') { $svcState = 'RUNNING' }
        elseif ($q -match 'STOPPED') { $svcState = 'STOPPED' }
        else { $svcState = 'other' }
    }

    $procs = Get-Process UserAudit -ErrorAction SilentlyContinue
    $procCount = @($procs).Count
    $ramMax = 0.0
    if ($procs) {
        $ramMax = ($procs | Measure-Object WorkingSet64 -Maximum).Maximum / 1MB
    }

    $cpuPct = 0.0
    if ($procs) {
        $before = ($procs | Measure-Object CPU -Sum).Sum
        Start-Sleep -Seconds 1
        $afterProcs = Get-Process UserAudit -ErrorAction SilentlyContinue
        if ($afterProcs) {
            $after = ($afterProcs | Measure-Object CPU -Sum).Sum
            $cpuPct = [math]::Max(0, ($after - $before) / [Environment]::ProcessorCount * 100)
        }
    }

    $logDir = Join-Path $env:ProgramData 'UserAudit\logs'
    $logBytes = 0
    if (Test-Path $logDir) {
        $logBytes = (Get-ChildItem $logDir -Filter '*.jsonl.enc' -ErrorAction SilentlyContinue |
            Measure-Object Length -Sum).Sum
    }

    $decryptOk = 'skip'
    if ((Test-Path $agent) -and ($svcState -eq 'RUNNING')) {
        & $agent --decrypt --verify *> $null
        $decryptOk = if ($LASTEXITCODE -eq 0) { 'ok' } else { "fail_$LASTEXITCODE" }
    }

    "$ts,$svcState,$procCount,{0:N2},$([math]::Round($cpuPct,3)),$logBytes,$decryptOk" -f $ramMax |
        Add-Content $OutputCsv -Encoding ascii

    Write-Host ("[{0}] svc={1} procs={2} ram={3:N1}MB cpu={4:N3}% logs={5}B decrypt={6}" -f
        (Get-Date -Format 'HH:mm:ss'), $svcState, $procCount, $ramMax, $cpuPct, $logBytes, $decryptOk)

    Start-Sleep -Seconds $IntervalSeconds
}

Write-Host "Soak test completed: $OutputCsv"

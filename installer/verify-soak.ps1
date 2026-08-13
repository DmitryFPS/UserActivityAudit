#Requires -Version 5.1
<#
.SYNOPSIS
    Validates soak-test CSV against pilot acceptance (default: 12 hours on reference PC).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $CsvPath,
    [double] $MinHours = 12,
    [double] $MaxRamMb = 15.5,
    [double] $MinOkRatio = 0.95
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $CsvPath)) {
    Write-Error "CSV not found: $CsvPath"
}

function Parse-SoakRow {
    param([string] $Line)
    if ($Line -match '^timestamp,') { return $null }

    $parts = $Line -split ','
    if ($parts.Count -ge 8 -and $parts[-1] -match '^(ok|fail_\d+|skip)$') {
        $ram = [double]("{0}.{1}" -f $parts[3], $parts[4])
        return [pscustomobject]@{
            timestamp     = $parts[0]
            service_state = $parts[1]
            process_count = $parts[2]
            ram_mb_max    = $ram
            cpu_pct       = [double]$parts[5]
            log_bytes     = $parts[6]
            decrypt_ok    = $parts[7]
        }
    }

    if ($parts.Count -ge 7 -and $parts[-1] -match '^(ok|fail_\d+|skip)$') {
        return [pscustomobject]@{
            timestamp     = $parts[0]
            service_state = $parts[1]
            process_count = $parts[2]
            ram_mb_max    = [double]$parts[3]
            cpu_pct       = [double]$parts[4]
            log_bytes     = $parts[5]
            decrypt_ok    = $parts[6]
        }
    }

    return $null
}

$rows = @(Get-Content $CsvPath | ForEach-Object { Parse-SoakRow $_ } | Where-Object { $_ })
if ($rows.Count -lt 2) {
    Write-Host '[FAIL] Too few samples'
    exit 1
}

$first = [datetimeoffset]::Parse($rows[0].timestamp)
$last = [datetimeoffset]::Parse($rows[-1].timestamp)
$spanHours = ($last - $first).TotalHours

Write-Host '=== UserActivityAudit soak verification ==='
Write-Host "File: $CsvPath"
Write-Host ("Span: {0:N1} h ({1} samples)" -f $spanHours, $rows.Count)

$ok = $true

if ($spanHours -lt $MinHours) {
    Write-Host ("[FAIL] Span {0:N1}h < minimum {1}h" -f $spanHours, $MinHours)
    $ok = $false
} else {
    Write-Host ("[OK] Span >= {0}h" -f $MinHours)
}

$notRunning = @($rows | Where-Object { $_.service_state -ne 'RUNNING' })
if ($notRunning.Count -gt 0) {
    Write-Host ("[FAIL] service_state not RUNNING on {0} sample(s)" -f $notRunning.Count)
    $ok = $false
} else {
    Write-Host '[OK] service_state RUNNING on all samples'
}

$ramMax = ($rows | Measure-Object ram_mb_max -Maximum).Maximum
if ($ramMax -gt $MaxRamMb) {
    Write-Host ("[FAIL] ram_mb_max {0:N2} > {1}" -f $ramMax, $MaxRamMb)
    $ok = $false
} else {
    Write-Host ("[OK] ram_mb_max {0:N2} MB" -f $ramMax)
}

$afterWarmup = $rows | Select-Object -Skip 2
$decryptSamples = @($afterWarmup | Where-Object { $_.decrypt_ok -match '^(ok|fail_)' })
$decryptOk = @($decryptSamples | Where-Object { $_.decrypt_ok -eq 'ok' })
$ratio = if ($decryptSamples.Count -gt 0) { $decryptOk.Count / $decryptSamples.Count } else { 0 }

if ($ratio -lt $MinOkRatio) {
    Write-Host ("[FAIL] decrypt_ok ratio {0:P0} < {1:P0}" -f $ratio, $MinOkRatio)
    $ok = $false
} else {
    Write-Host ("[OK] decrypt_ok ratio {0:P0} ({1}/{2})" -f $ratio, $decryptOk.Count, $decryptSamples.Count)
}

if ($ok) {
    Write-Host '=== PASS ==='
    exit 0
}

Write-Host '=== FAIL ==='
exit 1

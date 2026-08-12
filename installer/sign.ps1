#Requires -Version 5.1
<#
.SYNOPSIS
    Sign UserActivityAudit production binaries with signtool.

.PARAMETER DistRoot
    Path to dist\UserActivityAudit-x.y.z folder.

.PARAMETER Pfx
    Path to EV code signing PFX.

.PARAMETER CertThumbprint
    Certificate thumbprint in CurrentUser\My store.

.PARAMETER TimestampUrl
    RFC3161 timestamp server.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $DistRoot,

    [string] $Pfx = '',
    [securestring] $Password,
    [string] $CertThumbprint = '',
    [string] $TimestampUrl = 'http://timestamp.digicert.com'
)

$ErrorActionPreference = 'Stop'
$signtool = @(
    "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe",
    "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe",
    "${env:ProgramFiles(x86)}\Windows Kits\10\bin\x64\signtool.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $signtool) { throw 'signtool.exe not found. Install Windows SDK.' }

$files = @(
    (Join-Path $DistRoot 'Agent\UserAudit.exe'),
    (Join-Path $DistRoot 'Agent\UserAuditAdmin.exe'),
    (Join-Path $DistRoot 'Agent\UserAuditWatchdog.exe'),
    (Join-Path $DistRoot 'Tools\UserAuditKeygen.exe'),
    (Join-Path $DistRoot 'Admin\UserAudit.Dashboard.exe'),
    (Join-Path $DistRoot 'Agent\UserAuditSetup.msi')
) | Where-Object { Test-Path $_ }

if ($files.Count -eq 0) { throw "No files to sign under $DistRoot" }

foreach ($file in $files) {
    Write-Host "Signing $file"
    if ($Pfx) {
        if (-not $Password) { throw 'Password required for PFX' }
        $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($Password)
        try {
            $plain = [Runtime.InteropServices.Marshal]::PtrToStringAuto($bstr)
        } finally {
            [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
        }
        & $signtool sign /fd SHA256 /f $Pfx /p $plain /tr $TimestampUrl /td SHA256 $file
    } elseif ($CertThumbprint) {
        & $signtool sign /fd SHA256 /sha1 $CertThumbprint /tr $TimestampUrl /td SHA256 $file
    } else {
        & $signtool sign /fd SHA256 /a /tr $TimestampUrl /td SHA256 $file
    }
    if ($LASTEXITCODE -ne 0) { throw "sign failed: $file" }
}

Write-Host 'All files signed.'

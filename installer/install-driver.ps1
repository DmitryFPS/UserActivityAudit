#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Build, test-sign, and install UserAuditFilter minifilter (integrated into deploy.ps1).
#>
[CmdletBinding()]
param(
    [switch] $SkipBuild,
    [switch] $SkipSign,
    [switch] $ContinueAfterReboot,
    [switch] $AllowReboot,
    [string] $PfxPath = '',
    [string] $PfxPassword = 'test1234'
)

$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BuildScript = Join-Path $PSScriptRoot 'build-driver.ps1'
$SysSrc = Join-Path $RepoRoot 'build\native\UserAuditFilter\Release\UserAuditFilter.sys'
$InfSrc = Join-Path $RepoRoot 'native\UserAuditFilter\UserAuditFilter.inf'
$StageDir = Join-Path $RepoRoot 'build\native\UserAuditFilter\stage'
$SysDst = Join-Path $env:SystemRoot 'System32\drivers\UserAuditFilter.sys'
$RunOnceName = 'UserAuditDriverInstall'

function Test-TestSigningEnabled {
    $out = & bcdedit.exe /enum '{current}' 2>&1 | Out-String
    return $out -match '(?i)testsigning\s+Yes'
}

function Enable-TestSigning {
    if (Test-TestSigningEnabled) { return $false }
    & bcdedit.exe /set testsigning on | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'bcdedit /set testsigning on failed' }
    Write-Host '[OK] testsigning enabled (reboot required before load)'
    return $true
}

function Ensure-TestCert {
    param([string] $OutPfx)
    $existing = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue |
        Where-Object { $_.Subject -like '*UserAudit Test*' } |
        Select-Object -First 1
    if ($existing) {
        if (-not (Test-Path $OutPfx)) {
            $sec = ConvertTo-SecureString $PfxPassword -AsPlainText -Force
            Export-PfxCertificate -Cert $existing -FilePath $OutPfx -Password $sec | Out-Null
        }
        return $existing
    }
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=UserAudit Test' `
        -CertStoreLocation 'Cert:\CurrentUser\My' -HashAlgorithm SHA256
    $sec = ConvertTo-SecureString $PfxPassword -AsPlainText -Force
    Export-PfxCertificate -Cert $cert -FilePath $OutPfx -Password $sec | Out-Null
    return $cert
}

function Install-TestCertToMachineStores {
    param([System.Security.Cryptography.X509Certificates.X509Certificate2] $Cert)
    foreach ($storeName in @('Root', 'TrustedPublisher')) {
        $store = New-Object System.Security.Cryptography.X509Certificates.X509Store(
            $storeName, 'LocalMachine')
        $store.Open('ReadWrite')
        $found = $store.Certificates | Where-Object { $_.Thumbprint -eq $Cert.Thumbprint }
        if (-not $found) { $store.Add($Cert) | Out-Null }
        $store.Close()
    }
}

function Register-DriverInstallAfterReboot {
    $runOnce = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\RunOnce'
    $cmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -SkipBuild -ContinueAfterReboot"
    New-ItemProperty -Path $runOnce -Name $RunOnceName -Value $cmd -PropertyType String -Force | Out-Null
}

function Clear-DriverInstallAfterReboot {
    $runOnce = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\RunOnce'
    Remove-ItemProperty -Path $runOnce -Name $RunOnceName -ErrorAction SilentlyContinue
}

function Test-FilterLoaded {
    $flt = fltmc filters 2>&1 | Out-String
    return $flt -match 'UserAudit'
}

function Install-DriverManual {
    fltmc unload UserAuditFilter 2>$null | Out-Null
    sc.exe stop UserAuditFilter 2>$null | Out-Null
    Start-Sleep -Seconds 1

    Copy-Item (Join-Path $StageDir 'UserAuditFilter.sys') $SysDst -Force
    Write-Host "[OK] Copied to $SysDst"

    $svcKey = 'HKLM:\SYSTEM\CurrentControlSet\Services\UserAuditFilter'
    $instKey = Join-Path $svcKey 'Instances'
    $instName = 'UserAuditFilter Instance'

    New-Item -Path $svcKey -Force | Out-Null
    New-ItemProperty -Path $svcKey -Name 'Type' -Value 2 -PropertyType DWord -Force | Out-Null
    New-ItemProperty -Path $svcKey -Name 'Start' -Value 3 -PropertyType DWord -Force | Out-Null
    New-ItemProperty -Path $svcKey -Name 'ErrorControl' -Value 1 -PropertyType DWord -Force | Out-Null
    New-ItemProperty -Path $svcKey -Name 'Group' -Value 'FSFilter Activity Monitor' -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $svcKey -Name 'DependOnService' -Value 'FltMgr' -PropertyType MultiString -Force | Out-Null
    New-ItemProperty -Path $svcKey -Name 'ImagePath' -Value 'System32\drivers\UserAuditFilter.sys' -PropertyType ExpandString -Force | Out-Null

    New-Item -Path $instKey -Force | Out-Null
    New-ItemProperty -Path $instKey -Name 'DefaultInstance' -Value $instName -PropertyType String -Force | Out-Null
    New-Item -Path (Join-Path $instKey $instName) -Force | Out-Null
    New-ItemProperty -Path (Join-Path $instKey $instName) -Name 'Altitude' -Value '370030' -PropertyType String -Force | Out-Null
    New-ItemProperty -Path (Join-Path $instKey $instName) -Name 'Flags' -Value 0 -PropertyType DWord -Force | Out-Null

    sc.exe create UserAuditFilter type= kernel start= demand binPath= "System32\drivers\UserAuditFilter.sys" DisplayName= "UserAuditFilter" 2>$null | Out-Null
    sc.exe config UserAuditFilter start= demand | Out-Null
    sc.exe start UserAuditFilter 2>&1 | Out-String | Write-Host
    if ($LASTEXITCODE -ne 0) {
        throw "sc start UserAuditFilter failed (exit $LASTEXITCODE)"
    }

    Start-Sleep -Seconds 2
    if (-not (Test-FilterLoaded)) {
        throw 'UserAuditFilter not in fltmc after sc start'
    }
    Write-Host '[OK] Minifilter loaded (manual registry + sc start)'
}

function Install-DriverPackage {
    param([string] $InfPath)

    Copy-Item (Join-Path $StageDir 'UserAuditFilter.sys') $SysDst -Force
    Write-Host "[OK] Copied to $SysDst"

    $pnputil = & pnputil.exe /add-driver $InfPath /install 2>&1 | Out-String
    Write-Host $pnputil
    if ($LASTEXITCODE -eq 0 -and (Test-FilterLoaded)) {
        Write-Host '[OK] Minifilter loaded (pnputil)'
        return
    }

    Write-Host '[WARN] pnputil failed or filter not loaded — trying manual install...'
    Install-DriverManual
}

if (-not $SkipBuild) {
    & $BuildScript
}

if (-not (Test-Path $SysSrc)) {
    throw "Driver not built: $SysSrc"
}

New-Item -ItemType Directory -Force -Path $StageDir | Out-Null
Copy-Item $SysSrc (Join-Path $StageDir 'UserAuditFilter.sys') -Force
Copy-Item $InfSrc (Join-Path $StageDir 'UserAuditFilter.inf') -Force
$SysWork = Join-Path $StageDir 'UserAuditFilter.sys'
$InfWork = Join-Path $StageDir 'UserAuditFilter.inf'

if (-not $SkipSign) {
    if (-not $PfxPath) { $PfxPath = Join-Path $StageDir 'UserAuditTest.pfx' }
    $cert = Ensure-TestCert -OutPfx $PfxPath
    Install-TestCertToMachineStores -Cert $cert

    $signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $signtool) { throw 'signtool.exe not found in Windows Kits' }
    & $signtool.FullName sign /fd SHA256 /f $PfxPath /p $PfxPassword $SysWork
    if ($LASTEXITCODE -ne 0) { throw 'Driver signing failed' }
    Write-Host '[OK] Driver signed (test cert)'
}

if ($ContinueAfterReboot) {
    try {
        Install-DriverPackage -InfPath $InfWork
        Clear-DriverInstallAfterReboot
        $svc = Get-Service -Name UserAuditSvc -ErrorAction SilentlyContinue
        if ($svc -and $svc.Status -eq 'Running') {
            Restart-Service UserAuditSvc -Force
            Write-Host '[OK] UserAuditSvc restarted (driver port reconnect)'
        }
        exit 0
    } catch {
        Write-Host "[FAIL] Post-reboot driver install: $($_.Exception.Message)"
        exit 1
    }
}

$needsReboot = Enable-TestSigning
if (-not (Test-TestSigningEnabled)) {
    $needsReboot = $true
}

if ($needsReboot -and -not (Test-TestSigningEnabled)) {
    throw 'testsigning could not be enabled'
}

if ($needsReboot) {
    Register-DriverInstallAfterReboot
    if ($AllowReboot) {
        Write-Host '[INFO] Reboot in 45s to activate testsigning and load minifilter...'
        shutdown.exe /r /t 45 /c 'UserActivityAudit: finishing minifilter install'
        exit 10
    }
    Write-Host '[PENDING] Reboot required. Re-run deploy.ps1 or reboot and RunOnce will finish driver install.'
    exit 10
}

try {
    Install-DriverPackage -InfPath $InfWork
    exit 0
} catch {
    if (-not $AllowReboot) { throw }
    Register-DriverInstallAfterReboot
    Write-Host "[INFO] Install failed before reboot ($($_.Exception.Message)); reboot scheduled."
    shutdown.exe /r /t 45 /c 'UserActivityAudit: finishing minifilter install'
    exit 10
}

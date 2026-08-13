#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Deploy UserActivityAudit on a single PC (standalone mode).

.PARAMETER Profile
    low | standard | full | auto

.PARAMETER HostId
    Host id written into audit events (default: computer name).

.PARAMETER IngestUrl
    Ingest API URL (empty = no server upload).

.EXAMPLE
    .\deploy.ps1 -Profile low
#>
[CmdletBinding()]
param(
    [ValidateSet('auto', 'low', 'standard', 'full')]
    [string] $Profile = 'auto',

    [string] $HostId = $env:COMPUTERNAME,

    [string] $IngestUrl = '',

    [string] $BinDir = '',

    [switch] $SkipBuild,

    [switch] $SkipSmoke,

    [switch] $SkipDriver
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BinDir) {
    $BinDir = Join-Path $RepoRoot 'build\native\UserAuditSvc\Release'
}

$InstallDir = Join-Path $env:ProgramFiles 'UserAudit'
$ProgramData = Join-Path $env:ProgramData 'UserAudit'
$ServiceName = 'UserAuditSvc'

function Write-Step([string] $Message) {
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Ensure-CmakeInPath {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) {
        $fallback = 'C:\Program Files\CMake\bin\cmake.exe'
        if (Test-Path $fallback) {
            $env:Path = "C:\Program Files\CMake\bin;$env:Path"
        } else {
            throw "cmake not found. Install CMake or Visual Studio Build Tools."
        }
    }
}

Write-Step 'Native build (Release)'
if (-not $SkipBuild) {
    Ensure-CmakeInPath
    $nativeBuild = Join-Path $RepoRoot 'build\native'
    & cmake -S (Join-Path $RepoRoot 'native') -B $nativeBuild -G 'Visual Studio 17 2022' -A x64 | Out-Null
    & cmake --build $nativeBuild --config Release
    if ($LASTEXITCODE -ne 0) { throw "Native build failed with exit code $LASTEXITCODE." }
}

$agentExe = Join-Path $BinDir 'UserAudit.exe'
if (-not (Test-Path $agentExe)) {
    throw "UserAudit.exe not found: $agentExe"
}

Write-Step 'Stop previous service'
$installedAgent = Join-Path $InstallDir 'UserAudit.exe'
if (Get-Service -Name $ServiceName -ErrorAction SilentlyContinue) {
    Stop-Service -Name $ServiceName -Force -ErrorAction SilentlyContinue
    Get-Process UserAudit -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 3
    $svc = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
    if ($svc -and $svc.Status -ne 'Stopped') {
        sc.exe stop $ServiceName | Out-Null
        Start-Sleep -Seconds 3
    }
    sc.exe delete $ServiceName | Out-Null
    Start-Sleep -Seconds 2
}

Write-Step 'Copy binaries'
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
$nativeRelease = Join-Path $RepoRoot 'build\native'
$copyMap = @{
    'UserAudit.exe' = Join-Path $nativeRelease 'UserAuditSvc\Release\UserAudit.exe'
    'UserAuditWatchdog.exe' = Join-Path $nativeRelease 'UserAuditWatchdog\Release\UserAuditWatchdog.exe'
    'UserAuditAdmin.exe' = Join-Path $nativeRelease 'UserAuditAdmin\Release\UserAuditAdmin.exe'
    'UserAuditKeygen.exe' = Join-Path $nativeRelease 'UserAuditKeygen\Release\UserAuditKeygen.exe'
}
foreach ($pair in $copyMap.GetEnumerator()) {
    if (-not (Test-Path $pair.Value)) {
        throw "Missing $($pair.Key): $($pair.Value). Build native Release first."
    }
    Copy-Item -Force $pair.Value (Join-Path $InstallDir $pair.Key)
}

Write-Step 'ProgramData config'
New-Item -ItemType Directory -Force -Path $ProgramData | Out-Null
$configPath = Join-Path $ProgramData 'config.json'
$configTemplate = Join-Path $RepoRoot 'installer\config.example.json'
$config = Get-Content $configTemplate -Raw | ConvertFrom-Json
$config.profile = $Profile
$config.host_id = $HostId
$config.server.ingest_url = $IngestUrl
if ($IngestUrl) {
    if (-not $config.server.upload_interval_minutes -or $config.server.upload_interval_minutes -eq 0) {
        $config.server.upload_interval_minutes = 15
    }
} else {
    $config.server.upload_interval_minutes = 0
}
$configJson = $config | ConvertTo-Json -Depth 6
if (Test-Path $configPath) {
    Write-Warning "Keeping existing config.json (ACL-protected). Edit profile/host_id manually if needed."
} else {
    [System.IO.File]::WriteAllText($configPath, $configJson, (New-Object System.Text.UTF8Encoding $false))
}

Write-Step 'Install service'
& $installedAgent --install
if ($LASTEXITCODE -ne 0) {
    sc.exe config $ServiceName binPath= "`"$installedAgent`"" start= auto | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "UserAudit.exe --install failed with exit code $LASTEXITCODE."
    }
}
sc.exe config $ServiceName start= auto | Out-Null

if (-not $SkipDriver) {
    Write-Step 'Minifilter driver (UserAuditFilter.sys)'
    $driverScript = Join-Path $PSScriptRoot 'install-driver.ps1'
    $driverSys = Join-Path $RepoRoot 'build\native\UserAuditFilter\Release\UserAuditFilter.sys'
    try {
        if (-not (Test-Path $driverSys)) {
            & (Join-Path $PSScriptRoot 'build-driver.ps1')
        }
        & $driverScript -SkipBuild -AllowReboot
        if ($LASTEXITCODE -eq 10) {
            Write-Host 'Reboot scheduled — after login RunOnce completes driver install and service reconnect.'
            exit 10
        }
    } catch {
        Write-Warning "Minifilter install skipped: $($_.Exception.Message)"
    }
}

Write-Step 'Enable lock/unlock audit (Security 4800/4801 backup)'
# Primary lock/unlock source is WTS in user-agent; this enables Security log fallback.
$auditGuid = '{0CCE922B-69AE-11D9-BED3-505054503030}'
auditpol /set /subcategory:$auditGuid /success:enable 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Warning 'auditpol failed — lock/unlock still work via WTS user-agent.'
}

Write-Step 'Start service'
sc.exe start $ServiceName | Out-Null
$running = $false
for ($i = 0; $i -lt 15; $i++) {
    Start-Sleep -Seconds 2
    $state = sc.exe query $ServiceName 2>&1
    if ($state -match 'RUNNING') {
        $running = $true
        break
    }
}
if (-not $running) {
    throw "Service $ServiceName is not RUNNING."
}

Write-Step 'Verify logs and decrypt'
$logDir = Join-Path $ProgramData 'logs'
$deadline = (Get-Date).AddSeconds(30)
while ((Get-ChildItem $logDir -Filter '*.jsonl.enc' -ErrorAction SilentlyContinue).Count -eq 0) {
    if ((Get-Date) -gt $deadline) {
        throw 'Encrypted logs did not appear within 30 seconds.'
    }
    Start-Sleep -Seconds 2
}
$verify = Start-Process -FilePath $installedAgent -ArgumentList '--decrypt', '--verify' -Wait -PassThru `
    -WindowStyle Hidden -RedirectStandardOutput ([System.IO.Path]::GetTempFileName()) `
    -RedirectStandardError ([System.IO.Path]::GetTempFileName())
if ($verify.ExitCode -ne 0) {
    throw "--decrypt --verify exit code $($verify.ExitCode)"
}

if (-not $SkipSmoke) {
    Write-Step 'SmokeImport (WPF Core)'
    if (-not $SkipBuild) {
        dotnet build (Join-Path $RepoRoot 'admin\UserActivityAudit.Admin.slnx') -c Release | Out-Null
        dotnet build (Join-Path $RepoRoot 'installer\SmokeImport\SmokeImport.csproj') -c Release | Out-Null
    }
    dotnet run --project (Join-Path $RepoRoot 'installer\SmokeImport\SmokeImport.csproj') -c Release --no-build
    if ($LASTEXITCODE -ne 0) {
        throw "SmokeImport exit code $LASTEXITCODE"
    }
}

Write-Step 'Done'
Write-Host "  Service:    $ServiceName (RUNNING)"
Write-Host "  Agent:      $installedAgent"
Write-Host "  Logs:       $logDir"
Write-Host "  Dashboard:  admin\UserAudit.Dashboard (Release)"

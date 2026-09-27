#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$cleaner = Join-Path $repoRoot 'android/get_deps/clean-sdk-packages.ps1'
. (Join-Path $repoRoot 'android/helpers/host_processes.ps1')
$realInventory = ${function:Get-DxxHostProcessInventory}
$injectTool = $false
function Get-DxxHostProcessInventory {
    param([switch]$IncludePaths)
    & $realInventory -IncludePaths:$IncludePaths
    if ($injectTool) {
        [pscustomobject]@{ ProcessId = -1; ParentProcessId = 0; Name = 'java'; CommandLine = ''; ExecutablePath = $null; WorkingDirectory = $null; PathInspectionFailed = $true }
    }
}
$root = Join-Path $repoRoot ('android/temp/sdk_package_cleanup_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix sdk_package_cleanup_ -MinimumFreeSpaceGB 0.01
$fixtureLock = $null
$savedJava = $env:JAVA_HOME
$savedPath = $env:PATH
$savedPwsh = $env:DXX_FIXTURE_PWSH
function Set-Pins([int]$Version) {
    Set-Content -LiteralPath (Join-Path $checkout 'android/get_deps/tool_versions.conf') -Value "COMPILE_SDK=$Version`nEMULATOR_API_LEVEL=34`nBUILD_TOOLS_VERSION=$Version.0.0`nCMAKE_VERSION=3.31.6`nJDK_MAJOR=21"
}
function New-Package([int]$Version) {
    $path = Join-Path $sdk "build-tools/$Version.0.0"
    New-Item -ItemType Directory -Path $path -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $path 'package.xml') -Value "<repository><localPackage path='build-tools;$Version.0.0'/></repository>"
    Set-Content -LiteralPath (Join-Path $path 'payload') -Value preserve
    $path
}
function Assert-Action($Results, [int]$Version, [string]$Action, [string]$Reason = '') {
    $item = @($Results | Where-Object PackageId -CEQ "build-tools;$Version.0.0")
    if ($item.Count -ne 1 -or $item[0].Action -ne $Action -or ($Reason -and $item[0].Reason -notmatch $Reason)) { throw "Unexpected cleanup result: $($item | ConvertTo-Json -Compress)" }
}
try {
    New-Item -ItemType Directory -Path $root | Out-Null
    $fixtureLock = [IO.File]::Open((Join-Path $root 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $checkout = Join-Path $root 'checkout'
    $deps = Join-Path $root 'shared tools'
    $sdk = Join-Path $deps 'android-sdk'
    $tools = Join-Path $sdk 'cmdline-tools/latest/bin'
    $avds = Join-Path $root 'avds'
    New-Item -ItemType Directory -Path (Join-Path $checkout 'android/get_deps'), $tools, $avds -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $checkout 'dependency_base.txt') -Value $deps
    $env:JAVA_HOME = Join-Path $deps 'jdk-21'
    New-Item -ItemType Directory -Path (Join-Path $env:JAVA_HOME 'bin') -Force | Out-Null
    $windows = [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT
    $javaName = if ($windows) { 'java.exe' } else { 'java' }
    Set-Content -LiteralPath (Join-Path $env:JAVA_HOME "bin/$javaName") -Value fixture
    $env:DXX_FIXTURE_PWSH = (Get-Process -Id $PID).Path
    Set-Content -LiteralPath (Join-Path $tools 'fixture_sdkmanager.ps1') -Value @'
$ErrorActionPreference = 'Stop'
if ($args.Count -ne 3 -or -not $args[0].StartsWith('--sdk_root=') -or $args[1] -ne '--uninstall') { throw 'Incorrect SDK invocation' }
$sdk = $args[0].Substring('--sdk_root='.Length)
$id = $args[2]
Add-Content -LiteralPath (Join-Path $sdk 'uninstall.log') -Value $id
if (Test-Path -LiteralPath (Join-Path $sdk 'fail-uninstall')) { exit 23 }
if (Test-Path -LiteralPath (Join-Path $sdk 'fail-partial')) {
    Remove-Item -LiteralPath (Join-Path $sdk ($id.Replace(';', '/') + '/.dxx-sdk-managed')) -Force
    if (Test-Path -LiteralPath (Join-Path $sdk 'fail-metadata')) { Remove-Item -LiteralPath (Join-Path $sdk ($id.Replace(';', '/') + '/package.xml')) -Force }
    exit 23
}
Remove-Item -LiteralPath (Join-Path $sdk $id.Replace(';', '/')) -Recurse -Force
if (Test-Path -LiteralPath (Join-Path $sdk 'fail-after-uninstall')) { exit 23 }
exit 0
'@
    if ($windows) {
        Set-Content -LiteralPath (Join-Path $tools 'sdkmanager.bat') -Value '@"%DXX_FIXTURE_PWSH%" -NoProfile -File "%~dp0fixture_sdkmanager.ps1" %*'
    } else {
        $manager = Join-Path $tools 'sdkmanager'
        Set-Content -LiteralPath $manager -Value "#!/bin/sh`nexec `"`$DXX_FIXTURE_PWSH`" -NoProfile -File `"`$(dirname `"`$0`")/fixture_sdkmanager.ps1`" `"`$@`""
        & chmod +x $manager
        if ($LASTEXITCODE -ne 0) { throw 'Could not prepare SDK fixture' }
    }
    $old = New-Package 35
    $current = New-Package 36
    New-Package 99 | Out-Null
    Set-Pins 35
    & $cleaner -RepoRoot $checkout -AvdRoots $avds -RegisterCurrent
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 35 Protected 'reference'
    Set-Pins 36
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 35 Protected 'replacement'
    & $cleaner -RepoRoot $checkout -AvdRoots $avds -RegisterCurrent
    $originalSearchPath = $env:PATH
    try {
        $env:PATH = $old + [IO.Path]::PathSeparator + $originalSearchPath
        Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 35 Protected 'PATH override'
    } finally { $env:PATH = $originalSearchPath }
    $injectTool = $true
    try { Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 35 Protected 'cannot inspect' }
    finally { $injectTool = $false }
    . (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')
    $start = [Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
    Set-HeadlessProcessArguments -StartInfo $start -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Seconds 30')
    $start.UseShellExecute = $false
    $start.WorkingDirectory = $sdk
    $child = [Diagnostics.Process]::Start($start)
    try { Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 35 Protected "active SDK process $($child.Id)" }
    finally {
        if (-not $child.HasExited) { $child.Kill(); $child.WaitForExit() }
        $child.Dispose()
    }
    $cache = Join-Path $checkout 'buildd2-asan/CMakeCache.txt'
    New-Item -ItemType Directory -Path (Split-Path $cache) -Force | Out-Null
    Set-Content -LiteralPath $cache -Value $old
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 35 Protected 'CMake cache'
    Remove-Item -LiteralPath $cache
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds) 35 Eligible
    if (-not (Test-Path -LiteralPath $old)) { throw 'Preview removed a package' }
    Set-Content -LiteralPath (Join-Path $sdk 'fail-uninstall') -Value fail
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 35 Protected 'uninstall incomplete'
    $statePath = Join-Path $sdk '.dxx-sdk-ownership.json'
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    if (@($state.Packages | Where-Object Status -eq Retiring).Count -ne 1) { throw 'Failed uninstall lost its journal' }
    Remove-Item -LiteralPath (Join-Path $sdk 'fail-uninstall')
    $results = @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply)
    Assert-Action $results 35 Removed
    Assert-Action $results 36 Protected 'reference'
    Assert-Action $results 99 Protected unmanaged
    if (Test-Path -LiteralPath $old) { throw 'SDK uninstall did not remove old payload' }
    if (-not (Test-Path -LiteralPath (Join-Path $current 'payload'))) { throw 'SDK uninstall removed the replacement' }
    if (@(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply | Where-Object Action -eq Removed).Count) { throw 'Repeated retirement was not idempotent' }
    # A lost success response is reconciled without repeating a completed deletion
    New-Package 37 | Out-Null
    Set-Pins 37
    & $cleaner -RepoRoot $checkout -AvdRoots $avds -RegisterCurrent
    Set-Content -LiteralPath (Join-Path $sdk 'fail-after-uninstall') -Value fail
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 36 Protected 'uninstall incomplete'
    if (Test-Path -LiteralPath $current) { throw 'Fixture did not delete package before failing' }
    Remove-Item -LiteralPath (Join-Path $sdk 'fail-after-uninstall')
    & $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply | Out-Null
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    if (@($state.Packages | Where-Object Status -eq Retiring).Count) { throw 'Completed uninstall was not reconciled' }
    # A recorded partial deletion can recover only unchanged original payloads
    New-Package 38 | Out-Null
    Set-Pins 38
    & $cleaner -RepoRoot $checkout -AvdRoots $avds -RegisterCurrent
    Set-Content -LiteralPath (Join-Path $sdk 'fail-partial') -Value fail
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 37 Protected 'uninstall incomplete'
    $calls = @(Get-Content -LiteralPath (Join-Path $sdk 'uninstall.log')).Count
    $partial = Join-Path $sdk 'build-tools/37.0.0'
    Set-Content -LiteralPath (Join-Path $partial 'payload') -Value changed
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 37 Protected 'payload changed'
    Set-Content -LiteralPath (Join-Path $partial 'payload') -Value preserve
    Set-Content -LiteralPath (Join-Path $partial 'new-user-file') -Value preserve
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 37 Protected 'New file'
    Remove-Item -LiteralPath (Join-Path $partial 'new-user-file')
    Set-Pins 37
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 37 Protected 'reference'
    Set-Pins 38
    $originalPartial = Join-Path $root 'original partial package'
    Move-Item -LiteralPath $partial -Destination $originalPartial
    Copy-Item -LiteralPath $originalPartial -Destination $partial -Recurse -Force
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 37 Protected 'directory identity changed'
    Remove-Item -LiteralPath $partial -Recurse -Force
    Move-Item -LiteralPath $originalPartial -Destination $partial
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds) 37 Eligible
    Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) 37 Removed
    if (@(Get-Content -LiteralPath (Join-Path $sdk 'uninstall.log')).Count -ne $calls) { throw 'Recovery invoked sdkmanager after metadata loss' }
    if (Test-Path -LiteralPath $partial) { throw 'Verified partial package was not recovered' }
    Remove-Item -LiteralPath (Join-Path $sdk 'fail-partial')

    $crashWorker = Join-Path $root 'crash-recovery.ps1'
    Set-Content -LiteralPath $crashWorker -Value @'
param([string]$Cleaner, [string]$Checkout, [string]$CrashPoint)
$ErrorActionPreference = 'Stop'
function Move-Item {
    [CmdletBinding()]
    param([string]$LiteralPath, [string]$Destination)
    Microsoft.PowerShell.Management\Move-Item @PSBoundParameters
    if ($CrashPoint -eq 'move' -and (Split-Path $Destination -Leaf) -like '.dxx-sdk-retired-*') { [Diagnostics.Process]::GetCurrentProcess().Kill() }
}
function Remove-Item {
    [CmdletBinding()]
    param([string[]]$LiteralPath, [switch]$Recurse, [switch]$Force)
    if ($CrashPoint -eq 'delete' -and $LiteralPath.Count -eq 1 -and (Split-Path $LiteralPath[0] -Leaf) -like '.dxx-sdk-retired-*') {
        $file = Get-ChildItem -LiteralPath $LiteralPath[0] -File -Recurse -Force | Select-Object -First 1
        if ($file) { Microsoft.PowerShell.Management\Remove-Item -LiteralPath $file.FullName -Force }
        [Diagnostics.Process]::GetCurrentProcess().Kill()
    }
    Microsoft.PowerShell.Management\Remove-Item @PSBoundParameters
}
& $Cleaner -RepoRoot $Checkout -Apply
'@
    foreach ($point in @('move', 'delete')) {
        $oldVersion = if ($point -eq 'move') { 38 } else { 39 }
        New-Package ($oldVersion + 1) | Out-Null
        Set-Pins ($oldVersion + 1)
        & $cleaner -RepoRoot $checkout -AvdRoots $avds -RegisterCurrent
        Set-Content -LiteralPath (Join-Path $sdk 'fail-partial') -Value fail
        Set-Content -LiteralPath (Join-Path $sdk 'fail-metadata') -Value fail
        Assert-Action @(& $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply) $oldVersion Protected 'uninstall incomplete'
        Remove-Item -LiteralPath (Join-Path $sdk 'fail-partial'), (Join-Path $sdk 'fail-metadata')
        $start = [Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
        Set-HeadlessProcessArguments -StartInfo $start -Arguments @('-NoProfile', '-File', $crashWorker, '-Cleaner', $cleaner, '-Checkout', $checkout, '-CrashPoint', $point)
        $start.UseShellExecute = $false
        $child = [Diagnostics.Process]::Start($start)
        try {
            if (-not $child.WaitForExit(30000)) { throw 'SDK recovery crash fixture timed out' }
            if ($child.ExitCode -eq 0) { throw "SDK recovery did not terminate at $point" }
        } finally {
            if (-not $child.HasExited) { $child.Kill(); $child.WaitForExit() }
            $child.Dispose()
        }
        $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
        if ($state.Schema -ne 2) { throw 'Pending quarantine did not block older cleaners' }
        $pending = @($state.Packages | Where-Object Status -eq Retiring)
        if ($pending.Count -ne 1) { throw 'Recovery journal was lost' }
        $quarantine = Join-Path $sdk ('.dxx-sdk-retired-' + $pending[0].Identity)
        if (-not (Test-Path -LiteralPath $quarantine)) { throw 'Interrupted recovery lost quarantine' }
        Assert-Action @(& $cleaner -RepoRoot $checkout) $oldVersion Eligible
        if (-not (Test-Path -LiteralPath $quarantine)) { throw 'Preview deleted quarantine' }
        Assert-Action @(& $cleaner -RepoRoot $checkout -Apply) $oldVersion Removed
        if (Test-Path -LiteralPath $quarantine) { throw 'Quarantine was not removed after retry' }
        $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
        if ($state.Schema -ne 1 -or @($state.Packages | Where-Object Status -eq Retiring).Count) { throw 'Completed recovery left incompatible journal state' }
    }
    # Persist custom AVD roots so a later caller cannot forget an old image reference
    foreach ($api in @(23, 34)) {
        $image = Join-Path $sdk "system-images/android-$api/google_apis/x86_64"
        New-Item -ItemType Directory -Path $image -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $image 'package.xml') -Value "<repository><localPackage path='system-images;android-$api;google_apis;x86_64'/></repository>"
    }
    $pins = Join-Path $checkout 'android/get_deps/tool_versions.conf'
    (Get-Content -LiteralPath $pins -Raw).Replace('EMULATOR_API_LEVEL=34', 'EMULATOR_API_LEVEL=23') | Set-Content -LiteralPath $pins
    $device = Join-Path $avds 'retained.avd'
    New-Item -ItemType Directory -Path $device | Out-Null
    Set-Content -LiteralPath (Join-Path $device 'config.ini') -Value 'image.sysdir.1=system-images/android-23/google_apis/x86_64/'
    & $cleaner -RepoRoot $checkout -AvdRoots $avds -RegisterCurrent
    Set-Pins 40
    & $cleaner -RepoRoot $checkout -RegisterCurrent
    $results = @(& $cleaner -RepoRoot $checkout -Apply)
    $oldImage = @($results | Where-Object PackageId -eq 'system-images;android-23;google_apis;x86_64')
    if ($oldImage.Count -ne 1 -or $oldImage[0].Action -ne 'Protected' -or $oldImage[0].Reason -notmatch 'reference') { throw 'Persistent AVD reference was lost' }
    Remove-Item -LiteralPath $device -Recurse -Force
    $results = @(& $cleaner -RepoRoot $checkout -Apply)
    $oldImage = @($results | Where-Object PackageId -eq 'system-images;android-23;google_apis;x86_64')
    if ($oldImage.Count -ne 1 -or $oldImage[0].Action -ne 'Removed') { throw 'Unreferenced owned image was not retired' }
    $lock = [IO.File]::Open((Join-Path $sdk 'cmdline-tools/.dxx-install-state/lock'), [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $blocked = $false
        try { & $cleaner -RepoRoot $checkout -AvdRoots $avds -Apply -LockWaitSeconds 0 | Out-Null } catch { $blocked = $_ -match 'locked' }
        if (-not $blocked) { throw 'SDK cleaner ignored the provisioning lock' }
    } finally { $lock.Dispose() }
    Write-Host 'PASS: SDK ownership, pins, replacement admission, process/cache guards, preview, supervised uninstall, failure retry, partial-payload manifests, forced termination during quarantine recovery and serialization'
} finally {
    $env:JAVA_HOME = $savedJava
    $env:PATH = $savedPath
    $env:DXX_FIXTURE_PWSH = $savedPwsh
    if ($fixtureLock) { $fixtureLock.Dispose() }
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}

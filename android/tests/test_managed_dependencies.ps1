#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$cleaner = Join-Path $repoRoot 'android/get_deps/clean-dependencies.ps1'
$root = Join-Path $repoRoot ('android/temp/managed-dependencies-' + [guid]::NewGuid().ToString('N'))
$deps = Join-Path $root 'shared tools'
$checkouts = @((Join-Path $root 'checkout one'), (Join-Path $root 'checkout two'))
$child = $null
$fixtureLock = $null
. (Join-Path $repoRoot 'android/helpers/host_processes.ps1')
$realProcessInventory = ${function:Get-DxxHostProcessInventory}
# Hosted runners can have unreadable system tools. Isolate only those already
# present before this fixture starts; retain real inspection of fixture children
$unreadableBackground = @(& $realProcessInventory -IncludePaths | Where-Object PathInspectionFailed)
$injectUnreadableTool = $false
function Get-DxxHostProcessInventory {
    param([switch]$IncludePaths)
    foreach ($process in (& $realProcessInventory -IncludePaths:$IncludePaths)) {
        $background = @($unreadableBackground | Where-Object {
                $_.ProcessId -eq $process.ProcessId -and $_.Name -ceq $process.Name -and $_.CommandLine -ceq $process.CommandLine
            })
        if (-not ($process.PathInspectionFailed -and $background.Count)) { $process }
    }
    if ($injectUnreadableTool) {
        [pscustomobject]@{
            ProcessId = -1; ParentProcessId = 0; Name = 'python3'
            CommandLine = 'python3 background-service'; ExecutablePath = $null
            WorkingDirectory = $null; PathInspectionFailed = $true
        }
    }
}

function Set-FixtureVersion([string]$Checkout, [int]$Version) {
    Set-Content -LiteralPath (Join-Path $Checkout 'android/get_deps/tool_versions.conf') -Value "SHFMT_VERSION=$Version"
}

function Assert-Result($Results, [string]$Name, [string]$Action, [string]$Reason) {
    $entry = @($Results | Where-Object { (Split-Path $_.Path -Leaf) -eq $Name })
    if ($entry.Count -ne 1 -or $entry[0].Action -ne $Action -or $entry[0].Reason -notmatch $Reason) {
        throw "Unexpected result for ${Name}: $($entry | ConvertTo-Json -Compress)"
    }
}

try {
    & (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix 'managed-dependencies-' -MinimumFreeSpaceGB 0.01
    New-Item -ItemType Directory -Path $deps -Force | Out-Null
    $fixtureLock = [IO.File]::Open((Join-Path $root 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    foreach ($checkout in $checkouts) {
        New-Item -ItemType Directory -Path (Join-Path $checkout 'android/get_deps') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $checkout 'dependency_base.txt') -Value $deps
        Set-FixtureVersion $checkout 1
    }
    foreach ($version in @(1, 2, 99)) {
        $directory = Join-Path $deps "shfmt-$version"
        New-Item -ItemType Directory -Path $directory | Out-Null
        Set-Content -LiteralPath (Join-Path $directory 'shfmt') -Value ('payload' * 1024)
    }
    & $cleaner -RepoRoot $checkouts[0] -RegisterCurrent
    # A successful reinstall of the same version replaces the installation marker
    $marker = Join-Path $deps 'shfmt-1/.dxx-managed-install'
    $oldIdentity = Get-Content -LiteralPath $marker -Raw
    Remove-Item -LiteralPath $marker -Force
    & $cleaner -RepoRoot $checkouts[0] -RegisterCurrent
    if ((Get-Content -LiteralPath $marker -Raw) -eq $oldIdentity) { throw 'Replacement did not receive a new installation identity' }
    & $cleaner -RepoRoot $checkouts[1] -RegisterRepository
    Set-FixtureVersion $checkouts[0] 2
    & $cleaner -RepoRoot $checkouts[0] -RegisterCurrent
    $results = @(& $cleaner -RepoRoot $checkouts[0] -Apply)
    Assert-Result $results 'shfmt-1' Protected 'registered checkout'
    Assert-Result $results 'shfmt-99' Protected 'unmanaged'
    Set-FixtureVersion $checkouts[1] 2
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        $caseCheckout = Join-Path $root 'Checkout Two'
        New-Item -ItemType Directory -Path (Join-Path $caseCheckout 'android/get_deps') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $caseCheckout 'dependency_base.txt') -Value $deps
        Set-FixtureVersion $caseCheckout 1
        & $cleaner -RepoRoot $caseCheckout -RegisterRepository
        Assert-Result @(& $cleaner -RepoRoot $checkouts[0] -Apply) 'shfmt-1' Protected 'registered checkout'
        & $cleaner -RepoRoot $checkouts[0] -UnregisterRepository $caseCheckout
        $owners = Get-Content -LiteralPath (Join-Path $deps '.dxx-dependency-ownership.json') -Raw | ConvertFrom-Json
        if (@($owners.Repositories).Count -ne 2 -or $owners.Repositories -ccontains $caseCheckout) {
            throw 'Unregistration confused two case-distinct checkouts'
        }
    }
    $replacementTool = Join-Path $deps 'shfmt-2/shfmt'
    Remove-Item -LiteralPath $replacementTool -Force
    Assert-Result @(& $cleaner -RepoRoot $checkouts[0] -Apply) 'shfmt-1' Protected 'replacement is not installed'
    Set-Content -LiteralPath $replacementTool -Value 'replacement'
    $injectUnreadableTool = $true
    try {
        Assert-Result @(& $cleaner -RepoRoot $checkouts[0] -Apply) 'shfmt-1' Protected 'cannot inspect tool process -1'
        if (-not (Test-Path -LiteralPath (Join-Path $deps 'shfmt-1'))) { throw 'Unreadable tool process did not prevent deletion' }
    } finally { $injectUnreadableTool = $false }
    $savedPath = $env:PATH
    try {
        $env:PATH = (Join-Path $deps 'shfmt-1') + [IO.Path]::PathSeparator + $savedPath
        Assert-Result @(& $cleaner -RepoRoot $checkouts[0] -Apply) 'shfmt-1' Protected 'PATH override'
    } finally { $env:PATH = $savedPath }

    # Real process use must protect a retired version independently of config
    $worker = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
        Join-Path $deps 'shfmt-1/active.ps1'
    } else {
        # On Linux, protect cwd even when argv and the executable are elsewhere
        Join-Path $root 'active.ps1'
    }
    $ready = Join-Path $root 'ready'
    Set-Content -LiteralPath $worker -Value 'param($Ready); Set-Content -LiteralPath $Ready -Value ready; Start-Sleep -Seconds 120'
    $start = [Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
    . (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')
    Set-HeadlessProcessArguments -StartInfo $start -Arguments @('-NoProfile', '-File', $worker, $ready)
    $start.UseShellExecute = $false
    $start.WorkingDirectory = Join-Path $deps 'shfmt-1'
    $child = [Diagnostics.Process]::Start($start)
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while (-not (Test-Path -LiteralPath $ready)) {
        if ($child.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw 'Dependency user did not start' }
        Start-Sleep -Milliseconds 50
    }
    $results = @(& $cleaner -RepoRoot $checkouts[0] -Apply)
    Assert-Result $results 'shfmt-1' Protected "active process $($child.Id)"
    $child.Kill()
    $child.WaitForExit()
    $child.Dispose()
    $child = $null

    $cacheDir = Join-Path $checkouts[1] 'buildd1'
    New-Item -ItemType Directory -Path $cacheDir | Out-Null
    $cache = Join-Path $cacheDir 'CMakeCache.txt'
    Set-Content -LiteralPath $cache -Value (Join-Path $deps 'shfmt-1/shfmt')
    Assert-Result @(& $cleaner -RepoRoot $checkouts[0] -Apply) 'shfmt-1' Protected 'CMake cache'
    Remove-Item -LiteralPath $cache -Force

    $lock = [IO.File]::Open((Join-Path $deps '.dxx-dependency-ownership.lock'), [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $rejected = $false
        try { & $cleaner -RepoRoot $checkouts[0] -Apply -LockWaitSeconds 0 | Out-Null }
        catch { if ($_ -notmatch 'locked') { throw }; $rejected = $true }
        if (-not $rejected) { throw 'Cleaner bypassed the held ownership lock' }
    } finally { $lock.Dispose() }

    # Missing checkout state must never be interpreted as an unused version
    $otherConfig = Join-Path $checkouts[1] 'android/get_deps/tool_versions.conf'
    Remove-Item -LiteralPath $otherConfig -Force
    $rejected = $false
    try { & $cleaner -RepoRoot $checkouts[0] -Apply | Out-Null } catch { $rejected = $true }
    if (-not $rejected -or -not (Test-Path -LiteralPath (Join-Path $deps 'shfmt-1'))) { throw 'Unavailable checkout lost its dependency' }
    Set-FixtureVersion $checkouts[1] 2

    $external = Join-Path $root 'external data'
    New-Item -ItemType Directory -Path $external | Out-Null
    $sentinel = Join-Path $external 'sentinel'
    Set-Content -LiteralPath $sentinel -Value preserve
    $link = Join-Path $deps 'shfmt-1/external'
    $linkType = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { 'Junction' } else { 'SymbolicLink' }
    New-Item -ItemType $linkType -Path $link -Target $external | Out-Null
    Assert-Result @(& $cleaner -RepoRoot $checkouts[0] -Apply) 'shfmt-1' Protected 'Linked dependency tree'
    Remove-Item -LiteralPath $link -Force
    if (-not (Test-Path -LiteralPath $sentinel)) { throw 'Cleanup followed an external link' }
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        # Normal venv installations contain file links to a system interpreter
        New-Item -ItemType SymbolicLink -Path (Join-Path $deps 'shfmt-1/interpreter') -Target $sentinel | Out-Null
    }

    $results = @(& $cleaner -RepoRoot $checkouts[0])
    Assert-Result $results 'shfmt-1' Eligible '^$'
    if (-not (Test-Path -LiteralPath (Join-Path $deps 'shfmt-1'))) { throw 'Preview removed a managed version' }
    # Kill the actual cleaner after quarantine and partial deletion, bypassing
    # finally blocks and leaving the real on-disk retirement journal to recover
    $interruptedCleaner = Join-Path $root 'interrupt-cleaner.ps1'
    Set-Content -LiteralPath $interruptedCleaner -Value @'
param($Cleaner, $Checkout, $InventoryHelper, $IgnoredProcesses, $FilterDefinition)
$ErrorActionPreference = 'Stop'
# The crash-recovery child must use the same fixture isolation as its parent
. $InventoryHelper
$realProcessInventory = ${function:Get-DxxHostProcessInventory}
$unreadableBackground = @(ConvertFrom-Json $IgnoredProcesses)
$injectUnreadableTool = $false
Set-Item Function:Get-DxxHostProcessInventory -Value ([scriptblock]::Create($FilterDefinition))
function Remove-Item {
    [CmdletBinding()]
    param([string]$LiteralPath, [switch]$Recurse, [switch]$Force)
    if ((Split-Path $LiteralPath -Leaf) -like '.dxx-retired-*') {
        Microsoft.PowerShell.Management\Remove-Item -LiteralPath (Join-Path $LiteralPath '.dxx-managed-install') -Force
        Stop-Process -Id $PID -Force
        throw 'Forced termination returned unexpectedly'
    }
    Microsoft.PowerShell.Management\Remove-Item @PSBoundParameters
}
& $Cleaner -RepoRoot $Checkout -Apply
throw 'Fixture never interrupted retirement'
'@
    $start = [Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
    Set-HeadlessProcessArguments -StartInfo $start -Arguments @(
        '-NoProfile', '-File', $interruptedCleaner, $cleaner, $checkouts[0],
        (Join-Path $repoRoot 'android/helpers/host_processes.ps1'),
        (ConvertTo-Json -InputObject $unreadableBackground -Compress),
        ${function:Get-DxxHostProcessInventory}.ToString()
    )
    $start.UseShellExecute = $false
    $child = [Diagnostics.Process]::Start($start)
    if (-not $child.WaitForExit(30000)) { throw 'Interrupted cleaner did not exit' }
    if ($child.ExitCode -eq 0) { throw 'Cleaner unexpectedly survived forced termination' }
    $child.Dispose()
    $child = $null
    $ownershipFile = Join-Path $deps '.dxx-dependency-ownership.json'
    $state = Get-Content -LiteralPath $ownershipFile -Raw | ConvertFrom-Json
    if (@($state.Retirements).Count -ne 1) { throw 'Interrupted cleanup lost its retirement journal' }
    $retired = Join-Path $deps ('.dxx-retired-' + $state.Retirements[0].Identity)
    if (-not (Test-Path -LiteralPath $retired) -or (Test-Path -LiteralPath (Join-Path $retired '.dxx-managed-install'))) {
        throw 'Fixture did not leave a partially deleted retirement'
    }
    $results = @(& $cleaner -RepoRoot $checkouts[0] -Apply)
    Assert-Result $results 'shfmt-1' Removed '^$'
    if (Test-Path -LiteralPath $retired) { throw 'Partial retirement survived recovery' }
    if ([IO.File]::ReadAllText($sentinel).Trim() -ne 'preserve') { throw 'Retirement changed a linked external file' }
    $state = Get-Content -LiteralPath $ownershipFile -Raw | ConvertFrom-Json
    if (@($state.Retirements).Count) { throw 'Completed retirement left a stale journal entry' }
    if (Test-Path -LiteralPath (Join-Path $deps 'shfmt-1')) { throw 'Superseded version survived cleanup' }
    foreach ($version in @(2, 99)) {
        if (-not (Test-Path -LiteralPath (Join-Path $deps "shfmt-$version/shfmt"))) { throw 'Current or unmanaged dependency was removed' }
    }
    $results = @(& $cleaner -RepoRoot $checkouts[0] -Apply)
    if (@($results | Where-Object Action -eq Removed).Count) { throw 'Repeated cleanup was not idempotent' }
    Write-Host 'PASS: managed dependency retention, shared pins, active users, caches, locks, links, forced-termination recovery, preview, and repeat cleanup'
} finally {
    if ($child) {
        if (-not $child.HasExited) { $child.Kill(); $child.WaitForExit() }
        $child.Dispose()
    }
    if ($fixtureLock) { $fixtureLock.Dispose() }
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}

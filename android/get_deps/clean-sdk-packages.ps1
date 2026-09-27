#!/usr/bin/env pwsh
# Retire owned, superseded SDK packages through sdkmanager
[CmdletBinding()]
param(
    [string]$RepoRoot = (Split-Path (Split-Path $PSScriptRoot)),
    [string[]]$AvdRoots,
    [switch]$RegisterCurrent,
    [switch]$Apply,
    [ValidateRange(0, 300)][int]$LockWaitSeconds = 30
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '../helpers/sdk_package_inventory.ps1')
. (Join-Path $PSScriptRoot '../helpers/sdk_retirement_snapshot.ps1')
. (Join-Path $PSScriptRoot '../helpers/host_processes.ps1')
. (Join-Path $PSScriptRoot '../helpers/test_host_platform.ps1')
. (Join-Path $PSScriptRoot '../helpers/headless_process_pool.ps1')
. (Join-Path $PSScriptRoot '../helpers/atomic_text_file.ps1')
$RepoRoot = [IO.Path]::GetFullPath($RepoRoot)
$comparison = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
$base = [IO.Path]::GetFullPath((Get-Content -LiteralPath (Join-Path $RepoRoot 'dependency_base.txt') -First 1).Trim())
$sdk = Join-Path $base 'android-sdk'
Assert-DxxPlainSdkPath $sdk
if (-not (Test-Path -LiteralPath $sdk -PathType Container)) { throw "SDK not installed: $sdk" }
$statePath = Join-Path $sdk '.dxx-sdk-ownership.json'
$lockPath = Join-Path $sdk 'cmdline-tools/.dxx-install-state/lock'
Assert-DxxPlainSdkPath $statePath
Assert-DxxPlainSdkPath "$statePath.new"
Assert-DxxPlainSdkPath $lockPath
# This inode is also used by the Linux shell provisioning/command-line installers
New-Item -ItemType Directory -Path (Split-Path $lockPath) -Force | Out-Null
$lock = $null

function Save-SdkOwnership {
    # Older cleaners must reject pending quarantine recovery rather than orphan it
    $state.Schema = if (@($state.Packages | Where-Object { $_.Retirement -and $_.Retirement.Recovering }).Count) { 2 } else { 1 }
    Write-Utf8NoBomTextAtomically -Path $statePath -Text (($state | ConvertTo-Json -Depth 8) + "`n")
}
function Get-SdkFamily([string]$Id) {
    if ($Id -match '^(build-tools|platforms|cmake|ndk);[A-Za-z0-9][A-Za-z0-9._+-]*$') { return $Matches[1] }
    if ($Id -match '^system-images;android-[A-Za-z0-9._+-]+;([A-Za-z0-9._+-]+);([A-Za-z0-9._+-]+)$') { return "system-images;$($Matches[1]);$($Matches[2])" }
    return $null
}
function Assert-SdkReceipt($Entry, $Package) {
    if ($Package.MetadataProblem) { throw 'SDK package metadata is incomplete or changed' }
    $marker = Join-Path $Package.Path '.dxx-sdk-managed'
    Assert-DxxPlainSdkPath $marker
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf) -or (Get-Content -LiteralPath $marker -Raw).Trim() -cne $Entry.Identity) { throw 'SDK ownership marker is missing or changed' }
    if ((Get-FileHash -LiteralPath (Join-Path $Package.Path 'package.xml') -Algorithm SHA256).Hash -cne $Entry.MetadataSha256) { throw 'SDK package metadata changed since registration' }
}
function Get-SdkRetirementUse($Entry, [string]$Path) {
    $fresh = Get-DxxSdkInventory -RepoRoot $RepoRoot -AvdRoots $roots
    $uses = @($fresh.References | Where-Object PackageId -CEQ $Entry.PackageId)
    $package = [pscustomobject]@{
        PackageId = $Entry.PackageId; Path = $Path; MetadataProblem = $null
        ReferenceState = if ($uses.Count) { 'Referenced' } else { 'Unreferenced' }
    }
    # Metadata can be gone, but references and cache paths still name the original
    Get-SdkUse $package $fresh -SkipPayloadInspection
}
function Get-SdkUse($Package, $Inventory, [switch]$SkipPayloadInspection) {
    if ($Package.ReferenceState -ne 'Unreferenced') { return 'checkout or AVD reference, or unknown metadata' }
    foreach ($key in @('ANDROID_NDK_ROOT', 'ANDROID_NDK_HOME', 'CMAKE_ROOT')) {
        $value = [Environment]::GetEnvironmentVariable($key)
        if ($value) {
            $value = [IO.Path]::GetFullPath($value)
            if ($value.Equals($Package.Path, $comparison) -or $value.StartsWith($Package.Path + [IO.Path]::DirectorySeparatorChar, $comparison)) { return "environment override $key" }
        }
    }
    foreach ($value in ($env:PATH -split [regex]::Escape([IO.Path]::PathSeparator))) {
        if ($value -and ($value.TrimEnd('\', '/').Equals($Package.Path, $comparison) -or $value.StartsWith($Package.Path + [IO.Path]::DirectorySeparatorChar, $comparison))) { return 'PATH override' }
    }
    $processes = @(Get-DxxHostProcessInventory -IncludePaths)
    $ancestors = [Collections.Generic.HashSet[int]]::new()
    $ancestorId = $PID
    while ($ancestorId -and $ancestors.Add($ancestorId)) {
        $ancestor = @($processes | Where-Object ProcessId -eq $ancestorId)
        $ancestorId = if ($ancestor.Count) { $ancestor[0].ParentProcessId } else { 0 }
    }
    foreach ($process in $processes) {
        foreach ($property in @('CommandLine', 'ExecutablePath', 'WorkingDirectory')) {
            $value = $process.PSObject.Properties[$property]
            if ($value -and $value.Value -and ([string]$value.Value).IndexOf($sdk, $comparison) -ge 0) { return "active SDK process $($process.ProcessId)" }
        }
        if ($process.PathInspectionFailed -and $process.Name -match '^(java|emulator.*|qemu.*|sdkmanager|avdmanager|cmake|ninja|clang.*)(\.exe)?$') { return "cannot inspect SDK tool process $($process.ProcessId)" }
        if (-not $ancestors.Contains([int]$process.ProcessId) -and $process.CommandLine -match '[/\\]get_deps[/\\](get_all\.sh|check-updates\.ps1|helpers[/\\](get_|finalize|create_))') { return "dependency provisioning process $($process.ProcessId)" }
    }
    foreach ($checkout in $Inventory.Checkouts) {
        foreach ($relative in @('buildd1', 'buildd2', 'buildd1-asan', 'buildd2-asan', 'android/app/.cxx')) {
            $buildRoot = Join-Path $checkout $relative
            Assert-DxxPlainSdkPath $buildRoot
            if (-not (Test-Path -LiteralPath $buildRoot -PathType Container)) { continue }
            foreach ($cache in Get-ChildItem -LiteralPath $buildRoot -Recurse -Depth 3 -File -Filter CMakeCache.txt -ErrorAction Stop) {
                Assert-DxxPlainSdkPath $cache.FullName
                $content = [IO.File]::ReadAllText($cache.FullName)
                if ($content.IndexOf($Package.Path, $comparison) -ge 0 -or $content.IndexOf($Package.Path.Replace('\', '/'), $comparison) -ge 0) { return "retained CMake cache $($cache.FullName)" }
            }
        }
    }
    # Do not recurse through linked package content or nested user repositories
    if (-not $SkipPayloadInspection) {
        $pending = [Collections.Generic.Stack[string]]::new()
        $pending.Push($Package.Path)
        while ($pending.Count) {
            foreach ($item in Get-ChildItem -LiteralPath $pending.Pop() -Force -ErrorAction Stop) {
                if ($item.Name -eq '.git' -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { return "linked or user-owned package content $($item.FullName)" }
                if ($item.PSIsContainer) { $pending.Push($item.FullName) }
            }
        }
    }
    $family = Get-SdkFamily $Package.PackageId
    foreach ($replacement in @($Inventory.Packages | Where-Object { (Get-SdkFamily $_.PackageId) -eq $family -and $_.ReferenceState -eq 'Referenced' -and @($_.References | Where-Object Kind -eq Checkout).Count })) {
        $entry = @($state.Packages | Where-Object PackageId -CEQ $replacement.PackageId)
        if ($entry.Count -eq 1 -and $entry[0].Status -eq 'Installed') {
            try { Assert-SdkReceipt $entry[0] $replacement; return $null } catch { }
        }
    }
    return 'configured replacement is not installed and registered'
}
try {
    $deadline = [DateTime]::UtcNow.AddSeconds($LockWaitSeconds)
    do {
        try { $lock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
        catch [IO.IOException] {
            if ([DateTime]::UtcNow -ge $deadline) { throw 'SDK provisioning or cleanup is locked by another operation' }
            Start-Sleep -Milliseconds 100
        }
    } until ($lock)
    $state = if (Test-Path -LiteralPath $statePath) { Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json } else {
        [pscustomobject]@{ Schema = 1; AvdRoots = @(); Packages = @() }
    }
    if ($state.Schema -notin @(1, 2)) { throw 'Unsupported SDK ownership schema' }
    foreach ($entry in $state.Packages) {
        if (-not $entry.PSObject.Properties['Retirement']) { $entry | Add-Member -NotePropertyName Retirement -NotePropertyValue $null }
        if (-not (Get-SdkFamily $entry.PackageId) -or $entry.Identity -notmatch '^[0-9a-f]{32}$' -or $entry.MetadataSha256 -notmatch '^[0-9A-F]{64}$' -or $entry.Status -notin @('Installed', 'Retiring')) { throw 'Invalid SDK ownership record' }
    }
    $roots = @(@(Get-DxxAvdSearchRoots) + @($state.AvdRoots) + @($AvdRoots) | Where-Object { $_ } | Select-Object -Unique)
    $inventory = Get-DxxSdkInventory -RepoRoot $RepoRoot -AvdRoots $roots
    if ($RegisterCurrent) {
        $state.AvdRoots = $roots
        foreach ($package in $inventory.Packages) {
            if (-not (Get-SdkFamily $package.PackageId) -or $package.MetadataProblem -or
                -not @($package.References | Where-Object { $_.Kind -eq 'Checkout' -and $_.Source.Equals($RepoRoot, $comparison) }).Count) { continue }
            $marker = Join-Path $package.Path '.dxx-sdk-managed'
            Assert-DxxPlainSdkPath $marker
            $entry = @($state.Packages | Where-Object PackageId -CEQ $package.PackageId)
            if ($entry.Count -and $entry[0].Status -eq 'Retiring') { throw "Incomplete SDK retirement requires inspection: $($package.PackageId)" }
            if ($entry.Count -and (Test-Path -LiteralPath $marker)) {
                if ((Get-Content -LiteralPath $marker -Raw).Trim() -cne $entry[0].Identity) { throw 'SDK ownership marker changed' }
                # Explicit registration admits a successfully updated current package
                $entry[0].MetadataSha256 = (Get-FileHash -LiteralPath (Join-Path $package.Path 'package.xml') -Algorithm SHA256).Hash
                continue
            }
            $identity = [guid]::NewGuid().ToString('N')
            [IO.File]::WriteAllText($marker, $identity + "`n")
            $state.Packages = @($state.Packages | Where-Object PackageId -CNE $package.PackageId) + [pscustomobject]@{
                PackageId = $package.PackageId; Identity = $identity; Status = 'Installed'; Retirement = $null
                MetadataSha256 = (Get-FileHash -LiteralPath (Join-Path $package.Path 'package.xml') -Algorithm SHA256).Hash
            }
        }
        Save-SdkOwnership
        if (-not $Apply) { return }
    }
    $recoveryIds = @()
    foreach ($entry in @($state.Packages | Where-Object Status -eq Retiring)) {
        $path = Join-Path $sdk $entry.PackageId.Replace(';', '/')
        $quarantine = Join-Path $sdk ('.dxx-sdk-retired-' + $entry.Identity)
        Assert-DxxPlainSdkPath $path
        Assert-DxxPlainSdkPath $quarantine
        if (-not (Test-Path -LiteralPath $path) -and -not (Test-Path -LiteralPath $quarantine)) {
            if ($Apply) {
                $state.Packages = @($state.Packages | Where-Object PackageId -CNE $entry.PackageId)
                Save-SdkOwnership
            }
            continue
        }
        if (-not $entry.Retirement) { continue }
        $recoveryIds += $entry.PackageId
        $action = 'Protected'
        $reason = $null
        try {
            if ((Test-Path -LiteralPath $path) -and (Test-Path -LiteralPath $quarantine)) { throw 'Both original and quarantined SDK package exist' }
            $remaining = if (Test-Path -LiteralPath $quarantine) { $quarantine } else { $path }
            $reason = Get-SdkRetirementUse $entry $path
            if (-not $reason) {
                Assert-DxxSdkRemainingPayload $remaining $entry.Retirement
                $action = 'Eligible'
                if ($Apply) {
                    $reason = Get-SdkRetirementUse $entry $path
                    if ($reason) { throw $reason }
                    $entry.Retirement.Recovering = $true
                    Save-SdkOwnership
                    if ($remaining -eq $path) { Move-Item -LiteralPath $path -Destination $quarantine -ErrorAction Stop }
                    Assert-DxxSdkRemainingPayload $quarantine $entry.Retirement
                    $reason = Get-SdkRetirementUse $entry $path
                    if ($reason) { throw $reason }
                    Remove-Item -LiteralPath $quarantine -Recurse -Force -ErrorAction Stop
                    $state.Packages = @($state.Packages | Where-Object PackageId -CNE $entry.PackageId)
                    Save-SdkOwnership
                    $action = 'Removed'
                }
            }
        } catch { $action = 'Protected'; $reason = $_.Exception.Message }
        [pscustomobject]@{ PackageId = $entry.PackageId; Path = $path; Action = $action; Reason = $reason }
    }
    foreach ($package in $inventory.Packages) {
        if ($package.PackageId -in $recoveryIds) { continue }
        if (-not (Get-SdkFamily $package.PackageId)) { continue }
        $entry = @($state.Packages | Where-Object PackageId -CEQ $package.PackageId)
        $reason = 'unmanaged SDK package'
        $action = 'Protected'
        if ($entry.Count -eq 1) {
            try {
                Assert-SdkReceipt $entry[0] $package
                $reason = Get-SdkUse $package $inventory
                if (-not $reason) {
                    $action = 'Eligible'
                    if ($Apply) {
                        $fresh = Get-DxxSdkInventory -RepoRoot $RepoRoot -AvdRoots $roots
                        $current = @($fresh.Packages | Where-Object PackageId -CEQ $package.PackageId)
                        if ($current.Count -ne 1) { throw 'SDK package changed during inspection' }
                        Assert-SdkReceipt $entry[0] $current[0]
                        $reason = Get-SdkUse $current[0] $fresh
                        if ($reason) { $action = 'Protected' }
                        else {
                            Initialize-RegressionJavaEnvironment -RepoRoot $RepoRoot
                            $entry[0].Retirement = New-DxxSdkRetirementSnapshot $package.Path
                            $reason = Get-SdkRetirementUse $entry[0] $package.Path
                            if ($reason) { $entry[0].Retirement = $null; throw $reason }
                            $entry[0].Status = 'Retiring'
                            Save-SdkOwnership
                            $execution = @{ Result = $null }
                            Invoke-HeadlessProcessPool -Tasks @([pscustomobject]@{
                                    FilePath = Get-RegressionCurrentPwshPath
                                    Arguments = @('-NoProfile', '-File', (Join-Path $PSScriptRoot 'helpers/uninstall_sdk_package.ps1'), '-SdkRoot', $sdk, '-PackageId', $package.PackageId)
                                    WorkingDirectory = $RepoRoot; TimeoutSeconds = 180
                                }) -MaxParallel 1 -OnCompleted { param($task, $result) $execution.Result = $result }
                            $result = $execution.Result
                            if ($result.TimedOut -or $result.ExitCode -ne 0 -or (Test-Path -LiteralPath $package.Path)) {
                                throw "SDK uninstall incomplete: $($result.StandardOutput) $($result.StandardError)"
                            }
                            $state.Packages = @($state.Packages | Where-Object PackageId -CNE $package.PackageId)
                            Save-SdkOwnership
                            $action = 'Removed'
                        }
                    }
                }
            } catch { $action = 'Protected'; $reason = $_.Exception.Message }
        }
        [pscustomobject]@{ PackageId = $package.PackageId; Path = $package.Path; Action = $action; Reason = $reason }
    }
} finally { if ($lock) { $lock.Dispose() } }

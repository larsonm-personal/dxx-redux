#!/usr/bin/env pwsh
# Manage versioned installations shared by registered checkouts
[CmdletBinding()]
param(
    [string]$RepoRoot = (Split-Path (Split-Path $PSScriptRoot)),
    [switch]$RegisterRepository,
    [switch]$RegisterCurrent,
    [string[]]$UnregisterRepository = @(),
    [switch]$Apply,
    [ValidateRange(0, 300)][int]$LockWaitSeconds = 30
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '../helpers/host_processes.ps1')
. (Join-Path $PSScriptRoot '../helpers/verified_dependencies.ps1')
$comparison = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
$pathComparer = if ($comparison -eq [StringComparison]::Ordinal) { [StringComparer]::Ordinal } else { [StringComparer]::OrdinalIgnoreCase }
$RepoRoot = [IO.Path]::GetFullPath($RepoRoot)
$baseFile = Join-Path $RepoRoot 'dependency_base.txt'
$dependencyRoot = [IO.Path]::GetFullPath((Get-Content -LiteralPath $baseFile -First 1).Trim())
$families = @(
    @{ Key = 'JDK_MAJOR'; Prefix = 'jdk-'; Files = @('bin/java', 'bin/java.exe'); Required = @('release') },
    @{ Key = 'NDK_VERSION'; Prefix = 'android-ndk-'; Files = @('build/cmake/android.toolchain.cmake'); Required = @('source.properties') },
    @{ Key = 'CMAKE_VERSION'; Prefix = 'cmake-'; Files = @('bin/cmake', 'bin/cmake.exe') },
    @{ Key = 'CLANG_FORMAT_VERSION'; Prefix = 'clang-format-'; Files = @('clang-format', 'clang-format.exe') },
    @{ Key = 'SHELLCHECK_VERSION'; Prefix = 'shellcheck-'; Files = @('shellcheck', 'shellcheck.exe') },
    @{ Key = 'SHFMT_VERSION'; Prefix = 'shfmt-'; Files = @('shfmt', 'shfmt.exe') },
    @{ Key = 'KTLINT_VERSION'; Prefix = 'ktlint-'; Files = @('ktlint.jar') },
    @{ Key = 'CMAKELANG_VERSION'; Prefix = 'cmakelang-'; Files = @('venv/bin/cmake-format', 'python/python.exe') },
    @{ Key = 'SEVENZIP_VERSION'; Prefix = '7z-'; Files = @('7zz', '7za.exe') },
    @{ Key = 'POWERSHELL_VERSION'; Prefix = 'powershell-'; Files = @('pwsh', 'pwsh.exe') },
    @{ Key = 'DOSBOX_DIR_NAME'; Prefix = 'dosbox-'; DirectoryName = $true; Files = @('dosbox-x.exe') },
    @{ Key = 'PYTHON_BOUNDED_LINUX_DIR_NAME'; Prefix = 'python-bounded-'; DirectoryName = $true; Files = @('python/bin/python3.12') }
)

function Assert-PlainDependencyPath([string]$Path) {
    $cursor = [IO.Path]::GetFullPath($Path)
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            if ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Linked dependency path is protected: $cursor"
            }
        }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
}

function Get-ConfiguredDependencyNames([string]$Checkout) {
    $config = Read-DxxDependencyConfig -RepoRoot $Checkout
    foreach ($family in $families) {
        if ($config.ContainsKey($family.Key)) {
            $version = $config[$family.Key]
            if ($version -notmatch '^[A-Za-z0-9][A-Za-z0-9._+-]*$') { throw "Invalid dependency version: $($family.Key)" }
            if ($family.ContainsKey('DirectoryName')) {
                if ($version -cnotmatch ('^' + [regex]::Escape($family.Prefix) + '[A-Za-z0-9][A-Za-z0-9._+-]*$')) {
                    throw "Invalid dependency directory: $($family.Key)"
                }
                $version
            } else {
                $family.Prefix + $version
            }
        }
    }
}

function Save-DependencyOwnership {
    $temporary = "$statePath.new"
    try {
        [IO.File]::WriteAllText($temporary, ($state | ConvertTo-Json -Depth 8) + "`n", [Text.UTF8Encoding]::new($false))
        if (Test-Path -LiteralPath $statePath) {
            [IO.File]::Replace($temporary, $statePath, [NullString]::Value)
        } else {
            [IO.File]::Move($temporary, $statePath)
        }
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
}

function Get-DependencyTree([string]$Path) {
    $records = [Collections.Generic.List[string]]::new()
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($Path)
    $bytes = 0L
    while ($pending.Count) {
        foreach ($item in Get-ChildItem -LiteralPath $pending.Pop() -Force -ErrorAction Stop) {
            if ($item.Name -eq '.git') { throw "Nested repository is protected: $($item.FullName)" }
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                if ($item.PSIsContainer -or [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
                    throw "Linked dependency tree is protected: $($item.FullName)"
                }
                # Unix file links (including venv interpreters) are unlinked, never followed
            } elseif ($item.PSIsContainer) {
                $pending.Push($item.FullName)
            }
            $length = if ($item.PSIsContainer) { 0L } else { $item.Length }
            $bytes += $length
            $records.Add("$($item.FullName)`0$length`0$($item.LastWriteTimeUtc.Ticks)")
        }
    }
    $sorted = $records.ToArray()
    [Array]::Sort($sorted, [StringComparer]::Ordinal)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $digest = [Convert]::ToBase64String($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($sorted -join "`n")))) }
    finally { $sha.Dispose() }
    [pscustomobject]@{ Bytes = $bytes; Digest = $digest }
}

function Get-DependencyUse([string]$Path) {
    foreach ($key in @('JAVA_HOME', 'ANDROID_NDK_ROOT', 'ANDROID_NDK_HOME', 'CMAKE_ROOT')) {
        $value = [Environment]::GetEnvironmentVariable($key)
        if ($value -and ($value.Equals($Path, $comparison) -or $value.StartsWith($Path + [IO.Path]::DirectorySeparatorChar, $comparison))) {
            return "environment override $key"
        }
    }
    foreach ($value in ($env:PATH -split [regex]::Escape([IO.Path]::PathSeparator))) {
        if ($value -and ($value.TrimEnd('\', '/').Equals($Path, $comparison) -or $value.StartsWith($Path + [IO.Path]::DirectorySeparatorChar, $comparison))) {
            return 'PATH override'
        }
    }
    # Linux bootstrap links may live on PATH outside the versioned installation
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        foreach ($directory in ($env:PATH -split [regex]::Escape([IO.Path]::PathSeparator))) {
            if (-not $directory) { continue }
            foreach ($command in @('pwsh', 'pwsh-preview')) {
                $entry = Get-Item -LiteralPath (Join-Path $directory $command) -Force -ErrorAction SilentlyContinue
                if (-not $entry -or -not ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)) { continue }
                try { $target = $entry.ResolveLinkTarget($true) }
                catch { return "cannot resolve PowerShell command link $($entry.FullName)" }
                if ($target -and ($target.FullName.Equals($Path, $comparison) -or $target.FullName.StartsWith($Path + [IO.Path]::DirectorySeparatorChar, $comparison))) {
                    return "PowerShell command link $($entry.FullName)"
                }
            }
        }
    }
    $processes = @(Get-DxxHostProcessInventory -IncludePaths)
    $ancestors = [Collections.Generic.HashSet[int]]::new()
    $ancestorId = $PID
    while ($ancestorId -and $ancestors.Add($ancestorId)) {
        $ancestor = @($processes | Where-Object ProcessId -eq $ancestorId)
        $ancestorId = if ($ancestor.Count) { $ancestor[0].ParentProcessId } else { 0 }
    }
    foreach ($process in $processes) {
        if (-not $ancestors.Contains([int]$process.ProcessId) -and $process.CommandLine -match '[/\\]get_deps[/\\](get_all\.sh|check-updates\.ps1|helpers[/\\](get_|finalize))') {
            return "dependency installer process $($process.ProcessId) is active"
        }
        foreach ($property in @('CommandLine', 'ExecutablePath', 'WorkingDirectory')) {
            $value = $process.PSObject.Properties[$property]
            if ($value -and $value.Value -and ([string]$value.Value).IndexOf($Path, $comparison) -ge 0) {
                return "active process $($process.ProcessId) ($($process.Name))"
            }
        }
        $unreadable = $process.PSObject.Properties['PathInspectionFailed']
        if ($unreadable -and $unreadable.Value -and $process.Name -match '^(java|cmake|ninja|clang.*|gcc|g\+\+|cc1.*|shfmt|shellcheck|python.*|pwsh.*|powershell|7zz|7za|dosbox-x)(\.exe)?$') {
            return "cannot inspect tool process $($process.ProcessId)"
        }
    }
    foreach ($cache in $buildCaches) {
        Assert-PlainDependencyPath $cache
        $cacheText = [IO.File]::ReadAllText($cache)
        if ($cacheText.IndexOf($Path.Replace('\', '/'), $comparison) -ge 0 -or
            $cacheText.IndexOf($Path, $comparison) -ge 0) {
            return "retained CMake cache $cache"
        }
    }
    return $null
}

function Test-ManagedReplacement([string]$Name) {
    $family = @($families | Where-Object { $Name.StartsWith($_.Prefix, [StringComparison]::Ordinal) })[0]
    $desired = @($protected | Where-Object { $_.StartsWith($family.Prefix, [StringComparison]::Ordinal) })
    if (-not $desired.Count) { return $true }
    foreach ($replacement in @($state.Installations | Where-Object Directory -in $desired)) {
        $path = Join-Path $dependencyRoot $replacement.Directory
        $marker = Join-Path $path '.dxx-managed-install'
        Assert-PlainDependencyPath $marker
        if ($family.ContainsKey('Required') -and @($family.Required | Where-Object { -not (Test-Path -LiteralPath (Join-Path $path $_) -PathType Leaf) }).Count) { continue }
        if ((Test-Path -LiteralPath $marker -PathType Leaf) -and
            (Get-Content -LiteralPath $marker -Raw).Trim() -eq $replacement.Identity -and
            @($family.Files | Where-Object { Test-Path -LiteralPath (Join-Path $path $_) -PathType Leaf }).Count) {
            return $true
        }
    }
    return $false
}

# The ownership journal survives termination during recursive deletion
# Quarantine names are deterministic, so retries cannot accumulate new copies
function Complete-DependencyRetirement($Retirement) {
    if ($Retirement.Identity -notmatch '^[0-9a-f]{32}$' -or
        -not @($families | Where-Object { $Retirement.Directory -match ('^' + [regex]::Escape($_.Prefix) + '[A-Za-z0-9][A-Za-z0-9._+-]*$') }).Count) {
        throw 'Invalid dependency retirement record'
    }
    $original = Join-Path $dependencyRoot $Retirement.Directory
    $retired = Join-Path $dependencyRoot ('.dxx-retired-' + $Retirement.Identity)
    if (-not (Test-Path -LiteralPath $retired)) {
        if (-not $Apply) {
            [pscustomobject]@{ Path = $original; Action = 'Protected'; Reason = 'incomplete retirement journal; apply to reconcile'; Bytes = 0L }
            return
        }
        if (-not (Test-Path -LiteralPath $original)) {
            $state.Installations = @($state.Installations | Where-Object Identity -ne $Retirement.Identity)
        }
        $state.Retirements = @($state.Retirements | Where-Object Identity -ne $Retirement.Identity)
        Save-DependencyOwnership
        return
    }
    $reason = $null
    $bytes = 0L
    try {
        Assert-PlainDependencyPath $retired
        if (Test-Path -LiteralPath (Join-Path $original '.dxx-managed-install')) {
            if ((Get-Content -LiteralPath (Join-Path $original '.dxx-managed-install') -Raw).Trim() -eq $Retirement.Identity) {
                throw 'Both original and retired installation have the same identity'
            }
        }
        $reason = Get-DependencyUse $original
        if (-not $reason) { $reason = Get-DependencyUse $retired }
        if (-not $reason) {
            $tree = Get-DependencyTree $retired
            $bytes = $tree.Bytes
            if ($Apply) {
                $reason = Get-DependencyUse $retired
                if (-not $reason -and (Get-DependencyTree $retired).Digest -ne $tree.Digest) { $reason = 'retirement changed during inspection' }
                if (-not $reason) {
                    Remove-Item -LiteralPath $retired -Recurse -Force
                    $state.Installations = @($state.Installations | Where-Object Identity -ne $Retirement.Identity)
                    $state.Retirements = @($state.Retirements | Where-Object Identity -ne $Retirement.Identity)
                    Save-DependencyOwnership
                }
            }
        }
    } catch { $reason = $_.Exception.Message }
    [pscustomobject]@{ Path = $original; StoragePath = $retired; Action = if ($reason) { 'Protected' } elseif ($Apply) { 'Removed' } else { 'Eligible' }; Reason = $reason; Bytes = $bytes }
}

Assert-PlainDependencyPath $dependencyRoot
$statePath = Join-Path $dependencyRoot '.dxx-dependency-ownership.json'
$lockPath = Join-Path $dependencyRoot '.dxx-dependency-ownership.lock'
foreach ($path in @($statePath, "$statePath.new", $lockPath)) { Assert-PlainDependencyPath $path }
$lock = $null
$deadline = [DateTime]::UtcNow.AddSeconds($LockWaitSeconds)
try {
    do {
        try { $lock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
        catch [IO.IOException] {
            if ([DateTime]::UtcNow -ge $deadline) { throw 'Dependency ownership is locked by another operation' }
            Start-Sleep -Milliseconds 100
        }
    } until ($lock)
    $state = if (Test-Path -LiteralPath $statePath) { Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json } else {
        [pscustomobject]@{ Schema = 1; Repositories = @(); Installations = @(); Retirements = @() }
    }
    if ($state.Schema -ne 1) { throw 'Unsupported dependency ownership schema' }
    if ($RegisterRepository -or $RegisterCurrent -or $UnregisterRepository.Count) {
        $repositories = [Collections.Generic.HashSet[string]]::new($pathComparer)
        foreach ($checkout in $state.Repositories) { [void]$repositories.Add([IO.Path]::GetFullPath($checkout)) }
        foreach ($checkout in $UnregisterRepository) { [void]$repositories.Remove([IO.Path]::GetFullPath($checkout)) }
        if ($RegisterRepository -or $RegisterCurrent) { [void]$repositories.Add($RepoRoot) }
        $state.Repositories = @($repositories)
        Save-DependencyOwnership
    }
    $currentNames = @(Get-ConfiguredDependencyNames $RepoRoot)
    if ($RegisterCurrent) {
        foreach ($name in $currentNames) {
            $path = Join-Path $dependencyRoot $name
            if (-not (Test-Path -LiteralPath $path -PathType Container)) { continue }
            Assert-PlainDependencyPath $path
            $family = @($families | Where-Object { $name.StartsWith($_.Prefix, [StringComparison]::Ordinal) })[0]
            if (-not @($family.Files | Where-Object { Test-Path -LiteralPath (Join-Path $path $_) -PathType Leaf }).Count) {
                throw "Configured installation has no recognized tool: $path"
            }
            if ($family.ContainsKey('Required')) {
                foreach ($required in $family.Required) {
                    if (-not (Test-Path -LiteralPath (Join-Path $path $required) -PathType Leaf)) { throw "Incomplete configured dependency: $path" }
                }
            }
            $marker = Join-Path $path '.dxx-managed-install'
            Assert-PlainDependencyPath $marker
            $entry = @($state.Installations | Where-Object Directory -eq $name)
            if ($entry.Count) {
                if ((Test-Path -LiteralPath $marker) -and (Get-Content -LiteralPath $marker -Raw).Trim() -ne $entry[0].Identity) {
                    throw "Managed installation was replaced outside registration: $path"
                }
            }
            if (-not $entry.Count -or -not (Test-Path -LiteralPath $marker)) {
                $state.Installations = @($state.Installations | Where-Object Directory -ne $name)
                $identity = [guid]::NewGuid().ToString('N')
                [IO.File]::WriteAllText($marker, $identity + "`n")
                $state.Installations = @($state.Installations) + [pscustomobject]@{ Directory = $name; Identity = $identity }
                Save-DependencyOwnership
            }
        }
    }
    if (-not $Apply -and ($RegisterRepository -or $RegisterCurrent -or $UnregisterRepository.Count)) { return }

    $protected = @($currentNames)
    $buildCaches = @()
    $referenceCheckouts = [Collections.Generic.HashSet[string]]::new($pathComparer)
    foreach ($checkout in @($state.Repositories) + $RepoRoot) { [void]$referenceCheckouts.Add($checkout) }
    foreach ($checkout in $referenceCheckouts) {
        # An unavailable checkout is not evidence that its versions are unused
        $otherBase = [IO.Path]::GetFullPath((Get-Content -LiteralPath (Join-Path $checkout 'dependency_base.txt') -First 1).Trim())
        if (-not $otherBase.Equals($dependencyRoot, $comparison)) { continue }
        $protected += @(Get-ConfiguredDependencyNames $checkout)
        foreach ($relative in @('buildd1', 'buildd2', 'buildd1-asan', 'buildd2-asan', 'android/app/.cxx')) {
            $buildRoot = Join-Path $checkout $relative
            if (Test-Path -LiteralPath $buildRoot -PathType Container) {
                Assert-PlainDependencyPath $buildRoot
                $buildCaches += @(Get-ChildItem -LiteralPath $buildRoot -Recurse -Depth 3 -Filter CMakeCache.txt -File -Force | ForEach-Object FullName)
            }
        }
    }
    foreach ($retirement in @($state.Retirements)) { Complete-DependencyRetirement $retirement }
    foreach ($entry in @($state.Installations)) {
        if ($entry.Identity -in @($state.Retirements | ForEach-Object Identity)) { continue }
        $name = [string]$entry.Directory
        if (-not @($families | Where-Object { $name -match ('^' + [regex]::Escape($_.Prefix) + '[A-Za-z0-9][A-Za-z0-9._+-]*$') }).Count -or
            $entry.Identity -notmatch '^[0-9a-f]{32}$') { throw 'Invalid managed dependency record' }
        $path = Join-Path $dependencyRoot $name
        if (-not (Test-Path -LiteralPath $path)) { continue }
        $reason = if ($name -in $protected) { 'configured by a registered checkout' } else { $null }
        $bytes = 0L
        if (-not $reason) {
            try {
                Assert-PlainDependencyPath $path
                $marker = Join-Path $path '.dxx-managed-install'
                Assert-PlainDependencyPath $marker
                if ((Get-Content -LiteralPath $marker -Raw).Trim() -ne $entry.Identity) { throw 'Installation identity changed' }
                $reason = Get-DependencyUse $path
                if (-not $reason -and -not (Test-ManagedReplacement $name)) { $reason = 'configured replacement is not installed and registered' }
                if (-not $reason) {
                    $tree = Get-DependencyTree $path
                    $bytes = $tree.Bytes
                    if ($Apply) {
                        $reason = Get-DependencyUse $path
                        if (-not $reason -and -not (Test-ManagedReplacement $name)) { $reason = 'configured replacement changed during inspection' }
                        if (-not $reason -and (Get-DependencyTree $path).Digest -ne $tree.Digest) { $reason = 'installation changed during inspection' }
                        if (-not $reason) {
                            foreach ($checkout in @($state.Repositories) + $RepoRoot) {
                                if ($name -in @(Get-ConfiguredDependencyNames $checkout)) { $reason = 'configuration changed during inspection'; break }
                            }
                        }
                        if (-not $reason) {
                            $retired = Join-Path $dependencyRoot ('.dxx-retired-' + $entry.Identity)
                            if (Test-Path -LiteralPath $retired) { throw 'Unexpected retirement destination already exists' }
                            $retirement = [pscustomobject]@{ Directory = $name; Identity = $entry.Identity }
                            $state.Retirements = @($state.Retirements) + $retirement
                            Save-DependencyOwnership
                            Move-Item -LiteralPath $path -Destination $retired
                            $result = Complete-DependencyRetirement $retirement
                            $reason = $result.Reason
                        }
                    }
                }
            } catch { $reason = $_.Exception.Message }
        }
        [pscustomobject]@{ Path = $path; Action = if ($reason) { 'Protected' } elseif ($Apply) { 'Removed' } else { 'Eligible' }; Reason = $reason; Bytes = $bytes }
    }
    foreach ($directory in Get-ChildItem -LiteralPath $dependencyRoot -Directory -Force) {
        if ($directory.Name -in @($state.Installations | ForEach-Object Directory)) { continue }
        if (@($families | Where-Object { $directory.Name.StartsWith($_.Prefix, [StringComparison]::Ordinal) }).Count) {
            [pscustomobject]@{ Path = $directory.FullName; Action = 'Protected'; Reason = 'unmanaged installation'; Bytes = 0L }
        }
    }
} finally {
    # Keep the lock inode stable for waiting processes
    if ($lock) { $lock.Dispose() }
}

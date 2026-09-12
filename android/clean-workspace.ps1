#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Remove generated workspace temp files unattended
.DESCRIPTION
    Deletes all ignored scratch contents by default, including fresh arbitrary
    runner outputs, engine copies, reports, logs and backups. Discovers temp
    roots at any repository depth outside source assets and dependency trees
    Preserves tracked/nonignored work, nested repositories, links and live jobs
    Preview and WhatIf never delete. TemporaryOnly skips build/download scans
    Build generation retention and producer-specific cleanup remain available
#>
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'None')]
param(
    [switch]$Preview,
    [switch]$AutoOnly,
    [switch]$BuildsOnly,
    [string[]]$BuildRoots,
    [switch]$Producer,
    [switch]$PayloadsOnly,
    [switch]$TemporaryOnly,
    [ValidateRange(0, 3600)][int]$BusyWaitSeconds = 60,
    [ValidateRange(0, 8760)][double]$PayloadGraceHours = 1,
    [ValidateRange(0, 3650)][int]$TempDays = 0,
    [ValidateRange(1, 3650)][int]$ArtifactDays = 30,
    [ValidateRange(1, 100)][int]$KeepBuildGenerations = 1,
    [ValidateRange(0, 8760)][double]$BuildGraceHours = 0,
    [string]$RepositoryRoot = ''
)

$ErrorActionPreference = 'Stop'
if ($Producer -and (-not $BuildsOnly -or -not $BuildRoots)) { throw 'Producer cleanup requires BuildsOnly and explicit BuildRoots' }
if (@($BuildsOnly, $PayloadsOnly, $TemporaryOnly | Where-Object { $_ }).Count -gt 1) { throw 'BuildsOnly, PayloadsOnly and TemporaryOnly are mutually exclusive' }
Set-StrictMode -Version Latest
if (-not $RepositoryRoot) { $RepositoryRoot = Split-Path $PSScriptRoot }
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd('\', '/')
$comparison = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Unix) {
    [StringComparison]::Ordinal
} else { [StringComparison]::OrdinalIgnoreCase }
$comparer = if ($comparison -eq [StringComparison]::Ordinal) {
    [StringComparer]::Ordinal
} else { [StringComparer]::OrdinalIgnoreCase }
$dryRun = $Preview -or $WhatIfPreference
$now = [DateTime]::UtcNow
$candidates = [Collections.Generic.List[object]]::new()
$seen = [Collections.Generic.HashSet[string]]::new($comparer)
$protectedPaths = [Collections.Generic.HashSet[string]]::new($comparer)
$buildGenerations = [Collections.Generic.List[object]]::new()
$buildContainers = [Collections.Generic.HashSet[string]]::new($comparer)
$registeredBuilds = [Collections.Generic.HashSet[string]]::new($comparer)

function Format-CleanupBytes {
    param([long]$Bytes)
    if ($Bytes -ge 1GB) { return '{0:n2} GiB' -f ($Bytes / 1GB) }
    if ($Bytes -ge 1MB) { return '{0:n1} MiB' -f ($Bytes / 1MB) }
    return '{0:n1} KiB' -f ($Bytes / 1KB)
}

function Assert-CleanupPath {
    param([string]$Path)
    $full = [IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($RepositoryRoot + [IO.Path]::DirectorySeparatorChar, $comparison)) {
        throw "Cleanup path is outside the repository: $full"
    }
    # Check every ancestor, including the repository itself, before traversal
    $current = $full
    while ($true) {
        $item = Get-Item -LiteralPath $current -Force
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Preserving path through a link: $current"
        }
        if ($current.Equals($RepositoryRoot, $comparison)) { break }
        $current = [IO.Path]::GetDirectoryName($current)
    }
}

function Update-ProtectedPaths {
    $protectedPaths.Clear()
    # Include untracked non-ignored work as well as tracked files, even when ignored
    $paths = & git -C $RepositoryRoot -c core.quotepath=false ls-files --cached --others --exclude-standard -z
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read Git protection list; cleanup aborted' }
    foreach ($relative in (($paths -join "`n") -split "`0")) {
        if (-not $relative) { continue }
        $path = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $relative))
        while ($path -and -not $path.Equals($RepositoryRoot, $comparison)) {
            $null = $protectedPaths.Add($path)
            $path = [IO.Path]::GetDirectoryName($path)
        }
    }
}

function Test-CleanupGitProtection {
    param([string]$Path)
    $relative = $Path.Substring($RepositoryRoot.Length + 1).Replace('\', '/')
    $paths = & git -C $RepositoryRoot --literal-pathspecs ls-files --cached --others --exclude-standard -z -- $relative
    if ($LASTEXITCODE -ne 0) { throw 'Cannot recheck Git protection; cleanup aborted' }
    return [bool]$paths
}

function Assert-CleanupIdle {
    # Do not kill processes or confuse idle Gradle/Kotlin daemons with active builds
    if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
        $idleWait = [Diagnostics.Stopwatch]::StartNew()
        while ($true) {
            $processes = @(Get-CimInstance Win32_Process)
            $producerAncestors = @($PID)
            if ($Producer) {
                # Gradle daemons launch cleanup separately from the wrapper's test/build caller
                $clients = @($PID) + @($processes | Where-Object {
                        $_.CommandLine -and $_.CommandLine.Contains($RepositoryRoot) -and $_.CommandLine -match 'GradleWrapperMain'
                    } | ForEach-Object ProcessId)
                foreach ($clientId in $clients) {
                    $currentId = $clientId
                    if ($currentId -notin $producerAncestors) { $producerAncestors += $currentId }
                    while ($currentId) {
                        $owner = $processes | Where-Object ProcessId -eq $currentId | Select-Object -First 1
                        if (-not $owner -or $owner.ParentProcessId -in $producerAncestors) { break }
                        $currentId = $owner.ParentProcessId
                        $producerAncestors += $currentId
                    }
                }
            }
            $busy = @($processes | Where-Object {
                    $_.ProcessId -notin $producerAncestors -and
                    # Emulators use installed APKs, not the producer's scoped native build generations
                    -not ($Producer -and $_.Name -match '^(emulator|qemu-system-.*)\.exe$') -and
                    (
                        $_.Name -match '^(cl|clang|clang\+\+|ninja|cmake|ctest|cargo|rustc|dxx-redux.*|d[12]x-redux|emulator|qemu-system-.*)\.exe$' -or
                        ($_.CommandLine -and $_.CommandLine.Contains($RepositoryRoot) -and
                        $_.CommandLine -match '(GradleWrapperMain|run[_-].*tests?\.ps1|test_[^ ]+\.ps1|regenerate[^ ]*\.ps1|run_mission[^ ]*\.ps1|run-code-quality\.ps1)')
                    )
                })
            if (-not $busy.Count) { break }
            if ($idleWait.Elapsed.TotalSeconds -ge $BusyWaitSeconds) {
                throw "Build/test/formatter processes are active (PIDs: $($busy.ProcessId -join ', ')); idle wait expired after $BusyWaitSeconds seconds"
            }
            Write-Progress -Activity 'Waiting for active jobs before cleanup' -Status (($busy | ForEach-Object { "$($_.Name) PID $($_.ProcessId)" }) -join ', ')
            Start-Sleep -Seconds 1
        }
        Write-Progress -Activity 'Waiting for active jobs before cleanup' -Completed
    } else {
        throw 'Deletion currently requires Windows process checks; use -Preview on other hosts'
    }
}

function Get-CleanupTreeInfo {
    param([string]$Path, [switch]$AllowBuildDependencies, [switch]$IncludeCreationTime, [switch]$IgnoreRootWriteTime, [switch]$AllowEmulatorLockMarkers)
    Assert-CleanupPath $Path
    $stack = [Collections.Generic.Stack[IO.FileSystemInfo]]::new()
    $stack.Push((Get-Item -LiteralPath $Path -Force))
    $bytes = 0L
    $count = 0L
    $latest = [DateTime]::MinValue
    while ($stack.Count) {
        $item = $stack.Pop()
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Preserving tree containing a link: $($item.FullName)"
        }
        if ((-not $IgnoreRootWriteTime -or $item.FullName -ne $Path) -and $item.LastWriteTimeUtc -gt $latest) { $latest = $item.LastWriteTimeUtc }
        if ($IncludeCreationTime -and $item.CreationTimeUtc -gt $latest) { $latest = $item.CreationTimeUtc }
        if ($item -is [IO.DirectoryInfo]) {
            foreach ($child in $item.EnumerateFileSystemInfos()) {
                if ($child.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                    throw "Preserving tree containing a link: $($child.FullName)"
                }
                if ($child.Name -in @('.git', '.hg', '.svn')) {
                    # FetchContent clones live below an owning CMake binary directory
                    $generatedClone = $false
                    $sourceRoot = $item
                    # Submodule .git files are inert files here, never paths to traverse
                    while ($AllowBuildDependencies -and $child.Name -eq '.git' -and
                        $sourceRoot.FullName.StartsWith($Path + [IO.Path]::DirectorySeparatorChar, $comparison)) {
                        if ($sourceRoot.Name -like '*-src' -and $sourceRoot.Parent.Name -eq '_deps') {
                            $binaryRoot = $sourceRoot.Parent.Parent
                            $generatedClone = Test-Path -LiteralPath (Join-Path $binaryRoot.FullName 'CMakeCache.txt') -PathType Leaf
                            if (-not $generatedClone -and $binaryRoot.Name -match '^(?<abi>armeabi-v7a|arm64-v8a|x86_64|x86)\.stale[-_].+$') {
                                # ABI rebuild recovery can leave only _deps in its stale sibling
                                $ownerCache = Join-Path $binaryRoot.Parent.FullName "$($Matches.abi)/CMakeCache.txt"
                                $generatedClone = Test-Path -LiteralPath $ownerCache -PathType Leaf
                            }
                            break
                        }
                        $sourceRoot = $sourceRoot.Parent
                    }
                    if (-not $generatedClone) {
                        throw "Preserving tree containing a nested repository: $($child.FullName)"
                    }
                }
                if ($child.Name -match '(\.(lock|lck)(\.json)?$|^\.ninja_lock$)') {
                    # CMake/Gradle leave lock files behind after releasing the lock
                    if ($child -is [IO.DirectoryInfo]) {
                        # Emulator processes are checked before every deletion; these
                        # known marker directories remain after an unclean shutdown
                        $orphanMarker = $AllowEmulatorLockMarkers -and $item.Name -like '*.avd' -and
                        $child.Name -in @('hardware-qemu.ini.lock', 'snapshot.lock.lock') -and
                        (Test-Path -LiteralPath (Join-Path $item.FullName 'config.ini') -PathType Leaf)
                        if (-not $orphanMarker) { throw "Preserving directory lock: $($child.FullName)" }
                    } else {
                        $lockStream = [IO.File]::Open($child.FullName, 'Open', 'Read', 'None')
                        $lockStream.Dispose()
                    }
                }
                $stack.Push($child)
            }
        } else {
            $bytes += $item.Length
            $count++
        }
    }
    return [pscustomobject]@{ Bytes = $bytes; Count = $count; Latest = $latest }
}

function Add-CleanupCandidate {
    param([IO.FileSystemInfo]$Item, [string]$Category, [int]$Depth = 0)
    if (-not $seen.Add($Item.FullName)) { return }
    if ($buildContainers.Contains($Item.FullName)) {
        # Never offer a parent that would also remove a retained build generation
        if ($Item.PSIsContainer -and -not $registeredBuilds.Contains($Item.FullName)) {
            foreach ($child in Get-ChildItem -LiteralPath $Item.FullName -Force) {
                Add-CleanupCandidate $child $Category ($Depth + 1)
            }
        }
        return
    }
    if ($Item.Name -match '\.(lock|lck)$' -or $protectedPaths.Contains($Item.FullName)) { return }
    Write-Progress -Activity 'Scanning cleanup candidates' -Status $Item.FullName
    try { $info = Get-CleanupTreeInfo $Item.FullName } catch {
        Write-Warning $_.Exception.Message
        return
    }
    $automatic = -not $Item.PSIsContainer -and $Category -eq 'scratch' -and
    $Item.Extension -in @('.log', '.tmp', '.temp')
    $age = if ($automatic) { $TempDays } else { $ArtifactDays }
    if ($info.Latest -gt $now.AddDays(-$age)) {
        # A collection with recent runs may still contain old run directories
        if ($Category -eq 'scratch' -and $Item.PSIsContainer -and $Depth -lt 2 -and
            -not (Test-Path -LiteralPath (Join-Path $Item.FullName 'CMakeCache.txt'))) {
            foreach ($child in Get-ChildItem -LiteralPath $Item.FullName -Force) {
                Add-CleanupCandidate $child $Category ($Depth + 1)
            }
        }
        return
    }
    $candidates.Add([pscustomobject]@{
            Path = $Item.FullName; Category = $Category; Automatic = $automatic
            Bytes = $info.Bytes; Count = $info.Count; Latest = $info.Latest; Days = $age; Retained = $null
        })
}

function Add-TemporaryCandidate {
    param([IO.FileSystemInfo]$Item)
    if ($Item.Attributes -band [IO.FileAttributes]::ReparsePoint) { return }
    if ($Item.PSIsContainer -and @('.git', '.hg', '.svn' | Where-Object {
                Test-Path -LiteralPath (Join-Path $Item.FullName $_)
            }).Count) { return }
    if (-not $Item.PSIsContainer -and $protectedPaths.Contains($Item.FullName)) { return }
    $descend = $Item.PSIsContainer -and $protectedPaths.Contains($Item.FullName)
    if (-not $descend) {
        try {
            $info = Get-CleanupTreeInfo $Item.FullName -IncludeCreationTime -AllowBuildDependencies -AllowEmulatorLockMarkers
            if ($Item.Name -match '\.(lock|lck)(\.json)?$' -and -not $Item.PSIsContainer) {
                $stream = [IO.File]::Open($Item.FullName, 'Open', 'Read', 'None')
                $stream.Dispose()
            }
            if ($TempDays -eq 0 -or $info.Latest -le $now.AddDays(-$TempDays)) {
                if ($seen.Add($Item.FullName)) {
                    $candidates.Add([pscustomobject]@{
                            Path = $Item.FullName; Category = 'temporary'; Automatic = $true
                            Bytes = $info.Bytes; Count = $info.Count; Latest = $info.Latest
                            Days = $TempDays; Retained = $null
                        })
                }
                return
            }
            $descend = $Item.PSIsContainer
        } catch {
            Write-Warning $_.Exception.Message
            # A protected subtree must not strand unrelated siblings in a collection
            $descend = $Item.PSIsContainer -and $_.Exception.Message -match 'containing a (link|nested repository)'
        }
    }
    if ($descend) {
        foreach ($child in Get-ChildItem -LiteralPath $Item.FullName -Force) {
            Add-TemporaryCandidate $child
        }
    }
}

function Find-TemporaryFiles {
    # Discover by scratch role, not timestamp format or a list of individual runs
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($RepositoryRoot)
    while ($pending.Count) {
        $directory = $pending.Pop()
        foreach ($item in Get-ChildItem -LiteralPath $directory -Force) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
            if ($item.PSIsContainer) {
                if ($item.Name -match '^(temp(?:[_-].*)?|tmp(?:[_-].*)?|\.tmp|\.temp|test-results|test-reports|regression-results|regression-output)$') {
                    Add-TemporaryCandidate $item
                } elseif ($item.Name -notin @('.git', '.hg', '.svn', 'game_data', 'game_data_to_copy_to_emulator',
                        'regression_demos', 'fixtures', 'node_modules', 'vcpkg_installed', '_deps', '.gradle', '.cxx') -and
                    $item.Name -notmatch '^build(?:$|[_-]|d[12]|2)' -and
                    -not (Test-Path -LiteralPath (Join-Path $item.FullName '.git')) -and
                    -not (Test-Path -LiteralPath (Join-Path $item.FullName '.hg')) -and
                    -not (Test-Path -LiteralPath (Join-Path $item.FullName '.svn'))) {
                    $pending.Push($item.FullName)
                }
            } elseif ($item.Extension -in @('.tmp', '.temp', '.log')) {
                Add-TemporaryCandidate $item
            }
        }
    }
}

function Add-BuildGeneration {
    param([IO.FileSystemInfo]$Item, [string]$Family)
    if (-not $registeredBuilds.Add($Item.FullName)) { return }
    # Mark the tree even if unreadable/protected so it cannot fall back to parent deletion
    $path = $Item.FullName
    while ($path -and -not $path.Equals($RepositoryRoot, $comparison)) {
        $null = $buildContainers.Add($path)
        $path = [IO.Path]::GetDirectoryName($path)
    }
    Write-Progress -Activity 'Scanning cleanup candidates' -Status $Item.FullName
    try { $info = Get-CleanupTreeInfo $Item.FullName -AllowBuildDependencies } catch {
        Write-Warning $_.Exception.Message
        return
    }
    $buildGenerations.Add([pscustomobject]@{
            Path = $Item.FullName; Family = $Family; Bytes = $info.Bytes
            Count = $info.Count; Latest = $info.Latest
        })
}

function Find-RegressionPayloads {
    # Only these runners define raw/stages as reproducible mission asset copies
    foreach ($collection in @('mission_zip_host_metadata', 'guidebot_simulation_regression')) {
        $container = Join-Path $RepositoryRoot "android/temp/$collection"
        if (-not (Test-Path -LiteralPath $container -PathType Container)) { continue }
        Assert-CleanupPath $container
        foreach ($run in Get-ChildItem -LiteralPath $container -Directory -Force) {
            if ($run.Name -notmatch '^\d{8}_\d{6}$') { continue }
            Assert-CleanupPath $run.FullName
            $workspaces = @($run.FullName)
            $workers = Join-Path $run.FullName 'workers'
            if (Test-Path -LiteralPath $workers -PathType Container) {
                Assert-CleanupPath $workers
                $workspaces += @(Get-ChildItem -LiteralPath $workers -Directory -Force | ForEach-Object FullName)
            }
            foreach ($workspace in $workspaces) {
                foreach ($role in @('raw', 'stages')) {
                    $parent = Join-Path $workspace $role
                    if (-not (Test-Path -LiteralPath $parent -PathType Container)) { continue }
                    Assert-CleanupPath $parent
                    # Group pure payload trees, but preserve raw JSON reports in mixed directories
                    $entries = @(Get-ChildItem -LiteralPath $parent -Force)
                    $grouped = $false
                    if ($entries.Count -gt 0 -and @($entries | Where-Object { -not $_.PSIsContainer }).Count -eq 0 -and
                        -not $protectedPaths.Contains($parent)) {
                        try {
                            # Removing old children changes the parent's write time, not its remaining payloads
                            $groupInfo = Get-CleanupTreeInfo $parent -IncludeCreationTime -IgnoreRootWriteTime
                            if ($groupInfo.Latest -le $now.AddHours(-$PayloadGraceHours)) {
                                $candidates.Add([pscustomobject]@{
                                        Path = $parent; Category = 'regression-payload-group'; Automatic = $true
                                        Bytes = $groupInfo.Bytes; Count = $groupInfo.Count; Latest = $groupInfo.Latest
                                        Days = $PayloadGraceHours / 24; Retained = $null
                                    })
                                $null = $seen.Add($parent)
                                $grouped = $true
                            }
                        } catch { Write-Warning $_.Exception.Message }
                    }
                    if ($grouped) { continue }
                    foreach ($payload in $entries | Where-Object PSIsContainer) {
                        $null = $seen.Add($payload.FullName)
                        if ($protectedPaths.Contains($payload.FullName)) { continue }
                        try { $info = Get-CleanupTreeInfo $payload.FullName -IncludeCreationTime } catch {
                            Write-Warning $_.Exception.Message
                            continue
                        }
                        if ($info.Latest -gt $now.AddHours(-$PayloadGraceHours)) { continue }
                        $candidates.Add([pscustomobject]@{
                                Path = $payload.FullName; Category = 'regression-payload'; Automatic = $true
                                Bytes = $info.Bytes; Count = $info.Count; Latest = $info.Latest
                                Days = $PayloadGraceHours / 24; Retained = $null
                            })
                    }
                }
            }
        }
    }
}

function Find-BuildGenerations {
    param([IO.FileSystemInfo]$Item, [int]$Depth = 0)
    if ($Item.Attributes -band [IO.FileAttributes]::ReparsePoint) { return }
    if (-not $Item.PSIsContainer) {
        # Versioned packages emitted by the build scripts, including unique deploy suffixes
        if ($Item.Name -match '^(?<family>.+)-v\d+(?:-[a-f0-9]{32})?(?<ext>\.(?:apk|aab|zip))$') {
            $packageFamily = $Matches.family -replace '[-_]\d{8}[-_]\d{6}$', ''
            Add-BuildGeneration $Item "package|$($Item.DirectoryName)|$packageFamily|$($Matches.ext)"
        }
        return
    }
    Assert-CleanupPath $Item.FullName
    if (Test-Path -LiteralPath (Join-Path $Item.FullName '.git')) { return }
    if ($Item.Name -match '^\.cxx(?:$|[-_])') {
        # AGP stores a separate configuration-hash family for each build type
        foreach ($variant in Get-ChildItem -LiteralPath $Item.FullName -Directory -Force) {
            if ($variant.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
            foreach ($generation in Get-ChildItem -LiteralPath $variant.FullName -Directory -Force) {
                if ($generation.Name -match '^[a-z0-9]{8}$') {
                    if ($generation.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
                    $abis = @(Get-ChildItem -LiteralPath $generation.FullName -Directory -Force | Where-Object {
                            $_.Name -match '^(armeabi-v7a|arm64-v8a|x86|x86_64|riscv64)$'
                        })
                    if ($abis.Count) { Add-BuildGeneration $generation "cxx|$($variant.FullName)" }
                }
            }
        }
        return
    }
    $cache = Join-Path $Item.FullName 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cache -PathType Leaf) {
        Assert-CleanupPath $cache
        $settings = @{}
        foreach ($line in Get-Content -LiteralPath $cache) {
            if ($line -match '^([^#/:][^:]*):[^=]+=(.*)$') { $settings[$Matches[1]] = $Matches[2] }
        }
        if ($settings['CMAKE_HOME_DIRECTORY'] -and $settings['CMAKE_GENERATOR']) {
            # Keep separate source projects, configurations, architectures, and feature sets
            $keys = @('CMAKE_HOME_DIRECTORY', 'CMAKE_BUILD_TYPE', 'CMAKE_GENERATOR',
                'CMAKE_GENERATOR_PLATFORM', 'CMAKE_GENERATOR_TOOLSET', 'CMAKE_TOOLCHAIN_FILE',
                'VCPKG_TARGET_TRIPLET', 'ANDROID_ABI', 'BUILD_TESTING', 'BUILD_SHARED_LIBS',
                'ASM', 'EDITOR', 'OPENGL', 'OPENGLMERGE', 'PNG', 'SDLMIXER', 'UDP', 'TRACKER', 'TRACE_IN_RELEASE')
            $keys += @($settings.Keys | Where-Object { $_ -match '^(DXX_|ANDROID_USE_|CMAKE_(C|CXX)_FLAGS|USE_|ENABLE_)' } | Sort-Object)
            $identity = ($keys | ForEach-Object { "$_=$($settings[$_])" }) -join '|'
            Add-BuildGeneration $Item "cmake|$identity"
            return
        }
    }
    if ($Depth -ge 4) { return }
    foreach ($child in Get-ChildItem -LiteralPath $Item.FullName -Force) {
        Find-BuildGenerations $child ($Depth + 1)
    }
}

# Restrict discovery to known output roles, never arbitrary ignored trees
$gitRoot = & git -C $RepositoryRoot rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0 -or -not ([IO.Path]::GetFullPath($gitRoot)).Equals($RepositoryRoot, $comparison)) {
    throw 'RepositoryRoot must be the root of a Git working tree'
}
Update-ProtectedPaths
if (-not $dryRun) { Assert-CleanupIdle }
if ($PayloadsOnly) { Find-RegressionPayloads }
if (-not $BuildsOnly -and -not $PayloadsOnly) { Find-TemporaryFiles }
$roots = [Collections.Generic.List[object]]::new()
foreach ($parentRelative in @('', 'android', 'server')) {
    $parent = if ($parentRelative) { Join-Path $RepositoryRoot $parentRelative } else { $RepositoryRoot }
    if (-not (Test-Path -LiteralPath $parent)) { continue }
    foreach ($item in Get-ChildItem -LiteralPath $parent -Force) {
        if ($item.Name -match '^(temp(?:[_-].*)?|\.tmp|test-results|test-reports|regression-results|regression-output|downloads|download-cache)$') {
            $roots.Add([pscustomobject]@{ Item = $item; Category = if ($item.Name -match 'download') { 'downloads' } else { 'scratch' }; Children = $item.PSIsContainer })
        } elseif ($item.Name -match '^build(?:$|[_-]|d[12]|2)' -and $item.PSIsContainer) {
            $roots.Add([pscustomobject]@{ Item = $item; Category = 'build'; Children = $item.Name -eq 'build-outputs' })
        }
    }
}
foreach ($relative in @('android/.cxx', 'android/.gradle', 'android/.kotlin',
        'android/app/build', 'android/app/.cxx', 'android/app/.gradle',
        'android/mission-metadata-core/build', 'android/mission-metadata-cli/build',
        'd1/out', 'd2/out', 'server/target')) {
    $path = Join-Path $RepositoryRoot $relative
    if (Test-Path -LiteralPath $path) {
        $roots.Add([pscustomobject]@{ Item = Get-Item -LiteralPath $path -Force; Category = 'build'; Children = $false })
    }
}
$androidRoot = Join-Path $RepositoryRoot 'android'
if (Test-Path -LiteralPath $androidRoot) {
    foreach ($module in Get-ChildItem -LiteralPath $androidRoot -Directory -Force) {
        if ($module.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
        foreach ($nativeRoot in Get-ChildItem -LiteralPath $module.FullName -Directory -Force | Where-Object { $_.Name -match '^\.cxx(?:$|[-_])' }) {
            $roots.Add([pscustomobject]@{ Item = $nativeRoot; Category = 'build'; Children = $false })
        }
    }
}
if ($BuildRoots) {
    $roots.Clear()
    foreach ($buildRoot in $BuildRoots) {
        $full = [IO.Path]::GetFullPath($buildRoot)
        if (-not (Test-Path -LiteralPath $full -PathType Container)) { continue }
        Assert-CleanupPath $full
        $item = Get-Item -LiteralPath $full
        if ($item.Name -notmatch '^(\.cxx(?:$|[-_])|build-outputs$)') {
            throw "Explicit build roots must be .cxx or build-outputs directories: $full"
        }
        $roots.Add([pscustomobject]@{ Item = $item; Category = 'build'; Children = $false })
    }
}
$discoveredBuildRoots = [Collections.Generic.HashSet[string]]::new($comparer)
foreach ($root in $roots) {
    if ($PayloadsOnly -or $TemporaryOnly) { break }
    if (-not $BuildsOnly -and $root.Category -eq 'scratch') { continue }
    if ($root.Category -notin @('build', 'scratch')) { continue }
    if (-not $discoveredBuildRoots.Add($root.Item.FullName)) { continue }
    Write-Host "Finding build generations in $($root.Item.FullName)"
    try { Find-BuildGenerations $root.Item } catch { Write-Warning $_.Exception.Message }
}
foreach ($family in $buildGenerations | Group-Object Family) {
    $ranked = @($family.Group | Sort-Object @{ Expression = 'Latest'; Descending = $true }, Path)
    foreach ($keep in $ranked | Select-Object -First $KeepBuildGenerations) {
        Write-Host "[KEEP] newest build generation: $($keep.Path)"
    }
    foreach ($generation in $ranked | Select-Object -Skip $KeepBuildGenerations) {
        # Rank ties by path and honor an explicitly requested grace period
        if (($BuildGraceHours -gt 0 -and $generation.Latest -gt $now.AddHours(-$BuildGraceHours)) -or
            $protectedPaths.Contains($generation.Path)) { continue }
        $candidates.Add([pscustomobject]@{
                Path = $generation.Path; Category = 'superseded-build'; Automatic = $true
                Bytes = $generation.Bytes; Count = $generation.Count; Latest = $generation.Latest
                Days = $BuildGraceHours / 24; Retained = $ranked[0].Path
            })
    }
}
foreach ($root in $roots) {
    if ($BuildsOnly -or $PayloadsOnly -or $TemporaryOnly) { break }
    if ($root.Category -eq 'scratch') { continue }
    Write-Host "Scanning $($root.Item.FullName)"
    try { Assert-CleanupPath $root.Item.FullName } catch { Write-Warning $_.Exception.Message; continue }
    if ($root.Children) {
        foreach ($child in Get-ChildItem -LiteralPath $root.Item.FullName -Force) {
            Add-CleanupCandidate $child $root.Category
        }
    } else { Add-CleanupCandidate $root.Item $root.Category }
}

$ordered = @($candidates | Sort-Object Bytes -Descending)
Write-Progress -Activity 'Scanning cleanup candidates' -Completed
$totalBytes = 0L
foreach ($item in $ordered) { $totalBytes += $item.Bytes }
Write-Host "Automatic cleanup: $($ordered.Count) artifacts ($(Format-CleanupBytes $totalBytes))"
$removed = 0
$reclaimed = 0L
foreach ($candidate in $ordered) {
    Write-Verbose "[AUTO] $(Format-CleanupBytes $candidate.Bytes) | $($candidate.Category) | $($candidate.Path)"
    Write-Progress -Activity 'Removing generated artifacts' -Status $candidate.Path
    if ($dryRun) { continue }
    Assert-CleanupIdle
    if (Test-CleanupGitProtection $candidate.Path) { Write-Warning "Now protected by Git: $($candidate.Path)"; continue }
    try {
        if ($candidate.Retained) {
            $replacement = Get-CleanupTreeInfo $candidate.Retained -AllowBuildDependencies
            if ($replacement.Latest -lt $candidate.Latest) {
                Write-Warning "Newer retained build is no longer available; preserving $($candidate.Path)"
                continue
            }
        }
        $current = Get-CleanupTreeInfo $candidate.Path -AllowBuildDependencies:([bool]$candidate.Retained -or $candidate.Category -eq 'temporary') `
            -IncludeCreationTime:($candidate.Category -like 'regression-payload*' -or $candidate.Category -eq 'temporary') `
            -IgnoreRootWriteTime:($candidate.Category -eq 'regression-payload-group') `
            -AllowEmulatorLockMarkers:($candidate.Category -eq 'temporary')
        if ($current.Latest -ne $candidate.Latest -or $current.Bytes -ne $candidate.Bytes -or
            $current.Count -ne $candidate.Count -or ($candidate.Days -gt 0 -and $current.Latest -gt [DateTime]::UtcNow.AddDays(-$candidate.Days))) {
            Write-Warning "Changed since scan; preserving $($candidate.Path)"
            continue
        }
        if ($PSCmdlet.ShouldProcess($candidate.Path, 'Delete generated artifact')) {
            # The full tree and its ancestors were checked immediately above
            Remove-Item -LiteralPath $candidate.Path -Recurse -Force -ErrorAction Stop
            $removed++
            $reclaimed += $current.Bytes
        }
    } catch { Write-Warning "Preserved or incompletely removed $($candidate.Path): $($_.Exception.Message)" }
}
Write-Progress -Activity 'Removing generated artifacts' -Completed
if ($dryRun) { Write-Host 'Preview only: no files deleted and no prompts answered' }
Write-Host "Removed $removed artifacts ($(Format-CleanupBytes $reclaimed)); sizes are logical bytes, not guaranteed disk savings"

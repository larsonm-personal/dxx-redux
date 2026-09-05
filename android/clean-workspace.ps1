#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Clean old generated workspace files, prompting before removing reusable artifacts
.DESCRIPTION
    Default: delete ignored loose .log/.tmp/.temp files older than TempDays in
    known scratch directories, and prompt for older build/download/regression
    artifacts. Superseded native builds and versioned packages are automatic,
    keeping the newest generation per family and a 24-hour recent-build grace
    Enter keeps a prompted item. NoToAll skips all remaining prompts
    Use -Preview or -WhatIf to inspect without deleting or prompting
    Use -AutoOnly for unattended cleanup of temporary files and superseded builds
    Source files, Git-visible files, game data, demos, fixtures, SDKs, global
    caches, nested repositories, links, and recent artifacts are preserved
.EXAMPLE
    .\android\clean-workspace.ps1 -Preview
.EXAMPLE
    .\android\clean-workspace.ps1
.EXAMPLE
    .\android\clean-workspace.ps1 -AutoOnly -TempDays 14
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [switch]$Preview,
    [switch]$AutoOnly,
    [switch]$BuildsOnly,
    [ValidateRange(1, 3650)][int]$TempDays = 7,
    [ValidateRange(1, 3650)][int]$ArtifactDays = 30,
    [ValidateRange(1, 100)][int]$KeepBuildGenerations = 1,
    [ValidateRange(0, 8760)][double]$BuildGraceHours = 24,
    [string]$RepositoryRoot = (Split-Path $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
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
        $busy = @(Get-CimInstance Win32_Process | Where-Object {
                $_.ProcessId -ne $PID -and (
                    $_.Name -match '^(cl|clang|clang\+\+|ninja|cmake|ctest|cargo|rustc|dxx-redux.*)\.exe$' -or
                    ($_.CommandLine -and $_.CommandLine.Contains($RepositoryRoot) -and
                    $_.CommandLine -match '(GradleWrapperMain|run[_-].*tests?\.ps1|test_[^ ]+\.ps1|regenerate[^ ]*\.ps1|run_mission[^ ]*\.ps1|run-code-quality\.ps1)')
                )
            })
        if ($busy.Count) {
            throw "Build/test/formatter processes are active (PIDs: $($busy.ProcessId -join ', ')); stop them before cleanup or use -Preview"
        }
    } else {
        throw 'Deletion currently requires Windows process checks; use -Preview on other hosts'
    }
}

function Get-CleanupTreeInfo {
    param([string]$Path, [switch]$AllowBuildDependencies)
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
        if ($item.LastWriteTimeUtc -gt $latest) { $latest = $item.LastWriteTimeUtc }
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
                    if ($child -is [IO.DirectoryInfo]) { throw "Preserving directory lock: $($child.FullName)" }
                    $lockStream = [IO.File]::Open($child.FullName, 'Open', 'Read', 'None')
                    $lockStream.Dispose()
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
$discoveredBuildRoots = [Collections.Generic.HashSet[string]]::new($comparer)
foreach ($root in $roots) {
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
        # Do not break timestamp ties or delete builds touched within the grace period
        if ($generation.Latest -ge $ranked[$KeepBuildGenerations - 1].Latest -or
            $generation.Latest -gt $now.AddHours(-$BuildGraceHours) -or
            $protectedPaths.Contains($generation.Path)) { continue }
        $candidates.Add([pscustomobject]@{
                Path = $generation.Path; Category = 'superseded-build'; Automatic = $true
                Bytes = $generation.Bytes; Count = $generation.Count; Latest = $generation.Latest
                Days = $BuildGraceHours / 24; Retained = $ranked[0].Path
            })
    }
}
foreach ($root in $roots) {
    if ($BuildsOnly) { break }
    Write-Host "Scanning $($root.Item.FullName)"
    try { Assert-CleanupPath $root.Item.FullName } catch { Write-Warning $_.Exception.Message; continue }
    if ($root.Children) {
        foreach ($child in Get-ChildItem -LiteralPath $root.Item.FullName -Force) {
            Add-CleanupCandidate $child $root.Category
        }
    } else { Add-CleanupCandidate $root.Item $root.Category }
}

$ordered = @($candidates | Sort-Object Bytes -Descending)
$auto = @($ordered | Where-Object Automatic)
$review = @($ordered | Where-Object { -not $_.Automatic })
Write-Progress -Activity 'Scanning cleanup candidates' -Completed
$autoBytes = 0L
$reviewBytes = 0L
foreach ($item in $auto) { $autoBytes += $item.Bytes }
foreach ($item in $review) { $reviewBytes += $item.Bytes }
Write-Host "Automatic: $($auto.Count) temporary files and superseded builds ($(Format-CleanupBytes $autoBytes))"
Write-Host "Review: $($review.Count) artifacts ($(Format-CleanupBytes $reviewBytes))"
Write-Host 'Review items may contain useful diagnostics or require rebuilding/downloading'
$removed = 0
$reclaimed = 0L
$skipPrompts = $AutoOnly.IsPresent
$approved = [Collections.Generic.HashSet[string]]::new($comparer)
$reviewed = [Collections.Generic.HashSet[string]]::new($comparer)
foreach ($candidate in $ordered) {
    $action = if ($candidate.Automatic) { 'AUTO' } else { 'ASK' }
    $description = "[$action] $(Format-CleanupBytes $candidate.Bytes) | $($candidate.Category) | last write $($candidate.Latest.ToString('yyyy-MM-dd')) | $($candidate.Path)"
    Write-Host $description
    if ($dryRun) { continue }
    if (-not $candidate.Automatic) {
        if ($skipPrompts) { continue }
        if (-not $approved.Contains($candidate.Path)) {
            $answer = Read-Host 'Delete this artifact? [y/N/folder/noToAll/quit]'
            if ($answer -match '^(?i:q|quit)$') { break }
            if ($answer -match '^(?i:notoall)$') { $skipPrompts = $true; continue }
            if ($answer -match '^(?i:folder)$') {
                $parent = [IO.Path]::GetDirectoryName($candidate.Path)
                $siblings = @($review | Where-Object {
                        -not $reviewed.Contains($_.Path) -and $_.Category -eq $candidate.Category -and
                        [IO.Path]::GetDirectoryName($_.Path).Equals($parent, $comparison)
                    })
                $groupBytes = 0L
                foreach ($sibling in $siblings) {
                    Write-Host "  $(Format-CleanupBytes $sibling.Bytes) | $($sibling.Path)"
                    $groupBytes += $sibling.Bytes
                }
                $answer = Read-Host "Delete these $($siblings.Count) listed artifacts ($(Format-CleanupBytes $groupBytes))? [y/N]"
                if ($answer -match '^(?i:y|yes)$') {
                    foreach ($sibling in $siblings) { $null = $approved.Add($sibling.Path) }
                }
            }
            $null = $reviewed.Add($candidate.Path)
            if ($answer -notmatch '^(?i:y|yes)$') { continue }
        }
    }
    Assert-CleanupIdle
    if (Test-CleanupGitProtection $candidate.Path) { Write-Warning "Now protected by Git: $($candidate.Path)"; continue }
    try {
        if ($candidate.Retained) {
            $replacement = Get-CleanupTreeInfo $candidate.Retained -AllowBuildDependencies
            if ($replacement.Latest -le $candidate.Latest) {
                Write-Warning "Newer retained build is no longer available; preserving $($candidate.Path)"
                continue
            }
        }
        $current = Get-CleanupTreeInfo $candidate.Path -AllowBuildDependencies:([bool]$candidate.Retained)
        if ($current.Latest -ne $candidate.Latest -or $current.Bytes -ne $candidate.Bytes -or
            $current.Count -ne $candidate.Count -or $current.Latest -gt [DateTime]::UtcNow.AddDays(-$candidate.Days)) {
            Write-Warning "Changed since scan; preserving $($candidate.Path)"
            continue
        }
        if ($PSCmdlet.ShouldProcess($candidate.Path, 'Delete old generated artifact')) {
            # The full tree and its ancestors were checked immediately above
            Remove-Item -LiteralPath $candidate.Path -Recurse -Force -ErrorAction Stop
            $removed++
            $reclaimed += $current.Bytes
        }
    } catch { Write-Warning "Preserved or incompletely removed $($candidate.Path): $($_.Exception.Message)" }
}
if ($dryRun) { Write-Host 'Preview only: no files deleted and no prompts answered' }
Write-Host "Removed $removed artifacts ($(Format-CleanupBytes $reclaimed)); sizes are logical bytes, not guaranteed disk savings"

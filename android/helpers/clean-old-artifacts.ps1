#!/usr/bin/env pwsh
# Retains the newest generations of timestamp-named scratch artifacts
# Usage:
#   .\android\helpers\clean-old-artifacts.ps1
#   .\android\helpers\clean-old-artifacts.ps1 -apply
#   .\android\helpers\clean-old-artifacts.ps1 -KeepDirectoryGenerations 2 -KeepFileGenerations 3
#
# Scratch roots are discovered by role. Direct children and one collection level
# are classified. Deeper stable trees are preserved as workspaces or caches.

[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [switch]$apply,
    [ValidateRange(1, 1000)][int]$KeepDirectoryGenerations = 1,
    [ValidateRange(1, 1000)][int]$KeepFileGenerations = 1,
    [ValidateRange(0, 8760)][double]$MinimumAgeHours = 1,
    [string]$RepositoryRoot = "",
    [string[]]$Roots
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not $RepositoryRoot) {
    $RepositoryRoot = Split-Path (Split-Path $PSScriptRoot)
}
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
if (-not (Test-Path -LiteralPath $RepositoryRoot -PathType Container)) {
    throw "Repository root not found: $RepositoryRoot"
}

$pathComparison = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Unix) {
    [StringComparison]::Ordinal
} else {
    [StringComparison]::OrdinalIgnoreCase
}
$separators = [char[]]@([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
$cutoff = (Get-Date).AddHours(-$MinimumAgeHours)
$artifactClasses = [ordered]@{
    "timestamped-generation-directory" = [pscustomobject]@{ Keep = $KeepDirectoryGenerations; IsRemovable = $true }
    "timestamped-output-file"           = [pscustomobject]@{ Keep = $KeepFileGenerations; IsRemovable = $true }
    "stable-workspace-directory"        = [pscustomobject]@{ Keep = 0; IsRemovable = $false }
    "stable-output-file"                = [pscustomobject]@{ Keep = 0; IsRemovable = $false }
    "reparse-point"                     = [pscustomobject]@{ Keep = 0; IsRemovable = $false }
}

function Format-ArtifactBytes {
    param([long]$Bytes)

    if ($Bytes -ge 1TB) { return "{0:n2} TiB" -f ($Bytes / 1TB) }
    if ($Bytes -ge 1GB) { return "{0:n2} GiB" -f ($Bytes / 1GB) }
    if ($Bytes -ge 1MB) { return "{0:n2} MiB" -f ($Bytes / 1MB) }
    if ($Bytes -ge 1KB) { return "{0:n2} KiB" -f ($Bytes / 1KB) }
    return "$Bytes bytes"
}

function Test-PathWithinRoot {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Root
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd($separators)
    return $fullPath.StartsWith($fullRoot + [IO.Path]::DirectorySeparatorChar, $pathComparison)
}

function Assert-SafeTreePath {
    param([Parameter(Mandatory)][string]$Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not (Test-PathWithinRoot -Path $fullPath -Root $RepositoryRoot)) {
        throw "Scratch path is outside the repository root: $fullPath"
    }

    $relative = $fullPath.Substring($RepositoryRoot.TrimEnd($separators).Length).TrimStart($separators)
    $current = $RepositoryRoot
    foreach ($segment in $relative.Split($separators, [StringSplitOptions]::RemoveEmptyEntries)) {
        $current = Join-Path $current $segment
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Scratch path uses a reparse point: $current"
            }
        }
    }
    return $fullPath
}

function Get-DefaultScratchRoots {
    $parents = @($RepositoryRoot, (Join-Path $RepositoryRoot "android"))
    return @($parents |
            Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
            ForEach-Object {
                Get-ChildItem -LiteralPath $_ -Force -Directory |
                    Where-Object { $_.Name -match '^(temp($|[_-])|build-outputs$)' }
                } |
                Sort-Object FullName -Unique)
}

function Resolve-ScratchRoots {
    $items = if ($Roots) {
        @($Roots | ForEach-Object {
                $candidate = if ([IO.Path]::IsPathRooted($_)) { $_ } else { Join-Path $RepositoryRoot $_ }
                if (-not (Test-Path -LiteralPath $candidate -PathType Container)) {
                    throw "Scratch root not found: $candidate"
                }
                Get-Item -LiteralPath $candidate -Force
            })
    } else {
        @(Get-DefaultScratchRoots)
    }

    $resolved = @()
    foreach ($item in $items | Sort-Object FullName -Unique) {
        $path = Assert-SafeTreePath -Path $item.FullName
        if (-not @($resolved | Where-Object { Test-PathWithinRoot -Path $path -Root $_ }).Count) {
            $resolved += $path
        }
    }
    return $resolved
}

function Get-TimestampIdentity {
    param([Parameter(Mandatory)][string]$Name)

    # These are the timestamp forms emitted by current PowerShell producers
    $formats = @(
        [pscustomobject]@{ Regex = '(?i)(^|[_.-])(?<stamp>\d{8}_\d{6})(?=$|[_.-])'; Format = 'yyyyMMdd_HHmmss' },
        [pscustomobject]@{ Regex = '(?i)(^|[_.-])(?<stamp>\d{8}-\d{6})(?=$|[_.-])'; Format = 'yyyyMMdd-HHmmss' },
        [pscustomobject]@{ Regex = '(?i)(^|[_.-])(?<stamp>\d{4}-\d{2}-\d{2})(?=$|[_.-])'; Format = 'yyyy-MM-dd' },
        [pscustomobject]@{ Regex = '(?i)(^|[_.-])(?<stamp>\d{8})(?=$|[_.-])'; Format = 'yyyyMMdd' }
    )
    foreach ($format in $formats) {
        $match = [regex]::Match($Name, $format.Regex)
        if (-not $match.Success) {
            continue
        }
        $timestamp = [DateTime]::MinValue
        if (-not [DateTime]::TryParseExact(
                $match.Groups['stamp'].Value,
                $format.Format,
                [Globalization.CultureInfo]::InvariantCulture,
                [Globalization.DateTimeStyles]::None,
                [ref]$timestamp
            )) {
            continue
        }
        $stamp = $match.Groups['stamp']
        $template = $Name.Substring(0, $stamp.Index) + '{timestamp}' + $Name.Substring($stamp.Index + $stamp.Length)
        $template = $template -replace '(?i)-v\d+(?=\.)', '-v{version}'
        return [pscustomobject]@{ Timestamp = $timestamp; Template = $template.ToLowerInvariant() }
    }
    return $null
}

function Test-StableTreeBoundary {
    param([Parameter(Mandatory)][System.IO.DirectoryInfo]$Directory)

    if ($Directory.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        return $true
    }
    foreach ($marker in @('.git', '.hg', '.svn', 'CMakeCache.txt', 'build.ninja')) {
        if (Test-Path -LiteralPath (Join-Path $Directory.FullName $marker)) {
            return $true
        }
    }
    return $false
}

function New-ObservedItem {
    param(
        [Parameter(Mandatory)][System.IO.FileSystemInfo]$Item,
        [Parameter(Mandatory)][string]$Root
    )

    $identity = Get-TimestampIdentity -Name $Item.Name
    $isReparsePoint = [bool]($Item.Attributes -band [IO.FileAttributes]::ReparsePoint)
    $class = if ($isReparsePoint) {
        'reparse-point'
    } elseif ($identity -and $Item.PSIsContainer) {
        'timestamped-generation-directory'
    } elseif ($identity) {
        'timestamped-output-file'
    } elseif ($Item.PSIsContainer) {
        'stable-workspace-directory'
    } else {
        'stable-output-file'
    }
    $parent = [IO.Path]::GetDirectoryName($Item.FullName)
    $family = if ($identity) {
        "$class|$($parent.ToLowerInvariant())|$($identity.Template)"
    } else {
        ""
    }
    return [pscustomobject]@{
        Class       = $class
        Family      = $family
        Identity    = $identity
        Item        = $Item
        Parent      = $parent
        Root        = $Root
        IsBoundary  = $Item.PSIsContainer -and (Test-StableTreeBoundary -Directory $Item)
    }
}

function Get-ObservedItems {
    param([Parameter(Mandatory)][string[]]$ScratchRoots)

    $observed = @()
    foreach ($root in $ScratchRoots) {
        foreach ($item in Get-ChildItem -LiteralPath $root -Force) {
            $record = New-ObservedItem -Item $item -Root $root
            $observed += $record
            if ($item.PSIsContainer -and -not $record.Identity -and -not $record.IsBoundary) {
                foreach ($child in Get-ChildItem -LiteralPath $item.FullName -Force) {
                    $observed += New-ObservedItem -Item $child -Root $root
                }
            }
        }
    }
    return $observed
}

function Assert-DirectChildPath {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Parent,
        [Parameter(Mandatory)][string]$Root
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullParent = [IO.Path]::GetFullPath($Parent).TrimEnd($separators)
    $actualParent = [IO.Path]::GetDirectoryName($fullPath).TrimEnd($separators)
    if (-not $actualParent.Equals($fullParent, $pathComparison)) {
        throw "Cleanup target is not a direct child of its discovered parent: $fullPath"
    }
    if (-not (Test-PathWithinRoot -Path $fullPath -Root $Root)) {
        throw "Cleanup target is outside its scratch root: $fullPath"
    }
    return $fullPath
}

function Get-ArtifactStats {
    param([Parameter(Mandatory)][string]$Path)

    $item = Get-Item -LiteralPath $Path -Force
    $latestWrite = $item.LastWriteTime
    $bytes = if ($item.PSIsContainer) { 0L } else { [long]$item.Length }
    $hasReparsePoint = [bool]($item.Attributes -band [IO.FileAttributes]::ReparsePoint)
    if ($item.PSIsContainer -and -not $hasReparsePoint) {
        foreach ($child in Get-ChildItem -LiteralPath $item.FullName -Force -Recurse) {
            if ($child.LastWriteTime -gt $latestWrite) { $latestWrite = $child.LastWriteTime }
            if ($child.Attributes -band [IO.FileAttributes]::ReparsePoint) { $hasReparsePoint = $true }
            if (-not $child.PSIsContainer) { $bytes += [long]$child.Length }
        }
    }
    return [pscustomobject]@{ Bytes = $bytes; LatestWriteTime = $latestWrite; HasReparsePoint = $hasReparsePoint }
}

$scratchRoots = @(Resolve-ScratchRoots)
$observed = @(Get-ObservedItems -ScratchRoots $scratchRoots)
$kept = @()
$candidates = @()
foreach ($className in @('timestamped-generation-directory', 'timestamped-output-file')) {
    $keepCount = $artifactClasses[$className].Keep
    foreach ($family in @($observed | Where-Object Class -eq $className | Group-Object Family)) {
        $sorted = @($family.Group | Sort-Object @{ Expression = { $_.Identity.Timestamp }; Descending = $true }, @{ Expression = { $_.Item.LastWriteTime }; Descending = $true })
        $kept += @($sorted | Select-Object -First $keepCount)
        $candidates += @($sorted | Select-Object -Skip $keepCount)
    }
}

$eligible = @()
$protected = @()
foreach ($record in $candidates) {
    $path = Assert-DirectChildPath -Path $record.Item.FullName -Parent $record.Parent -Root $record.Root
    $stats = Get-ArtifactStats -Path $path
    $candidate = [pscustomobject]@{
        Class           = $record.Class
        Family          = $record.Family
        Path            = $path
        Parent          = $record.Parent
        Root            = $record.Root
        IsDirectory     = [bool]$record.Item.PSIsContainer
        Bytes           = $stats.Bytes
        LatestWriteTime = $stats.LatestWriteTime
        Reason          = ""
    }
    if ($stats.HasReparsePoint) {
        $candidate.Reason = 'contains a reparse point'
        $protected += $candidate
    } elseif ($stats.LatestWriteTime -gt $cutoff) {
        $candidate.Reason = "modified after $($cutoff.ToString('s'))"
        $protected += $candidate
    } else {
        $eligible += $candidate
    }
}

Write-Output "Old artifact cleanup $(if ($apply) { 'apply' } else { 'preview' })"
Write-Output "Scratch roots: $($scratchRoots.Count)"
foreach ($root in $scratchRoots) { Write-Verbose "ROOT $root" }
foreach ($className in $artifactClasses.Keys) {
    $found = @($observed | Where-Object Class -eq $className)
    $familyCount = @($found | Where-Object Family | Select-Object -ExpandProperty Family -Unique).Count
    $classKept = @($kept | Where-Object Class -eq $className).Count
    $classEligible = @($eligible | Where-Object Class -eq $className)
    $classProtected = @($protected | Where-Object Class -eq $className).Count
    $bytes = 0L
    if ($classEligible.Count -gt 0) {
        $bytes = [long](($classEligible | Measure-Object Bytes -Sum).Sum)
    }
    Write-Output ("{0}: found {1}, families {2}, kept {3}, eligible {4} ({5}), protected {6}" -f $className, $found.Count, $familyCount, $classKept, $classEligible.Count, (Format-ArtifactBytes $bytes), $classProtected)
}
foreach ($record in $eligible) { Write-Verbose "$(if ($apply) { 'REMOVE' } else { 'WOULD REMOVE' }) [$($record.Class)] $($record.Path)" }
foreach ($record in $protected) { Write-Warning "PROTECTED [$($record.Class)] $($record.Path): $($record.Reason)" }

$totalBytes = 0L
if ($eligible.Count -gt 0) {
    $totalBytes = [long](($eligible | Measure-Object Bytes -Sum).Sum)
}
Write-Output "Eligible: $($eligible.Count) item(s), $(Format-ArtifactBytes $totalBytes)"
if (-not $apply) {
    Write-Output "Preview only: rerun with -apply to remove eligible items"
    exit 0
}

$removedCount = 0
$removedBytes = 0L
foreach ($record in $eligible) {
    if (-not (Test-Path -LiteralPath $record.Path)) { continue }
    [void](Assert-SafeTreePath -Path $record.Root)
    $path = Assert-DirectChildPath -Path $record.Path -Parent $record.Parent -Root $record.Root
    $currentStats = Get-ArtifactStats -Path $path
    if ($currentStats.HasReparsePoint -or $currentStats.LatestWriteTime -gt $cutoff) {
        Write-Warning "Changed since discovery; preserving $path"
        continue
    }
    if ($PSCmdlet.ShouldProcess($path, "Remove old $($record.Class)")) {
        if ($record.IsDirectory) {
            Remove-Item -LiteralPath $path -Recurse -Force
        } else {
            Remove-Item -LiteralPath $path -Force
        }
        $removedCount++
        $removedBytes += $record.Bytes
    }
}
Write-Output "Removed $removedCount item(s), $(Format-ArtifactBytes $removedBytes)"

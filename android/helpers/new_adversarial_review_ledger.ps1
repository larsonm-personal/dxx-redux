#!/usr/bin/env pwsh
# new_adversarial_review_ledger.ps1 -- Create a deterministic branch review queue and ledger
# Usage:
#   .\android\helpers\new_adversarial_review_ledger.ps1
#   .\android\helpers\new_adversarial_review_ledger.ps1 -BaseRef upstream/main -HeadRef HEAD

[CmdletBinding()]
param(
    [string]$BaseRef = "upstream/main",
    [string]$HeadRef = "HEAD",
    [ValidatePattern('^[A-Za-z][A-Za-z0-9_-]{0,15}$')]
    [string]$CampaignId = "R1",
    [string]$OutputPath = "android/ai tool plans/code management/branch_adversarial_review_ledger.md",
    [ValidateRange(100, 5000)]
    [int]$SourceLinesPerChunk = 900,
    [ValidateRange(100, 5000)]
    [int]$TestLinesPerChunk = 1100,
    [ValidateRange(5, 200)]
    [int]$BatchPathsPerChunk = 50,
    [switch]$Overwrite
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Invoke-GitLines {
    param([Parameter(Mandatory)][string[]]$Arguments)

    $result = @(& git -C $repoRoot @Arguments)
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
    }
    return $result
}

function Resolve-GitCommit {
    param([Parameter(Mandatory)][string]$Ref)

    $resolved = @(Invoke-GitLines -Arguments @("rev-parse", "--verify", "$Ref^{commit}"))
    if ($resolved.Count -ne 1 -or $resolved[0] -notmatch '^[0-9a-f]{40}$') {
        throw "Could not resolve '$Ref' to one commit"
    }
    return $resolved[0]
}

function Get-ReviewKind {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][bool]$IsBinary
    )

    $normalized = $Path.Replace('\', '/')
    $leaf = [IO.Path]::GetFileName($normalized)
    $extension = [IO.Path]::GetExtension($leaf).ToLowerInvariant()

    if ($IsBinary -or $extension -in @(
            ".png", ".jpg", ".jpeg", ".gif", ".webp", ".ico", ".bin", ".hog",
            ".obj", ".exe", ".dll", ".so", ".a", ".db", ".db-shm", ".db-wal", ".tmp"
        )) {
        return "artifact"
    }
    if ($normalized -like "android/ai tool plans/*") {
        return "historical-plan"
    }
    if ($normalized -like "game_data/mission_files/*.json" -or
        $normalized -like "android/test_fixtures/*.json" -or
        $normalized -like "android/tests/fixtures/*.json" -or
        $normalized -eq "game_data/test_data_manifest.json") {
        return "generated-fixture"
    }
    if ($extension -eq ".lock" -or $leaf -in @("go.sum", "package-lock.json", "yarn.lock")) {
        return "dependency-lock"
    }
    if ($normalized -match '(?i)(^|/)(debuglog|test_output|build_output)[^/]*\.txt$' -or
        $extension -eq ".log") {
        return "artifact"
    }

    $sourceExtensions = @(
        ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".java", ".kt", ".kts",
        ".rs", ".ps1", ".psm1", ".psd1", ".sh", ".bash", ".py", ".cmake", ".gradle",
        ".xml", ".yml", ".yaml", ".toml", ".properties", ".bat"
    )
    $specialSourceNames = @(
        "CMakeLists.txt", "Makefile", ".gitignore", ".gitattributes", ".editorconfig",
        ".clang-format", ".cmake-format.yaml"
    )
    if ($extension -in $sourceExtensions -or $leaf -in $specialSourceNames) {
        if ($normalized -match '(?i)(^|/)(test|tests|test_fixtures|game_scripts)(/|_)' -or
            $leaf -match '(?i)(^test_|_test\.)') {
            return "test-source"
        }
        if ($extension -in @(".ps1", ".psm1", ".psd1", ".sh", ".bash", ".py", ".cmake", ".gradle", ".kts", ".bat") -or
            $leaf -in $specialSourceNames -or
            $normalized -match '(?i)(^|/)(cmake|helpers|get_deps|docker)(/|$)') {
            return "build-script"
        }
        return "authored-source"
    }
    if ($extension -in @(".json", ".jsonc", ".conf", ".ini", ".cfg", ".template", ".default", ".example")) {
        if ($normalized -match '(?i)(^|/)(test|tests|game_scripts)(/|_)') {
            return "test-source"
        }
        return "authored-config"
    }
    if ($extension -in @(".md", ".txt") -or $leaf -in @("README", "COPYING")) {
        return "documentation"
    }
    return "other-data"
}

function Get-ReviewRisk {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Kind
    )

    if ($Kind -in @("artifact", "historical-plan", "generated-fixture", "dependency-lock", "other-data")) {
        return "mechanical"
    }
    if ($Path -like "server/src/*" -or
        $Path -match '(?i)(jni|socket|relay|auth|credential|archive|extract|zip|cue|iso|parser)') {
        return "critical"
    }
    if ($Path -like "d1/*" -or $Path -like "d2/*" -or
        $Path -like "android/app/src/main/cpp/*" -or $Path -like "server/*" -or
        $Path -match '(?i)(network|storage|filesystem|file_import|playsave|serialize|protocol|database)') {
        return "high"
    }
    if ($Kind -in @("authored-source", "authored-config", "build-script")) {
        return "medium"
    }
    return "low"
}

function Get-ChunkLimit {
    param(
        [Parameter(Mandatory)][string]$Kind,
        [Parameter(Mandatory)][string]$Risk
    )

    if ($Risk -eq "critical") {
        return [Math]::Min($SourceLinesPerChunk, 600)
    }
    if ($Risk -eq "high") {
        return [Math]::Min($SourceLinesPerChunk, 750)
    }
    if ($Kind -in @("test-source", "documentation")) {
        return $TestLinesPerChunk
    }
    return $SourceLinesPerChunk
}

function Get-ReviewBucket {
    param([Parameter(Mandatory)][string]$Path)

    $parts = @($Path.Replace('\', '/') -split '/')
    if ($parts.Count -le 1) {
        return "[root]"
    }
    $depth = if ($parts[0] -eq "android" -and $parts.Count -ge 5 -and $parts[1] -eq "app") {
        5
    } elseif ($parts[0] -eq "game_data" -and $parts.Count -ge 2 -and $parts[1] -eq "CD images") {
        2
    } else {
        [Math]::Min(2, $parts.Count - 1)
    }
    return ($parts[0..($depth - 1)] -join '/')
}

function Add-SourcePack {
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][Collections.Generic.List[object]]$Destination,
        [Parameter(Mandatory)][object[]]$Members,
        [Parameter(Mandatory)][string]$Bucket
    )

    if ($Members.Count -eq 1) {
        $Destination.Add($Members[0])
        return
    }
    $weight = ($Members | Measure-Object Weight -Sum).Sum
    $details = @($Members | ForEach-Object { "$($_.Path)`t$($_.Scope)" }) -join "`n"
    $Destination.Add([pscustomobject]@{
            Phase   = "source"
            Risk    = $Members[0].Risk
            Kind    = $Members[0].Kind
            Path    = "$($Members.Count) related paths"
            Scope   = "$weight review lines under $Bucket"
            Weight  = $weight
            Details = $details
            Bucket  = $Bucket
            Limit   = $Members[0].Limit
            Order   = $Members[0].Order
        })
}

function Get-AllDiffHunks {
    param(
        [Parameter(Mandatory)][string]$BaseCommit,
        [Parameter(Mandatory)][string]$HeadCommit
    )

    $diff = @(Invoke-GitLines -Arguments @(
            "-c", "core.quotePath=false", "diff", "--unified=0", "--no-color",
            "--diff-filter=MDRC", $BaseCommit, $HeadCommit, "--"
        ))
    $hunksByPath = @{}
    $currentPath = ""
    $ordinal = 0
    foreach ($line in $diff) {
        if ($line -match '^diff --git ') {
            $currentPath = ""
            $ordinal = 0
            continue
        }
        if ($line -match '^\+\+\+ b/(?<Path>.+)$') {
            $currentPath = $Matches["Path"]
            if (-not $hunksByPath.ContainsKey($currentPath)) {
                $hunksByPath[$currentPath] = [Collections.Generic.List[object]]::new()
            }
            continue
        }
        if ($line -match '^--- a/(?<Path>.+)$' -and -not $currentPath) {
            $currentPath = $Matches["Path"]
            if (-not $hunksByPath.ContainsKey($currentPath)) {
                $hunksByPath[$currentPath] = [Collections.Generic.List[object]]::new()
            }
            continue
        }
        if ($line -notmatch '^@@ -(?<OldStart>\d+)(?:,(?<OldCount>\d+))? \+(?<NewStart>\d+)(?:,(?<NewCount>\d+))? @@') {
            continue
        }
        if (-not $currentPath) {
            throw "Found a diff hunk before its path header"
        }
        $ordinal++
        $oldCount = if ($Matches.ContainsKey("OldCount") -and $Matches["OldCount"] -ne "") {
            [int]$Matches["OldCount"]
        } else {
            1
        }
        $newCount = if ($Matches.ContainsKey("NewCount") -and $Matches["NewCount"] -ne "") {
            [int]$Matches["NewCount"]
        } else {
            1
        }
        $hunksByPath[$currentPath].Add([pscustomobject]@{
                Ordinal  = $ordinal
                OldStart = [int]$Matches.OldStart
                OldCount = $oldCount
                NewStart = [int]$Matches.NewStart
                NewCount = $newCount
                Weight   = [Math]::Max(1, $oldCount + $newCount)
            })
    }
    return $hunksByPath
}

function Get-ModifiedFileChunks {
    param(
        [Parameter(Mandatory)][object[]]$Hunks,
        [Parameter(Mandatory)][int]$Limit
    )

    if ($Hunks.Count -eq 0) {
        return @([pscustomobject]@{ Scope = "whole diff"; Weight = 1; Order = 1 })
    }

    $groups = [Collections.Generic.List[object]]::new()
    $current = [Collections.Generic.List[object]]::new()
    $currentWeight = 0
    foreach ($hunk in $Hunks) {
        if ($current.Count -gt 0 -and $currentWeight + $hunk.Weight -gt $Limit) {
            $groups.Add([pscustomobject]@{ Hunks = @($current); Weight = $currentWeight })
            $current = [Collections.Generic.List[object]]::new()
            $currentWeight = 0
        }
        $current.Add($hunk)
        $currentWeight += $hunk.Weight
    }
    if ($current.Count -gt 0) {
        $groups.Add([pscustomobject]@{ Hunks = @($current); Weight = $currentWeight })
    }

    return @($groups | ForEach-Object {
            $first = $_.Hunks[0]
            $last = $_.Hunks[-1]
            $newStarts = @($_.Hunks | ForEach-Object { [Math]::Max(1, $_.NewStart) })
            $newEnds = @($_.Hunks | ForEach-Object {
                    [Math]::Max(1, $_.NewStart + [Math]::Max(1, $_.NewCount) - 1)
                })
            [pscustomobject]@{
                Scope  = "diff hunks $($first.Ordinal)-$($last.Ordinal), new L$(($newStarts | Measure-Object -Minimum).Minimum)-L$(($newEnds | Measure-Object -Maximum).Maximum)"
                Weight = $_.Weight
                Order  = $first.Ordinal
            }
        })
}

function Escape-MarkdownCell {
    param([AllowEmptyString()][string]$Value)

    return $Value.Replace('|', '\|').Replace("`r", ' ').Replace("`n", ' ')
}

$targetTip = Resolve-GitCommit -Ref $BaseRef
$headCommit = Resolve-GitCommit -Ref $HeadRef
$mergeBaseLines = @(Invoke-GitLines -Arguments @("merge-base", $targetTip, $headCommit))
if ($mergeBaseLines.Count -ne 1) {
    throw "Could not find one merge base for '$BaseRef' and '$HeadRef'"
}
$reviewBase = $mergeBaseLines[0]

$statusByPath = @{}
$oldPathByPath = @{}
$statusLines = @(Invoke-GitLines -Arguments @(
        "diff", "--name-status", "--find-renames", $reviewBase, $headCommit, "--"
    ))
foreach ($line in $statusLines) {
    $parts = $line -split "`t"
    if ($parts.Count -lt 2) {
        continue
    }
    $status = $parts[0]
    $path = $parts[-1]
    $statusByPath[$path] = $status
    if ($parts.Count -ge 3) {
        $oldPathByPath[$path] = $parts[-2]
    }
}

$files = [Collections.Generic.List[object]]::new()
$numstatLines = @(Invoke-GitLines -Arguments @("diff", "--numstat", $reviewBase, $headCommit, "--"))
foreach ($line in $numstatLines) {
    $parts = $line -split "`t", 3
    if ($parts.Count -lt 3) {
        continue
    }
    $path = $parts[2]
    $isBinary = $parts[0] -eq "-" -or $parts[1] -eq "-"
    $added = if ($isBinary) { 0 } else { [int64]$parts[0] }
    $deleted = if ($isBinary) { 0 } else { [int64]$parts[1] }
    $kind = Get-ReviewKind -Path $path -IsBinary $isBinary
    $status = if ($statusByPath.ContainsKey($path)) { $statusByPath[$path] } else { "M" }
    $files.Add([pscustomobject]@{
            Path     = $path
            OldPath  = if ($oldPathByPath.ContainsKey($path)) { $oldPathByPath[$path] } else { "" }
            Status   = $status
            Added    = $added
            Deleted  = $deleted
            IsBinary = $isBinary
            Kind     = $kind
            Risk     = Get-ReviewRisk -Path $path -Kind $kind
        })
}

$lineReviewKinds = @("authored-source", "test-source", "build-script", "authored-config", "documentation")
$chunks = [Collections.Generic.List[object]]::new()
$rawSourceChunks = [Collections.Generic.List[object]]::new()
$allDiffHunks = Get-AllDiffHunks -BaseCommit $reviewBase -HeadCommit $headCommit
$lineReviewFiles = @($files | Where-Object { $_.Kind -in $lineReviewKinds } | Sort-Object Path)
foreach ($file in $lineReviewFiles) {
    $limit = Get-ChunkLimit -Kind $file.Kind -Risk $file.Risk
    $bucket = Get-ReviewBucket -Path $file.Path
    if ($file.Status -match '^A') {
        $lineCount = [Math]::Max(1, $file.Added)
        for ($start = 1; $start -le $lineCount; $start += $limit) {
            $end = [Math]::Min($lineCount, $start + $limit - 1)
            $rawSourceChunks.Add([pscustomobject]@{
                    Phase   = "source"
                    Risk    = $file.Risk
                    Kind    = $file.Kind
                    Path    = $file.Path
                    Scope   = "L$start-L$end"
                    Weight  = $end - $start + 1
                    Details = ""
                    Bucket  = $bucket
                    Limit   = $limit
                    Order   = $start
                })
        }
        continue
    }

    $fileHunks = if ($allDiffHunks.ContainsKey($file.Path)) { @($allDiffHunks[$file.Path]) } else { @() }
    $fileChunks = @(Get-ModifiedFileChunks -Hunks $fileHunks -Limit $limit)
    foreach ($fileChunk in $fileChunks) {
        $details = if ($file.OldPath) { "renamed from $($file.OldPath)" } else { "" }
        $rawSourceChunks.Add([pscustomobject]@{
                Phase   = "source"
                Risk    = $file.Risk
                Kind    = $file.Kind
                Path    = $file.Path
                Scope   = $fileChunk.Scope
                Weight  = $fileChunk.Weight
                Details = $details
                Bucket  = $bucket
                Limit   = $limit
                Order   = $fileChunk.Order
            })
    }
}

$sourceGroups = @($rawSourceChunks | Group-Object {
        "$($_.Risk)`t$($_.Kind)`t$($_.Bucket)`t$($_.Limit)"
    } | Sort-Object Name)
foreach ($group in $sourceGroups) {
    $members = @($group.Group | Sort-Object Path, Order)
    $pack = [Collections.Generic.List[object]]::new()
    $packWeight = 0
    foreach ($member in $members) {
        $mustFlush = $pack.Count -gt 0 -and (
            $pack.Count -ge 16 -or
            $packWeight + $member.Weight -gt $member.Limit -or
            $member.Weight -ge [Math]::Floor($member.Limit * 0.8)
        )
        if ($mustFlush) {
            Add-SourcePack -Destination $chunks -Members @($pack) -Bucket $pack[0].Bucket
            $pack = [Collections.Generic.List[object]]::new()
            $packWeight = 0
        }
        if ($member.Weight -ge [Math]::Floor($member.Limit * 0.8)) {
            Add-SourcePack -Destination $chunks -Members @($member) -Bucket $member.Bucket
            continue
        }
        $pack.Add($member)
        $packWeight += $member.Weight
    }
    if ($pack.Count -gt 0) {
        Add-SourcePack -Destination $chunks -Members @($pack) -Bucket $pack[0].Bucket
    }
}

$mechanicalGroups = @($files |
        Where-Object { $_.Kind -notin $lineReviewKinds } |
        Sort-Object Kind, Path |
        Group-Object Kind)
foreach ($group in $mechanicalGroups) {
    $groupFiles = @($group.Group | Sort-Object Path)
    for ($offset = 0; $offset -lt $groupFiles.Count; $offset += $BatchPathsPerChunk) {
        $batch = @($groupFiles | Select-Object -Skip $offset -First $BatchPathsPerChunk)
        $chunks.Add([pscustomobject]@{
                Phase   = "mechanical"
                Risk    = "mechanical"
                Kind    = $group.Name
                Path    = "$($batch.Count) paths"
                Scope   = "batch $([int]($offset / $BatchPathsPerChunk) + 1)"
                Weight  = ($batch | Measure-Object Added -Sum).Sum + ($batch | Measure-Object Deleted -Sum).Sum
                Details = ($batch.Path -join "`n")
                Bucket  = $group.Name
                Order   = $offset
            })
    }
}

$riskOrder = @{ critical = 0; high = 1; medium = 2; low = 3; mechanical = 4 }
$orderedChunks = @($chunks | Sort-Object @{ Expression = { $riskOrder[$_.Risk] } }, Kind, Bucket, Path, Order)

$preflight = @(
    [pscustomobject]@{ Id = "$CampaignId-PREFLIGHT-001"; Phase = "preflight"; Risk = "critical"; Kind = "pr-scope"; Path = "complete diff"; Scope = "PR composition, provenance, generated artifacts, secrets, licenses, and split strategy" },
    [pscustomobject]@{ Id = "$CampaignId-PREFLIGHT-002"; Phase = "preflight"; Risk = "high"; Kind = "architecture"; Path = "complete diff"; Scope = "Subsystem map, intended behavior, ownership boundaries, and highest-risk data flows" },
    [pscustomobject]@{ Id = "$CampaignId-PREFLIGHT-003"; Phase = "preflight"; Risk = "high"; Kind = "change-history"; Path = "complete diff"; Scope = "Commit clusters, superseded approaches, partial migrations, and abandoned compatibility paths" }
)

$sweeps = @(
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-001"; Phase = "sweep"; Risk = "high"; Kind = "d1-d2"; Path = "d1/ and d2/"; Scope = "Parity, minimal upstream edits, platform guards, and shared-new-code boundaries" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-002"; Phase = "sweep"; Risk = "critical"; Kind = "native-boundary"; Path = "Android JNI and native code"; Scope = "Ownership, lifetimes, thread attachment, references, bounds, and exception paths" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-003"; Phase = "sweep"; Risk = "high"; Kind = "android-lifecycle"; Path = "Android Kotlin and Java"; Scope = "Activity lifecycle, state restoration, cancellation, permissions, backgrounding, and touch-only operation" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-004"; Phase = "sweep"; Risk = "critical"; Kind = "files-data"; Path = "Import, archive, storage, config, and save paths"; Scope = "Trust boundaries, traversal, size limits, transactions, schema ownership, and C source of truth" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-005"; Phase = "sweep"; Risk = "critical"; Kind = "server-network"; Path = "server/ and multiplayer clients"; Scope = "Protocol validation, abuse cases, rate and size limits, timeouts, cleanup, deadlocks, and compatibility" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-006"; Phase = "sweep"; Risk = "high"; Kind = "concurrency-resources"; Path = "complete diff"; Scope = "Threads, locks, cancellation, handles, memory, GL resources, and lifecycle cleanup" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-007"; Phase = "sweep"; Risk = "high"; Kind = "build-portability"; Path = "Build, packaging, dependency, and release files"; Scope = "Pinned inputs, host preservation, ABI matrix, paths with spaces, exit codes, and reproducibility" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-008"; Phase = "sweep"; Risk = "high"; Kind = "tests"; Path = "Tests, automation, fixtures, and runners"; Scope = "Meaningful assertions, false passes, cleanup, timeouts, determinism, and missing integration coverage" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-009"; Phase = "sweep"; Risk = "high"; Kind = "determinism"; Path = "Simulation, save, replay, and metadata code"; Scope = "RNG ownership, floating point, serialization completeness, state restore, and demo transparency" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-010"; Phase = "sweep"; Risk = "medium"; Kind = "performance"; Path = "Hot paths and large-data workflows"; Scope = "Per-frame work, allocations, blocking I/O, repeated parsing, caching, and unbounded growth" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-011"; Phase = "sweep"; Risk = "high"; Kind = "errors-logging"; Path = "Complete diff"; Scope = "Fail-safe behavior, cleanup on error, actionable diagnostics, privacy, and Android debug-log routing" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-012"; Phase = "sweep"; Risk = "high"; Kind = "interfaces"; Path = "Cross-language and cross-process interfaces"; Scope = "API, ABI, protocol, schema, duplicated constants, versioning, and compatibility" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-013"; Phase = "sweep"; Risk = "medium"; Kind = "maintainability"; Path = "Complete diff"; Scope = "Confusing names, unnecessary layers, duplication, dead code, stale comments, and simpler alternatives" },
    [pscustomobject]@{ Id = "$CampaignId-SWEEP-014"; Phase = "sweep"; Risk = "high"; Kind = "pr-hygiene"; Path = "Complete diff"; Scope = "Unexpected artifacts, generated files, historical plans, private data, licenses, and reviewability" },
    [pscustomobject]@{ Id = "$CampaignId-CLOSE-001"; Phase = "closure"; Risk = "critical"; Kind = "coverage"; Path = "Ledger and live branch"; Scope = "Reconcile every path and chunk, validate findings, inspect head delta, and produce closure summary" }
)

$numberedChunks = [Collections.Generic.List[object]]::new()
$chunkNumber = 0
foreach ($chunk in $orderedChunks) {
    $chunkNumber++
    $numberedChunks.Add([pscustomobject]@{
            Id      = "$CampaignId-CHUNK-{0:D4}" -f $chunkNumber
            Phase   = $chunk.Phase
            Risk    = $chunk.Risk
            Kind    = $chunk.Kind
            Path    = $chunk.Path
            Scope   = $chunk.Scope
            Weight  = $chunk.Weight
            Details = $chunk.Details
        })
}

$allQueueItems = @($preflight) + @($numberedChunks) + @($sweeps)
$outputFullPath = if ([IO.Path]::IsPathRooted($OutputPath)) {
    [IO.Path]::GetFullPath($OutputPath)
} else {
    [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputPath))
}
if (Test-Path -LiteralPath $outputFullPath) {
    if (-not $Overwrite) {
        throw "Output already exists: $outputFullPath"
    }
}
$outputDirectory = Split-Path -Parent $outputFullPath
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory | Out-Null
}

$kindSummary = @($files | Group-Object Kind | Sort-Object Name | ForEach-Object {
        $added = ($_.Group | Measure-Object Added -Sum).Sum
        $deleted = ($_.Group | Measure-Object Deleted -Sum).Sum
        "| $($_.Name) | $($_.Count) | $added | $deleted |"
    })
$tick = [char]96
$queueLines = @($allQueueItems | ForEach-Object {
        $path = Escape-MarkdownCell -Value $_.Path
        $scope = Escape-MarkdownCell -Value $_.Scope
        "| $($_.Id) | [ ] TODO | $($_.Phase) | $($_.Risk) | $($_.Kind) | ${tick}${path}${tick} | $scope | - |"
    })
$batchDetailLines = [Collections.Generic.List[string]]::new()
foreach ($chunk in $numberedChunks | Where-Object { $_.Details }) {
    $batchDetailLines.Add("### $($chunk.Id)")
    $batchDetailLines.Add("")
    if ($chunk.Kind -eq "historical-plan" -or $chunk.Kind -eq "artifact" -or
        $chunk.Kind -eq "generated-fixture" -or $chunk.Kind -eq "dependency-lock" -or
        $chunk.Kind -eq "other-data") {
        foreach ($path in $chunk.Details -split "`n") {
            $batchDetailLines.Add("- ${tick}${path}${tick}")
        }
    } else {
        foreach ($detail in $chunk.Details -split "`n") {
            $parts = $detail -split "`t", 2
            if ($parts.Count -eq 2) {
                $batchDetailLines.Add("- ${tick}$($parts[0])${tick}: $($parts[1])")
            } else {
                $batchDetailLines.Add("- $detail")
            }
        }
    }
    $batchDetailLines.Add("")
}

$shortBase = $reviewBase.Substring(0, 12)
$shortHead = $headCommit.Substring(0, 12)
$generatedUtc = [DateTime]::UtcNow.ToString("yyyy-MM-dd HH:mm:ss 'UTC'")
$totalAdded = if ($files.Count -gt 0) { ($files | Measure-Object Added -Sum).Sum } else { 0 }
$totalDeleted = if ($files.Count -gt 0) { ($files | Measure-Object Deleted -Sum).Sum } else { 0 }
$ledger = @"
# Adversarial Branch Review Ledger

This is the only canonical file for review progress, findings, dispositions, and closure

## Frozen campaign snapshot

- Generated: $generatedUtc
- Campaign ID: ${tick}${CampaignId}${tick}
- Target ref at generation: ${tick}${BaseRef}${tick} -> ${tick}${targetTip}${tick}
- Review base: ${tick}${reviewBase}${tick}
- Review head: ${tick}${headCommit}${tick}
- Diff command: ${tick}git diff $reviewBase $headCommit${tick}
- Changed paths: $($files.Count)
- Diff lines: +$totalAdded/-$totalDeleted
- Generated review chunks: $($numberedChunks.Count)
- Process: ${tick}android/ai tool plans/code management/branch_adversarial_review_process.md${tick}

Review the frozen commits even if the live branch moves. ${tick}$CampaignId-CLOSE-001$tick must account for every later commit before the campaign can finish

## Status legend

- ${tick}[ ] TODO${tick}: not reviewed
- ${tick}[-] ACTIVE${tick}: claimed by the current call
- ${tick}[x] DONE${tick}: reviewed and recorded, including explicit no-finding results
- ${tick}[x] SKIP${tick}: disposition recorded with a concrete reason and substitute validation
- ${tick}[!] BLOCKED${tick}: cannot be completed without named evidence or authority

Only one call may edit this file at a time. Replace the state in place and put finding IDs or ${tick}none$tick in the Result column

## Inventory summary

| Kind | Paths | Added | Deleted |
|---|---:|---:|---:|
$($kindSummary -join "`n")

## Review queue

| ID | State | Phase | Risk | Kind | Path | Assigned scope | Result |
|---|---|---|---|---|---|---|---|
$($queueLines -join "`n")

## Mechanical batch path lists

$($batchDetailLines -join "`n")
## Chunk completion notes

Append one completion note for every finished queue item using the process template

## Findings

Append findings here in numeric order using the exact template in the process document

## Disposition log

Append a dated entry whenever a finding becomes fixed, dismissed, deferred, or a duplicate. Include evidence and the deciding person or call

## Campaign closure

- [ ] Every queue item is ${tick}DONE$tick or has an approved ${tick}SKIP$tick disposition
- [ ] No queue item remains ${tick}ACTIVE$tick or ${tick}BLOCKED$tick
- [ ] Every P0 and P1 finding received an independent verification call
- [ ] Every finding has a final disposition or an explicitly accepted deferral owner
- [ ] Required build, lint, unit, integration, emulator, and server validations are recorded
- [ ] ${tick}git diff $headCommit..HEAD$tick was reviewed through a delta campaign or proven empty
- [ ] A human maintainer reviewed open risks and AI-generated dispositions
- [ ] The closing summary records counts by severity, category, and disposition
"@

$utf8NoBom = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($outputFullPath, $ledger.Replace("`r`n", "`n") + "`n", $utf8NoBom)

Write-Host "Created adversarial review ledger"
Write-Host "  Output: $outputFullPath"
Write-Host "  Snapshot: $shortBase..$shortHead"
Write-Host "  Paths: $($files.Count)"
Write-Host "  Review chunks: $($numberedChunks.Count)"
Write-Host "  Queue items with preflight and sweeps: $($allQueueItems.Count)"

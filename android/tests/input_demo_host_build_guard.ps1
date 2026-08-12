#!/usr/bin/env pwsh
. (Join-Path (Join-Path (Split-Path $PSScriptRoot -Parent) 'helpers') 'test_host_platform.ps1')

function Get-InputDemoRelativeRepoPath {
    param(
        [string]$RepoRoot,
        [string]$Path
    )

    try {
        return [System.IO.Path]::GetRelativePath($RepoRoot, $Path)
    } catch {
        return $Path
    }
}

function Get-InputDemoBuildTarget {
    param([string]$GameName)

    switch ($GameName) {
        'd1' { return 'd1' }
        'd2' { return 'd2' }
    }

    throw "Unsupported game: $GameName"
}

function Get-InputDemoBuildStampPath {
    param(
        [string]$RepoRoot,
        [string]$GameName
    )

    return Join-RegressionPath $RepoRoot 'temp' 'input_demo_build_stamps' "$GameName.stamp"
}

function Get-InputDemoSourceRevision {
    param([string]$RepoRoot)

    $revision = & git -C $RepoRoot rev-parse HEAD 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $revision) {
        return $null
    }
    return ([string]$revision).Trim()
}

function Test-InputDemoBuildStampRevision {
    param(
        [string]$RepoRoot,
        [string]$GameName,
        [string]$SourceRevision
    )

    $buildStampPath = Get-InputDemoBuildStampPath -RepoRoot $RepoRoot -GameName $GameName
    if (-not (Test-Path -LiteralPath $buildStampPath -PathType Leaf)) {
        return $false
    }
    $stampRevision = ([System.IO.File]::ReadAllText($buildStampPath)).Trim()
    return $stampRevision -and $stampRevision -eq $SourceRevision
}

function Get-InputDemoExecutablePath {
    param(
        [string]$RepoRoot,
        [string]$GameName,
        [switch]$PreferHeadlessConsole
    )

    switch ($GameName) {
        'd1' {
            return Join-RegressionPath $RepoRoot 'buildd1' 'main' ((Get-RegressionHostExecutableNames -BaseName 'd1x-redux')[0])
        }
        'd2' {
            if ($PreferHeadlessConsole) {
                return Join-RegressionPath $RepoRoot 'buildd2' 'main' ((Get-RegressionHostExecutableNames -BaseName 'dxx-redux-d2-headless')[0])
            }
            return Join-RegressionPath $RepoRoot 'buildd2' 'main' ((Get-RegressionHostExecutableNames -BaseName 'd2x-redux')[0])
        }
    }

    throw "Unsupported game: $GameName"
}

function Get-InputDemoRecordedGameName {
    param([string]$DemoPath)

    foreach ($line in [System.IO.File]::ReadLines($DemoPath)) {
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith('//')) {
            continue
        }

        $record = $trimmed | ConvertFrom-Json -AsHashtable
        if ($record.type -ne 'header') {
            throw "Demo file does not start with a header record: $DemoPath"
        }

        $gameName = [string]$record.game
        if ($gameName -notin @('d1', 'd2')) {
            throw "Demo file has unsupported game metadata '$gameName': $DemoPath"
        }
        return $gameName
    }

    throw "Demo file is missing a header record: $DemoPath"
}

function Get-InputDemoFreshnessSourceRoots {
    param(
        [string]$RepoRoot,
        [string]$GameName
    )

    $roots = @(
        (Join-Path $RepoRoot $GameName),
        (Join-Path $RepoRoot 'common'),
        (Join-Path $RepoRoot 'arch'),
        (Join-RegressionPath $RepoRoot 'android' 'app' 'src' 'main' 'cpp' 'shared')
    )

    return $roots | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -Unique
}

function Get-InputDemoLatestSourceFileStamp {
    param(
        [string]$RepoRoot,
        [string]$GameName
    )

    $latestFile = $null
    foreach ($root in (Get-InputDemoFreshnessSourceRoots -RepoRoot $RepoRoot -GameName $GameName)) {
        $candidate = Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Extension -in @('.c', '.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx', '.inl')
            } |
            Sort-Object -Property LastWriteTimeUtc -Descending |
            Select-Object -First 1
        if (-not $candidate) {
            continue
        }
        if (-not $latestFile -or $candidate.LastWriteTimeUtc -gt $latestFile.LastWriteTimeUtc) {
            $latestFile = $candidate
        }
    }

    if (-not $latestFile) {
        return $null
    }

    return @{
        Path = $latestFile.FullName
        TimestampUtc = $latestFile.LastWriteTimeUtc
    }
}

function Invoke-InputDemoHostBuild {
    param(
        [string]$RepoRoot,
        [string]$GameName
    )

    $buildTarget = Get-InputDemoBuildTarget -GameName $GameName
    Write-Host "Build guardrail: rebuilding host target $buildTarget"
    Invoke-RegressionHostBuild -RepoRoot $RepoRoot -Target $buildTarget -Label $GameName
}

function Get-InputDemoExecutableFreshnessIssue {
    param(
        [string]$RepoRoot,
        [string]$GameName,
        [string]$ExecutablePath,
        [string]$Description = 'Executable'
    )

    if (-not (Test-Path -LiteralPath $ExecutablePath)) {
        return @{
            Missing = $true
            SourceRelative = $null
            Message = "Built executable not found: $ExecutablePath"
        }
    }

    $sourceStamp = Get-InputDemoLatestSourceFileStamp -RepoRoot $RepoRoot -GameName $GameName
    if (-not $sourceStamp) {
        return $null
    }

    $exeItem = Get-Item -LiteralPath $ExecutablePath
    $buildStampPath = Get-InputDemoBuildStampPath -RepoRoot $RepoRoot -GameName $GameName
    $sourceRevision = Get-InputDemoSourceRevision -RepoRoot $RepoRoot
    if ($sourceRevision -and
        -not (Test-InputDemoBuildStampRevision -RepoRoot $RepoRoot -GameName $GameName -SourceRevision $sourceRevision)) {
        return @{
            Missing = $false
            SourceRelative = 'source revision'
            Message = "$Description was not built from the current source revision`nExe: $ExecutablePath`nCurrent revision: $sourceRevision`nRun: $(if (Test-RegressionWindowsHost) { '.\\run-windows-build.ps1 -Target ' + $GameName } else { './run-linux-build.sh --target ' + $GameName })"
        }
    }
    $buildStampTimeUtc = if (Test-Path -LiteralPath $buildStampPath -PathType Leaf) {
        (Get-Item -LiteralPath $buildStampPath).LastWriteTimeUtc
    } else {
        [DateTime]::MinValue
    }
    if ($exeItem.LastWriteTimeUtc -ge $sourceStamp.TimestampUtc -or
        $buildStampTimeUtc -ge $sourceStamp.TimestampUtc) {
        return $null
    }

    $sourceRelative = Get-InputDemoRelativeRepoPath -RepoRoot $RepoRoot -Path $sourceStamp.Path
    return @{
        Missing = $false
        SourceRelative = $sourceRelative
        Message = "$Description is older than source files`nExe: $ExecutablePath`nExe time (utc): $($exeItem.LastWriteTimeUtc)`nNewest source: $sourceRelative`nSource time (utc): $($sourceStamp.TimestampUtc)`nRun: $(if (Test-RegressionWindowsHost) { '.\\run-windows-build.ps1 -Target ' + $GameName } else { './run-linux-build.sh --target ' + $GameName })"
    }
}

function Ensure-InputDemoExecutable {
    param(
        [string]$RepoRoot,
        [string]$GameName,
        [string]$ExecutablePath,
        [string]$Description = 'Executable',
        [switch]$BuildBeforeRun,
        [switch]$RequireFreshBuild
    )

    $relativeExecutablePath = Get-InputDemoRelativeRepoPath -RepoRoot $RepoRoot -Path $ExecutablePath

    if ($BuildBeforeRun) {
        Write-Host "Build guardrail: $Description needs rebuild: $relativeExecutablePath (requested by caller)"
        Invoke-InputDemoHostBuild -RepoRoot $RepoRoot -GameName $GameName
        return
    }

    $issue = Get-InputDemoExecutableFreshnessIssue -RepoRoot $RepoRoot -GameName $GameName -ExecutablePath $ExecutablePath -Description $Description
    if (-not $issue) {
        Write-Host "Build guardrail: $Description up to date: $relativeExecutablePath"
        return
    }

    if ($issue.Missing) {
        Write-Host "Build guardrail: $Description needs rebuild: $relativeExecutablePath (missing)"
    } else {
        Write-Host "Build guardrail: $Description needs rebuild: $relativeExecutablePath (older than $($issue.SourceRelative))"
    }

    if ($RequireFreshBuild) {
        throw $issue.Message
    }

    if ($issue.Missing) {
        Write-Host "Auto-rebuild: missing $Description $ExecutablePath"
    } else {
        Write-Host "Auto-rebuild: $Description $ExecutablePath is older than $($issue.SourceRelative)"
    }
    Invoke-InputDemoHostBuild -RepoRoot $RepoRoot -GameName $GameName
}

function Ensure-InputDemoGameBuild {
    param(
        [string]$RepoRoot,
        [string]$GameName,
        [switch]$PreferHeadlessConsole,
        [switch]$BuildBeforeRun,
        [switch]$RequireFreshBuild
    )

    $useHeadlessConsole = $PreferHeadlessConsole -and $GameName -eq 'd2'
    $description = if ($useHeadlessConsole) { 'Headless executable' } else { 'Executable' }
    $executablePath = Get-InputDemoExecutablePath -RepoRoot $RepoRoot -GameName $GameName -PreferHeadlessConsole:$useHeadlessConsole

    Ensure-InputDemoExecutable -RepoRoot $RepoRoot -GameName $GameName -ExecutablePath $executablePath -Description $description -BuildBeforeRun:$BuildBeforeRun -RequireFreshBuild:$RequireFreshBuild
}

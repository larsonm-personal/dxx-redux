#!/usr/bin/env pwsh
param(
    [string]$FixtureListPath = 'android/tests/input_demo_determinism_fixtures.txt',
    [int]$TimeoutSeconds = 300,
    [switch]$IncludeHeadless
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$replayScript = Join-Path $PSScriptRoot 'run_input_demo_replay.ps1'
$headlessScript = Join-Path $PSScriptRoot 'run_input_demo_headless.ps1'
$pwsh = (Get-Process -Id $PID).Path

function Resolve-RepoPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

function Get-RelativeRepoPath {
    param([string]$Path)

    try {
        return [System.IO.Path]::GetRelativePath($repoRoot, $Path)
    } catch {
        return $Path
    }
}

function Read-Fixtures {
    param([string]$ListPath)

    $resolvedListPath = Resolve-RepoPath -Path $ListPath
    if (-not (Test-Path -LiteralPath $resolvedListPath)) {
        throw "Fixture list not found: $resolvedListPath"
    }

    $fixtures = New-Object System.Collections.Generic.List[object]
    $lineNumber = 0
    foreach ($line in [System.IO.File]::ReadLines($resolvedListPath)) {
        $lineNumber++
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith('#')) {
            continue
        }

        $parts = $trimmed -split '\|', 2
        if ($parts.Count -ne 2) {
            throw "Invalid fixture entry at line ${lineNumber}: $trimmed"
        }

        $game = $parts[0].Trim().ToLowerInvariant()
        $path = $parts[1].Trim()
        if ($game -notin @('d1', 'd2')) {
            throw "Invalid fixture game '$game' at line ${lineNumber}"
        }

        $resolvedDemoPath = Resolve-RepoPath -Path $path
        if (-not (Test-Path -LiteralPath $resolvedDemoPath)) {
            throw "Fixture demo not found: $resolvedDemoPath"
        }

        $fixtures.Add([ordered]@{
            Game = $game
            DemoPath = $resolvedDemoPath
            DemoRelative = Get-RelativeRepoPath -Path $resolvedDemoPath
        })
    }

    if ($fixtures.Count -eq 0) {
        throw "No fixtures found in list: $resolvedListPath"
    }

    return ,$fixtures
}

function New-RunVariants {
    param([string]$Game)

    $variants = New-Object System.Collections.Generic.List[object]
    $variants.Add([ordered]@{
        Name = 'windowed-default'
        Script = $replayScript
        Args = @('-Mode', 'accelerated', '-RenderProfile', 'default', '-CompareStateTrace')
    })
    $variants.Add([ordered]@{
        Name = 'windowed-lowres'
        Script = $replayScript
        Args = @('-Mode', 'accelerated', '-RenderProfile', 'lowres-assets', '-CompareStateTrace')
    })
    $variants.Add([ordered]@{
        Name = 'windowed-norender'
        Script = $replayScript
        Args = @('-Mode', 'accelerated', '-NoRender', '-CompareStateTrace', '-AllowMissingActualResult')
    })

    if ($IncludeHeadless -and $Game -eq 'd2') {
        $variants.Add([ordered]@{
            Name = 'headless-console'
            Script = $headlessScript
            Args = @('-Mode', 'accelerated', '-CompareStateTrace')
        })
    }

    return ,$variants
}

function Get-FirstMismatchLine {
    param([string]$LogPath)

    foreach ($line in [System.IO.File]::ReadLines($LogPath)) {
        if ($line -match '^frame=\d+') {
            return $line.Trim()
        }
        if ($line -match '^result\.') {
            return $line.Trim()
        }
    }
    return ''
}

function Get-StageFromMismatchLine {
    param([string]$MismatchLine)

    if (-not $MismatchLine) {
        return ''
    }
    if ($MismatchLine -match 'stage=([^\s]+)') {
        return $matches[1]
    }
    if ($MismatchLine -match '^frame=\d+\s+missing from actual trace') {
        return 'missing_trace'
    }
    if ($MismatchLine -match '^frame=\d+') {
        return 'state_trace'
    }
    if ($MismatchLine -match '^result\.') {
        return 'final_result'
    }
    return ''
}

$fixtures = Read-Fixtures -ListPath $FixtureListPath

$outDir = Join-Path $repoRoot 'temp\input_demo_determinism_matrix'
if (-not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$runDir = Join-Path $outDir $timestamp
New-Item -ItemType Directory -Path $runDir -Force | Out-Null

$results = New-Object System.Collections.Generic.List[object]

foreach ($fixture in $fixtures) {
    $variants = New-RunVariants -Game $fixture.Game
    foreach ($variant in $variants) {
        $safeFixture = ([System.IO.Path]::GetFileNameWithoutExtension($fixture.DemoPath) -replace '[^A-Za-z0-9_.-]', '_')
        $safeVariant = ($variant.Name -replace '[^A-Za-z0-9_.-]', '_')
        $logPath = Join-Path $runDir ("{0}__{1}.log" -f $safeFixture, $safeVariant)
        $statePath = Join-Path $runDir ("{0}__{1}.actual_state.jsonl" -f $safeFixture, $safeVariant)

        $arguments = @(
            '-NoProfile',
            '-ExecutionPolicy', 'Bypass',
            '-File', $variant.Script,
            '-DemoPath', $fixture.DemoPath,
            '-Game', $fixture.Game,
            '-TimeoutSeconds', [string]$TimeoutSeconds,
            '-StateLogPath', $statePath
        )
        $arguments += $variant.Args

        Write-Host ("RUN fixture={0} variant={1}" -f $fixture.DemoRelative, $variant.Name)

        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        & $pwsh @arguments *> $logPath
        $exitCode = $LASTEXITCODE
        $sw.Stop()

        $firstMismatch = Get-FirstMismatchLine -LogPath $logPath
        $firstStage = Get-StageFromMismatchLine -MismatchLine $firstMismatch

        $results.Add([ordered]@{
            fixture = $fixture.DemoRelative
            game = $fixture.Game
            variant = $variant.Name
            status = if ($exitCode -eq 0) { 'PASS' } else { 'FAIL' }
            exit_code = $exitCode
            duration_sec = [Math]::Round($sw.Elapsed.TotalSeconds, 3)
            first_stage = $firstStage
            first_mismatch = $firstMismatch
            log = Get-RelativeRepoPath -Path $logPath
            state_trace = Get-RelativeRepoPath -Path $statePath
        })
    }
}

$csvPath = Join-Path $runDir 'matrix_results.csv'
$results | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding ascii

Write-Host ''
Write-Host ('Wrote matrix results: {0}' -f (Get-RelativeRepoPath -Path $csvPath))
Write-Host ('Run logs dir: {0}' -f (Get-RelativeRepoPath -Path $runDir))
Write-Host ''
$results | Select-Object fixture, game, variant, status, exit_code, first_stage, duration_sec | Format-Table -AutoSize | Out-String | Write-Host

$failCount = ($results | Where-Object { $_.status -ne 'PASS' }).Count
if ($failCount -gt 0) {
    exit 1
}

exit 0

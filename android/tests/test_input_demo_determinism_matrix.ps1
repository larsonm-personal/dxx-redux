#!/usr/bin/env pwsh
param(
    [string]$FixtureListPath = 'android/tests/input_demo_determinism_fixtures.txt',
    [int]$TimeoutSeconds = 300,
    [switch]$IncludeHeadless,
    [switch]$NoOracle
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'input_demo_host_build_guard.ps1')
$replayScript = Join-Path $PSScriptRoot 'run_input_demo_replay.ps1'
$headlessScript = Join-Path $PSScriptRoot 'run_input_demo_headless.ps1'
$compareTraceScript = Join-Path $PSScriptRoot 'compare_input_demo_state_trace.ps1'
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

        $fixtures.Add([pscustomobject][ordered]@{
                Game = $game
                DemoPath = $resolvedDemoPath
                DemoRelative = Get-RelativeRepoPath -Path $resolvedDemoPath
            })
    }

    if ($fixtures.Count -eq 0) {
        throw "No fixtures found in list: $resolvedListPath"
    }

    return , $fixtures
}

function New-RunVariants {
    param([string]$Game)

    $variants = New-Object System.Collections.Generic.List[object]
    $defaultReplayArgs = @('-Mode', 'accelerated')
    if ($NoOracle) {
        $defaultReplayArgs += '-SkipExpectedChecks'
    } else {
        $defaultReplayArgs += '-CompareStateTrace'
    }

    $variants.Add([ordered]@{
            Name = 'windowed-default'
            Script = $replayScript
            Args = @($defaultReplayArgs + @('-RenderProfile', 'default'))
        })
    $variants.Add([ordered]@{
            Name = 'windowed-lowres'
            Script = $replayScript
            Args = @($defaultReplayArgs + @('-RenderProfile', 'lowres-assets'))
        })
    $variants.Add([ordered]@{
            Name = 'windowed-norender'
            Script = $replayScript
            Args = if ($NoOracle) {
                @('-Mode', 'accelerated', '-NoRender', '-AllowMissingActualResult', '-SkipExpectedChecks')
            } else {
                @('-Mode', 'accelerated', '-NoRender', '-CompareStateTrace', '-AllowMissingActualResult')
            }
        })

    if ($IncludeHeadless -and $Game -eq 'd2') {
        $variants.Add([ordered]@{
                Name = 'headless-console'
                Script = $headlessScript
                Args = if ($NoOracle) {
                    @('-Mode', 'accelerated', '-SkipExpectedChecks')
                } else {
                    @('-Mode', 'accelerated', '-CompareStateTrace')
                }
            })
    }

    return , $variants
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
$fixtureGames = @($fixtures | Select-Object -ExpandProperty Game -Unique)
foreach ($fixtureGame in $fixtureGames) {
    Ensure-InputDemoGameBuild -RepoRoot $repoRoot -GameName $fixtureGame
}
if ($IncludeHeadless -and $fixtureGames -contains 'd2') {
    Ensure-InputDemoGameBuild -RepoRoot $repoRoot -GameName 'd2' -PreferHeadlessConsole
}

$outDir = Join-Path $repoRoot 'temp\input_demo_determinism_matrix'
if (-not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$runDir = Join-Path $outDir $timestamp
New-Item -ItemType Directory -Path $runDir -Force | Out-Null

$results = New-Object System.Collections.Generic.List[object]
$stateTraceByKey = @{}

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

        $results.Add([pscustomobject][ordered]@{
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

        $stateTraceByKey["$($fixture.DemoRelative)|$($variant.Name)"] = $statePath
    }

    if ($NoOracle) {
        $fixtureResults = @($results | Where-Object { $_.fixture -eq $fixture.DemoRelative })
        if ($fixtureResults.Count -gt 0) {
            $baseline = $fixtureResults | Where-Object { $_.variant -eq 'windowed-norender' } | Select-Object -First 1
            if (-not $baseline) {
                $baseline = $fixtureResults | Where-Object { $_.variant -eq 'headless-console' } | Select-Object -First 1
            }
            if (-not $baseline) {
                $baseline = $fixtureResults[0]
            }

            $baselineKey = "$($baseline.fixture)|$($baseline.variant)"
            $baselineStatePath = $stateTraceByKey[$baselineKey]

            foreach ($row in $fixtureResults) {
                if ($row.variant -eq $baseline.variant) {
                    $row.status = 'PASS'
                    $row.exit_code = 0
                    $row.first_stage = ''
                    $row.first_mismatch = ''
                    continue
                }

                $rowKey = "$($row.fixture)|$($row.variant)"
                $rowStatePath = $stateTraceByKey[$rowKey]

                $compareOutput = & $pwsh -NoProfile -ExecutionPolicy Bypass -File $compareTraceScript -ExpectedPath $baselineStatePath -ActualPath $rowStatePath -CompareFrameMetadata 2>&1
                $compareExitCode = $LASTEXITCODE

                $firstMismatch = ''
                foreach ($line in $compareOutput) {
                    $text = [string]$line
                    if (-not $firstMismatch -and $text -match '^frame=') {
                        $firstMismatch = $text.Trim()
                    }
                }

                $row.status = if ($compareExitCode -eq 0) { 'PASS' } else { 'FAIL' }
                $row.exit_code = $compareExitCode
                $row.first_mismatch = $firstMismatch
                $row.first_stage = Get-StageFromMismatchLine -MismatchLine $firstMismatch
            }
        }
    }
}

$csvPath = Join-Path $runDir 'matrix_results.csv'
$results | Export-Csv -LiteralPath $csvPath -Encoding ascii

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

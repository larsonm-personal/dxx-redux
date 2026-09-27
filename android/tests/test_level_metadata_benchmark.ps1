#!/usr/bin/env pwsh
param(
    [switch]$AcceptBaseline,
    [switch]$RequireAssets,
    [switch]$SkipBuild,
    [switch]$PolicySelfTest,
    [switch]$NoHistoryUpdate,
    [switch]$SkipDigestValidation,
    [int]$MeasuredRuns = 0,
    [string[]]$LevelId = @(),
    [string]$D1DataDir,
    [string]$D2DataDir
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$androidRoot = Join-Path $repoRoot 'android'
. (Join-Path $androidRoot 'helpers/test_host_platform.ps1')
. (Join-Path $androidRoot 'helpers/standard_game_data.ps1')
. (Join-Path $androidRoot 'helpers/headless_process_pool.ps1')
. (Join-Path $androidRoot 'helpers/atomic_text_file.ps1')
$manifestPath = Join-Path $androidRoot 'benchmarks\level_metadata_analysis_manifest.jsonc'
$historyPath = Join-Path $androidRoot 'benchmarks\level_metadata_analysis_history.json'
$detailPath = Join-Path $androidRoot 'temp\level_metadata_analysis_current.json'

function Write-NormalizedJson {
    param([Parameter(Mandatory)]$Value, [Parameter(Mandatory)][string]$Path)
    $parent = Split-Path -Parent $Path
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    $json = ($Value | ConvertTo-Json -Depth 100) -replace "`r`n", "`n"
    Write-Utf8NoBomTextAtomically -Path $Path -Text ($json + "`n")
}

function Get-Median {
    param([double[]]$Values)
    $ordered = @($Values | Sort-Object)
    if (-not $ordered.Count) { return 0.0 }
    $middle = [int][Math]::Floor($ordered.Count / 2)
    if (($ordered.Count % 2) -eq 1) { return [double]$ordered[$middle] }
    return ([double]$ordered[$middle - 1] + [double]$ordered[$middle]) / 2.0
}

function Test-SignificantChange {
    param(
        [double]$Before,
        [double]$After,
        [double]$AbsoluteFloor
    )
    if ($Before -le 0) { return $false }
    $absolute = [Math]::Abs($After - $Before)
    $relative = 100.0 * $absolute / $Before
    return $relative -ge 10.0 -and $absolute -ge $AbsoluteFloor
}

function Test-HistoryPolicy {
    if (Test-SignificantChange 10.0 10.9 0.25) { throw '9 percent change was treated as significant' }
    if (Test-SignificantChange 0.2 0.29 0.1) { throw 'small absolute change was treated as significant' }
    if (-not (Test-SignificantChange 10.0 11.1 0.25)) { throw '11 percent aggregate change was missed' }
    if (-not (Test-SignificantChange 1.0 1.11 0.1)) { throw '11 percent phase change was missed' }
    $lfDigest = Get-MetadataDigest "{`n  `"value`": 1`n}`n"
    if ($lfDigest -cne (Get-MetadataDigest "{`r`n  `"value`": 1`r`n}`r`n")) { throw 'Metadata digest depends on host line endings' }
    if ($lfDigest -ceq (Get-MetadataDigest "{`n  `"value`": 2`n}`n")) { throw 'Metadata digest ignored a content change' }
    Write-Host 'Level metadata benchmark policy self-test: PASS'
}

function Get-MetadataDigest {
    param([string]$Text)
    # Existing manifest hashes were recorded from Windows text-mode output
    # Canonicalize only line endings; retain all JSON fields and spacing
    $canonical = $Text.Replace("`r`n", "`n").Replace("`n", "`r`n")
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($canonical)))).Replace('-', '').ToLowerInvariant()
    } finally { $sha.Dispose() }
}

function Invoke-HeadlessBuild {
    if ($SkipBuild) { return }
    Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target both -MaxParallel 2
}

function Resolve-DataDirectory {
    param([string]$Game, [string]$Requested)
    $names = if ($Game -eq 'd1') { @('descent.hog', 'descent.pig') } else { @('descent2.hog', 'descent2.ham', 'groupa.pig') }
    $requirements = @(Get-StandardGameDataDeps | Where-Object file -in $names)
    $candidates = if ($Requested) { @($Requested) } else { @(Get-StandardGameDataCandidates -RepoRoot $repoRoot -Game $Game) }
    try {
        return (Resolve-StandardGameDataDirectory -Candidates $candidates -Dependencies $requirements -Label $Game).Path
    } catch {
        if ($Requested -or $RequireAssets) { throw }
        return $null
    }
}

function Get-EnvironmentIdentity {
    $cpu = if (Test-RegressionWindowsHost) {
        (Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name).Trim()
    } else {
        $line = Select-String -Path '/proc/cpuinfo' -Pattern '^model name\s*:\s*(.+)$' | Select-Object -First 1
        if ($line) { $line.Matches[0].Groups[1].Value.Trim() } else { 'unknown' }
    }
    $compiler = Select-String -LiteralPath (Join-Path $repoRoot 'buildd1\CMakeCache.txt') -Pattern '^CMAKE_CXX_COMPILER:[^=]+=(.+)$' | Select-Object -First 1
    $buildType = Select-String -LiteralPath (Join-Path $repoRoot 'buildd1/CMakeCache.txt') -Pattern '^CMAKE_BUILD_TYPE:STRING=(.*)$' | Select-Object -First 1
    return [ordered]@{
        os = [Environment]::OSVersion.VersionString
        architecture = [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
        cpu = $cpu
        compiler = if ($compiler) { $compiler.Matches[0].Groups[1].Value } else { 'unknown' }
        build = if ($buildType) { $buildType.Matches[0].Groups[1].Value } else { 'unknown' }
    }
}

function Get-EnvironmentKey {
    param($Environment)
    return @($Environment.os, $Environment.architecture, $Environment.cpu, $Environment.compiler, $Environment.build) -join '|'
}

function Invoke-BenchmarkProcess {
    param([string]$Exe, [string[]]$Arguments, [string]$OutputPath, [string]$ErrorPath, [int]$TimeoutSeconds)
    $execution = @{ Result = $null }
    $task = [pscustomobject]@{ FilePath = $Exe; Arguments = $Arguments; WorkingDirectory = $repoRoot; TimeoutSeconds = $TimeoutSeconds }
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        Write-Utf8NoBomTextAtomically -Path $OutputPath -Text $result.StandardOutput
        Write-Utf8NoBomTextAtomically -Path $ErrorPath -Text $result.StandardError
        $execution.Result = $result
    }
    if ($execution.Result.TimedOut -or $execution.Result.ExitCode -ne 0) {
        throw "Benchmark process failed (exit=$($execution.Result.ExitCode), timeout=$($execution.Result.TimedOut)); see $ErrorPath"
    }
}

function Invoke-LevelRun {
    param($Config, [string]$Exe, [string]$DataDir, [string]$MissionDir, [string]$RunDir, [int]$Index)
    $metadataPath = Join-Path $RunDir "$($Config.id)-$Index-metadata.json"
    $timingPath = Join-Path $RunDir "$($Config.id)-$Index-timing.json"
    $arguments = @('-hogdir', $DataDir, '-secretarea-json-out', $metadataPath, '-level', [string]$Config.level, '-analysis-timing-json-out', $timingPath)
    if ($MissionDir) { $arguments += @('-extra-dir', $MissionDir, '-mission', [string]$Config.mission) }
    $outputPath = Join-Path $RunDir "$($Config.id)-$Index-stdout.txt"
    $errorPath = Join-Path $RunDir "$($Config.id)-$Index-stderr.txt"
    Invoke-BenchmarkProcess -Exe $Exe -Arguments $arguments -OutputPath $outputPath -ErrorPath $errorPath -TimeoutSeconds ([int]$manifest.measurement.per_level_timeout_seconds)
    return [pscustomobject]@{
        Digest = Get-MetadataDigest ([IO.File]::ReadAllText($metadataPath))
        Timing = Get-Content -LiteralPath $timingPath -Raw | ConvertFrom-Json
    }
}

Test-HistoryPolicy
if ($PolicySelfTest) { exit 0 }
if ($SkipDigestValidation) { Write-Warning 'Diagnostic measurement: expected metadata digest validation is disabled; repeat determinism is still checked' }

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$levels = @($manifest.levels)
if ($LevelId.Count) {
    foreach ($id in $LevelId) {
        if ($id -cnotin @($levels.id)) { throw "Unknown benchmark level: $id" }
    }
    if ($AcceptBaseline) { throw 'Filtered benchmark runs cannot update full-suite baselines' }
    $levels = @($levels | Where-Object id -cin $LevelId)
    $NoHistoryUpdate = $true
    $detailPath = Join-Path $androidRoot 'temp/level_metadata_analysis_filtered_current.json'
}
# Verify only archives used by the selected levels before building or staging
$archives = @($manifest.archives.psobject.Properties | Where-Object Name -in @($levels.archive | Where-Object { $_ }))
foreach ($property in $archives) {
    $archivePath = Join-Path $repoRoot ([string]$property.Value.path)
    if (-not (Test-Path -LiteralPath $archivePath) -or (Get-FileHash $archivePath -Algorithm SHA256).Hash.ToLowerInvariant() -cne [string]$property.Value.sha256) {
        if ($RequireAssets) { throw "Pinned mission archive missing or changed: $archivePath" }
        Write-Host "RESULT: SKIP (benchmark mission archive unavailable: $archivePath)" -ForegroundColor Yellow
        exit 2
    }
}
if ($MeasuredRuns -le 0) { $MeasuredRuns = [int]$manifest.measurement.measured_runs }
$d1Data = Resolve-DataDirectory -Game d1 -Requested $D1DataDir
$d2Data = Resolve-DataDirectory -Game d2 -Requested $D2DataDir
if (-not $d1Data -or -not $d2Data) {
    if ($RequireAssets) { throw 'Pinned D1/D2 benchmark game data was not found' }
    Write-Host 'RESULT: SKIP (pinned D1/D2 benchmark game data unavailable)' -ForegroundColor Yellow
    exit 2
}

$runDir = Join-Path $androidRoot ('temp/level_metadata_benchmark/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $androidRoot 'helpers/retain-recent-artifacts.ps1') -Artifacts $runDir -DirectoryPrefix run_
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$missionDirs = @{}
$producerLock = $null
$detailLock = $null
try {
    $producerLock = [IO.File]::Open((Join-Path $runDir 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $detailLock = [IO.File]::Open("$detailPath.lock", [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    Write-Host "Benchmark diagnostics: $runDir"
    Invoke-HeadlessBuild
    $executables = @{
        d1 = Join-RegressionPath $repoRoot buildd1 main (Get-RegressionHostExecutableNames -BaseName 'dxx-redux-d1-headless-metadata')[0]
        d2 = Join-RegressionPath $repoRoot buildd2 main (Get-RegressionHostExecutableNames -BaseName 'dxx-redux-d2-headless-metadata')[0]
    }
    foreach ($exe in $executables.Values) {
        if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Benchmark executable missing: $exe" }
    }
    $liveBenchmarkName = if (Test-RegressionWindowsHost) { 'test_guidebot_route_certifier.exe' } else { 'test_guidebot_route_certifier' }
    $liveBenchmarkExe = Join-Path (Join-Path $repoRoot 'buildd2\tests') $liveBenchmarkName
    if (-not (Test-Path -LiteralPath $liveBenchmarkExe -PathType Leaf)) {
        throw "GuideBot live benchmark executable missing: $liveBenchmarkExe"
    }
    $liveOutput = Join-Path $runDir 'guidebot-live.json'
    Invoke-BenchmarkProcess -Exe $liveBenchmarkExe -Arguments @('--benchmark') -OutputPath $liveOutput -ErrorPath (Join-Path $runDir 'guidebot-live-stderr.txt') -TimeoutSeconds 45
    $liveBenchmark = Get-Content -LiteralPath $liveOutput -Raw | ConvertFrom-Json

    foreach ($property in $archives) {
        $archivePath = Join-Path $repoRoot ([string]$property.Value.path)
        $destination = Join-Path $runDir $property.Name
        $missionDirs[$property.Name] = $destination
        $missionsDestination = Join-Path $destination 'missions'
        New-Item -ItemType Directory -Force -Path $missionsDestination | Out-Null
        Expand-Archive -LiteralPath $archivePath -DestinationPath $missionsDestination -Force
    }

    $suiteWatch = [Diagnostics.Stopwatch]::StartNew()
    $levelResults = @()
    foreach ($level in $levels) {
        $game = [string]$level.game
        $dataDir = if ($game -eq 'd1') { $d1Data } else { $d2Data }
        $missionDir = if ($level.archive) { $missionDirs[[string]$level.archive] } else { $null }
        Write-Host "Benchmarking $($level.id)..."
        $runs = @()
        for ($run = 0; $run -lt (1 + $MeasuredRuns); ++$run) {
            $sample = Invoke-LevelRun -Config $level -Exe $executables[$game] -DataDir $dataDir -MissionDir $missionDir -RunDir $runDir -Index $run
            if (-not $SkipDigestValidation -and $level.expected_sha256 -and $sample.Digest -cne [string]$level.expected_sha256) {
                throw "$($level.id) metadata digest changed: expected $($level.expected_sha256), got $($sample.Digest)"
            }
            if ($run -gt 0) { $runs += $sample }
        }
        $digests = @($runs | ForEach-Object Digest | Select-Object -Unique)
        if ($digests.Count -ne 1) { throw "$($level.id) produced nondeterministic metadata digests" }
        $phaseNames = @($runs[0].Timing.phases.psobject.Properties.Name)
        $phaseSummary = [ordered]@{}
        foreach ($phase in $phaseNames) {
            $phaseSummary[$phase] = [ordered]@{
                cpu_seconds = Get-Median @($runs | ForEach-Object { [double]$_.Timing.phases.$phase.cpu_seconds })
                wall_seconds = Get-Median @($runs | ForEach-Object { [double]$_.Timing.phases.$phase.wall_seconds })
                tasks = [int]$runs[0].Timing.phases.$phase.tasks
            }
        }
        $work = [ordered]@{}
        foreach ($name in $runs[0].Timing.work.psobject.Properties.Name) { $work[$name] = $runs[0].Timing.work.$name }
        $levelResults += [ordered]@{
            id = [string]$level.id
            metadata_sha256 = $digests[0]
            median_total_cpu_seconds = Get-Median @($runs | ForEach-Object { [double]$_.Timing.total.cpu_seconds })
            median_total_wall_seconds = Get-Median @($runs | ForEach-Object { [double]$_.Timing.total.wall_seconds })
            phases = $phaseSummary
            work = $work
        }
        if ($suiteWatch.Elapsed.TotalSeconds -gt [int]$manifest.measurement.suite_timeout_seconds) {
            throw "Benchmark exceeded its $($manifest.measurement.suite_timeout_seconds) second suite budget"
        }
    }

    $aggregateCpu = [double](($levelResults | ForEach-Object { [double]$_.median_total_cpu_seconds } | Measure-Object -Sum).Sum)
    $aggregateWall = [double](($levelResults | ForEach-Object { [double]$_.median_total_wall_seconds } | Measure-Object -Sum).Sum)
    $environment = Get-EnvironmentIdentity
    $snapshot = [ordered]@{
        recorded_utc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
        selected_level_ids = @($levels.id)
        complete_suite = ($LevelId.Count -eq 0)
        expected_digest_validation = (-not $SkipDigestValidation)
        metadata_digest_format = 'utf8-crlf-sha256'
        source_revision = (& git -C $repoRoot rev-parse HEAD).Trim()
        environment = $environment
        measured_runs = $MeasuredRuns
        elapsed_wall_seconds = $suiteWatch.Elapsed.TotalSeconds
        aggregate_cpu_seconds = [double]$aggregateCpu
        aggregate_wall_seconds = [double]$aggregateWall
        guidebot_live_calculations = $liveBenchmark
        levels = $levelResults
    }
    Write-NormalizedJson -Value $snapshot -Path $detailPath

    if ($NoHistoryUpdate) {
        Write-Host ("Level metadata benchmark: PASS levels={0} aggregate_cpu={1:N3}s measured_elapsed={2:N3}s detail={3} history=unchanged" -f $levelResults.Count, $aggregateCpu, $suiteWatch.Elapsed.TotalSeconds, $detailPath)
        exit 0
    }

    $history = Get-Content -LiteralPath $historyPath -Raw | ConvertFrom-Json
    $key = Get-EnvironmentKey $environment
    $series = @($history.series | Where-Object { (Get-EnvironmentKey $_.environment) -ceq $key }) | Select-Object -First 1
    if (-not $series) {
        if ($AcceptBaseline) {
            $newSeries = [ordered]@{ environment = $environment; snapshots = @($snapshot) }
            $history.series = @($history.series) + @($newSeries)
            Write-NormalizedJson -Value $history -Path $historyPath
            Write-Host 'Accepted first benchmark baseline for this environment.'
        } else {
            Write-Host 'No comparable baseline for this environment; history was not changed.' -ForegroundColor Yellow
        }
    } else {
        $previous = @($series.snapshots)[-1]
        $significant = Test-SignificantChange ([double]$previous.aggregate_cpu_seconds) ([double]$snapshot.aggregate_cpu_seconds) 0.25
        if (-not $significant) {
            foreach ($currentLevel in $snapshot.levels) {
                $previousLevel = @($previous.levels | Where-Object id -ceq $currentLevel.id) | Select-Object -First 1
                if (-not $previousLevel) { continue }
                foreach ($phaseName in $currentLevel.phases.Keys) {
                    if ($previousLevel.phases.$phaseName -and (Test-SignificantChange ([double]$previousLevel.phases.$phaseName.cpu_seconds) ([double]$currentLevel.phases[$phaseName].cpu_seconds) 0.1)) {
                        $significant = $true
                        break
                    }
                }
                if ($significant) { break }
            }
        }
        if ($significant) {
            $series.snapshots = @($series.snapshots) + @($snapshot)
            Write-NormalizedJson -Value $history -Path $historyPath
            Write-Host 'Benchmark changed significantly; appended a history snapshot.' -ForegroundColor Yellow
        } else {
            Write-Host 'Benchmark is within thresholds; history was not changed.'
        }
    }

    Write-Host ("Level metadata benchmark: PASS levels={0} aggregate_cpu={1:N3}s measured_elapsed={2:N3}s detail={3}" -f $levelResults.Count, $aggregateCpu, $suiteWatch.Elapsed.TotalSeconds, $detailPath)
} finally {
    try {
        foreach ($directory in $missionDirs.Values) {
            if (Test-Path -LiteralPath $directory) { Remove-Item -LiteralPath $directory -Recurse -Force }
        }
    } finally {
        if ($detailLock) { $detailLock.Dispose() }
        if ($producerLock) { $producerLock.Dispose() }
    }
}

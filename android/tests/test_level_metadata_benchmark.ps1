#!/usr/bin/env pwsh
param(
    [switch]$AcceptBaseline,
    [switch]$RequireAssets,
    [switch]$SkipBuild,
    [switch]$PolicySelfTest,
    [switch]$NoHistoryUpdate,
    [int]$MeasuredRuns = 0,
    [string]$D1DataDir,
    [string]$D2DataDir
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$androidRoot = Join-Path $repoRoot 'android'
$manifestPath = Join-Path $androidRoot 'benchmarks\level_metadata_analysis_manifest.jsonc'
$historyPath = Join-Path $androidRoot 'benchmarks\level_metadata_analysis_history.json'
$detailPath = Join-Path $androidRoot 'temp\level_metadata_analysis_current.json'

function Write-NormalizedJson {
    param([Parameter(Mandatory)]$Value, [Parameter(Mandatory)][string]$Path)
    $parent = Split-Path -Parent $Path
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    $json = ($Value | ConvertTo-Json -Depth 100) -replace "`r`n", "`n"
    $temporary = "$Path.tmp-$PID"
    [IO.File]::WriteAllText($temporary, $json + "`n", [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporary -Destination $Path -Force
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
    Write-Host 'Level metadata benchmark policy self-test: PASS'
}

function Invoke-HeadlessBuild {
    if ($SkipBuild) { return }
    $cmakeCache = Join-Path $repoRoot 'buildd1\CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cmakeCache)) { throw "Build tree missing: $cmakeCache" }
    $cmakeLine = Select-String -LiteralPath $cmakeCache -Pattern '^CMAKE_COMMAND:INTERNAL=(.+)$' | Select-Object -First 1
    if (-not $cmakeLine) { throw 'CMAKE_COMMAND not found in buildd1/CMakeCache.txt' }
    $cmake = $cmakeLine.Matches[0].Groups[1].Value
    if ($env:OS -eq 'Windows_NT') {
        $vcvars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat'
        if (-not (Test-Path -LiteralPath $vcvars)) { throw "vcvarsall.bat missing: $vcvars" }
        $command = 'call "' + $vcvars + '" x86 && "' + $cmake + '" --build "' + (Join-Path $repoRoot 'buildd1') + '" --target dxx-redux-d1-headless-metadata && "' + $cmake + '" --build "' + (Join-Path $repoRoot 'buildd2') + '" --target dxx-redux-d2-headless-metadata'
        & cmd.exe /d /c $command | Out-Host
    } else {
        & $cmake --build (Join-Path $repoRoot 'buildd1') --target dxx-redux-d1-headless-metadata | Out-Host
        if ($LASTEXITCODE -eq 0) { & $cmake --build (Join-Path $repoRoot 'buildd2') --target dxx-redux-d2-headless-metadata | Out-Host }
    }
    if ($LASTEXITCODE -ne 0) { throw 'Headless metadata benchmark build failed' }
}

function Resolve-DataDirectory {
    param([string]$Game, [string]$Requested)
    $requirements = if ($Game -eq 'd1') {
        @(
            @{ Name = 'DESCENT.HOG'; Hash = '83d76ff0c46bb2e7348a49bdd287ad764abeda0d851bfb16b42c1ede93b21052' },
            @{ Name = 'DESCENT.PIG'; Hash = '093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe' }
        )
    } else {
        @(
            @{ Name = 'DESCENT2.HOG'; Hash = 'f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703' },
            @{ Name = 'DESCENT2.HAM'; Hash = '5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d' },
            @{ Name = 'GROUPA.PIG'; Hash = 'facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b' }
        )
    }
    $candidates = @($Requested, (Join-Path $repoRoot 'game_data_to_copy_to_emulator\temp'), (Join-Path $repoRoot 'game_data_to_copy_to_emulator\data')) | Where-Object { $_ }
    foreach ($candidate in $candidates) {
        if (-not (Test-Path -LiteralPath $candidate -PathType Container)) { continue }
        $valid = $true
        foreach ($required in $requirements) {
            $file = Get-ChildItem -LiteralPath $candidate -File | Where-Object Name -ieq $required.Name | Select-Object -First 1
            if (-not $file -or (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant() -cne $required.Hash) {
                $valid = $false
                break
            }
        }
        if ($valid) { return (Resolve-Path -LiteralPath $candidate).Path }
    }
    return $null
}

function Get-EnvironmentIdentity {
    $cpu = if ($env:OS -eq 'Windows_NT') {
        (Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name).Trim()
    } else {
        $line = Select-String -Path '/proc/cpuinfo' -Pattern '^model name\s*:\s*(.+)$' | Select-Object -First 1
        if ($line) { $line.Matches[0].Groups[1].Value.Trim() } else { 'unknown' }
    }
    $compiler = Select-String -LiteralPath (Join-Path $repoRoot 'buildd1\CMakeCache.txt') -Pattern '^CMAKE_CXX_COMPILER:[^=]+=(.+)$' | Select-Object -First 1
    return [ordered]@{
        os = [Environment]::OSVersion.VersionString
        architecture = [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
        cpu = $cpu
        compiler = if ($compiler) { $compiler.Matches[0].Groups[1].Value } else { 'unknown' }
        build = 'RelWithDebInfo-x86'
    }
}

function Get-EnvironmentKey {
    param($Environment)
    return @($Environment.os, $Environment.architecture, $Environment.cpu, $Environment.compiler, $Environment.build) -join '|'
}

function Invoke-LevelRun {
    param($Config, [string]$Exe, [string]$DataDir, [string]$MissionDir, [string]$RunDir, [int]$Index)
    $metadataPath = Join-Path $RunDir "$($Config.id)-$Index-metadata.json"
    $timingPath = Join-Path $RunDir "$($Config.id)-$Index-timing.json"
    $arguments = @('-hogdir', $DataDir, '-secretarea-json-out', $metadataPath, '-level', [string]$Config.level, '-analysis-timing-json-out', $timingPath)
    if ($MissionDir) { $arguments += @('-extra-dir', $MissionDir, '-mission', [string]$Config.mission) }
    $outputPath = Join-Path $RunDir "$($Config.id)-$Index-stdout.txt"
    $errorPath = Join-Path $RunDir "$($Config.id)-$Index-stderr.txt"
    $process = Start-Process -FilePath $Exe -ArgumentList $arguments -PassThru -WindowStyle Hidden -RedirectStandardOutput $outputPath -RedirectStandardError $errorPath
    if (-not $process.WaitForExit(45000)) {
        $process.Kill()
        throw "$($Config.id) exceeded its 45 second per-level timeout"
    }
    if ($process.ExitCode -ne 0) {
        throw "$($Config.id) failed with exit $($process.ExitCode): $(Get-Content -LiteralPath $errorPath -Raw)"
    }
    return [pscustomobject]@{
        Digest = (Get-FileHash -LiteralPath $metadataPath -Algorithm SHA256).Hash.ToLowerInvariant()
        Timing = Get-Content -LiteralPath $timingPath -Raw | ConvertFrom-Json
    }
}

Test-HistoryPolicy
if ($PolicySelfTest) { exit 0 }

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($MeasuredRuns -le 0) { $MeasuredRuns = [int]$manifest.measurement.measured_runs }
$d1Data = Resolve-DataDirectory -Game d1 -Requested $D1DataDir
$d2Data = Resolve-DataDirectory -Game d2 -Requested $D2DataDir
if (-not $d1Data -or -not $d2Data) {
    if ($RequireAssets) { throw 'Pinned D1/D2 benchmark game data was not found' }
    Write-Host 'Level metadata benchmark: SKIP (pinned D1/D2 game data unavailable)' -ForegroundColor Yellow
    exit 0
}

Invoke-HeadlessBuild
$executables = @{
    d1 = Join-Path $repoRoot 'buildd1\main\dxx-redux-d1-headless-metadata.exe'
    d2 = Join-Path $repoRoot 'buildd2\main\dxx-redux-d2-headless-metadata.exe'
}
foreach ($exe in $executables.Values) {
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Benchmark executable missing: $exe" }
}

$runDir = Join-Path $androidRoot "temp\level_metadata_benchmark_$PID"
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$missionDirs = @{}
foreach ($property in $manifest.archives.psobject.Properties) {
    $archive = $property.Value
    $archivePath = Join-Path $repoRoot ([string]$archive.path)
    if (-not (Test-Path -LiteralPath $archivePath) -or (Get-FileHash $archivePath -Algorithm SHA256).Hash.ToLowerInvariant() -cne [string]$archive.sha256) {
        if ($RequireAssets) { throw "Pinned mission archive missing or changed: $archivePath" }
        Write-Host "Level metadata benchmark: SKIP (mission archive unavailable: $archivePath)" -ForegroundColor Yellow
        exit 0
    }
    $destination = Join-Path $runDir $property.Name
    $missionsDestination = Join-Path $destination 'missions'
    New-Item -ItemType Directory -Force -Path $missionsDestination | Out-Null
    Expand-Archive -LiteralPath $archivePath -DestinationPath $missionsDestination -Force
    $missionDirs[$property.Name] = $destination
}

$suiteWatch = [Diagnostics.Stopwatch]::StartNew()
$levelResults = @()
foreach ($level in $manifest.levels) {
    $game = [string]$level.game
    $dataDir = if ($game -eq 'd1') { $d1Data } else { $d2Data }
    $missionDir = if ($level.archive) { $missionDirs[[string]$level.archive] } else { $null }
    Write-Host "Benchmarking $($level.id)..."
    $runs = @()
    for ($run = 0; $run -lt (1 + $MeasuredRuns); ++$run) {
        $sample = Invoke-LevelRun -Config $level -Exe $executables[$game] -DataDir $dataDir -MissionDir $missionDir -RunDir $runDir -Index $run
        if ($level.expected_sha256 -and $sample.Digest -cne [string]$level.expected_sha256) {
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
    source_revision = (& git -C $repoRoot rev-parse HEAD).Trim()
    environment = $environment
    measured_runs = $MeasuredRuns
    elapsed_wall_seconds = $suiteWatch.Elapsed.TotalSeconds
    aggregate_cpu_seconds = [double]$aggregateCpu
    aggregate_wall_seconds = [double]$aggregateWall
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

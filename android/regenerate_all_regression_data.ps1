#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Regenerates checked-in regression data by category.

.DESCRIPTION
  Runs the canonical CD regression, fingerprint/AcoustID, and mission metadata
  regeneration wrappers. Zero-parameter interactive runs show a category menu.
  Use -Category for unattended runs. Every selected stage runs even if an
  earlier stage fails, with child output and a JSON summary saved under
  temp\regression_data_reports. Complete all-category runs also write a timing
  report used by later full and partial runs for remaining-time estimates.

.PARAMETER Category
  Menu, All, Cd, Fingerprints, Metadata, MissingMetadata, or Simulation. Menu is the
  interactive default.

.PARAMETER ReportDir
  Directory for durable run artifacts and full-run timing reports.

.PARAMETER Target45Minutes
  Runs the same percentage of each regeneration target type in resumable hash
  order, using prior full-run timings to cross approximately 45 minutes.

.PARAMETER SampleSeed
  Optional seed for reproducing the initial fallback position when no prior
  targeted run cursor can be recovered.

.EXAMPLE
  .\android\regenerate_all_regression_data.ps1
  .\android\regenerate_all_regression_data.ps1 -Category All
  .\android\regenerate_all_regression_data.ps1 -Category Metadata
  .\android\regenerate_all_regression_data.ps1 -Category MissingMetadata
  .\android\regenerate_all_regression_data.ps1 -Target45Minutes
#>

[CmdletBinding()]
param(
    [ValidateSet('Menu', 'All', 'Cd', 'Fingerprints', 'Metadata', 'MissingMetadata', 'Simulation')]
    [string]$Category = 'Menu',
    [ValidateSet('Headless', 'Headed')]
    [string]$SimulationMode = 'Headless',
    [string]$ReportDir,
    [switch]$Target45Minutes,
    [ValidateRange(0, [int]::MaxValue)]
    [int]$SampleSeed = 0
)

$ErrorActionPreference = 'Stop'
$script:RepoRoot = Split-Path $PSScriptRoot -Parent
$script:HelpersDir = Join-Path $PSScriptRoot 'helpers'
. (Join-Path $script:HelpersDir 'test_suite_progress.ps1')
. (Join-Path $script:HelpersDir 'runtime_targeted_sampling.ps1')

function Get-RegressionDataStages {
    param(
        [string]$RepoRoot,
        [ValidateSet('All', 'Cd', 'Fingerprints', 'Metadata', 'MissingMetadata', 'Simulation')]
        [string]$Category = 'All',
        [ValidateSet('Headless', 'Headed')]
        [string]$SimulationMode = 'Headless'
    )

    $stages = @(
        [pscustomobject]@{
            Key = 'Cd'
            Name = 'CD extraction regression data'
            Description = 'Host extraction, oracle validation, Android import, and launch checks'
            Script = Join-Path $RepoRoot 'game_data\run_all_cd_regressions.ps1'
            Arguments = @('-RefreshOracle')
            DefaultEstimatedRuntime = 5558
        },
        [pscustomobject]@{
            Key = 'Fingerprints'
            Name = 'Disc and music fingerprint data'
            Description = 'Disc hashes, CD audio fingerprints, tags, and AcoustID data'
            Script = Join-Path $RepoRoot 'game_data\update_all_fingerprints.ps1'
            Arguments = @('-Force')
            DefaultEstimatedRuntime = 667
        },
        [pscustomobject]@{
            Key = 'Metadata'
            Name = 'Mission level metadata'
            Description = 'Mission archive and extracted-CD level metadata'
            Script = Join-Path $RepoRoot 'android\helpers\regenerate_all_mission_metadata.ps1'
            Arguments = @()
            DefaultEstimatedRuntime = 3312
        },
        [pscustomobject]@{
            Key = 'Simulation'
            Name = 'GuideBot engine route simulations'
            Description = 'Deterministic engine confirmation for mission route metadata'
            Script = Join-Path $RepoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
            Arguments = if ($SimulationMode -eq 'Headed') { @('-Mode', 'Headed') } else { @('-Mode', 'Headless', '-WriteRegression') }
            DefaultEstimatedRuntime = 7200
        }
    )
    if ($Category -eq 'All') {
        return $stages
    }
    if ($Category -eq 'MissingMetadata') {
        $stage = @($stages | Where-Object { $_.Key -eq 'Metadata' })[0]
        $stage.Name = 'Missing mission archive metadata'
        $stage.Description = 'Mission archives without checked-in level metadata JSON'
        $stage.Arguments = @('-MissingOnly')
        return @($stage)
    }
    return @($stages | Where-Object { $_.Key -eq $Category })
}

function Select-RegressionDataCategory {
    Write-Host ''
    Write-Host 'Regression data regeneration' -ForegroundColor Cyan
    Write-Host '  1. Run all categories'
    Write-Host '  2. CD extraction, Android import, and launch regressions'
    Write-Host '  3. Disc and music fingerprints, hashes, tags, and AcoustID data'
    Write-Host '  4. Mission level metadata'
    Write-Host '  5. Missing mission archive metadata only'
    Write-Host '  6. GuideBot engine route simulations'
    Write-Host '  M. Search for and watch one GuideBot route'
    Write-Host '  T. Resumable hash-ring sample targeting 45 minutes'
    Write-Host '  Q. Cancel'
    while ($true) {
        switch ((Read-Host 'Choose a category').Trim().ToLowerInvariant()) {
            '1' { return 'All' }
            'all' { return 'All' }
            '2' { return 'Cd' }
            'cd' { return 'Cd' }
            '3' { return 'Fingerprints' }
            'fingerprints' { return 'Fingerprints' }
            '4' { return 'Metadata' }
            'metadata' { return 'Metadata' }
            '5' { return 'MissingMetadata' }
            'missing' { return 'MissingMetadata' }
            'missingmetadata' { return 'MissingMetadata' }
            '6' { return 'Simulation' }
            'simulation' { return 'Simulation' }
            'm' { return 'ManualSimulation' }
            'manual' { return 'ManualSimulation' }
            't' { return 'Target45' }
            'target' { return 'Target45' }
            'q' { return $null }
            'quit' { return $null }
            default { Write-Host 'Enter 1, 2, 3, 4, 5, 6, M, T, or Q' -ForegroundColor Yellow }
        }
    }
}

function Set-RegressionDataStageEstimates {
    param(
        [Parameter(Mandatory)][object[]]$Stages,
        [Parameter(Mandatory)][string]$ReportDir
    )

    $history = @{}
    $runtimeReader = Join-Path $script:HelpersDir 'get-test-report-runtimes.ps1'
    try {
        # Failed stages often stop early and are not representative runtime
        # samples for target selection
        foreach ($record in @(& $runtimeReader -ReportDir $ReportDir -IncludeStatuses PASS)) {
            $history[$record.Name] = [Math]::Max(1, [int]$record.Seconds)
        }
    } catch {
        Write-Host "WARN: Could not read regression timing history: $_" -ForegroundColor Yellow
    }

    for ($index = 0; $index -lt $Stages.Count; $index++) {
        $stage = $Stages[$index]
        $estimate = if ($history.ContainsKey($stage.Name)) {
            $history[$stage.Name]
        } else {
            [int]$stage.DefaultEstimatedRuntime
        }
        $stage | Add-Member -NotePropertyName ProgressIndex -NotePropertyValue ($index + 1) -Force
        $stage | Add-Member -NotePropertyName EstimatedRuntime -NotePropertyValue $estimate -Force
    }
    return $Stages
}

function Set-RegressionDataTargetSample {
    param(
        [Parameter(Mandatory)][object[]]$Stages,
        [ValidateRange(0, [int]::MaxValue)][int]$Seed = 0,
        [ValidateRange(1, [int]::MaxValue)][int]$TargetSeconds = 2700,
        [string]$StatePath
    )

    if ($Seed -eq 0) { $Seed = Get-Random -Minimum 1 -Maximum ([int]::MaxValue) }
    $fullSeconds = @($Stages | Measure-Object -Property EstimatedRuntime -Sum).Sum
    $fraction = [Math]::Min(1.0, $TargetSeconds / [Math]::Max(1, [double]$fullSeconds))
    $fractionArgument = $fraction.ToString('R', [Globalization.CultureInfo]::InvariantCulture)
    foreach ($stage in $Stages) {
        $stage.Arguments = @($stage.Arguments) + @('-SampleFraction', $fractionArgument, '-SampleSeed', [string]$Seed,
            '-SampleStatePath', $StatePath)
        $stage.EstimatedRuntime = [Math]::Max(1, [int][Math]::Ceiling([double]$stage.EstimatedRuntime * $fraction))
    }
    $estimatedSeconds = @($Stages | Measure-Object -Property EstimatedRuntime -Sum).Sum
    if ($fraction -lt 1.0 -and $estimatedSeconds -le $TargetSeconds) {
        $Stages[-1].EstimatedRuntime += $TargetSeconds + 1 - $estimatedSeconds
        $estimatedSeconds = $TargetSeconds + 1
    }
    return [pscustomobject]@{
        Stages = $Stages
        Seed = $Seed
        Fraction = $fraction
        EstimatedSeconds = $estimatedSeconds
    }
}

function ConvertTo-RegressionArtifactName {
    param([Parameter(Mandatory)][string]$Name)

    return (([regex]::Replace($Name.ToLowerInvariant(), '[^a-z0-9]+', '_')).Trim('_'))
}

function Invoke-RegressionDataStageProcess {
    param(
        [Parameter(Mandatory)][string]$PowerShellPath,
        [Parameter(Mandatory)][string]$ScriptPath,
        [string[]]$Arguments = @(),
        [Parameter(Mandatory)][string]$LogPath,
        [int]$PollMilliseconds = 200
    )

    # Poll files instead of piping child output. A daemon can inherit and keep a
    # pipe or file handle open, but it cannot keep the direct process alive.
    # The sidecar also avoids a Windows PowerShell 5.1 Start-Process bug that
    # can detach redirected Process objects and make ExitCode unavailable.
    $stderrPath = "$LogPath.stderr"
    $invocationId = [Guid]::NewGuid().ToString('N')
    $requestPath = "$LogPath.$invocationId.request.json"
    $exitCodePath = "$LogPath.$invocationId.exitcode"
    [System.IO.File]::WriteAllText($LogPath, '', [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText($stderrPath, '', [System.Text.UTF8Encoding]::new($false))
    $request = [ordered]@{
        file_path = $PowerShellPath
        arguments = @('-NoProfile', '-NonInteractive', '-File', $ScriptPath) + @($Arguments)
    }
    [System.IO.File]::WriteAllText(
        $requestPath,
        ($request | ConvertTo-Json -Depth 3 -Compress),
        [System.Text.UTF8Encoding]::new($false)
    )
    $wrapperPath = Join-Path $script:HelpersDir 'invoke_process_with_exit_code.ps1'
    $processArguments = @(
        '-NoProfile', '-NonInteractive', '-File', "`"$wrapperPath`"",
        '-RequestPath', "`"$requestPath`"",
        '-ExitCodePath', "`"$exitCodePath`""
    )
    $process = Start-Process -FilePath $PowerShellPath -ArgumentList $processArguments -NoNewWindow -PassThru `
        -RedirectStandardOutput $LogPath -RedirectStandardError $stderrPath
    $stdoutReader = $null
    $stderrReader = $null
    try {
        $stdoutReader = [System.IO.StreamReader]::new([System.IO.FileStream]::new(
                $LogPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read,
                [System.IO.FileShare]::ReadWrite -bor [System.IO.FileShare]::Delete
            ))
        $stderrReader = [System.IO.StreamReader]::new([System.IO.FileStream]::new(
                $stderrPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read,
                [System.IO.FileShare]::ReadWrite -bor [System.IO.FileShare]::Delete
            ))
        while (-not $process.HasExited) {
            while (-not $stdoutReader.EndOfStream) { Write-Host $stdoutReader.ReadLine() }
            while (-not $stderrReader.EndOfStream) { Write-Host $stderrReader.ReadLine() -ForegroundColor Red }
            Start-Sleep -Milliseconds $PollMilliseconds
        }
        $process.WaitForExit()
        while (-not $stdoutReader.EndOfStream) { Write-Host $stdoutReader.ReadLine() }
        while (-not $stderrReader.EndOfStream) { Write-Host $stderrReader.ReadLine() -ForegroundColor Red }
        if (-not (Test-Path -LiteralPath $exitCodePath -PathType Leaf)) {
            throw 'Regression stage process exited without recording an exit code'
        }
        $exitCodeText = [IO.File]::ReadAllText($exitCodePath).Trim()
        $exitCode = 0
        if (-not [int]::TryParse($exitCodeText, [ref]$exitCode)) {
            throw "Regression stage process recorded invalid exit code '$exitCodeText'"
        }
        return $exitCode
    } finally {
        if ($stdoutReader) { $stdoutReader.Dispose() }
        if ($stderrReader) { $stderrReader.Dispose() }
        $process.Dispose()
        Remove-Item -LiteralPath $requestPath, $exitCodePath -Force -ErrorAction SilentlyContinue
    }
}

function Write-RegressionDataJsonSummary {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Category,
        [Parameter(Mandatory)][object[]]$Results,
        [Parameter(Mandatory)][datetime]$StartedAt,
        [Parameter(Mandatory)][int]$SelectedStageCount
    )

    $summary = [ordered]@{
        category = $Category
        started_at = $StartedAt.ToString('o')
        completed_at = (Get-Date).ToString('o')
        status = if ($Results.Count -lt $SelectedStageCount) {
            'in_progress'
        } elseif (@($Results | Where-Object Status -eq 'FAIL').Count -eq 0) {
            'pass'
        } else {
            'fail'
        }
        selected_stage_count = $SelectedStageCount
        completed_stage_count = $Results.Count
        stages = @($Results | ForEach-Object {
                [ordered]@{
                    name = $_.Name
                    status = $_.Status.ToLowerInvariant()
                    exit_code = $_.ExitCode
                    elapsed_seconds = $_.Seconds
                    log_file = $_.LogFile
                }
            })
    }
    $json = ($summary | ConvertTo-Json -Depth 6) -replace "`r`n", "`n"
    [System.IO.File]::WriteAllText($Path, $json + "`n", [System.Text.UTF8Encoding]::new($false))
}

function Write-RegressionDataTimingReport {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][object[]]$Results
    )

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add('# Regression Data Regeneration Report')
    $lines.Add('')
    $lines.Add('| Status | Time | Test | Type |')
    $lines.Add('|--------|------|------|------|')
    foreach ($result in $Results) {
        $lines.Add("| $($result.Status) | $($result.Elapsed) | $($result.Name) | regeneration |")
    }
    [System.IO.File]::WriteAllText($Path, ($lines -join "`n") + "`n", [System.Text.UTF8Encoding]::new($false))
}

function Invoke-RegressionDataStages {
    param(
        [Parameter(Mandatory)][object[]]$Stages,
        [string]$PowerShellPath = (Get-Process -Id $PID).Path,
        [string]$ReportDir,
        [string]$Category = 'All',
        [switch]$RecordTiming
    )

    $startedAt = Get-Date
    $runStamp = $startedAt.ToString('yyyyMMdd_HHmmss')
    $runDir = if ($ReportDir) { Join-Path $ReportDir "run_$runStamp" } else { '' }
    if ($runDir) {
        New-Item -ItemType Directory -Path $runDir -Force | Out-Null
    }
    $results = @()
    for ($index = 0; $index -lt $Stages.Count; $index++) {
        if (-not ($Stages[$index].PSObject.Properties.Name -contains 'ProgressIndex')) {
            $Stages[$index] | Add-Member -NotePropertyName ProgressIndex -NotePropertyValue ($index + 1)
        }
        if (-not ($Stages[$index].PSObject.Properties.Name -contains 'EstimatedRuntime')) {
            $fallback = if ($Stages[$index].PSObject.Properties.Name -contains 'DefaultEstimatedRuntime') {
                [int]$Stages[$index].DefaultEstimatedRuntime
            } else {
                1
            }
            $Stages[$index] | Add-Member -NotePropertyName EstimatedRuntime -NotePropertyValue $fallback
        }
    }

    for ($index = 0; $index -lt $Stages.Count; $index++) {
        $stage = $Stages[$index]
        $remaining = Get-RemainingRuntimeEstimate -Tests $Stages -CurrentProgressIndex ($index + 1)
        $remainingText = Format-RunnerDurationEstimate -Seconds $remaining.Seconds
        $logPath = if ($runDir) {
            Join-Path $runDir ("{0:00}_{1}.log" -f ($index + 1), (ConvertTo-RegressionArtifactName $stage.Name))
        } else {
            ''
        }

        Write-Host ''
        Write-Host '============================================================' -ForegroundColor White
        Write-Host "  REGENERATION STAGE $($index + 1)/$($Stages.Count): $($stage.Name)" -ForegroundColor Cyan
        Write-Host "  Estimated $remainingText remaining ($($remaining.Percent)%)" -ForegroundColor Cyan
        if ($logPath) { Write-Host "  Log: $logPath" -ForegroundColor DarkGray }
        Write-Host '============================================================' -ForegroundColor White

        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        $exitCode = 1
        if (-not (Test-Path -LiteralPath $stage.Script -PathType Leaf)) {
            $message = "Regression data stage script not found: $($stage.Script)"
            Write-Host "FAILED: $message" -ForegroundColor Red
            if ($logPath) {
                [System.IO.File]::WriteAllText($logPath, $message + "`n", [System.Text.UTF8Encoding]::new($false))
            }
            $exitCode = 127
        } else {
            try {
                if ($logPath) {
                    $exitCode = Invoke-RegressionDataStageProcess -PowerShellPath $PowerShellPath `
                        -ScriptPath $stage.Script -Arguments @($stage.Arguments) -LogPath $logPath
                } else {
                    & $PowerShellPath -NoProfile -NonInteractive -File $stage.Script @($stage.Arguments)
                    $exitCode = $LASTEXITCODE
                }
            } catch {
                $message = "Stage runner exception: $_"
                Write-Host $message -ForegroundColor Red
                if ($logPath) { [IO.File]::AppendAllText($logPath, $message + [Environment]::NewLine, [Text.UTF8Encoding]::new($false)) }
                $exitCode = 126
            }
        }
        $stopwatch.Stop()
        $elapsed = if ($stopwatch.Elapsed.TotalHours -ge 1) {
            '{0:hh\:mm\:ss}' -f $stopwatch.Elapsed
        } else {
            '{0:mm\:ss}' -f $stopwatch.Elapsed
        }
        $status = if ($exitCode -eq 0) { 'PASS' } else { 'FAIL' }
        $results += [pscustomobject]@{
            Name = $stage.Name
            Status = $status
            ExitCode = $exitCode
            Seconds = [int][Math]::Ceiling($stopwatch.Elapsed.TotalSeconds)
            Elapsed = $elapsed
            LogFile = $logPath
        }
        Write-Host "  $status ($elapsed, exit $exitCode)" -ForegroundColor $(if ($exitCode -eq 0) { 'Green' } else { 'Red' })

        if ($runDir) {
            Write-RegressionDataJsonSummary -Path (Join-Path $runDir 'summary.json') -Category $Category `
                -Results $results -StartedAt $startedAt -SelectedStageCount $Stages.Count
        }
    }

    if ($RecordTiming -and $ReportDir -and $results.Count -eq $Stages.Count -and
        @($results | Where-Object Status -ne 'PASS').Count -eq 0) {
        Write-RegressionDataTimingReport -Path (Join-Path $ReportDir "report_$runStamp.md") -Results $results
    }
    return $results
}

if ($MyInvocation.InvocationName -ne '.') {
    if (-not $ReportDir) {
        $ReportDir = Join-Path $script:RepoRoot 'temp\regression_data_reports'
    }
    New-Item -ItemType Directory -Path $ReportDir -Force | Out-Null

    do {
        $selectedCategory = if ($Category -eq 'Menu' -and -not $Target45Minutes) { Select-RegressionDataCategory } else { $Category }
        if ($selectedCategory -eq 'ManualSimulation') {
            & (Join-Path $script:HelpersDir 'watch_guidebot_simulation.ps1')
        }
    } while ($selectedCategory -eq 'ManualSimulation')
    if (-not $selectedCategory) {
        Write-Host 'Regression data regeneration cancelled' -ForegroundColor Yellow
        exit 0
    }

    if ($Category -eq 'Menu' -and $selectedCategory -eq 'Simulation') {
        while ($true) {
            $modeChoice = (Read-Host 'Simulation mode: H for headless canonical output, V for visible headed diagnostics').Trim().ToLowerInvariant()
            if ($modeChoice -in @('', 'h', 'headless')) { $SimulationMode = 'Headless'; break }
            if ($modeChoice -in @('v', 'headed', 'visible')) { $SimulationMode = 'Headed'; break }
            Write-Host 'Enter H or V' -ForegroundColor Yellow
        }
    }

    $targetedSample = $Target45Minutes -or $selectedCategory -eq 'Target45'
    $stageCategory = if ($selectedCategory -eq 'Target45' -or $selectedCategory -eq 'Menu') { 'All' } else { $selectedCategory }
    $effectiveSimulationMode = if ($stageCategory -eq 'All') { 'Headless' } else { $SimulationMode }
    $stages = @(Get-RegressionDataStages -RepoRoot $script:RepoRoot -Category $stageCategory -SimulationMode $effectiveSimulationMode)
    $stages = @(Set-RegressionDataStageEstimates -Stages $stages -ReportDir $ReportDir)
    if ($targetedSample) {
        $sampleStatePath = Join-Path $ReportDir 'runtime_sample_state.json'
        $sample = Set-RegressionDataTargetSample -Stages $stages -Seed $SampleSeed -StatePath $sampleStatePath
        $stages = @($sample.Stages)
        $estimate = Format-RunnerDurationEstimate -Seconds $sample.EstimatedSeconds
        Write-Host "45-minute hash-ring fallback seed: $($sample.Seed)" -ForegroundColor Cyan
        Write-Host ("Sampling {0:P1} from every regeneration target type, estimated $estimate" -f $sample.Fraction) -ForegroundColor Cyan
        $selectedCategory = 'Target45'
    }
    $results = @(Invoke-RegressionDataStages -Stages $stages -ReportDir $ReportDir `
            -Category $selectedCategory -RecordTiming:($selectedCategory -eq 'All'))
    $failures = @($results | Where-Object Status -eq 'FAIL')

    Write-Host ''
    $results | Format-Table Name, Status, Elapsed, ExitCode, LogFile -AutoSize
    if ($failures.Count -gt 0) {
        $failureMessage = if ($results.Count -gt 1) {
            "$($failures.Count) regression data stage(s) failed; later stages were still attempted"
        } else {
            'Selected regression data stage failed'
        }
        Write-Host $failureMessage -ForegroundColor Red
        exit 1
    }
    Write-Host 'Selected regression data regenerated successfully' -ForegroundColor Green
}

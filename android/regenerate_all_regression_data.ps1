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
  Menu, All, Cd, Fingerprints, or Metadata. Menu is the interactive default.

.PARAMETER ReportDir
  Directory for durable run artifacts and full-run timing reports.

.EXAMPLE
  .\android\regenerate_all_regression_data.ps1
  .\android\regenerate_all_regression_data.ps1 -Category All
  .\android\regenerate_all_regression_data.ps1 -Category Metadata
#>

[CmdletBinding()]
param(
    [ValidateSet('Menu', 'All', 'Cd', 'Fingerprints', 'Metadata')]
    [string]$Category = 'Menu',
    [string]$ReportDir
)

$ErrorActionPreference = 'Stop'
$script:RepoRoot = Split-Path $PSScriptRoot -Parent
$script:HelpersDir = Join-Path $PSScriptRoot 'helpers'
. (Join-Path $script:HelpersDir 'test_suite_progress.ps1')

function Get-RegressionDataStages {
    param(
        [string]$RepoRoot,
        [ValidateSet('All', 'Cd', 'Fingerprints', 'Metadata')]
        [string]$Category = 'All'
    )

    $stages = @(
        [pscustomobject]@{
            Key = 'Cd'
            Name = 'CD extraction regression data'
            Description = 'Host extraction, oracle validation, Android import, and launch checks'
            Script = Join-Path $RepoRoot 'game_data\run_all_cd_regressions.ps1'
            Arguments = @('-RefreshOracle')
            DefaultEstimatedRuntime = 7200
        },
        [pscustomobject]@{
            Key = 'Fingerprints'
            Name = 'Disc and music fingerprint data'
            Description = 'Disc hashes, CD audio fingerprints, tags, and AcoustID data'
            Script = Join-Path $RepoRoot 'game_data\update_all_fingerprints.ps1'
            Arguments = @('-Force')
            DefaultEstimatedRuntime = 1800
        },
        [pscustomobject]@{
            Key = 'Metadata'
            Name = 'Mission level metadata'
            Description = 'Mission archive and extracted-CD level metadata'
            Script = Join-Path $RepoRoot 'android\helpers\regenerate_all_mission_metadata.ps1'
            Arguments = @()
            DefaultEstimatedRuntime = 7200
        }
    )
    if ($Category -eq 'All') {
        return $stages
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
            'q' { return $null }
            'quit' { return $null }
            default { Write-Host 'Enter 1, 2, 3, 4, or Q' -ForegroundColor Yellow }
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
        foreach ($record in @(& $runtimeReader -ReportDir $ReportDir)) {
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

function ConvertTo-RegressionArtifactName {
    param([Parameter(Mandatory)][string]$Name)

    return (([regex]::Replace($Name.ToLowerInvariant(), '[^a-z0-9]+', '_')).Trim('_'))
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
                    [System.IO.File]::WriteAllText($logPath, '', [System.Text.UTF8Encoding]::new($false))
                    & $PowerShellPath -NoProfile -NonInteractive -File $stage.Script @($stage.Arguments) 2>&1 |
                        Tee-Object -FilePath $logPath |
                        Out-Host
                } else {
                    & $PowerShellPath -NoProfile -NonInteractive -File $stage.Script @($stage.Arguments)
                }
                $exitCode = $LASTEXITCODE
            } catch {
                $message = "Stage runner exception: $_"
                Write-Host $message -ForegroundColor Red
                if ($logPath) { Add-Content -LiteralPath $logPath -Value $message -Encoding utf8 }
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

    if ($RecordTiming -and $ReportDir -and $results.Count -eq $Stages.Count) {
        Write-RegressionDataTimingReport -Path (Join-Path $ReportDir "report_$runStamp.md") -Results $results
    }
    return $results
}

if ($MyInvocation.InvocationName -ne '.') {
    if (-not $ReportDir) {
        $ReportDir = Join-Path $script:RepoRoot 'temp\regression_data_reports'
    }
    New-Item -ItemType Directory -Path $ReportDir -Force | Out-Null

    $selectedCategory = if ($Category -eq 'Menu') { Select-RegressionDataCategory } else { $Category }
    if (-not $selectedCategory) {
        Write-Host 'Regression data regeneration cancelled' -ForegroundColor Yellow
        exit 0
    }

    $stages = @(Get-RegressionDataStages -RepoRoot $script:RepoRoot -Category $selectedCategory)
    $stages = @(Set-RegressionDataStageEstimates -Stages $stages -ReportDir $ReportDir)
    $results = @(Invoke-RegressionDataStages -Stages $stages -ReportDir $ReportDir `
            -Category $selectedCategory -RecordTiming:($selectedCategory -eq 'All'))
    $failures = @($results | Where-Object Status -eq 'FAIL')

    Write-Host ''
    $results | Format-Table Name, Status, Elapsed, ExitCode, LogFile -AutoSize
    if ($failures.Count -gt 0) {
        Write-Host "$($failures.Count) regression data stage(s) failed; later stages were still attempted" -ForegroundColor Red
        exit 1
    }
    Write-Host 'Selected regression data regenerated successfully' -ForegroundColor Green
}

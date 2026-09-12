#!/usr/bin/env pwsh
param(
    [switch]$NoBuild,
    [switch]$AllCases,
    [string]$CaseFilter,
    [ValidateRange(0, [int]::MaxValue)][int]$Seed = 0
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'guidebot_route_regression_cases.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
. (Join-Path $repoRoot 'android/helpers/atomic_text_file.ps1')
. (Join-Path $repoRoot 'android/helpers/host_metadata_workspace.ps1')
$cases = @(Get-GuidebotRouteRegressionCases)
if ($Seed -eq 0) { $Seed = [int]([DateTime]::UtcNow.Date - [DateTime]'2026-01-01').TotalDays }
$selected = if ($CaseFilter) {
    @($cases | Where-Object { [IO.Path]::GetFileNameWithoutExtension($_.File) -like $CaseFilter })
} else {
    @(Select-GuidebotRouteRegressionCases -Cases $cases -Seed $Seed -AllCases:$AllCases)
}
if (-not $selected.Count) { throw "No physical route cases match '$CaseFilter'" }
$output = Join-Path $repoRoot ('android/temp/route_regression_cases/run_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output
New-Item -ItemType Directory -Path $output -Force | Out-Null
Write-Host "Physical route coverage: $($selected.Count)/$($cases.Count) cases, seed=$Seed, exhaustive=$AllCases"
if (-not $NoBuild) { Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target d2 }

$results = [Collections.Generic.List[object]]::new()
foreach ($case in $selected) {
    Write-Host "Route case $($results.Count + 1)/$($selected.Count): $($case.File) [$($case.Family)]"
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $process = $null
    $status = 'FAIL'
    $exitCode = -1
    $text = ''
    $logPath = Join-Path $output ($case.File + '.log')
    try {
        $startInfo = [Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = (Get-Process -Id $PID).Path
        foreach ($argument in @('-NoProfile', '-NonInteractive', '-File', (Join-Path $PSScriptRoot $case.File), '-NoBuild')) {
            $startInfo.ArgumentList.Add($argument)
        }
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $process = [Diagnostics.Process]::Start($startInfo)
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(600000)) {
            $status = 'TIMEOUT'
            $process.Kill($true)
            $process.WaitForExit()
        } else {
            $exitCode = $process.ExitCode
            if ($exitCode -eq 0) { $status = 'PASS' }
        }
        $text = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
        Write-Utf8NoBomTextAtomically -Path $logPath -Text $text
        if ($status -eq 'PASS') { Remove-GuidebotTestPayloads -RepoRoot $repoRoot -OutputText $text }
    } catch {
        $status = 'FAIL'
        Write-Utf8NoBomTextAtomically -Path $logPath -Text ($text + "`nRoute owner error: $_`n")
    } finally {
        if ($process) {
            if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }
            $process.Dispose()
        }
        $watch.Stop()
    }
    $results.Add([pscustomobject]@{
            name = $case.File; family = $case.Family; status = $status
            exit_code = $exitCode; seconds = [Math]::Round($watch.Elapsed.TotalSeconds, 2); log = $logPath
        })
    Write-Host "  $status ($([Math]::Round($watch.Elapsed.TotalSeconds, 1))s)"
    $summary = [ordered]@{
        seed = $Seed; exhaustive = [bool]$AllCases; filter = $CaseFilter; available = $cases.Count
        selected = @($selected.File); not_selected = @($cases.File | Where-Object { $_ -notin $selected.File })
        results = @($results.ToArray())
    }
    Write-Utf8NoBomTextAtomically -Path (Join-Path $output 'summary.json') -Text (($summary | ConvertTo-Json -Depth 6) + "`n")
}
Write-Host "Route case report: $output/summary.json"
$failures = @($results | Where-Object status -ne 'PASS')
if ($failures.Count) { throw "$($failures.Count) physical route case(s) failed; see individual logs in $output" }
Write-Host "PASS $($results.Count) physical route cases"

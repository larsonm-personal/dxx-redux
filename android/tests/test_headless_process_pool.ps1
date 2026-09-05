#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'android\helpers\headless_process_pool.ps1')

function Assert-Equal {
    param($Expected, $Actual, [string]$Label)
    if ($Expected -ne $Actual) { throw "$Label expected '$Expected', got '$Actual'" }
}

Assert-Equal 1 (Get-HeadlessProcessWorkerCount -ItemCount 10 -LogicalProcessorCount 1) 'single core'
Assert-Equal 4 (Get-HeadlessProcessWorkerCount -ItemCount 10 -LogicalProcessorCount 8) 'automatic half cores'
Assert-Equal 8 (Get-HeadlessProcessWorkerCount -ItemCount 20 -LogicalProcessorCount 64) 'automatic cap'
Assert-Equal 3 (Get-HeadlessProcessWorkerCount -Requested 7 -ItemCount 3) 'item cap'

$powershell = (Get-Process -Id $PID).Path
$tasks = @(
    [pscustomobject]@{ FilePath = $powershell; Arguments = @('-NoProfile', '-Command', 'Start-Sleep -Milliseconds 250; Write-Output slow'); TimeoutSeconds = 30; WorkingDirectory = ''; Id = 'slow' }
    [pscustomobject]@{ FilePath = $powershell; Arguments = @('-NoProfile', '-Command', 'Write-Output fast'); TimeoutSeconds = 30; WorkingDirectory = ''; Id = 'fast' }
    [pscustomobject]@{ FilePath = $powershell; Arguments = @('-NoProfile', '-Command', 'exit 7'); TimeoutSeconds = 30; WorkingDirectory = ''; Id = 'failed' }
    [pscustomobject]@{ FilePath = $powershell; Arguments = @('-NoProfile', '-Command', 'Start-Sleep -Seconds 5'); TimeoutSeconds = 1; WorkingDirectory = ''; Id = 'timeout' }
)
$testState = @{ Active = 0; Peak = 0; Results = [Collections.Generic.List[object]]::new() }
Invoke-HeadlessProcessPool -Tasks $tasks -MaxParallel 2 -OnStarted {
    $testState.Active++
    $testState.Peak = [Math]::Max($testState.Peak, $testState.Active)
} -OnCompleted {
    param($task, $result)
    $testState.Active--
    $testState.Results.Add([pscustomobject]@{ Id = $task.Id; Result = $result })
}

Assert-Equal 2 $testState.Peak 'parallelism cap'
Assert-Equal 0 $testState.Active 'active process balance'
Assert-Equal 4 $testState.Results.Count 'completion count'
Assert-Equal 'fast' $testState.Results[0].Id 'retirement order'
Assert-Equal 7 @($testState.Results | Where-Object Id -eq 'failed')[0].Result.ExitCode 'exit code'
Assert-Equal $true @($testState.Results | Where-Object Id -eq 'timeout')[0].Result.TimedOut 'timeout flag'
Assert-Equal 'fast' @($testState.Results | Where-Object Id -eq 'fast')[0].Result.StandardOutput.Trim() 'stdout capture'

Write-Host 'Headless process pool tests passed'

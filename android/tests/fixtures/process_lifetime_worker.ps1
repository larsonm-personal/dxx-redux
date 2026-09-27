param([string]$Mode, [string]$OutputRoot)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path (Split-Path $PSScriptRoot))
$shell = (Get-Process -Id $PID).Path
if ($Mode -in @('child', 'child-exit')) {
    $grandchild = Start-Process -FilePath $shell -ArgumentList @('-NoProfile', '-Command', 'Start-Sleep -Seconds 45') -NoNewWindow -PassThru
    [IO.File]::WriteAllText((Join-Path $OutputRoot 'child.json'), (@($PID, $grandchild.Id) | ConvertTo-Json))
    if ($Mode -eq 'child-exit') { exit 17 }
    Start-Sleep -Seconds 45
    exit
}
if ($Mode -eq 'stage') {
    . (Join-Path $repoRoot 'android/regenerate_all_regression_data.ps1')
    Invoke-RegressionDataStageProcess -PowerShellPath $shell -ScriptPath $PSCommandPath `
        -Arguments @('-Mode', 'child', '-OutputRoot', $OutputRoot) `
        -LogPath (Join-Path $OutputRoot 'stage.log') -PollMilliseconds 20
    exit
}
. (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')
$childMode = if ($Mode -eq 'exit') { 'child-exit' } else { 'child' }
$task = [pscustomobject]@{
    FilePath = $shell; Arguments = @('-NoProfile', '-File', $PSCommandPath, '-Mode', $childMode, '-OutputRoot', $OutputRoot)
    TimeoutSeconds = if ($Mode -eq 'timeout') { 8 } else { 40 }; WorkingDirectory = ''
}
if ($Mode -eq 'error') {
    try {
        Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnStarted {
            $deadline = [DateTime]::UtcNow.AddSeconds(15)
            while (-not (Test-Path -LiteralPath (Join-Path $OutputRoot 'child.json'))) {
                if ([DateTime]::UtcNow -gt $deadline) { throw 'Child startup timed out' }
                Start-Sleep -Milliseconds 50
            }
            throw 'Intentional callback failure'
        } -OnCompleted { }
    } catch {
        if ($_ -notmatch 'Intentional callback failure') { throw }
        [IO.File]::WriteAllText((Join-Path $OutputRoot 'returned'), 'cleaned up')
    }
    Start-Sleep -Seconds 45
} elseif ($Mode -in @('exit', 'timeout')) {
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        if ($Mode -eq 'timeout' -and -not $result.TimedOut) { throw 'Worker did not time out' }
        if ($Mode -eq 'exit' -and ($result.TimedOut -or $result.ExitCode -ne 17)) { throw 'Worker exit status was lost' }
    }
    [IO.File]::WriteAllText((Join-Path $OutputRoot 'returned'), 'cleaned up')
    # Keep the pool owner alive to ensure its job is not what cleans up descendants
    Start-Sleep -Seconds 45
} else {
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted { }
}

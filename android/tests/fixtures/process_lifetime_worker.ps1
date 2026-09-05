param([string]$Mode, [string]$OutputRoot)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path (Split-Path $PSScriptRoot))
$shell = (Get-Process -Id $PID).Path
if ($Mode -eq 'child') {
    $grandchild = Start-Process -FilePath $shell -ArgumentList @('-NoProfile', '-Command', 'Start-Sleep -Seconds 45') -NoNewWindow -PassThru
    [IO.File]::WriteAllText((Join-Path $OutputRoot 'child.json'), (@($PID, $grandchild.Id) | ConvertTo-Json))
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
$task = [pscustomobject]@{
    FilePath = $shell; Arguments = @('-NoProfile', '-File', $PSCommandPath, '-Mode', 'child', '-OutputRoot', $OutputRoot)
    TimeoutSeconds = 40; WorkingDirectory = ''
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
} else {
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted { }
}

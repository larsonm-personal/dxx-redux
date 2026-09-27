# Shared host setup and supervision for Original/live navigation tests
. (Join-Path $PSScriptRoot '../helpers/standard_game_data.ps1')
. (Join-Path $PSScriptRoot '../helpers/headless_process_pool.ps1')
. (Join-Path $PSScriptRoot '../helpers/atomic_text_file.ps1')

function New-GuidebotNavigationData {
    param([string]$RepoRoot, [string]$OutputRoot, [string]$HogDir)
    $candidates = if ($HogDir) {
        if (-not [IO.Path]::IsPathRooted($HogDir)) { $HogDir = Join-Path $RepoRoot $HogDir }
        @($HogDir)
    } else { @(Get-StandardGameDataCandidates -RepoRoot $RepoRoot -Game d2) }
    $dependencies = @(Get-StandardGameDataDeps | Where-Object file -notin @('descent.hog', 'descent.pig'))
    return (New-StandardGameDataStage -Destination (Join-Path $OutputRoot 'base-data') -Candidates $candidates -Dependencies $dependencies).Path
}

function Invoke-GuidebotNavigationProcess {
    param(
        [string]$Exe,
        [string[]]$Arguments,
        [string]$RepoRoot,
        [string]$LogPath,
        [ValidateRange(1, 300)][int]$TimeoutSeconds = 90
    )
    $execution = @{ Result = $null }
    $task = [pscustomobject]@{ FilePath = $Exe; Arguments = $Arguments; WorkingDirectory = $RepoRoot; TimeoutSeconds = $TimeoutSeconds }
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        Write-Utf8NoBomTextAtomically -Path $LogPath -Text ($result.StandardOutput + "`n" + $result.StandardError)
        $execution.Result = $result
    }
    if (-not $execution.Result -or $execution.Result.TimedOut -or $execution.Result.ExitCode -ne 0) {
        throw "Navigation process failed (exit=$($execution.Result.ExitCode), timeout=$($execution.Result.TimedOut)); see $LogPath"
    }
}

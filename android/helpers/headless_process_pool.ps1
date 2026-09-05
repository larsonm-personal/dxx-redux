. (Join-Path $PSScriptRoot 'process_lifetime.ps1')

function Get-HeadlessProcessWorkerCount {
    param(
        [ValidateRange(0, 128)][int]$Requested = 0,
        # Queue length is independent of the bounded number of active workers
        [ValidateRange(1, [int]::MaxValue)][int]$ItemCount = [int]::MaxValue,
        [ValidateRange(1, 1024)][int]$LogicalProcessorCount = [Environment]::ProcessorCount,
        [ValidateRange(1, 128)][int]$AutomaticLimit = 8
    )

    $workers = if ($Requested -gt 0) {
        $Requested
    } else {
        [Math]::Min($AutomaticLimit, [Math]::Max(1, [Math]::Ceiling($LogicalProcessorCount / 2)))
    }
    return [Math]::Max(1, [Math]::Min($workers, $ItemCount))
}

function Set-HeadlessProcessArguments {
    param(
        [Parameter(Mandatory)][Diagnostics.ProcessStartInfo]$StartInfo,
        [Parameter(Mandatory)][string[]]$Arguments
    )

    if ($null -ne $StartInfo.ArgumentList) {
        foreach ($argument in $Arguments) { $StartInfo.ArgumentList.Add($argument) }
        return
    }
    $StartInfo.Arguments = @($Arguments | ForEach-Object {
            $argument = [string]$_
            if ($argument.Length -gt 0 -and $argument -notmatch '[\s"]') { return $argument }
            $escaped = [regex]::Replace($argument, '(\\*)"', '$1$1\"')
            $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
            '"' + $escaped + '"'
        }) -join ' '
}

function Start-HeadlessProcessPoolItem {
    param([Parameter(Mandatory)]$Task)

    Initialize-RegressionProcessLifetime
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = [string]$Task.FilePath
    Set-HeadlessProcessArguments -StartInfo $startInfo -Arguments @($Task.Arguments)
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    if ($Task.WorkingDirectory) { $startInfo.WorkingDirectory = [string]$Task.WorkingDirectory }
    $process = [Diagnostics.Process]::Start($startInfo)
    return [pscustomobject]@{
        Task = $Task
        Process = $process
        StandardOutput = $process.StandardOutput.ReadToEndAsync()
        StandardError = $process.StandardError.ReadToEndAsync()
        StartedUtc = [DateTime]::UtcNow
    }
}

function Invoke-HeadlessProcessPool {
    param(
        [Parameter(Mandatory)][object[]]$Tasks,
        [ValidateRange(1, 128)][int]$MaxParallel,
        [scriptblock]$OnStarted,
        [Parameter(Mandatory)][scriptblock]$OnCompleted
    )

    Initialize-RegressionProcessLifetime
    $pending = [Collections.Generic.Queue[object]]::new()
    foreach ($task in $Tasks) { $pending.Enqueue($task) }
    $running = [Collections.Generic.List[object]]::new()
    try {
        while ($pending.Count -gt 0 -or $running.Count -gt 0) {
            while ($pending.Count -gt 0 -and $running.Count -lt $MaxParallel) {
                $task = $pending.Dequeue()
                $state = $null
                try {
                    $state = Start-HeadlessProcessPoolItem -Task $task
                    $running.Add($state)
                } catch {
                    & $OnCompleted $task ([pscustomobject]@{
                            ExitCode = -1; TimedOut = $false; StartError = $_.Exception.Message
                            StandardOutput = ''; StandardError = ''
                        })
                }
                if ($state -and $OnStarted) { & $OnStarted $task }
            }

            $retired = $false
            for ($i = $running.Count - 1; $i -ge 0; $i--) {
                $state = $running[$i]
                $timeoutSeconds = [int]$state.Task.TimeoutSeconds
                $timedOut = $timeoutSeconds -gt 0 -and
                ([DateTime]::UtcNow - $state.StartedUtc).TotalSeconds -ge $timeoutSeconds
                if (-not $state.Process.HasExited -and -not $timedOut) { continue }
                if ($timedOut -and -not $state.Process.HasExited) {
                    Stop-RegressionChildProcess -Process $state.Process
                }
                $state.Process.WaitForExit()
                $result = [pscustomobject]@{
                    ExitCode = if ($timedOut) { -1 } else { $state.Process.ExitCode }
                    TimedOut = $timedOut
                    StartError = ''
                    StandardOutput = $state.StandardOutput.GetAwaiter().GetResult()
                    StandardError = $state.StandardError.GetAwaiter().GetResult()
                }
                $state.Process.Dispose()
                $running.RemoveAt($i)
                & $OnCompleted $state.Task $result
                $retired = $true
            }
            if (-not $retired -and $running.Count -gt 0) { Start-Sleep -Milliseconds 50 }
        }
    } finally {
        foreach ($state in $running) {
            try { Stop-RegressionChildProcess -Process $state.Process }
            finally { $state.Process.Dispose() }
        }
    }
}

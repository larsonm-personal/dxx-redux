. (Join-Path $PSScriptRoot 'atomic_text_file.ps1')

function New-MetadataWorker {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [string[]]$Arguments = @()
    )

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    foreach ($argument in $Arguments) { $startInfo.ArgumentList.Add($argument) }
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    if ($env:DXX_METADATA_WORKER_DIAGNOSTICS_DIR) {
        New-Item -ItemType Directory -Path $env:DXX_METADATA_WORKER_DIAGNOSTICS_DIR -Force | Out-Null
    }
    $process = [Diagnostics.Process]::Start($startInfo)
    $worker = @{ Process = $process; ErrorTask = $process.StandardError.ReadToEndAsync(); Executable = $Executable; Arguments = $Arguments
        Stage = $env:DXX_SANITIZER_STAGE; DiagnosticsDirectory = $env:DXX_METADATA_WORKER_DIAGNOSTICS_DIR; LastRequestId = $null; LastShutdown = $null; LastStandardOutput = '' }
    if ($worker.DiagnosticsDirectory) {
        try {
            $started = @{ pid = $process.Id; stage = $worker.Stage; executable = $Executable; arguments = $Arguments }
            Write-Utf8NoBomTextAtomically -Path (Join-Path $worker.DiagnosticsDirectory "$($process.Id).start.json") -Text ($started | ConvertTo-Json -Depth 8)
        } catch {
            Stop-MetadataWorkerProcess -Worker $worker -Terminate -IgnoreFailure
            throw
        }
    }
    return $worker
}

function Invoke-MetadataWorker {
    param(
        [Parameter(Mandatory = $true)]$Worker,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Request,
        [Parameter(Mandatory = $true)][string]$RawOutputPath,
        [Parameter(Mandatory = $true)][string]$LogPath,
        # Idle watchdog: native scans report progress through their checkpoint file
        [ValidateRange(1, [int]::MaxValue)][int]$TimeoutSeconds = 120
    )

    if ($null -eq $Worker.Process -or $Worker.Process.HasExited) {
        Stop-MetadataWorkerProcess -Worker $Worker -IgnoreFailure
        $replacement = New-MetadataWorker -Executable $Worker.Executable -Arguments $Worker.Arguments
        foreach ($key in $replacement.Keys) { $Worker[$key] = $replacement[$key] }
    }
    $process = $Worker.Process
    $Worker.LastRequestId = $Request['request_id']
    $logLines = [Collections.Generic.List[string]]::new()
    $progressTimer = [Diagnostics.Stopwatch]::StartNew()
    $lastCheckpointProgress = $null
    $lastCheckpointText = $null
    $runtimeRestarts = 0
    $pendingLineTask = $null
    if (-not $Request.Contains('checkpoint_path')) { $Request['checkpoint_path'] = "$LogPath.checkpoint.json" }
    New-Item -ItemType Directory -Path (Split-Path -Parent $LogPath) -Force | Out-Null
    try {
        $process.StandardInput.WriteLine(($Request | ConvertTo-Json -Depth 20 -Compress))
        $process.StandardInput.Flush()
        while ($true) {
            $readTask = $process.StandardOutput.ReadLineAsync()
            $pendingLineTask = $readTask
            while (-not $readTask.Wait([int][Math]::Max(1, [Math]::Min(1000,
                            $TimeoutSeconds * 1000.0 - $progressTimer.Elapsed.TotalMilliseconds)))) {
                try {
                    $checkpointText = [IO.File]::ReadAllText($Request['checkpoint_path'])
                    $checkpoint = $checkpointText | ConvertFrom-Json
                    if ($checkpoint -and $checkpoint -isnot [array] -and $Request['request_id'] -and
                        $checkpoint.schema -ceq 'dxx-level-metadata-checkpoint-v1' -and
                        $checkpoint.request_id -ceq $Request['request_id']) {
                        $progress = $checkpoint | Select-Object stage, phase, detail, task_id,
                        completed, total, level_completed, level_total | ConvertTo-Json -Compress
                        if ($progress -cne $lastCheckpointProgress) {
                            $lastCheckpointProgress = $progress
                            $lastCheckpointText = $checkpointText
                            $progressTimer.Restart()
                        }
                    }
                } catch {
                    # Checkpoints may be absent or observed during a native write
                }
                if ($progressTimer.Elapsed.TotalSeconds -lt $TimeoutSeconds) { continue }
                if ($lastCheckpointText) { $logLines.Add("Last progress checkpoint: $lastCheckpointText") }
                throw "metadata worker timed out after $TimeoutSeconds seconds without output or checkpoint progress; log=$LogPath"
            }
            $line = $readTask.Result
            $pendingLineTask = $null
            if ($null -eq $line) {
                $exited = $process.WaitForExit(5000)
                $exitDetail = if ($exited) { " with exit code $($process.ExitCode)" } else { '' }
                throw "metadata worker closed its output unexpectedly${exitDetail}; log=$LogPath"
            }
            if ($line.StartsWith("DXXMETA`t", [StringComparison]::Ordinal)) {
                $json = $line.Substring(8)
                Write-Utf8NoBomTextAtomically -Path $RawOutputPath -Text ($json + "`n")
                Write-Utf8NoBomTextAtomically -Path $LogPath -Text (($logLines -join "`n") + $(if ($logLines.Count) { "`n" } else { "" }))
                $result = $json | ConvertFrom-Json
                # Native initialization and mount retirement own this protocol flag
                if ($result.PSObject.Properties['worker_restart_required'] -and $result.worker_restart_required) {
                    Stop-MetadataWorkerProcess -Worker $Worker
                    if ($result.failure_kind -eq 'busy' -and $runtimeRestarts++ -eq 0) {
                        $replacement = New-MetadataWorker -Executable $Worker.Executable -Arguments $Worker.Arguments
                        foreach ($key in $replacement.Keys) { $Worker[$key] = $replacement[$key] }
                        $Worker.LastRequestId = $Request['request_id']
                        $process = $Worker.Process
                        $logLines.Add('Metadata runtime context changed; retrying in a fresh worker')
                        $lastCheckpointProgress = $lastCheckpointText = $null
                        $progressTimer.Restart()
                        $process.StandardInput.WriteLine(($Request | ConvertTo-Json -Depth 20 -Compress))
                        $process.StandardInput.Flush()
                        continue
                    }
                }
                return $result
            }
            $logLines.Add($line)
            $progressTimer.Restart()
        }
    } catch {
        $requestError = $_
        # A failed request invalidates the protocol stream; later requests get a fresh worker
        Stop-MetadataWorkerProcess -Worker $Worker -PendingLineTask $pendingLineTask -Terminate -IgnoreFailure
        if ($Worker.LastStandardOutput) { $logLines.Add($Worker.LastStandardOutput) }
        try {
            if ($Worker.ErrorTask.IsCompleted) {
                $logLines.Add($Worker.ErrorTask.GetAwaiter().GetResult())
            }
        } catch { $logLines.Add("Could not read worker diagnostics: $($_.Exception.Message)") }
        $logLines.Add($requestError.Exception.Message)
        try { Write-Utf8NoBomTextAtomically -Path $LogPath -Text (($logLines -join "`n") + "`n") }
        catch { Write-Warning "Could not preserve failed request log: $($_.Exception.Message)" }
        throw $requestError
    }
}

function Stop-MetadataWorkerProcess {
    param(
        [AllowNull()]$Worker,
        $OutputTask = $null,
        $PendingLineTask = $null,
        [switch]$Terminate,
        [ValidateRange(1, [int]::MaxValue)][int]$TimeoutMilliseconds = 30000,
        [switch]$IgnoreFailure
    )

    if ($null -eq $Worker -or $null -eq $Worker.Process) { return }
    $process = $Worker.Process
    $workerId = $process.Id
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $timedOut = $false
    $stdout = $stderr = ''
    $stdoutComplete = $stderrComplete = $false
    $exitCode = $null
    $shutdownError = $null
    try {
        # Start draining before waiting: teardown diagnostics can fill either pipe.
        # A caller already draining stdout passes its existing task instead.
        try { $process.StandardInput.Close() } catch {}
        if ($Terminate -and -not $process.HasExited) {
            try { $process.Kill($true) } catch { try { $process.Kill() } catch {} }
        }
        if ($null -ne $PendingLineTask) {
            if (-not $PendingLineTask.Wait(5000)) { throw "Metadata worker $workerId pending output did not finish" }
            $pendingLine = $PendingLineTask.GetAwaiter().GetResult()
            if ($null -ne $pendingLine) { $stdout = $pendingLine + "`n" }
        }
        if ($null -eq $OutputTask) { $OutputTask = $process.StandardOutput.ReadToEndAsync() }
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            $timedOut = $true
            try { $process.Kill($true) } catch { try { $process.Kill() } catch {} }
            if (-not $process.WaitForExit(5000)) { throw "Metadata worker $workerId did not exit after termination" }
        }
        $exitCode = $process.ExitCode
        $stdoutComplete = $OutputTask.Wait(5000)
        $stderrComplete = $Worker.ErrorTask.Wait(5000)
        if ($stdoutComplete) { $stdout += $OutputTask.GetAwaiter().GetResult() }
        if ($stderrComplete) { $stderr = $Worker.ErrorTask.GetAwaiter().GetResult() }
    } catch {
        $shutdownError = $_.Exception.Message
        if (-not $process.HasExited) {
            try { $process.Kill($true) } catch { try { $process.Kill() } catch {} }
            $null = $process.WaitForExit(5000)
        }
    } finally {
        # A faulted stdout task must not discard independently available stderr.
        if (-not $stderrComplete) {
            try {
                if ($Worker.ErrorTask.Wait(5000)) {
                    $stderr = $Worker.ErrorTask.GetAwaiter().GetResult()
                    $stderrComplete = $true
                }
            } catch {
                $shutdownError = "$shutdownError`nCould not read worker stderr: $($_.Exception.Message)"
            }
        }
        if ($null -eq $exitCode -and $process.HasExited) { $exitCode = $process.ExitCode }
        $record = [ordered]@{ pid = $workerId; stage = $Worker.Stage; request_id = $Worker.LastRequestId
            executable = $Worker.Executable; exit_code = $exitCode; timed_out = $timedOut
            shutdown_ms = $watch.ElapsedMilliseconds; stdout_complete = $stdoutComplete; stderr_complete = $stderrComplete; shutdown_error = $shutdownError }
        $Worker.LastShutdown = [pscustomobject]$record
        $Worker.LastStandardOutput = $stdout
        try {
            if ($Worker.DiagnosticsDirectory) {
                $prefix = Join-Path $Worker.DiagnosticsDirectory "$workerId"
                Write-Utf8NoBomTextAtomically -Path "$prefix.stdout.log" -Text $stdout
                Write-Utf8NoBomTextAtomically -Path "$prefix.stderr.log" -Text $stderr
                Write-Utf8NoBomTextAtomically -Path "$prefix.shutdown.json" -Text ($record | ConvertTo-Json -Depth 8)
            }
        } catch {
            $shutdownError = "Could not preserve worker shutdown diagnostics: $($_.Exception.Message)"
        } finally {
            $process.Dispose()
            $Worker.Process = $null
        }
    }
    if ($shutdownError -or $timedOut -or $exitCode -ne 0 -or -not $stdoutComplete -or -not $stderrComplete) {
        $message = "Metadata worker shutdown failed: pid=$workerId stage=$($Worker.Stage) request=$($Worker.LastRequestId) exit=$exitCode timeout=$timedOut stdout_complete=$stdoutComplete stderr_complete=$stderrComplete"
        if ($shutdownError) { $message += "`n$shutdownError" }
        if ($stderr) { $message += "`n$stderr" }
        if ($stdout) { $message += "`n$stdout" }
        if (-not $IgnoreFailure) { throw $message }
        Write-Verbose $message
    }
}

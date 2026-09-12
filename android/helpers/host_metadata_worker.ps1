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
    $process = [Diagnostics.Process]::Start($startInfo)
    return @{ Process = $process; ErrorTask = $process.StandardError.ReadToEndAsync(); Executable = $Executable; Arguments = $Arguments }
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
        Stop-MetadataWorkerProcess -Worker $Worker
        $replacement = New-MetadataWorker -Executable $Worker.Executable -Arguments $Worker.Arguments
        $Worker.Process = $replacement.Process
        $Worker.ErrorTask = $replacement.ErrorTask
    }
    $process = $Worker.Process
    $logLines = [Collections.Generic.List[string]]::new()
    $progressTimer = [Diagnostics.Stopwatch]::StartNew()
    $lastCheckpointProgress = $null
    $lastCheckpointText = $null
    if (-not $Request.Contains('checkpoint_path')) { $Request['checkpoint_path'] = "$LogPath.checkpoint.json" }
    New-Item -ItemType Directory -Path (Split-Path -Parent $LogPath) -Force | Out-Null
    try {
        $process.StandardInput.WriteLine(($Request | ConvertTo-Json -Depth 20 -Compress))
        $process.StandardInput.Flush()
        while ($true) {
            $readTask = $process.StandardOutput.ReadLineAsync()
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
                try { $process.Kill($true) } catch { try { $process.Kill() } catch {} }
                $process.WaitForExit()
                $pendingLine = $readTask.GetAwaiter().GetResult()
                if ($null -ne $pendingLine) { $logLines.Add($pendingLine) }
                $logLines.Add($process.StandardOutput.ReadToEnd())
                if ($lastCheckpointText) { $logLines.Add("Last progress checkpoint: $lastCheckpointText") }
                throw "metadata worker timed out after $TimeoutSeconds seconds without output or checkpoint progress; log=$LogPath"
            }
            $line = $readTask.Result
            if ($null -eq $line) {
                $exited = $process.WaitForExit(5000)
                $exitDetail = if ($exited) { " with exit code $($process.ExitCode)" } else { '' }
                throw "metadata worker closed its output unexpectedly${exitDetail}; log=$LogPath"
            }
            if ($line.StartsWith("DXXMETA`t", [StringComparison]::Ordinal)) {
                $json = $line.Substring(8)
                Write-Utf8NoBomTextAtomically -Path $RawOutputPath -Text ($json + "`n")
                Write-Utf8NoBomTextAtomically -Path $LogPath -Text (($logLines -join "`n") + $(if ($logLines.Count) { "`n" } else { "" }))
                return $json | ConvertFrom-Json
            }
            $logLines.Add($line)
            $progressTimer.Restart()
        }
    } catch {
        # A failed request invalidates the protocol stream; later requests get a fresh worker
        Stop-MetadataWorkerProcess -Worker $Worker
        if ($Worker.ErrorTask.IsCompleted) {
            $logLines.Add($Worker.ErrorTask.GetAwaiter().GetResult())
        }
        $logLines.Add($_.Exception.Message)
        Write-Utf8NoBomTextAtomically -Path $LogPath -Text (($logLines -join "`n") + "`n")
        throw
    }
}

function Stop-MetadataWorkerProcess {
    param([AllowNull()]$Worker)

    if ($null -eq $Worker -or $null -eq $Worker.Process) { return }
    try { $Worker.Process.StandardInput.Close() } catch {}
    if (-not $Worker.Process.WaitForExit(2000)) {
        try { $Worker.Process.Kill($true) } catch { try { $Worker.Process.Kill() } catch {} }
        $Worker.Process.WaitForExit()
    }
    $Worker.Process.Dispose()
    $Worker.Process = $null
}

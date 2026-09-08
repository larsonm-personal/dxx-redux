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
        [int]$TimeoutSeconds = 120
    )

    if ($null -eq $Worker.Process -or $Worker.Process.HasExited) {
        Stop-MetadataWorkerProcess -Worker $Worker
        $replacement = New-MetadataWorker -Executable $Worker.Executable -Arguments $Worker.Arguments
        $Worker.Process = $replacement.Process
        $Worker.ErrorTask = $replacement.ErrorTask
    }
    $process = $Worker.Process
    $logLines = [Collections.Generic.List[string]]::new()
    if (-not $Request.Contains('checkpoint_path')) { $Request['checkpoint_path'] = "$LogPath.checkpoint.json" }
    New-Item -ItemType Directory -Path (Split-Path -Parent $LogPath) -Force | Out-Null
    try {
        $process.StandardInput.WriteLine(($Request | ConvertTo-Json -Depth 20 -Compress))
        $process.StandardInput.Flush()
        while ($true) {
            $readTask = $process.StandardOutput.ReadLineAsync()
            if (-not $readTask.Wait($TimeoutSeconds * 1000)) {
                try { $process.Kill($true) } catch { try { $process.Kill() } catch {} }
                $process.WaitForExit()
                $pendingLine = $readTask.GetAwaiter().GetResult()
                if ($null -ne $pendingLine) { $logLines.Add($pendingLine) }
                $logLines.Add($process.StandardOutput.ReadToEnd())
                throw "metadata worker timed out after $TimeoutSeconds seconds; log=$LogPath"
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


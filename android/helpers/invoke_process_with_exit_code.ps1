#!/usr/bin/env pwsh

param(
    [Parameter(Mandatory = $true)][string]$RequestPath,
    [Parameter(Mandatory = $true)][string]$ExitCodePath
)

$ErrorActionPreference = 'Stop'

function ConvertTo-WindowsCommandLineArgument {
    param([AllowEmptyString()][string]$Argument)

    if ($Argument.Length -gt 0 -and $Argument -notmatch '[\s"]') {
        return $Argument
    }

    $quoted = [Text.StringBuilder]::new($Argument.Length + 2)
    [void]$quoted.Append('"')
    $backslashes = 0
    foreach ($character in $Argument.ToCharArray()) {
        if ($character -eq '\') {
            $backslashes++
            continue
        }
        if ($character -eq '"') {
            [void]$quoted.Append('\', (2 * $backslashes) + 1)
        } elseif ($backslashes -gt 0) {
            [void]$quoted.Append('\', $backslashes)
        }
        [void]$quoted.Append($character)
        $backslashes = 0
    }
    if ($backslashes -gt 0) {
        [void]$quoted.Append('\', 2 * $backslashes)
    }
    [void]$quoted.Append('"')
    return $quoted.ToString()
}

try {
    $request = Get-Content -LiteralPath $RequestPath -Raw | ConvertFrom-Json
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = [string]$request.file_path
    $startInfo.Arguments = @(
        $request.arguments | ForEach-Object {
            ConvertTo-WindowsCommandLineArgument -Argument ([string]$_)
        }
    ) -join ' '
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) {
            throw "Could not start $($request.file_path)"
        }
        $stdoutTask = $process.StandardOutput.ReadLineAsync()
        $stderrTask = $process.StandardError.ReadLineAsync()
        while (-not $process.HasExited) {
            while ($stdoutTask -and $stdoutTask.IsCompleted) {
                $line = $stdoutTask.Result
                if ($null -eq $line) { $stdoutTask = $null }
                else {
                    Write-Output $line
                    $stdoutTask = $process.StandardOutput.ReadLineAsync()
                }
            }
            while ($stderrTask -and $stderrTask.IsCompleted) {
                $line = $stderrTask.Result
                if ($null -eq $line) { $stderrTask = $null }
                else {
                    [Console]::Error.WriteLine($line)
                    $stderrTask = $process.StandardError.ReadLineAsync()
                }
            }
            Start-Sleep -Milliseconds 20
        }
        $exitCode = $process.ExitCode

        # Drain lines already delivered by the direct child, but do not wait
        # for EOF because a detached descendant may still hold these pipes.
        $drainDeadline = [DateTime]::UtcNow.AddMilliseconds(250)
        while ([DateTime]::UtcNow -lt $drainDeadline -and ($stdoutTask -or $stderrTask)) {
            $madeProgress = $false
            while ($stdoutTask -and $stdoutTask.IsCompleted) {
                $line = $stdoutTask.Result
                if ($null -eq $line) { $stdoutTask = $null }
                else {
                    Write-Output $line
                    $stdoutTask = $process.StandardOutput.ReadLineAsync()
                }
                $madeProgress = $true
            }
            while ($stderrTask -and $stderrTask.IsCompleted) {
                $line = $stderrTask.Result
                if ($null -eq $line) { $stderrTask = $null }
                else {
                    [Console]::Error.WriteLine($line)
                    $stderrTask = $process.StandardError.ReadLineAsync()
                }
                $madeProgress = $true
            }
            if (-not $madeProgress) { Start-Sleep -Milliseconds 10 }
        }
        $process.StandardOutput.Dispose()
        $process.StandardError.Dispose()
    } finally {
        $process.Dispose()
    }

    $temporaryPath = "$ExitCodePath.$PID.tmp"
    [IO.File]::WriteAllText(
        $temporaryPath,
        ([string]$exitCode),
        [Text.UTF8Encoding]::new($false)
    )
    Move-Item -LiteralPath $temporaryPath -Destination $ExitCodePath -Force
} catch {
    Write-Error $_
    exit 1
}

exit 0

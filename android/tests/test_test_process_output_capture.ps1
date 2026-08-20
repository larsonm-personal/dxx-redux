#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\..\helpers\test_process_output.ps1"

$testRoot = Join-Path (Split-Path $PSScriptRoot -Parent) "temp\test_process_output_capture_$([guid]::NewGuid().ToString('N'))"
$parentScript = Join-Path $testRoot "parent.ps1"
$holderScript = Join-Path $testRoot "holder.ps1"
$stdoutPath = Join-Path $testRoot "stdout.log"
$stderrPath = Join-Path $testRoot "stderr.log"
$holderPidPath = Join-Path $testRoot "holder.pid"
$holderProcess = $null

New-Item -Path $testRoot -ItemType Directory -Force | Out-Null
@'
param([string]$PidPath)
Set-Content -LiteralPath $PidPath -Value $PID -Encoding ascii
Start-Sleep -Seconds 20
'@ | Set-Content -LiteralPath $holderScript -Encoding utf8NoBOM
@'
param([string]$HolderScript, [string]$PidPath)
Write-Output "parent stdout diagnostic"
Start-Process -FilePath "pwsh" -ArgumentList "-NoProfile", "-File", "`"$HolderScript`"", "`"$PidPath`"" -NoNewWindow | Out-Null
Write-Error "parent stderr diagnostic" -ErrorAction Continue
exit 1
'@ | Set-Content -LiteralPath $parentScript -Encoding utf8NoBOM

try {
    $process = Start-Process -FilePath "pwsh" `
        -ArgumentList "-NoProfile", "-File", "`"$parentScript`"", "`"$holderScript`"", "`"$holderPidPath`"" `
        -WorkingDirectory $testRoot -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath `
        -NoNewWindow -PassThru
    if (-not $process.WaitForExit(10000)) {
        throw "Parent capture process did not exit"
    }
    if ($process.ExitCode -ne 1) {
        throw "Parent capture process exited with $($process.ExitCode), expected 1"
    }

    for ($attempt = 0; $attempt -lt 20 -and -not (Test-Path -LiteralPath $holderPidPath); $attempt++) {
        Start-Sleep -Milliseconds 100
    }
    $holderPid = [int](Get-Content -LiteralPath $holderPidPath -Raw)
    $holderProcess = Get-Process -Id $holderPid -ErrorAction Stop

    $stdout = Read-SharedProcessOutput -Path $stdoutPath
    $stderr = Read-SharedProcessOutput -Path $stderrPath
    if ($stdout -notmatch "parent stdout diagnostic") {
        throw "Stdout was unavailable while a descendant retained the inherited handle"
    }
    if ($stderr -notmatch "parent stderr diagnostic") {
        throw "Stderr was unavailable while a descendant retained the inherited handle"
    }
    if (-not (Add-SharedProcessOutput -Path $stdoutPath -Text $stderr)) {
        throw "Stderr could not be merged into the durable stdout log"
    }
    $merged = Read-SharedProcessOutput -Path $stdoutPath
    if ($merged -notmatch "parent stdout diagnostic" -or $merged -notmatch "parent stderr diagnostic") {
        throw "Merged process log did not retain both output streams"
    }
} finally {
    if ($holderProcess -and -not $holderProcess.HasExited) {
        Stop-Process -Id $holderProcess.Id -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "PASS"

#!/usr/bin/env pwsh
Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$androidRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $androidRoot 'helpers/normalized_json_text.ps1')
. (Join-Path $androidRoot 'helpers/host_metadata_worker.ps1')
$root = Join-Path $androidRoot 'temp/test_host_metadata_worker'
New-Item -ItemType Directory -Path $root -Force | Out-Null
$fixture = Join-Path $root 'worker.ps1'
[IO.File]::WriteAllText($fixture, @'
while ($null -ne ($line = [Console]::ReadLine())) {
    $request = $line | ConvertFrom-Json
    [Console]::WriteLine("request=$($request.request_id)")
    if ($request.action -in @('progress', 'stale-progress', 'unchanged-progress', 'malformed-progress')) {
        foreach ($step in 1..8) {
            $checkpoint = @{
                schema = 'dxx-level-metadata-checkpoint-v1'
                request_id = if ($request.action -eq 'stale-progress') { 'previous-request' } else { $request.request_id }
                stage = 'level_progress'
                completed = if ($request.action -eq 'unchanged-progress') { 0 } else { $step }
                total = 8
            }
            $text = if ($request.action -eq 'malformed-progress') { '{' } else { $checkpoint | ConvertTo-Json -Compress }
            [IO.File]::WriteAllText($request.checkpoint_path, $text)
            Start-Sleep -Milliseconds 500
        }
    }
    if ($request.action -eq 'timeout') {
        [Console]::Error.WriteLine('timeout diagnostic')
        Start-Sleep -Seconds 60
    }
    if ($request.action -eq 'crash') {
        [Console]::Error.WriteLine('crash diagnostic')
        exit 23
    }
    [Console]::WriteLine("DXXMETA`t" + (@{ status = 'ok'; request_id = $request.request_id } | ConvertTo-Json -Compress))
}
'@)
$worker = New-MetadataWorker -Executable (Get-Process -Id $PID).Path -Arguments @('-NoProfile', '-File', $fixture)
try {
    foreach ($action in @('ok', 'progress', 'stale-progress', 'unchanged-progress', 'malformed-progress', 'timeout', 'ok', 'crash', 'ok')) {
        $requestId = [guid]::NewGuid().ToString('N')
        $log = Join-Path $root "$requestId.log"
        $raw = Join-Path $root "$requestId.json"
        $errorText = $null
        try {
            $result = Invoke-MetadataWorker -Worker $worker -Request @{ request_id = $requestId; action = $action } -RawOutputPath $raw -LogPath $log -TimeoutSeconds 2
        } catch { $errorText = $_.Exception.Message }
        if ($action -in @('ok', 'progress')) {
            if ($errorText -or $result.status -ne 'ok' -or $result.request_id -ne $requestId) {
                throw "Worker did not recover: $errorText"
            }
        } else {
            if (-not $errorText) { throw "Expected $action failure" }
            $text = Get-Content -LiteralPath $log -Raw
            if ($text -notmatch "request=$requestId" -or
                ($action -in @('timeout', 'crash') -and $text -notmatch "$action diagnostic")) {
                throw "Lost $action diagnostics"
            }
            if ($action -like '*progress' -and $errorText -notmatch 'timed out') {
                throw "Invalid checkpoint did not time out: $errorText"
            }
            if (Test-Path -LiteralPath $raw) { throw 'Failed request published a result' }
            if ($null -ne $worker.Process) { throw 'Failed worker was retained' }
        }
    }
    # A worker can also exit between requests
    $worker.Process.Kill()
    $worker.Process.WaitForExit()
    $result = Invoke-MetadataWorker -Worker $worker -Request @{ request_id = 'after-exit'; action = 'ok' } -RawOutputPath (Join-Path $root 'after-exit.json') -LogPath (Join-Path $root 'after-exit.log')
    if ($result.request_id -ne 'after-exit') { throw 'Exited worker was not replaced' }
} finally {
    Stop-MetadataWorkerProcess -Worker $worker
}
Write-Host 'PASS metadata worker progress watchdog, invalid/stale checkpoint rejection, and crash/exit recovery'

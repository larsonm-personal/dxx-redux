#!/usr/bin/env pwsh
Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$androidRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $androidRoot 'helpers/normalized_json_text.ps1')
. (Join-Path $androidRoot 'helpers/host_metadata_worker.ps1')
. (Join-Path $androidRoot 'helpers/test_host_platform.ps1')
$root = Join-Path $androidRoot ('temp/test_host_metadata_worker-' + [guid]::NewGuid().ToString('N'))
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
    # Exercise the real stop helper with harmless EOF-driven children.
    $python = Resolve-RegressionPythonCommand
    if (-not $python) { throw 'Python 3 is required for metadata worker shutdown fixtures' }
    $shutdownFixture = Join-Path $PSScriptRoot 'fixtures/metadata_shutdown_child.py'
    foreach ($mode in @('slow', 'flood', 'nonzero', 'timeout')) {
        $child = New-MetadataWorker -Executable $python.Path -Arguments (@($python.PrefixArguments) + @($shutdownFixture, $mode))
        if (-not $child.DiagnosticsDirectory) { $child.DiagnosticsDirectory = Join-Path $root 'shutdown' }
        $childId = $child.Process.Id
        $failure = $null
        try { Stop-MetadataWorkerProcess -Worker $child -TimeoutMilliseconds $(if ($mode -eq 'timeout') { 100 } else { 10000 }) }
        catch { $failure = $_.Exception.Message }
        $record = $child.LastShutdown
        if (($mode -in @('nonzero', 'timeout')) -ne [bool]$failure -or $null -ne $child.Process -or
            -not $record.stdout_complete -or -not $record.stderr_complete -or
            $record.timed_out -ne ($mode -eq 'timeout')) { throw "Invalid $mode shutdown result: $failure" }
        if ($mode -eq 'nonzero' -and ($record.exit_code -ne 23 -or $failure -notmatch 'shutdown diagnostic')) { throw 'Lost nonzero shutdown diagnostic' }
        if ($mode -in @('slow', 'flood') -and $record.exit_code -ne 0) { throw "Unexpected $mode shutdown exit" }
        if ($mode -eq 'slow' -and $record.shutdown_ms -lt 2000) { throw 'Slow shutdown fixture did not exercise the former two-second limit' }
        $stdout = [IO.File]::ReadAllText((Join-Path $child.DiagnosticsDirectory "$childId.stdout.log"))
        $stderr = [IO.File]::ReadAllText((Join-Path $child.DiagnosticsDirectory "$childId.stderr.log"))
        if ($mode -ne 'timeout' -and ($stdout -notmatch 'shutdown stdout' -or $stderr -notmatch 'shutdown diagnostic')) { throw 'Shutdown streams were not preserved' }
        if ($mode -eq 'flood' -and ($stdout.Length -lt 262144 -or $stderr.Length -lt 262144)) { throw 'Shutdown pipe drain truncated output' }
    }
    # A worker can also exit between requests
    $worker.Process.Kill()
    $worker.Process.WaitForExit()
    $result = Invoke-MetadataWorker -Worker $worker -Request @{ request_id = 'after-exit'; action = 'ok' } -RawOutputPath (Join-Path $root 'after-exit.json') -LogPath (Join-Path $root 'after-exit.log')
    if ($result.request_id -ne 'after-exit') { throw 'Exited worker was not replaced' }
} finally {
    Stop-MetadataWorkerProcess -Worker $worker
    Remove-Item -LiteralPath $root -Recurse -Force
}
Write-Host 'PASS metadata worker progress watchdog, invalid/stale checkpoint rejection, and crash/exit recovery'

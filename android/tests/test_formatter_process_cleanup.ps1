#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/formatter_processes.ps1')
. (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')
$root = Join-Path $repoRoot ('android/temp/formatter_cleanup/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix run_
New-Item -ItemType Directory -Path $root -Force | Out-Null
$producerLock = [IO.File]::Open((Join-Path $root 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
$children = @()
try {
    $mine = Join-Path $root 'checkout space'
    $other = "$mine-other"
    $record = [pscustomobject]@{ ProcessId = 123; Name = 'pwsh'; CommandLine = ''; Arguments = @('pwsh', '-File', './android/run-code-quality.ps1'); WorkingDirectory = $mine }
    if (-not (Test-DxxFormatterProcess -Record $record -RepositoryRoot $mine)) { throw 'Relative -File was not resolved' }
    $record.Arguments = @('pwsh', '-Command', "& './android/run-code-quality.ps1' -Fix")
    if (-not (Test-DxxFormatterProcess -Record $record -RepositoryRoot $mine)) { throw 'Relative -Command was not resolved' }
    $record.Arguments = @('pwsh', '-Command', "Write-Output 'run-code-quality.ps1'")
    if (Test-DxxFormatterProcess -Record $record -RepositoryRoot $mine) { throw 'A quoted mention was mistaken for invocation' }
    $record.Arguments = @('pwsh', '-File', (Join-Path $other 'android/run-code-quality.ps1'))
    if (Test-DxxFormatterProcess -Record $record -RepositoryRoot $mine) { throw 'Sibling checkout crossed the ownership boundary' }
    $record.Arguments = @()
    $record.WorkingDirectory = ''
    $identity = [pscustomobject]@{ pid = 123; process_start_ticks = '456'; repository_root = $mine }
    if (Test-DxxFormatterProcess -Record $record -RepositoryRoot $mine -Lock $identity -StartTicks '457') { throw 'Stale formatter identity matched' }
    if (-not (Test-DxxFormatterProcess -Record $record -RepositoryRoot $mine -Lock $identity -StartTicks '456')) { throw 'Valid lock identity did not match without cwd' }
    foreach ($invocation in @(
            @('python', '-m', 'ruff', 'format', 'source.py'),
            @('rustup', 'run', '1.96.1', 'rustfmt', 'source.rs'),
            @('node', 'android/tools/code-quality/format-text.mjs', 'files.json', 'fix')
        )) {
        $record.Name = $invocation[0]
        $record.Arguments = $invocation
        $record.WorkingDirectory = $mine
        if (-not (Test-DxxFormatterProcess -Record $record -RepositoryRoot $mine)) { throw 'New formatter process was not recognized' }
        $record.WorkingDirectory = $other
        if (Test-DxxFormatterProcess -Record $record -RepositoryRoot $mine) { throw 'New formatter crossed checkout boundary' }
    }
    $worker = @'
$root = Split-Path $PSScriptRoot
$start = [Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
$start.UseShellExecute = $false
foreach ($arg in @('-NoProfile', '-Command', 'Start-Sleep -Seconds 90')) { $start.ArgumentList.Add($arg) }
$child = [Diagnostics.Process]::Start($start)
New-Item -ItemType Directory -Path (Join-Path $root 'android/temp') -Force | Out-Null
@{ pid = $PID; process_start_ticks = (Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks.ToString(); repository_root = $root } | ConvertTo-Json | Set-Content (Join-Path $root 'android/temp/run-code-quality.lock.json')
$readyPath = Join-Path $root 'ready.json'
[IO.File]::WriteAllText("$readyPath.tmp", (@{ parent = $PID; child = $child.Id } | ConvertTo-Json))
[IO.File]::Move("$readyPath.tmp", $readyPath)
$child.WaitForExit()
'@
    foreach ($checkout in @($mine, $other)) {
        New-Item -ItemType Directory -Path (Join-Path $checkout 'android') -Force | Out-Null
        [IO.File]::WriteAllText((Join-Path $checkout 'android/run-code-quality.ps1'), $worker)
        $start = [Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
        $start.UseShellExecute = $false
        $start.WorkingDirectory = $checkout
        Set-HeadlessProcessArguments -StartInfo $start -Arguments @('-NoProfile', '-Command', "& './android/run-code-quality.ps1'")
        $process = [Diagnostics.Process]::Start($start)
        $children += $process
        $deadline = [DateTime]::UtcNow.AddSeconds(20)
        while (-not (Test-Path -LiteralPath (Join-Path $checkout 'ready.json'))) {
            if ($process.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw 'Formatter fixture did not become ready' }
            Start-Sleep -Milliseconds 50
        }
        $ready = Get-Content -LiteralPath (Join-Path $checkout 'ready.json') -Raw | ConvertFrom-Json
        $children += Get-Process -Id $ready.child
    }
    if ([Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Linux)) {
        Remove-Item -LiteralPath (Join-Path $mine 'android/temp/run-code-quality.lock.json')
    }
    $stopper = Join-Path $repoRoot 'android/helpers/stop-stale-formatters.ps1'
    $preview = & (Get-Process -Id $PID).Path -NoProfile -File $stopper -RepositoryRoot $mine 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or $preview -notmatch "PID=$($children[0].Id)\b" -or $preview -match "PID=$($children[2].Id)\b") { throw "Incorrect formatter preview: $preview" }
    if ($children[0].HasExited -or $children[2].HasExited) { throw 'Preview stopped a formatter' }
    $result = & (Get-Process -Id $PID).Path -NoProfile -File $stopper -RepositoryRoot $mine -Kill 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { throw "Formatter cleanup failed: $result" }
    foreach ($process in $children[0..1]) { if (-not $process.WaitForExit(5000)) { throw 'Matched formatter or descendant survived' } }
    foreach ($process in $children[2..3]) { if ($process.HasExited) { throw 'Cleanup stopped a sibling checkout process' } }
    Write-Host 'Formatter preview, relative invocation, identity, checkout isolation and descendant cleanup passed'
} finally {
    foreach ($process in $children) {
        Stop-RegressionChildProcess -Process $process
        $process.Dispose()
    }
    $producerLock.Dispose()
    Remove-Item -LiteralPath $root -Recurse -Force
}

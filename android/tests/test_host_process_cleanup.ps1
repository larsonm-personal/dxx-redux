#!/usr/bin/env pwsh
# Verify real process discovery and cleanup refusal using an isolated repository
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/host_processes.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
$fixture = Join-Path $repoRoot "temp/host process fixture-$([guid]::NewGuid().ToString('N'))"
$child = $null
try {
    $null = New-Item -ItemType Directory -Path (Join-Path $fixture 'temp') -Force
    & git -C $fixture init --quiet
    if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize fixture' }
    [IO.File]::WriteAllText((Join-Path $fixture '.gitignore'), "temp/`n")
    $artifact = Join-Path $fixture 'temp/disposable.bin'
    [IO.File]::WriteAllText($artifact, 'fixture')
    $worker = Join-Path $fixture 'test_active.ps1'
    $ready = Join-Path $fixture 'ready'
    [IO.File]::WriteAllText($worker, '[IO.File]::WriteAllText((Join-Path $PSScriptRoot "ready"), "ready"); Start-Sleep -Seconds 60')
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = Get-RegressionCurrentPwshPath
    $startInfo.UseShellExecute = $false
    $startInfo.Arguments = '-NoProfile -File "' + $worker + '"'
    $child = [Diagnostics.Process]::Start($startInfo)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while (-not (Test-Path -LiteralPath $ready)) {
        if ($child.HasExited -or $timer.Elapsed.TotalSeconds -gt 15) { throw 'Worker did not start' }
        Start-Sleep -Milliseconds 50
    }
    $record = @(Get-DxxHostProcessInventory | Where-Object ProcessId -eq $child.Id)
    if ($record.Count -ne 1 -or $record[0].ParentProcessId -ne $PID -or -not $record[0].CommandLine.Contains($worker)) {
        throw 'Process inventory lost child identity or command line'
    }
    $blocked = $false
    try {
        & (Join-Path $repoRoot 'android/clean-workspace.ps1') -RepositoryRoot $fixture -TemporaryOnly -BusyWaitSeconds 0
    } catch {
        $blocked = $_.Exception.Message -match 'processes are active' -and
        $_.Exception.Message -match "\b$($child.Id)\b"
    }
    if (-not $blocked -or -not (Test-Path -LiteralPath $artifact)) { throw 'Cleanup did not protect active worker' }
    $child.Kill()
    $child.WaitForExit()
    # Other unrelated native jobs may still be busy, so deletion is covered by the synthetic suite
    if (@(Get-DxxHostProcessInventory | Where-Object ProcessId -eq $child.Id).Count) { throw 'Exited child remains in inventory' }
    . (Join-Path $repoRoot 'android/helpers/output_disk_space.ps1')
    if ([Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Linux)) {
        $mount = [IO.DriveInfo]::GetDrives() | Where-Object Name -eq '/dev/shm' | Select-Object -First 1
        if ($mount) {
            $reserve = [Math]::Ceiling($mount.AvailableFreeSpace / 1GB) + 1
            $rejected = $false
            try { Assert-OutputDiskSpace -Paths '/dev/shm/dxx-uncreated-output' -MinimumFreeGB $reserve } catch {
                $rejected = $_.Exception.Message -match '^Insufficient output disk space on /dev/shm:'
            }
            if (-not $rejected) { throw 'Output reserve did not use the actual mounted filesystem' }
        }
    }
    Write-Host 'PASS: real process identity, cleanup refusal, exit detection, and output volume checks'
} finally {
    if ($child) {
        if (-not $child.HasExited) { $child.Kill(); $child.WaitForExit() }
        $child.Dispose()
    }
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}

#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    Write-Host 'RESULT: SKIP (Windows Job Object lifetime requires Windows; Linux supervision is covered by test_headless_process_pool)'
    exit 2
}
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')
$fixture = Join-Path $PSScriptRoot 'fixtures/process_lifetime_worker.ps1'
$output = Join-Path $repoRoot "android/temp/process_lifetime/run_$([guid]::NewGuid().ToString('N'))"
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output -DirectoryPrefix run_
New-Item -ItemType Directory -Path $output -Force | Out-Null
$producerLock = $null
try {
    $producerLock = [IO.File]::Open((Join-Path $output 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    foreach ($mode in @('pool', 'stage', 'error')) {
        $directory = New-Item -ItemType Directory -Path (Join-Path $output $mode)
        $start = [Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
        Set-HeadlessProcessArguments -StartInfo $start -Arguments @('-NoProfile', '-File', $fixture, '-Mode', $mode, '-OutputRoot', $directory.FullName)
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        $owner = [Diagnostics.Process]::Start($start)
        $descendants = @()
        try {
            $marker = Join-Path $directory.FullName 'child.json'
            $deadline = [DateTime]::UtcNow.AddSeconds(20)
            while (-not (Test-Path -LiteralPath $marker)) {
                if ($owner.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw "$mode owner did not start its descendants" }
                Start-Sleep -Milliseconds 50
            }
            $descendants = @(Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json | ForEach-Object {
                    # Error cleanup may already have retired the child before we read its PID
                    Get-Process -Id $_ -ErrorAction SilentlyContinue
                })
            if ($mode -ne 'error' -and $descendants.Count -ne 2) { throw "$mode descendants exited before owner termination" }
            if ($mode -eq 'error') {
                while (-not (Test-Path -LiteralPath (Join-Path $directory.FullName 'returned'))) {
                    if ($owner.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw 'Callback failure did not unwind the pool' }
                    Start-Sleep -Milliseconds 50
                }
            } else {
                # Kill only the owner, bypassing PowerShell finally and tree-kill helpers
                $owner.Kill()
                $owner.WaitForExit()
            }
            foreach ($process in $descendants) {
                if (-not $process.WaitForExit(5000)) { throw "$mode orphan survived owner termination: $($process.Id)" }
            }
            Write-Host "PASS $mode cleanup kills child and grandchild"
        } finally {
            Stop-RegressionChildProcess -Process $owner
            $owner.Dispose()
            foreach ($process in $descendants) {
                Stop-RegressionChildProcess -Process $process
                $process.Dispose()
            }
        }
    }
} finally {
    if ($producerLock) { $producerLock.Dispose() }
    Remove-Item -LiteralPath $output -Recurse -Force
}

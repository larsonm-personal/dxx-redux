#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/test_execution_evidence.ps1')
. (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')
$root = Join-Path $repoRoot ('android/temp/test_execution_evidence/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix run_
New-Item -ItemType Directory -Path $root -Force | Out-Null
$producerLock = [IO.File]::Open((Join-Path $root 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    $path = Join-Path $root 'evidence.json'
    $observation = @{ name = 'alpha'; status = 'PASS'; started_utc = '2026-01-01T00:00:00.0000000Z'; observed_utc = '2026-01-01T00:00:01.0000000Z' }
    Write-TestExecutionEvidence -Path $path -HostKey fixture -Observations @($observation)
    $orphan = Join-Path $root ('.evidence.json.' + [guid]::NewGuid().ToString('N') + '.tmp')
    [IO.File]::WriteAllText($orphan, 'interrupted publication')
    $observation.status = 'FAIL'
    $observation.observed_utc = '2026-01-01T00:00:02.0000000Z'
    Write-TestExecutionEvidence -Path $path -HostKey fixture -Observations @($observation)
    if (Test-Path -LiteralPath $orphan) { throw 'Interrupted publication scratch survived recovery' }
    $observation.status = 'PASS'
    $observation.observed_utc = '2026-01-01T00:00:01.0000000Z'
    Write-TestExecutionEvidence -Path $path -HostKey fixture -Observations @($observation)
    $document = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if ($document.observations.Count -ne 1 -or $document.observations[0].status -ne 'FAIL') { throw 'Stale completion replaced the latest failure' }
    foreach ($name in @('beta', 'gamma')) {
        $observation.name = $name
        $observation.status = if ($name -eq 'beta') { 'SKIP' } else { 'RUNNING' }
        $observation.started_utc = '2026-01-02T00:00:00.0000000Z'
        $observation.observed_utc = '2026-01-02T00:00:01.0000000Z'
        Write-TestExecutionEvidence -Path $path -HostKey fixture -Observations @($observation) -MaxEntries 2
    }
    $document = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if ($document.observations.Count -ne 2 -or $document.evicted_observations -ne 1 -or 'PASS' -in $document.observations.status -or 'RUNNING' -notin $document.observations.status -or 'alpha' -in $document.observations.name -or 'beta' -notin $document.observations.name -or 'gamma' -notin $document.observations.name) { throw 'Bounds or interrupted/skip status changed' }
    $observation.name = 'large'
    $observation.reason = 'x' * 800
    Write-TestExecutionEvidence -Path $path -HostKey fixture -Observations @($observation) -MaxBytes 1500
    if ((Get-Item -LiteralPath $path).Length -gt 1500) { throw 'Byte bound failed' }
    [IO.File]::WriteAllText($path, '{corrupt')
    $rejected = $false
    try { Write-TestExecutionEvidence -Path $path -HostKey fixture -Observations @($observation) } catch { $rejected = $true }
    if (-not $rejected -or [IO.File]::ReadAllText($path) -cne '{corrupt') { throw 'Corrupt evidence was silently replaced' }
    Remove-Item -LiteralPath $path

    $worker = Join-Path $root 'worker.ps1'
    [IO.File]::WriteAllText($worker, @'
param($Helper, $Path, $Name)
$ErrorActionPreference = 'Stop'
. $Helper
foreach ($number in 1..5) {
    $item = @{ name = $Name; status = 'PASS'; started_utc = '2026-01-01T00:00:00.0000000Z'; observed_utc = [DateTime]::UtcNow.ToString('o') }
    Write-TestExecutionEvidence -Path $Path -HostKey fixture -Observations @($item)
}
'@)
    $execution = @{ Failures = @() }
    $tasks = @(foreach ($name in @('first', 'second')) {
            [pscustomobject]@{ FilePath = Get-RegressionCurrentPwshPath; Arguments = @('-NoProfile', '-File', $worker, (Join-Path $repoRoot 'android/helpers/test_execution_evidence.ps1'), $path, $name); WorkingDirectory = $repoRoot; TimeoutSeconds = 30 }
        })
    Invoke-HeadlessProcessPool -Tasks $tasks -MaxParallel 2 -OnCompleted {
        param($task, $result)
        if ($result.TimedOut -or $result.ExitCode -ne 0) { $execution.Failures += $result.StandardError }
    }
    if ($execution.Failures.Count) { throw "Concurrent writers failed: $($execution.Failures)" }
    $document = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if ($document.observations.Count -ne 2) { throw 'Concurrent writer lost another test observation' }

    $reports = Join-Path $root 'reports'
    foreach ($filter in @('test_d2xxl_sound_format', 'test_download_verification', 'test_d2xxl_tga_layout')) {
        $execution = @{ Result = $null }
        $task = [pscustomobject]@{ FilePath = Get-RegressionCurrentPwshPath; Arguments = @('-NoProfile', '-File', (Join-Path $repoRoot 'android/run_all_tests.ps1'), '-HostOnly', '-Filter', $filter, '-ReportDir', $reports); WorkingDirectory = $repoRoot; TimeoutSeconds = 45 }
        Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
            param($task, $result)
            $execution.Result = $result
        }
        if ($execution.Result.TimedOut -or $execution.Result.ExitCode -ne 0) { throw "Master evidence integration failed: $($execution.Result.StandardOutput) $($execution.Result.StandardError)" }
    }
    $files = @(Get-ChildItem -LiteralPath $reports -Filter 'execution_evidence_*.json')
    if ($files.Count -ne 1) { throw 'Missing host-specific execution evidence' }
    Remove-Item -Path (Join-Path $reports '*.log'), (Join-Path $reports '*.md') -Force
    $document = Get-Content -LiteralPath $files[0].FullName -Raw | ConvertFrom-Json
    if ($document.observations.Count -ne 3) { throw 'Filtered master run discarded unrelated prior evidence' }
    foreach ($item in $document.observations) {
        $expectedSkip = $item.name -eq 'test_d2xxl_tga_layout' -and -not (Test-RegressionWindowsHost)
        $expectedStatus = if ($expectedSkip) { 'SKIP' } else { 'PASS' }
        $expectedExit = if ($expectedSkip) { 2 } else { 0 }
        if ($expectedSkip -and -not $item.reason) { throw 'Master lost the capability skip reason' }
        if ($item.status -ne $expectedStatus -or $item.requires -ne 'none' -or $item.source_sha256 -notmatch '^[0-9a-f]{64}$' -or $item.commit -notmatch '^[0-9a-f]{40}$' -or $item.exit_code -ne $expectedExit -or -not $item.runtime) { throw 'Incomplete master execution provenance' }
    }
    if (@(Get-ChildItem -LiteralPath $reports -Force -Filter '*.tmp').Count -or @(Get-ChildItem -LiteralPath $reports -Force -Filter '*.bak').Count) { throw 'Atomic publication leaked temporary files' }
    Write-Host 'PASS: bounded evidence, status fidelity, stale updates, concurrent writers and master integration'
} finally {
    $producerLock.Dispose()
    Remove-Item -LiteralPath $root -Recurse -Force
}

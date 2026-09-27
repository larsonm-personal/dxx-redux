#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')
$root = Join-Path $repoRoot ('android/temp/sdk_writer/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix run_
New-Item -ItemType Directory -Path $root -Force | Out-Null
$producerLock = [IO.File]::Open((Join-Path $root 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
$lock = $null
$previousWrapper = $env:DXX_TEST_SDK_WRAPPER
$previousPlatform = $env:DXX_TEST_SDK_PLATFORM
try {
    $fixture = Join-Path $root 'checkout space'
    $helpers = Join-Path $fixture 'android/get_deps/helpers'
    $sdkLock = Join-Path $root 'dependencies/android-sdk/cmdline-tools/.dxx-install-state/lock'
    New-Item -ItemType Directory -Path $helpers, (Split-Path $sdkLock) -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $fixture 'dependency_base.txt'), (Join-Path $root 'dependencies'))
    $worker = @'
#!/usr/bin/env bash
set -euo pipefail
[ -n "${DXX_SDK_WRITER_PARENT_PID:-}" ]
[ "$GET_ALL_RUNNING" = 1 ]
printf 'start %s\n' "$(basename "$0")" >>events
sleep 1
printf 'end %s\n' "$(basename "$0")" >>events
'@
    foreach ($name in @('get_sdk.sh', 'create_avd.sh')) { [IO.File]::WriteAllText((Join-Path $helpers $name), $worker.Replace("`r", '')) }
    $wrapper = Join-Path $repoRoot 'android/get_deps/helpers/invoke_sdk_writer.ps1'
    $tasks = @(foreach ($name in @('get_sdk.sh', 'create_avd.sh')) {
            [pscustomobject]@{ FilePath = Get-RegressionCurrentPwshPath; Arguments = @('-NoProfile', '-File', $wrapper, '-RepoRoot', $fixture, '-ScriptName', $name, '-LockWaitSeconds', '10'); WorkingDirectory = $repoRoot; TimeoutSeconds = 30 }
        })
    $execution = @{ Results = @() }
    Invoke-HeadlessProcessPool -Tasks $tasks -MaxParallel 2 -OnCompleted {
        param($task, $result)
        $execution.Results += $result
    }
    if (@($execution.Results | Where-Object { $_.TimedOut -or $_.ExitCode -ne 0 }).Count) { throw "SDK writers failed: $($execution.Results.StandardError)" }
    $events = @(Get-Content -LiteralPath (Join-Path $fixture 'events'))
    if ($events.Count -ne 4 -or $events[0] -notlike 'start *' -or $events[1] -cne $events[0].Replace('start ', 'end ') -or $events[2] -notlike 'start *' -or $events[3] -cne $events[2].Replace('start ', 'end ')) { throw 'SDK writers overlapped' }

    $lock = [IO.File]::Open($sdkLock, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $task = $tasks[0]
    $task.Arguments[-1] = '0'
    $execution.Results = @()
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        $execution.Results += $result
    }
    if ($execution.Results[0].TimedOut -or $execution.Results[0].ExitCode -eq 0 -or @(Get-Content -LiteralPath (Join-Path $fixture 'events')).Count -ne 4) { throw 'SDK writer bypassed another lock holder' }
    $lock.Dispose()
    $lock = $null

    $env:DXX_TEST_SDK_PLATFORM = (Join-Path $repoRoot 'android/get_deps/helpers/platform.sh').Replace('\', '/')
    [IO.File]::WriteAllText((Join-Path $helpers 'get_sdk.sh'), @'
#!/usr/bin/env bash
set -e
source "$DXX_TEST_SDK_PLATFORM"
mkdir -p other-tools
begin_dependency_install "$PWD/other-tools/latest"
'@)
    $execution.Results = @()
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        $execution.Results += $result
    }
    if ($execution.Results[0].TimedOut -or $execution.Results[0].ExitCode -eq 0 -or $execution.Results[0].StandardOutput -notmatch 'lock does not cover' -or (Test-Path -LiteralPath (Join-Path $fixture 'other-tools/.dxx-install-state'))) { throw 'SDK lock was reused for an unrelated destination' }
    [IO.File]::WriteAllText((Join-Path $helpers 'get_sdk.sh'), "#!/usr/bin/env bash`nexit 37`n")
    $execution.Results = @()
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        $execution.Results += $result
    }
    if ($execution.Results[0].ExitCode -ne 37) { throw 'SDK writer lost the child failure code' }
    [IO.File]::WriteAllText((Join-Path $helpers 'get_sdk.sh'), "#!/usr/bin/env bash`necho started > timeout-started`nsleep 30`necho survived > timeout-survivor`n")
    $task.Arguments += @('-TimeoutSeconds', '5')
    $execution.Results = @()
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        $execution.Results += $result
    }
    if ($execution.Results[0].TimedOut -or $execution.Results[0].ExitCode -eq 0 -or $execution.Results[0].StandardError -notmatch 'SDK writer timed out') { throw "SDK writer did not enforce its own timeout: $($execution.Results[0] | ConvertTo-Json -Compress)" }
    if (-not (Test-Path -LiteralPath (Join-Path $fixture 'timeout-started'))) { throw 'Timeout test did not start the SDK writer' }
    $lock = [IO.File]::Open($sdkLock, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    if (Test-Path -LiteralPath (Join-Path $fixture 'timeout-survivor')) { throw 'Timed-out SDK writer continued' }
    $lock.Dispose()
    $lock = $null
    $journal = Split-Path $sdkLock
    $previous = Join-Path $journal 'work/previous'
    New-Item -ItemType Directory -Path $previous -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $journal 'format'), 'dxx-install-v1')
    [IO.File]::WriteAllText((Join-Path $journal 'work/destination'), 'latest')
    [IO.File]::WriteAllText((Join-Path $previous 'restored-marker'), 'previous tools')
    [IO.File]::WriteAllText((Join-Path $helpers 'create_light_avds.ps1'), @'
param([switch]$Force, [string]$AvdName)
if (-not $env:DXX_SDK_WRITER_PARENT_PID -or $env:GET_ALL_RUNNING -ne '1' -or -not $Force -or $AvdName -ne 'Nexus5X_Light_2') { throw 'PowerShell writer arguments or ownership were lost' }
'@)
    $task.Arguments = @('-NoProfile', '-File', $wrapper, '-RepoRoot', $fixture, '-ScriptName', 'create_light_avds.ps1', '-Force', '-AvdName', 'Nexus5X_Light_2')
    $execution.Results = @()
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        $execution.Results += $result
    }
    if ($execution.Results[0].TimedOut -or $execution.Results[0].ExitCode -ne 0) { throw "PowerShell writer failed: $($execution.Results[0].StandardError)" }
    $restored = Join-Path (Split-Path $journal) 'latest/restored-marker'
    if (-not (Test-Path -LiteralPath $restored) -or (Test-Path -LiteralPath (Join-Path $journal 'work'))) { throw 'PowerShell SDK consumer did not recover interrupted publication' }
    # Exercise the real AVD entry point without allocating any AVD payload
    foreach ($file in @('create_light_avds.ps1', 'Get-DepPlatform.ps1')) {
        Copy-Item -LiteralPath (Join-Path $repoRoot "android/get_deps/helpers/$file") -Destination $helpers -Force
    }
    $env:DXX_TEST_SDK_WRAPPER = $wrapper
    [IO.File]::WriteAllText((Join-Path $helpers 'invoke_sdk_writer.ps1'), @'
param($RepoRoot, $ScriptName, [switch]$Force, [string]$AvdName)
& $env:DXX_TEST_SDK_WRAPPER @PSBoundParameters
exit $LASTEXITCODE
'@)
    $bin = Join-Path $root 'dependencies/android-sdk/cmdline-tools/latest/bin'
    New-Item -ItemType Directory -Path $bin -Force | Out-Null
    if (Test-RegressionWindowsHost) {
        [IO.File]::WriteAllText((Join-Path $bin 'avdmanager.bat'), "@echo off`r`necho Name: Nexus5X_Light_1`r`necho Name: Nexus5X_Light_2`r`n")
    } else {
        $manager = Join-Path $bin 'avdmanager'
        [IO.File]::WriteAllText($manager, "#!/usr/bin/env bash`necho 'Name: Nexus5X_Light_1'`necho 'Name: Nexus5X_Light_2'`n")
        & chmod +x $manager
        if ($LASTEXITCODE -ne 0) { throw 'Could not prepare native AVD fixture' }
    }
    $task.Arguments = @('-NoProfile', '-File', (Join-Path $helpers 'create_light_avds.ps1'))
    $execution.Results = @()
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        $execution.Results += $result
    }
    if ($execution.Results[0].TimedOut -or $execution.Results[0].ExitCode -ne 0 -or $execution.Results[0].StandardOutput -notmatch "AVD 'Nexus5X_Light_2' already exists") { throw "AVD entry point re-entry failed: $($execution.Results[0].StandardError)" }
    Write-Host 'PASS: SDK writer serialization, contention, failure, timeout release, PowerShell forwarding and cached AVD entry point'
} finally {
    $env:DXX_TEST_SDK_WRAPPER = $previousWrapper
    $env:DXX_TEST_SDK_PLATFORM = $previousPlatform
    if ($lock) { $lock.Dispose() }
    $producerLock.Dispose()
    Remove-Item -LiteralPath $root -Recurse -Force
}

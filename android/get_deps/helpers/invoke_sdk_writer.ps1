#!/usr/bin/env pwsh
# Serialize SDK producers with cleanup and supervise their descendants
[CmdletBinding()]
param(
    [string]$RepoRoot = (Split-Path (Split-Path (Split-Path $PSScriptRoot))),
    [Parameter(Mandatory)][ValidateSet('get_sdk.sh', 'finalize.sh', 'get_emulator.sh', 'create_avd.sh', 'create_light_avds.ps1')][string]$ScriptName,
    [ValidateRange(0, 300)][int]$LockWaitSeconds = 60,
    [ValidateRange(1, 7200)][int]$TimeoutSeconds = 3600,
    [switch]$Force,
    [ValidateSet('', 'Nexus5X_Light_1', 'Nexus5X_Light_2')][string]$AvdName = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Get-DepPlatform.ps1')
. (Join-Path $PSScriptRoot '../../helpers/sdk_package_inventory.ps1')
. (Join-Path $PSScriptRoot '../../helpers/headless_process_pool.ps1')
$RepoRoot = [IO.Path]::GetFullPath($RepoRoot)
$base = (Get-Content -LiteralPath (Join-Path $RepoRoot 'dependency_base.txt') -First 1).Trim()
$sdk = Join-Path $base 'android-sdk'
$lockPath = Join-Path $sdk 'cmdline-tools/.dxx-install-state/lock'
Assert-DxxPlainSdkPath $lockPath
New-Item -ItemType Directory -Path (Split-Path $lockPath) -Force | Out-Null
$lock = $null
$bootstrap = -not [string]::IsNullOrEmpty($env:GET_ALL_RUNNING)
$previousParent = $env:DXX_SDK_WRITER_PARENT_PID
$previousToolsParent = $env:DXX_SDK_WRITER_TOOLS_PARENT
$previousBootstrap = $env:GET_ALL_RUNNING
$execution = @{ Result = $null }
try {
    $deadline = [DateTime]::UtcNow.AddSeconds($LockWaitSeconds)
    while (-not $lock) {
        try { $lock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
        catch [IO.IOException] {
            if ([DateTime]::UtcNow -ge $deadline) { throw 'Another SDK writer owns the installation lock' }
            Start-Sleep -Milliseconds 100
        }
    }
    $env:DXX_SDK_WRITER_PARENT_PID = [string]$PID
    $env:DXX_SDK_WRITER_TOOLS_PARENT = [IO.Path]::GetFullPath((Join-Path $sdk 'cmdline-tools'))
    # Retention and interactive prompts must run after this wrapper releases the lock
    $env:GET_ALL_RUNNING = '1'
    $scriptPath = Join-Path $RepoRoot "android/get_deps/helpers/$ScriptName"
    if ($ScriptName.EndsWith('.ps1')) {
        # PowerShell consumers must recover command-line publication before use
        $recovery = [pscustomobject]@{
            FilePath = Get-BashCommandPath
            Arguments = @('-c', 'source "$1"; begin_dependency_install "$2/latest"', 'sdk-recovery', (Join-Path $PSScriptRoot 'platform.sh').Replace('\', '/'), $env:DXX_SDK_WRITER_TOOLS_PARENT.Replace('\', '/'))
            WorkingDirectory = $RepoRoot
            TimeoutSeconds = 30
        }
        Invoke-HeadlessProcessPool -Tasks @($recovery) -MaxParallel 1 -OnCompleted {
            param($task, $result)
            $execution.Result = $result
            if ($result.StandardError) { Write-Host $result.StandardError.TrimEnd() }
        }
        if ($execution.Result.TimedOut -or $execution.Result.ExitCode -ne 0) { throw 'SDK command-line transaction recovery failed' }

        $command = Get-RegressionCurrentPwshPath
        $arguments = @('-NoProfile', '-NonInteractive', '-File', $scriptPath)
        if ($Force) { $arguments += '-Force' }
        if ($AvdName) { $arguments += @('-AvdName', $AvdName) }
    } else {
        $command = Get-BashCommandPath
        $arguments = @($scriptPath.Replace('\', '/'))
    }
    $task = [pscustomobject]@{ FilePath = $command; Arguments = $arguments; WorkingDirectory = $RepoRoot; TimeoutSeconds = $TimeoutSeconds }
    Write-Host "Running SDK writer $ScriptName (timeout: ${TimeoutSeconds}s)"
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        $execution.Result = $result
        if ($result.StandardOutput) { Write-Host $result.StandardOutput.TrimEnd() }
        if ($result.StandardError) { Write-Host $result.StandardError.TrimEnd() }
    }
} finally {
    $env:DXX_SDK_WRITER_PARENT_PID = $previousParent
    $env:DXX_SDK_WRITER_TOOLS_PARENT = $previousToolsParent
    $env:GET_ALL_RUNNING = $previousBootstrap
    if ($lock) { $lock.Dispose() }
}
if ($execution.Result.TimedOut) { throw "SDK writer timed out after $TimeoutSeconds seconds" }
if ($execution.Result.ExitCode -ne 0) { exit $execution.Result.ExitCode }
if (-not $bootstrap -and $ScriptName -in @('finalize.sh', 'get_emulator.sh')) {
    & (Join-Path $RepoRoot 'android/get_deps/clean-sdk-packages.ps1') -RepoRoot $RepoRoot -RegisterCurrent -Apply
}
exit 0

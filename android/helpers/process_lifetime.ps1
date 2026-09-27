. (Join-Path $PSScriptRoot 'test_host_platform.ps1')

function Initialize-RegressionProcessLifetime {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { return }
    if (-not ('DxxRegression.ProcessLifetime' -as [type])) {
        Add-Type -Path (Join-Path $PSScriptRoot 'process_lifetime.cs')
    }
    # Enroll before starting children, avoiding a start-then-assign race
    # Windows closes the sole job handle even if the host is forcibly terminated
    [DxxRegression.ProcessLifetime]::Initialize()
}

function Stop-RegressionChildProcess {
    param([Parameter(Mandatory)][Diagnostics.Process]$Process)
    try { if ($Process.HasExited) { return } }
    catch [InvalidOperationException] { return } # Start failed before a process existed
    try { $Process.Kill($true) } catch {
        if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
            & taskkill.exe /PID $Process.Id /T /F *> $null
        } else { $Process.Kill() }
    }
    $Process.WaitForExit()
}

function Set-RegressionProcessLifetimeStartInfo {
    param([Parameter(Mandatory)][Diagnostics.ProcessStartInfo]$StartInfo)

    if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
        # A worker-local job also retires descendants when the immediate child exits
        # Tree enumeration alone can miss reparented Git Bash/MSYS descendants
        $payload = @{
            FileName = $StartInfo.FileName
            Arguments = $StartInfo.Arguments
            ArgumentList = @($StartInfo.ArgumentList)
        } | ConvertTo-Json -Compress
        $encoded = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($payload))
        $StartInfo.Arguments = ''
        if ($null -ne $StartInfo.ArgumentList) { $StartInfo.ArgumentList.Clear() }
        Set-HeadlessProcessArguments -StartInfo $StartInfo -Arguments @(
            '-NoProfile', '-NonInteractive', '-File', (Join-Path $PSScriptRoot 'process_lifetime_windows.ps1'), '-Payload', $encoded
        )
        $StartInfo.FileName = Get-RegressionCurrentPwshPath
        return
    }
    if (-not [Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Linux)) { return }
    if ($StartInfo.Arguments) { throw 'Linux supervision requires ArgumentList instead of a quoted Arguments string' }
    $python = Resolve-RegressionPythonCommand
    if (-not $python) { throw 'Python 3.9+ is required for Linux child supervision' }
    $command = @($StartInfo.FileName) + @($StartInfo.ArgumentList)
    $StartInfo.ArgumentList.Clear()
    foreach ($argument in @($python.PrefixArguments) + @('-I', (Join-Path $PSScriptRoot 'process_lifetime_linux.py'), [string]$PID, '--') + $command) {
        $StartInfo.ArgumentList.Add($argument)
    }
    $StartInfo.FileName = $python.Path
}

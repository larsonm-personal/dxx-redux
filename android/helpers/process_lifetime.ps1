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

#!/usr/bin/env pwsh
# test_helpers.ps1 -- Shared helper functions for Android emulator test scripts.
#
# Usage (dot-source from another script):
#   . "$PSScriptRoot\helpers\test_helpers.ps1"
#
# Provides:
#   $ADB, $PACKAGE, $ACTIVITY  -- standard paths/constants
#   Adb [-AdbArgs ...]         -- run adb with stderr tolerance
#   Adb-Timeout [-AdbArgs ...] -- run adb with a timeout (returns $null on hang)
#   Adb-Dev [-Serial ...]      -- run adb targeting a specific device
#   Adb-Dev-Timeout [...]      -- device-targeted adb with timeout
#   Test-EmulatorHealthy       -- check emulator process + adb + shell
#   Ensure-EmulatorHealthy     -- check + restart via emu_health.ps1 if needed
#   Test-DeviceOnline          -- check if a specific serial is online
#   Start-EmulatorIfNeeded     -- boot + provision an emulator if not running
#   Install-AppAndData         -- install APK + push game data to a device
#   Write-Status               -- timestamped colored output
#   Start-GameWithRetry        -- full launch flow: SetupActivity -> verify -> game -> verify

# Source shared environment setup (JAVA_HOME, cmake, cargo) and host helpers
. (Join-Path $PSScriptRoot "test_env.ps1")
. (Join-Path $PSScriptRoot "test_host_platform.ps1")
. (Join-Path $PSScriptRoot "standard_game_data.ps1")

$script:ANDROID_ROOT = Split-Path $PSScriptRoot
$script:REPO_ROOT = Split-Path $script:ANDROID_ROOT
$_depBaseFile = Join-Path $script:REPO_ROOT "dependency_base.txt"
if (-not (Test-Path $_depBaseFile)) {
    Write-Host "FAIL: dependency_base.txt not found at $_depBaseFile" -ForegroundColor Red
    exit 1
}
$script:DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
$script:ADB = Resolve-RegressionAndroidSdkTool -DepBase $script:DEP_BASE -Subdir "platform-tools" -ToolName "adb" -EnvironmentVariable "ADB"
$script:PACKAGE = "com.dxxredux.app"
$script:ACTIVITY = "com.dxxredux.app.SetupActivity"
$script:DEFAULT_SET_DIR = "files/imported/sets/default"
$script:PRIMARY_EMULATOR_SERIAL = "emulator-5554"
$script:SECONDARY_EMULATOR_SERIAL = "emulator-5556"
$script:PRIMARY_AVD_NAME = "Nexus5X_Light_1"
$script:SECONDARY_AVD_NAME = "Nexus5X_Light_2"

function Adb {
    param([string[]]$AdbArgs, [int]$Seconds = 30)
    return (Adb-Timeout -AdbArgs $AdbArgs -Seconds $Seconds -IncludeStandardError)
}

function Adb-Timeout {
    # Run an adb command with a timeout; returns $null if it hangs.
    # Uses ProcessStartInfo instead of Start-Job because Start-Job with
    # adb.exe hangs on Windows PowerShell 5.1 (pipe/handle inheritance issue).
    param([string[]]$AdbArgs, [int]$Seconds = 8, [switch]$IncludeStandardError)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $script:ADB
    $psi.Arguments = ($AdbArgs | ForEach-Object {
            if ($_ -match '\s') { "`"$_`"" } else { $_ }
        }) -join ' '
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    try {
        $proc = [System.Diagnostics.Process]::Start($psi)
        # Read both streams asynchronously to prevent buffer deadlock
        $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
        $stderrTask = $proc.StandardError.ReadToEndAsync()
        if (-not $proc.WaitForExit($Seconds * 1000)) {
            try { $proc.Kill() } catch {}
            return $null
        }
        # Process exited; wait for async reads to finish (already done since process exited)
        $out = $stdoutTask.GetAwaiter().GetResult()
        $err = $stderrTask.GetAwaiter().GetResult()
        $streams = if ($IncludeStandardError) { @($out, $err) } else { @($out) }
        return ($streams |
                Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
                ForEach-Object { $_.Trim() }) -join "`n"
    } catch {
        return $null
    }
}

function Test-DeviceFileMatches {
    param(
        [Parameter(Mandatory = $true)][string]$LocalPath,
        [Parameter(Mandatory = $true)][string]$DevicePath
    )

    $localFile = Get-Item -LiteralPath $LocalPath -ErrorAction Stop
    if (-not $localFile.PSIsContainer -and $localFile.Length -ge 0) {
        $deviceSize = Adb-Timeout -AdbArgs @('shell', "stat -c %s '$DevicePath'") -Seconds 10
        if (-not $deviceSize -or $deviceSize -notmatch '^\d+$' -or [long]$deviceSize -ne $localFile.Length) {
            return $false
        }

        $hashTimeoutSeconds = [Math]::Max(30, [int][Math]::Ceiling($localFile.Length / (4MB)) + 15)
        $deviceHashOutput = Adb-Timeout -AdbArgs @('shell', "sha256sum '$DevicePath'") -Seconds $hashTimeoutSeconds
        if ($deviceHashOutput -notmatch '^([0-9a-fA-F]{64})\s+') {
            return $false
        }
        $localHash = (Get-FileHash -LiteralPath $localFile.FullName -Algorithm SHA256).Hash
        return $Matches[1].Equals($localHash, [StringComparison]::OrdinalIgnoreCase)
    }
    return $false
}

function Push-VerifiedDeviceFile {
    param(
        [Parameter(Mandatory = $true)][string]$LocalPath,
        [Parameter(Mandatory = $true)][string]$DevicePath,
        [switch]$SkipPush
    )

    $localFile = Get-Item -LiteralPath $LocalPath -ErrorAction Stop
    if ($localFile.PSIsContainer) {
        Write-Status "FAIL: Cannot push a directory as a verified device file: $LocalPath" 'Red'
        return $false
    }

    if (Test-DeviceFileMatches -LocalPath $localFile.FullName -DevicePath $DevicePath) {
        $message = if ($SkipPush) { 'Skipping push (-SkipPush); device file is verified' } else { 'Device file already matches, skipping push' }
        Write-Status $message
        return $true
    }
    if ($SkipPush) {
        Write-Status "FAIL: Device file is missing or does not match: $DevicePath" 'Red'
        return $false
    }

    Write-Status "Pushing and verifying $($localFile.Name)..."
    # Size the command bound from the transfer itself. Large installer images
    # can legitimately exceed Adb's 30-second convenience default.
    $pushTimeoutSeconds = [Math]::Max(30, [int][Math]::Ceiling($localFile.Length / (2MB)) + 30)
    $pushOutput = Adb-Timeout -AdbArgs @('push', $localFile.FullName, $DevicePath) -Seconds $pushTimeoutSeconds -IncludeStandardError
    if ($pushOutput) { Write-Host $pushOutput }

    if (-not (Test-DeviceFileMatches -LocalPath $localFile.FullName -DevicePath $DevicePath)) {
        Write-Status "FAIL: Device file did not match after push: $DevicePath" 'Red'
        return $false
    }
    Write-Status 'Push verified'
    return $true
}

function Test-EmulatorHealthy {
    # Check if emulator process is running, adb sees it, and shell responds.
    $emuProc = Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -match 'qemu-system|emulator' -and (-not $_.Path -or $_.Path -match 'android|emulator')
    }
    if (-not $emuProc) { return $false }
    $devices = Adb-Timeout -AdbArgs "devices" -Seconds 5
    if ($devices -notmatch 'emulator-\d+\s+device') { return $false }
    $boot = Adb-Timeout -AdbArgs @("shell", "getprop", "sys.boot_completed") -Seconds 10
    if ($null -eq $boot -or $boot -ne "1") { return $false }
    $packageService = Adb-Timeout -AdbArgs @("shell", "cmd", "package", "list", "packages", "android") -Seconds 10
    if ($null -eq $packageService -or
        $packageService -match 'Can''t find service: package' -or
        $packageService -notmatch 'package:android') {
        return $false
    }
    return $true
}

function Test-AppPackageInstalled {
    $path = Adb-Timeout -AdbArgs @("shell", "pm", "path", $script:PACKAGE) -Seconds 10
    return ($path -and $path -match '^package:')
}

function Ensure-EmulatorHealthy {
    # Check emulator health; restart via emu_health.ps1 if needed.
    # Returns $true on success, exits on unrecoverable failure.
    Write-Status "Checking emulator health..."
    if (Test-EmulatorHealthy) {
        Write-Status "Emulator healthy" "Green"
        return $true
    }
    Write-Status "Emulator unhealthy -- attempting restart..." "Yellow"
    $healthScript = Join-Path $PSScriptRoot "emu_health.ps1"
    & $healthScript -Restart -Wait -TimeoutSeconds 120
    $emuExit = $LASTEXITCODE
    if ($emuExit -ne 0 -and $emuExit -ne 2) {
        Write-Status "FAIL: Emulator could not be restored (exit $emuExit)" "Red"
        exit 1
    }
    Write-Status "Emulator healthy" "Green"
    return $true
}

function Restart-AdbServer {
    Write-Status "Restarting ADB server..." "DarkGray"
    Get-Process adb -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
    Adb-Timeout -AdbArgs "start-server" -Seconds 10 | Out-Null
    Start-Sleep -Seconds 2
}

function Confirm-EmulatorHealthWithAdbRecovery {
    param([int]$RetryDelayMilliseconds = 2000)

    if (Test-EmulatorHealthy) { return $true }

    Write-Status "Health check failed, retrying..." "Yellow"
    if ($RetryDelayMilliseconds -gt 0) {
        Start-Sleep -Milliseconds $RetryDelayMilliseconds
    }
    if (Test-EmulatorHealthy) { return $true }

    Write-Status "Health check still failing, resetting ADB transport..." "Yellow"
    Restart-AdbServer
    return Test-EmulatorHealthy
}

function Invoke-LauncherStartupRecovery {
    param(
        [string]$Reason = "SetupActivity not responding",
        [int]$TimeoutSeconds = 180
    )

    Write-Status "$Reason -- restarting emulator for launcher recovery" "Yellow"
    Restart-AdbServer

    $healthScript = Join-Path $PSScriptRoot "emu_health.ps1"
    & $healthScript -Restart -Wait -ForceRestart -TimeoutSeconds $TimeoutSeconds -AvdName $script:PRIMARY_AVD_NAME
    $emuExit = $LASTEXITCODE
    if ($emuExit -ne 0 -and $emuExit -ne 2) {
        Write-Status "Launcher recovery failed: emulator restart exit $emuExit" "Red"
        return $false
    }

    Write-Status "Launcher recovery complete" "Green"
    return $true
}

function Start-PrimarySetupActivity {
    param([int]$TimeoutSeconds = 60)

    Stop-AppAndWait
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/setup_introspect.json") | Out-Null
    Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
    return Wait-SetupActivityReady -TimeoutSeconds $TimeoutSeconds
}

function Ensure-LauncherTestDeviceReady {
    param([int]$TimeoutSeconds = 60)

    Ensure-EmulatorHealthy | Out-Null
    if (-not (Test-AppPackageInstalled)) {
        Write-Status "Test APK is not installed -- installing it now" "Yellow"
        if (-not (Install-ApkOnDevice)) {
            return $false
        }
    }
    if (Start-PrimarySetupActivity -TimeoutSeconds $TimeoutSeconds) {
        Stop-AppAndWait
        return $true
    }

    if (-not (Invoke-LauncherStartupRecovery -Reason "SetupActivity preflight timed out")) {
        return $false
    }
    Ensure-EmulatorHealthy | Out-Null
    if (-not (Install-ApkOnDevice)) {
        return $false
    }
    if (-not (Start-PrimarySetupActivity -TimeoutSeconds $TimeoutSeconds)) {
        return $false
    }
    Stop-AppAndWait
    return $true
}

function Wait-ProcessDead {
    # Poll until app processes are gone after force-stop.
    param([int]$TimeoutMs = 5000)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $TimeoutMs) {
        $mainProcId = Adb-Timeout -AdbArgs @("shell", "pidof", $script:PACKAGE) -Seconds 3
        $gameProcId = Adb-Timeout -AdbArgs @("shell", "pidof", "$($script:PACKAGE):game") -Seconds 3
        if ((-not $mainProcId -or $mainProcId -notmatch '^\d+') -and
            (-not $gameProcId -or $gameProcId -notmatch '^\d+')) {
            return $true
        }
        Start-Sleep -Milliseconds 200
    }
    return $false
}

function Stop-AppAndWait {
    # Force-stop the app and wait for the process to actually die.
    Adb -AdbArgs @("shell", "am", "force-stop", $script:PACKAGE) | Out-Null
    Adb -AdbArgs @("shell", "am", "force-stop", "com.google.android.documentsui") | Out-Null
    Wait-ProcessDead | Out-Null
}

function Test-AppMainProcessRunning {
    $processId = Adb-Timeout -AdbArgs @("shell", "pidof", $script:PACKAGE) -Seconds 3
    return [bool]($processId -and $processId -match '^\d+')
}

function Test-AppGameProcessRunning {
    $processId = Adb-Timeout -AdbArgs @("shell", "pidof", "$($script:PACKAGE):game") -Seconds 3
    return [bool]($processId -and $processId -match '^\d+')
}

function Test-AppAutomationProcessRunning {
    return (Test-AppMainProcessRunning) -or (Test-AppGameProcessRunning)
}

function Wait-SetupCondition {
    # Poll setup_introspect.json until a condition is met. Returns $true if
    # the predicate returns $true, $false on timeout.
    # $Predicate receives the parsed JSON object and returns $true/$false.
    param(
        [Parameter(Mandatory)][scriptblock]$Predicate,
        [int]$TimeoutSeconds = 15,
        [int]$PollMs = 500
    )
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Adb-Timeout -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_INTROSPECT") -Seconds 5 | Out-Null
        Start-Sleep -Milliseconds $PollMs
        $json = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_introspect.json") -Seconds 3
        if ($json -and $json -match '^\s*\{') {
            try {
                $obj = $json | ConvertFrom-Json
                if (& $Predicate $obj) { return $true }
            } catch {}
        }
    }
    return $false
}

function Write-Status {
    param([string]$Msg, [string]$Color = "Cyan")
    $line = "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Msg"
    Write-Host $line -ForegroundColor $Color
    $logFileVar = Get-Variable -Name LogFile -Scope Script -ErrorAction SilentlyContinue
    if ($logFileVar -and $logFileVar.Value) {
        $line | Add-Content -Path $logFileVar.Value -Encoding utf8
    }
}

function Write-BoundedLines {
    param([string]$Text, [int]$Last = 40, [string]$Color = "Gray")
    if (-not $Text) {
        Write-Host "  (not available)" -ForegroundColor $Color
        return
    }
    ($Text -split "`n") | Select-Object -Last $Last | ForEach-Object {
        if ($_.Trim().Length -gt 0) {
            Write-Host "  $_" -ForegroundColor $Color
        }
    }
}

function Write-DeviceFailureDiagnostics {
    param([string]$Reason = "device failure")

    Write-Status "--- device diagnostics: $Reason ---" "Yellow"

    Write-Status "adb devices" "Yellow"
    Write-BoundedLines -Text (Adb-Timeout -AdbArgs @("devices", "-l") -Seconds 5) -Last 20

    Write-Status "device storage" "Yellow"
    Write-BoundedLines -Text (Adb-Timeout -AdbArgs @("shell", "df", "-h", "/data") -Seconds 5) -Last 20

    Write-Status "app memory" "Yellow"
    Write-BoundedLines -Text (Adb-Timeout -AdbArgs @(
            "shell", "sh", "-c",
            "dumpsys meminfo $($script:PACKAGE); dumpsys meminfo $($script:PACKAGE):game"
        ) -Seconds 10) -Last 80

    Write-Status "recent crash logcat" "Yellow"
    Write-BoundedLines -Text (Adb-Timeout -AdbArgs @(
            "logcat", "-d", "-t", "400", "-s",
            "AndroidRuntime:E", "libc:E", "DEBUG:*", "DXX:*"
        ) -Seconds 10) -Last 80

    Write-Status "recent tombstones" "Yellow"
    Write-BoundedLines -Text (Adb-Timeout -AdbArgs @(
            "shell", "sh", "-c", "ls -lt /data/tombstones 2>/dev/null | head -5"
        ) -Seconds 5) -Last 20

    Write-Status "automation log tail" "Yellow"
    Write-BoundedLines -Text (Adb-Timeout -AdbArgs @(
            "shell", "run-as", $script:PACKAGE, "cat", "files/automation_log.jsonl"
        ) -Seconds 5) -Last 30
}

# -- Multi-device helpers (for dual-emulator tests like test_mp, test_lan) --

function Adb-Dev {
    # Run adb targeting a specific device serial.
    param([string]$Serial, [string[]]$AdbArgs, [int]$Seconds = 30)
    return (Adb-Dev-Timeout -Serial $Serial -AdbArgs $AdbArgs -Seconds $Seconds -IncludeStandardError)
}

function Adb-Dev-Timeout {
    # Run adb targeting a specific device serial, with a timeout.
    param(
        [string]$Serial,
        [string[]]$AdbArgs,
        [int]$Seconds = 10,
        [switch]$IncludeStandardError
    )
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $script:ADB
    $allArgs = @("-s", $Serial) + $AdbArgs
    $psi.Arguments = ($allArgs | ForEach-Object {
            if ($_ -match '\s') { "`"$_`"" } else { $_ }
        }) -join ' '
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    try {
        $proc = [System.Diagnostics.Process]::Start($psi)
        $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
        $stderrTask = $proc.StandardError.ReadToEndAsync()
        if (-not $proc.WaitForExit($Seconds * 1000)) {
            try { $proc.Kill() } catch {}
            return $null
        }
        $out = $stdoutTask.GetAwaiter().GetResult()
        $err = $stderrTask.GetAwaiter().GetResult()
        $streams = if ($IncludeStandardError) { @($out, $err) } else { @($out) }
        return ($streams |
                Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
                ForEach-Object { $_.Trim() }) -join "`n"
    } catch {
        return $null
    }
}

function Test-DeviceOnline {
    param([string]$Serial)
    $devices = Adb-Timeout -AdbArgs "devices" -Seconds 5
    return $devices -match "$Serial\s+device"
}

function Send-MpCommand {
    # Send an MP_COMMAND broadcast to a specific emulator.
    param([string]$Serial, [string]$Command, [string[]]$Extras = @())
    $args_ = @("shell", "am", "broadcast", "-a", "com.dxxredux.MP_COMMAND",
        "--es", "command", $Command) + $Extras
    Adb-Dev-Timeout -Serial $Serial -AdbArgs $args_ -Seconds 10 | Out-Null
}

function Send-TapButton {
    # Tap a launcher button via accessibility click (MP_COMMAND tap_button).
    # The Kotlin handler dismisses the keyboard and scrolls down automatically.
    # Returns $true if the tap was acknowledged in logcat.
    param([string]$Serial, [string]$Text, [int]$PollMs = 2000, [int]$TimeoutSec = 15)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $shownAvailable = $false
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        Adb-Dev-Timeout -Serial $Serial -AdbArgs @("logcat", "-c") -Seconds 5 | Out-Null
        Send-MpCommand -Serial $Serial -Command "tap_button" -Extras @("--es", "text", $Text)
        Start-Sleep -Milliseconds $PollMs
        $log = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
            "logcat", "-d", "-t", "20", "-s", "DXX-MP:*"
        ) -Seconds 5
        if ($log -match "tap_button: tapped") { return $true }
        if ($log -match 'tap_button:.*not found \(available: ([^)]*)\)') {
            if (-not $shownAvailable) {
                Write-Status "  tap_button: '$Text' not found (available: $($Matches[1]))" "Gray"
                $shownAvailable = $true
            } else {
                Write-Status "  tap_button: '$Text' not found, retrying..." "Gray"
            }
        } elseif ($log -match "tap_button:.*not found") {
            Write-Status "  tap_button: '$Text' not found, retrying..." "Gray"
        }
    }
    Write-Status "  tap_button: '$Text' timed out after ${TimeoutSec}s" "Yellow"
    return $false
}

function Wait-ForCondition {
    # Poll until a condition is met, with configurable timeout and interval.
    param(
        [string]$Description,
        [scriptblock]$Condition,
        [int]$TimeoutSec = 30,
        [int]$PollMs = 1000
    )
    Write-Status "Waiting: $Description (timeout: ${TimeoutSec}s)"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        $result = & $Condition
        if ($result) {
            Write-Status "  OK: $Description" "Green"
            return $true
        }
        Start-Sleep -Milliseconds $PollMs
    }
    Write-Status "  TIMEOUT: $Description" "Red"
    return $false
}

function Wait-SetupReady {
    # Wait for SetupActivity to be ready on a specific device (multi-emulator).
    param([string]$Serial, [int]$TimeoutSec = 30)
    Adb-Dev -Serial $Serial -AdbArgs @(
        "shell", "run-as", $script:PACKAGE, "rm", "-f", "files/setup_introspect.json"
    ) | Out-Null
    return Wait-ForCondition -Description "SetupActivity ready on $Serial" -TimeoutSec $TimeoutSec -PollMs 2000 -Condition {
        Adb-Dev -Serial $Serial -AdbArgs @(
            "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_INTROSPECT"
        ) | Out-Null
        Start-Sleep -Milliseconds 500
        $json = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
            "shell", "run-as", $script:PACKAGE, "cat", "files/setup_introspect.json"
        ) -Seconds 5
        return ($json -and $json -match '"screen"')
    }
}

function Start-SetupActivity {
    # Force-stop, clear logcat, and launch SetupActivity on a specific device.
    # Returns $true if SetupActivity becomes responsive, $false on timeout.
    param([string]$Serial)
    Write-Status "Force-stopping app on $Serial..."
    Adb-Dev -Serial $Serial -AdbArgs @("shell", "am", "force-stop", $script:PACKAGE) | Out-Null
    Start-Sleep -Seconds 2
    Adb-Dev -Serial $Serial -AdbArgs @("logcat", "-c") | Out-Null
    Write-Status "Launching SetupActivity on $Serial..."
    Adb-Dev -Serial $Serial -AdbArgs @(
        "shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)"
    ) | Out-Null
    return Wait-SetupReady -Serial $Serial -TimeoutSec 30
}

function Setup-EmulatorRedir {
    # Send an emulator console redir command (e.g. "add udp:42500:42424").
    param([int]$ConsolePort, [string]$RedirSpec)
    $tokenPath = Join-Path (Get-RegressionHomeDirectory) ".emulator_console_auth_token"
    if (-not (Test-Path $tokenPath)) {
        Write-Status "  WARNING: No emulator auth token at $tokenPath" "Yellow"
        return "ERROR: no auth token"
    }
    $token = (Get-Content $tokenPath).Trim()
    try {
        $c = New-Object System.Net.Sockets.TcpClient("127.0.0.1", $ConsolePort)
        $s = $c.GetStream()
        $b = New-Object byte[] 4096
        Start-Sleep -Milliseconds 500
        if ($s.DataAvailable) { $s.Read($b, 0, 4096) | Out-Null }
        $w = [System.Text.Encoding]::ASCII.GetBytes("auth $token`r`n")
        $s.Write($w, 0, $w.Length)
        Start-Sleep -Milliseconds 300
        if ($s.DataAvailable) { $s.Read($b, 0, 4096) | Out-Null }
        $w = [System.Text.Encoding]::ASCII.GetBytes("redir $RedirSpec`r`n")
        $s.Write($w, 0, $w.Length)
        Start-Sleep -Milliseconds 300
        $result = ""
        if ($s.DataAvailable) { $n = $s.Read($b, 0, 4096); $result = [System.Text.Encoding]::ASCII.GetString($b, 0, $n) }
        $c.Close()
        return $result.Trim()
    } catch {
        return "ERROR: $($_.Exception.Message)"
    }
}

function Install-AppAndData {
    # Install APK and push game data to a specific device. Called after
    # a fresh emulator boot or whenever a device needs provisioning.
    # Uses Resolve-GameDataDeps (SHA256 index) so it works even when
    # game_data_to_copy_to_emulator/data/ is empty.
    param([Parameter(Mandatory)][string]$Serial)
    $apk = Join-RegressionPath $script:REPO_ROOT "android" "app" "build" "outputs" "apk" "debug" "app-debug.apk"
    if (Test-Path $apk) {
        Write-Status "  Installing APK on $Serial..."
        Adb-Dev-Timeout -Serial $Serial -AdbArgs @("install", "-r", $apk) -Seconds 180 | Out-Null
    }
    $ok = Ensure-StandardGameDataOnDevice -Serial $Serial
    if (-not $ok) {
        Write-Status "  WARN: Could not push game data to $Serial" "Yellow"
    }
    return $ok
}

function Ensure-StandardGameDataOnDevice {
    param([Parameter(Mandatory)][string]$Serial)

    # Set ANDROID_SERIAL so Adb/Adb-Timeout target the right device
    $prevSerial = $env:ANDROID_SERIAL
    $env:ANDROID_SERIAL = $Serial
    try {
        Write-Status "  Ensuring standard game data on $Serial..."
        return (Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps))
    } finally {
        if ($prevSerial) { $env:ANDROID_SERIAL = $prevSerial }
        else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}

function Start-EmulatorIfNeeded {
    # Boot an emulator if it isn't already online, then provision it.
    # $AvdMap: hashtable mapping serial -> AVD name (e.g. @{"emulator-5554"="Nexus5X_Light_1"})
    param(
        [Parameter(Mandatory)][string]$Serial,
        [Parameter(Mandatory)][hashtable]$AvdMap
    )
    if (Test-DeviceOnline -Serial $Serial) {
        Write-Status "  $Serial already online"
        return
    }
    $avd = $AvdMap[$Serial]
    if (-not $avd) { Write-Status "FAIL: Unknown AVD for $Serial" "Red"; exit 1 }
    if (-not (Start-ManagedEmulator -AvdName $avd -Serial $Serial -AppearTimeoutSeconds 90 -BootTimeoutSeconds 240)) {
        exit 1
    }
    Install-AppAndData -Serial $Serial | Out-Null
}

function Reset-GameState {
    # Reset shared config and preferences for a fresh test state.
    # Preserve pilot files so launcher-backed tests do not re-enter the
    # first-launch pilot flow on every run. Tests that need a missing pilot
    # state clear those files explicitly via setup_command clear_pilot_files.
    # Also reset the active file set to "default" so the game finds its HOGs.
    # Centralized here so every test uses the same cleanup logic.
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE,
        "find", "files", "-name", "'descent.cfg'", "-delete") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE,
        "rm", "-f", "files/controller_config.json") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE,
        "rm", "-f", "files/mods/mod_manifest.json",
        "files/audio_sources.json", "files/audio_playlist.json",
        "files/pending_resume_launch.json", "files/pending_resume_launch.json.tmp",
        "files/automation_result.json", "files/automation_result.json.tmp",
        "files/automation_log.jsonl", "files/introspect.json") | Out-Null
    # Remove artifacts installed by the quick-record integration test without
    # deleting user demos or dependency files staged for the next test.
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE,
        "find", "files/imported/sets", "-type", "f", "-name", "'quick_record_sidecar.*'", "-delete") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE,
        "rm", "-f", "shared_prefs/dxx_prefs.xml", "shared_prefs/dxx_prefs.xml.bak",
        "shared_prefs/launcher_prefs.xml", "shared_prefs/launcher_prefs.xml.bak") | Out-Null
    # Reset active file set to "default" -- previous tests may have
    # created/switched to a different set that is now empty.
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE,
        "rm", "-f", "files/file_sets.json") | Out-Null
    if (-not (Publish-DefaultActiveFileSet)) {
        throw "could not publish the default active game-data set"
    }
}

function Get-DefaultActiveFileSetPath {
    $appDataDir = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "pwd") -Seconds 5
    if (-not $appDataDir) { return $null }

    $appDataDir = $appDataDir.Trim()
    if ($appDataDir -notmatch '^/data/(?:data|user/\d+)/[A-Za-z0-9._]+$') {
        Write-Status "FAIL: unexpected app-private data path: $appDataDir" "Red"
        return $null
    }
    return "$appDataDir/$($script:DEFAULT_SET_DIR)"
}

function Publish-DefaultActiveFileSet {
    # The engines read these markers directly, independently of file_sets.json.
    # Publish through a same-directory temporary file so readers never see a
    # truncated path if a reset is interrupted.
    $defaultPath = Get-DefaultActiveFileSetPath
    if (-not $defaultPath) { return $false }

    $hostTemporary = [System.IO.Path]::GetTempFileName()
    $deviceTemporary = "/data/local/tmp/dxx_active_set_$([guid]::NewGuid().ToString('N'))"
    try {
        [System.IO.File]::WriteAllText($hostTemporary, $defaultPath, [System.Text.UTF8Encoding]::new($false))
        Adb-Timeout -AdbArgs @("push", $hostTemporary, $deviceTemporary) -Seconds 10 -IncludeStandardError | Out-Null

        foreach ($gameDir in @("d1x-redux", "d2x-redux")) {
            $markerDir = "files/$gameDir"
            $marker = "$markerDir/.active_set_path"
            $temporary = "$marker.tmp"
            Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "mkdir", "-p", $markerDir) -Seconds 5 -IncludeStandardError | Out-Null
            Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", $deviceTemporary, $temporary) -Seconds 5 -IncludeStandardError | Out-Null
            Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "chmod", "600", $temporary) -Seconds 5 -IncludeStandardError | Out-Null
            Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "mv", "-f", $temporary, $marker) -Seconds 5 -IncludeStandardError | Out-Null
            $published = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", $marker) -Seconds 5
            if (-not $published -or $published.Trim() -cne $defaultPath) {
                Write-Status "FAIL: could not activate the default file set for $gameDir" "Red"
                return $false
            }
        }
    } finally {
        Adb-Timeout -AdbArgs @("shell", "rm", "-f", $deviceTemporary) -Seconds 5 -IncludeStandardError | Out-Null
        Remove-Item -LiteralPath $hostTemporary -Force -ErrorAction SilentlyContinue
    }
    return $true
}

function Test-StandardGameDataActive {
    $defaultPath = Get-DefaultActiveFileSetPath
    if (-not $defaultPath) { return $false }

    foreach ($gameDir in @("d1x-redux", "d2x-redux")) {
        $marker = "files/$gameDir/.active_set_path"
        $published = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", $marker) -Seconds 5
        if (-not $published -or $published.Trim() -cne $defaultPath) {
            Write-Status "FAIL: $gameDir active file set is not the provisioned default set" "Red"
            return $false
        }
    }

    foreach ($hog in @("descent.hog", "descent2.hog")) {
        $size = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "stat", "-c", "%s", "$($script:DEFAULT_SET_DIR)/$hog") -Seconds 5
        $parsedSize = 0L
        if (-not [long]::TryParse($size, [ref]$parsedSize) -or $parsedSize -le 0) {
            Write-Status "FAIL: active standard game data is missing $hog" "Red"
            return $false
        }
    }
    return $true
}

function Test-StandardGameDataFailureReason {
    param([string]$Reason)
    return $Reason -match 'could not find (?:descent\.hog|descent2\.hog or d2demo\.hog)'
}

function Reset-DeviceGameState {
    param([Parameter(Mandatory)][string]$Serial)

    $previousSerial = $env:ANDROID_SERIAL
    try {
        $env:ANDROID_SERIAL = $Serial
        Stop-AppAndWait
        Reset-GameState
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial }
        else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}

function Wait-SetupActivityReady {
    # Poll until SetupActivity's broadcast receiver is alive (writes setup_introspect.json).
    # Returns $true if ready, $false if timed out.
    param([int]$TimeoutSeconds = 30)
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/setup_introspect.json") | Out-Null
    return (Wait-SetupCondition -TimeoutSeconds $TimeoutSeconds -PollMs 800 -Predicate {
            param($obj)
            return ($null -ne $obj -and $obj.screen)
        })
}

function Wait-GameStarted {
    # Poll logcat for "gameStarted=true" (logged by MainActivity.surfaceCreated).
    # Returns $true if detected, $false if timed out.
    param([int]$TimeoutSeconds = 30)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Start-Sleep -Seconds 1
        $log = Adb-Timeout -AdbArgs @("logcat", "-d", "-s", "DXX-Automate:*") -Seconds 5
        if ($null -ne $log -and $log -match 'gameStarted=true') {
            return $true
        }
    }
    return $false
}

# ── Declarative game data dependency resolution ──────────────────────────

$script:GameDataIndex = $null

function Read-GameDataIndex {
    # Parse game_data/game_data_index.txt into a hashtable: sha256 -> full path.
    # Cached in $script:GameDataIndex after first call.
    if ($script:GameDataIndex) { return $script:GameDataIndex }
    $repoRoot = $script:REPO_ROOT
    $indexFile = Join-RegressionPath $repoRoot "game_data" "game_data_index.txt"
    $generator = Join-RegressionPath $repoRoot "game_data" "generate_game_data_index.ps1"

    for ($attempt = 0; $attempt -lt 2; $attempt++) {
        if (-not (Test-Path $indexFile)) {
            if (Test-Path -LiteralPath $generator) {
                Write-Status "game_data_index.txt not found -- generating it now" "Yellow"
                $pwsh = Get-RegressionCurrentPwshPath
                & $pwsh -NoProfile -ExecutionPolicy Bypass -File $generator | ForEach-Object { Write-Status "  $_" "DarkGray" }
            }
        }

        if (-not (Test-Path $indexFile)) {
            Write-Status "WARN: game_data_index.txt not found -- run generate_game_data_index.ps1" "Yellow"
            return $null
        }

        $ht = @{}
        $staleCount = 0
        foreach ($line in (Get-Content $indexFile)) {
            if ($line -match '^\s*#' -or $line -match '^\s*$') { continue }
            $parts = $line -split '\s{2}', 2
            if ($parts.Count -eq 2) {
                $resolvedPath = Join-Path $repoRoot $parts[1]
                $ht[$parts[0]] = $resolvedPath
                if (-not (Test-Path -LiteralPath $resolvedPath)) {
                    $staleCount++
                }
            }
        }

        if ($staleCount -eq 0 -or -not (Test-Path -LiteralPath $generator) -or $attempt -eq 1) {
            $script:GameDataIndex = $ht
            return $ht
        }

        Write-Status "game_data_index.txt has $staleCount stale path(s) -- regenerating it now" "Yellow"
        $pwsh = Get-RegressionCurrentPwshPath
        & $pwsh -NoProfile -ExecutionPolicy Bypass -File $generator | ForEach-Object { Write-Status "  $_" "DarkGray" }
    }

    return $null
}

function Test-GameDataDepsResolvedByIndex {
    param(
        [Parameter(Mandatory = $true)][array]$Deps,
        [Parameter(Mandatory = $true)][hashtable]$Index
    )

    foreach ($dep in $Deps) {
        if (-not $Index.ContainsKey($dep.sha256)) {
            return $false
        }
        $path = $Index[$dep.sha256]
        if (-not $path -or -not (Test-Path -LiteralPath $path)) {
            return $false
        }
    }
    return $true
}

function Invoke-GameDataExtractionScript {
    param([Parameter(Mandatory = $true)][string]$ScriptPath)

    if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
        return $false
    }

    $pwsh = Get-RegressionCurrentPwshPath
    Write-Status "  Running $([System.IO.Path]::GetFileName($ScriptPath)) to materialize extracted game data..." "Yellow"
    & $pwsh -NoProfile -ExecutionPolicy Bypass -File $ScriptPath |
        ForEach-Object { Write-Status "    $_" "DarkGray" }
    return ($LASTEXITCODE -eq 0)
}

function Update-GameDataIndex {
    $generator = Join-RegressionPath $script:REPO_ROOT "game_data" "generate_game_data_index.ps1"
    if (-not (Test-Path -LiteralPath $generator -PathType Leaf)) {
        return $false
    }

    $pwsh = Get-RegressionCurrentPwshPath
    & $pwsh -NoProfile -ExecutionPolicy Bypass -File $generator |
        ForEach-Object { Write-Status "  $_" "DarkGray" }
    $script:GameDataIndex = $null
    return ($LASTEXITCODE -eq 0)
}

function Try-ExtractLocalGameDataSources {
    param([Parameter(Mandatory = $true)][array]$Deps)

    if ($env:DXX_SKIP_AUTO_EXTRACT_GAME_DATA -eq "1") {
        return $false
    }

    $repoRoot = $script:REPO_ROOT
    $gameDataDir = Join-RegressionPath $repoRoot "game_data"
    $generator = Join-RegressionPath $gameDataDir "generate_game_data_index.ps1"
    if (-not (Test-Path -LiteralPath $generator -PathType Leaf)) {
        return $false
    }

    $extractors = @(
        @{
            Script = Join-RegressionPath $gameDataDir "extract_all_gog.ps1"
            SourceDir = Join-RegressionPath $gameDataDir "gog installers"
        },
        @{
            Script = Join-RegressionPath $gameDataDir "extract_all_cds.ps1"
            SourceDir = Join-RegressionPath $gameDataDir "CD images"
        }
    )

    $ranAny = $false
    foreach ($extractor in $extractors) {
        if (-not (Test-Path -LiteralPath $extractor.SourceDir -PathType Container)) {
            continue
        }
        $sourceCount = @(Get-ChildItem -LiteralPath $extractor.SourceDir -File -Recurse -ErrorAction SilentlyContinue).Count
        if ($sourceCount -eq 0) {
            continue
        }

        if (-not (Invoke-GameDataExtractionScript -ScriptPath $extractor.Script)) {
            Write-Status "  WARN: $([System.IO.Path]::GetFileName($extractor.Script)) did not complete cleanly" "Yellow"
        }
        $ranAny = $true

        Update-GameDataIndex | Out-Null
        $script:GameDataIndex = $null
        $idx = Read-GameDataIndex
        if ($idx -and (Test-GameDataDepsResolvedByIndex -Deps $Deps -Index $idx)) {
            return $true
        }
    }

    return $ranAny
}

function Get-ScriptDeps {
    # Read a .json5 test script and return the _deps array from its _info element.
    # Returns array of dep objects ({file, sha256, target?}) or $null.
    # When -Vars is provided, ${VAR} placeholders in file and sha256 fields are replaced.
    param(
        [Parameter(Mandatory = $true)][string]$ScriptPath,
        [hashtable]$Vars = @{}
    )
    if (-not (Test-Path $ScriptPath)) { return $null }
    $raw = Get-Content $ScriptPath -Raw
    $raw = [regex]::Replace($raw, '//.*', '')
    $raw = [regex]::Replace($raw, ',\s*([}\]])', '$1')
    try {
        $arr = $raw | ConvertFrom-Json
        if ($arr.Count -gt 0 -and $arr[0]._info -and $arr[0]._info._deps) {
            $deps = @($arr[0]._info._deps)
            if ($Vars.Count -gt 0) {
                foreach ($dep in $deps) {
                    foreach ($k in $Vars.Keys) {
                        if ($dep.file) { $dep.file = $dep.file.Replace("`${$k}", $Vars[$k]) }
                        if ($dep.sha256) { $dep.sha256 = $dep.sha256.Replace("`${$k}", $Vars[$k]) }
                    }
                }
            }
            return $deps
        }
    } catch {}
    return $null
}

function Get-GameDataDepValue {
    param(
        [Parameter(Mandatory = $true)]$Dep,
        [Parameter(Mandatory = $true)][string]$Name
    )

    if ($Dep -is [hashtable]) {
        if ($Dep.ContainsKey($Name)) { return $Dep[$Name] }
        return $null
    }
    return $Dep.$Name
}

function Write-ResolvedGameDataAssetManifest {
    param(
        [Parameter(Mandatory = $true)][hashtable]$DepsByTarget,
        [Parameter(Mandatory = $true)][hashtable]$Index
    )

    $now = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    foreach ($target in $DepsByTarget.Keys) {
        if ($target -match '^/(sdcard|storage)/') { continue }

        $entries = @{}
        $manifestPath = "$target/assets.json"
        $existingJson = Adb-Timeout -AdbArgs @(
            "shell", "run-as", $script:PACKAGE, "cat", $manifestPath
        ) -Seconds 5
        if ($existingJson -and $existingJson -match '^\s*\[') {
            try {
                $existingEntries = $existingJson | ConvertFrom-Json
                foreach ($entry in ($existingEntries | ForEach-Object { $_ })) {
                    if ($entry.filename) {
                        $entries[$entry.filename.ToString().ToLowerInvariant()] = $entry
                    }
                }
            } catch {}
        }

        foreach ($dep in $DepsByTarget[$target]) {
            $file = (Get-GameDataDepValue -Dep $dep -Name "file")
            $sha256 = (Get-GameDataDepValue -Dep $dep -Name "sha256")
            if (-not $file -or -not $sha256) { continue }

            $fname = $file.ToString().ToLowerInvariant()
            $hash = $sha256.ToString().ToLowerInvariant()
            $localFile = $Index[$hash]
            if (-not $localFile -or -not (Test-Path -LiteralPath $localFile)) { continue }

            $existing = $entries[$fname]
            $importedAt = $now
            if ($existing -and $existing.importedAt) {
                try { $importedAt = [long]$existing.importedAt } catch {}
            }

            $entry = [ordered]@{
                filename = $fname
                sha256 = $hash
                sizeBytes = [long](Get-Item -LiteralPath $localFile).Length
                importedAt = $importedAt
            }
            $existingHash = if ($existing -and $existing.sha256) {
                $existing.sha256.ToString().ToLowerInvariant()
            } else {
                $null
            }
            $existingVersionName = $null
            if ($existing) {
                $existingVersionNameProperty = $existing.PSObject.Properties["versionName"]
                if ($existingVersionNameProperty) {
                    $existingVersionName = $existingVersionNameProperty.Value
                }
            }
            if ($existingHash -eq $hash -and $existingVersionName) {
                $entry.versionName = [string]$existingVersionName
            }
            $entries[$fname] = $entry
        }

        if ($entries.Count -eq 0) { continue }

        $manifestItems = @()
        foreach ($name in @($entries.Keys | Sort-Object)) {
            $manifestItems += $entries[$name]
        }
        $manifestJson = ConvertTo-Json -InputObject $manifestItems -Depth 5
        $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("dxx-assets-{0}.json" -f ([guid]::NewGuid()))
        $prevEAP = $ErrorActionPreference
        try {
            $ErrorActionPreference = "Continue"
            [System.IO.File]::WriteAllText($tmp, $manifestJson, [System.Text.UTF8Encoding]::new($false))
            & $script:ADB push $tmp "/data/local/tmp/assets.json" 2>&1 | Out-Null
            & $script:ADB shell "run-as $($script:PACKAGE) sh -c 'cat /data/local/tmp/assets.json > /data/data/$($script:PACKAGE)/$manifestPath'" 2>&1 | Out-Null
            & $script:ADB shell "rm -f /data/local/tmp/assets.json" 2>&1 | Out-Null
        } finally {
            $ErrorActionPreference = $prevEAP
            Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
        }
    }
}

function Test-ResolvedGameDataCaseNormalized {
    param([Parameter(Mandatory = $true)][hashtable]$DepsByTarget)

    $ok = $true
    foreach ($target in $DepsByTarget.Keys) {
        $isExternal = $target -match '^/(sdcard|storage)/'
        $listing = if ($isExternal) {
            Adb-Timeout -AdbArgs @("shell", "ls", "-la", "$target/") -Seconds 5
        } else {
            Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "ls", "-la", "$target/") -Seconds 5
        }
        if (-not $listing) { continue }

        $expected = @{}
        foreach ($dep in $DepsByTarget[$target]) {
            $file = Get-GameDataDepValue -Dep $dep -Name "file"
            if ($file) { $expected[$file.ToString().ToLowerInvariant()] = $true }
        }

        $seen = @{}
        foreach ($line in ($listing -split "`n")) {
            if ($line.Trim() -notmatch '^\S+\s+\S+\s+\S+\s+\S+\s+\d+\s+\S+\s+\S+\s+(\S+)$') {
                continue
            }
            $name = $matches[1].Trim()
            $lowerName = $name.ToLowerInvariant()
            if (-not $expected.ContainsKey($lowerName)) { continue }
            if (-not $seen.ContainsKey($lowerName)) { $seen[$lowerName] = @() }
            $seen[$lowerName] += $name
        }

        foreach ($lowerName in $seen.Keys) {
            $variants = @($seen[$lowerName] | Sort-Object -Unique)
            if ($variants.Count -gt 1 -or $variants[0] -cne $lowerName) {
                Write-Status "FAIL: game data case variants remain in ${target}: $($variants -join ', ')" "Red"
                $ok = $false
            }
        }
    }
    return $ok
}

function Resolve-GameDataDeps {
    # Resolve declarative game data deps: look up each by sha256 in the index,
    # check if present on device, push if missing. Returns $true on success.
    param([Parameter(Mandatory = $true)][array]$Deps)

    $idx = Read-GameDataIndex
    if (-not $idx) {
        Write-Status "FAIL: No game data index available" "Red"
        return $false
    }

    if (-not (Test-GameDataDepsResolvedByIndex -Deps $Deps -Index $idx)) {
        Write-Status "Required game data is not indexed yet; checking local installers/images for extractable files" "Yellow"
        Update-GameDataIndex | Out-Null
        $idx = Read-GameDataIndex
    }

    if (-not (Test-GameDataDepsResolvedByIndex -Deps $Deps -Index $idx)) {
        if (Try-ExtractLocalGameDataSources -Deps $Deps) {
            $script:GameDataIndex = $null
            $idx = Read-GameDataIndex
        }
    }

    $defaultTarget = $script:DEFAULT_SET_DIR
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "mkdir", "-p", $defaultTarget) | Out-Null

    # Group deps by target dir, list what's on device per target
    $byTarget = @{}
    foreach ($dep in $Deps) {
        $depTarget = Get-GameDataDepValue -Dep $dep -Name "target"
        $t = if ($depTarget) { $depTarget } else { $defaultTarget }
        if (-not $byTarget.ContainsKey($t)) { $byTarget[$t] = @() }
        $byTarget[$t] += $dep
    }

    $pushCount = 0
    foreach ($target in $byTarget.Keys) {
        # External storage targets (e.g. /sdcard/Download) use direct adb push;
        # app-private targets use run-as staging through /data/local/tmp/.
        $isExternal = $target -match '^/(sdcard|storage)/'

        if ($isExternal) {
            Adb -AdbArgs @("shell", "mkdir", "-p", $target) | Out-Null
            $listing = Adb-Timeout -AdbArgs @("shell", "ls", "-la", "$target/") -Seconds 5
        } else {
            Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "mkdir", "-p", $target) 2>&1 | Out-Null
            $listing = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "ls", "-la", "$target/") -Seconds 5
        }
        $deviceFiles = @{}  # lowercase filename -> size
        $deviceOrigNames = @{}  # lowercase filename -> original cased name
        $deviceNamesByLower = @{}  # lowercase filename -> all original cased names
        if ($listing) {
            foreach ($line in ($listing -split "`n")) {
                # ls -la: perms links owner group SIZE date time filename
                if ($line.Trim() -match '^\S+\s+\S+\s+\S+\s+\S+\s+(\d+)\s+\S+\s+\S+\s+(\S+)$') {
                    $origName = $matches[2].Trim()
                    $lowerName = $origName.ToLowerInvariant()
                    if (-not $deviceNamesByLower.ContainsKey($lowerName)) {
                        $deviceNamesByLower[$lowerName] = @()
                    }
                    $deviceNamesByLower[$lowerName] += $origName
                    if (-not $deviceFiles.ContainsKey($lowerName) -or $origName -ceq $lowerName) {
                        $deviceFiles[$lowerName] = [long]$matches[1]
                        $deviceOrigNames[$lowerName] = $origName
                    }
                }
            }
        }

        if (-not $isExternal -and $target -eq "files/mods") {
            $expectedModFiles = @{}
            foreach ($dep in $byTarget[$target]) {
                $expectedModFiles[$dep.file.ToLowerInvariant()] = $true
            }
            $staleModFiles = @($deviceNamesByLower.Keys | Where-Object { -not $expectedModFiles.ContainsKey($_) })
            if ($staleModFiles.Count -gt 0) {
                Write-Status "Game data: removing $($staleModFiles.Count) stale files/mods entries" "Yellow"
                foreach ($lowerName in $staleModFiles) {
                    foreach ($origName in @($deviceNamesByLower[$lowerName] | Sort-Object -Unique)) {
                        & $script:ADB shell "run-as $($script:PACKAGE) rm -f '$target/$origName'" 2>&1 | Out-Null
                    }
                    $deviceFiles.Remove($lowerName)
                    $deviceOrigNames.Remove($lowerName)
                    $deviceNamesByLower.Remove($lowerName)
                }
            }
        }

        foreach ($dep in $byTarget[$target]) {
            $fname = $dep.file.ToLower()
            $localFile = $idx[$dep.sha256]
            $needsPush = $false
            $caseVariants = @()
            if ($deviceNamesByLower.ContainsKey($fname)) {
                $caseVariants = @($deviceNamesByLower[$fname] | Sort-Object -Unique)
            }

            if ($caseVariants.Count -gt 0) {
                $exactLowercase = @($caseVariants | Where-Object { $_ -ceq $fname })
                $prevEAP = $ErrorActionPreference
                $ErrorActionPreference = "Continue"
                if ($exactLowercase.Count -gt 0) {
                    foreach ($variant in $caseVariants) {
                        if ($variant -cne $fname) {
                            if ($isExternal) {
                                & $script:ADB shell "rm -f '$target/$variant'" 2>&1 | Out-Null
                            } else {
                                & $script:ADB shell "run-as $($script:PACKAGE) rm -f '$target/$variant'" 2>&1 | Out-Null
                            }
                        }
                    }
                    $deviceOrigNames[$fname] = $fname
                    $deviceNamesByLower[$fname] = @($fname)
                } elseif ($caseVariants.Count -gt 0) {
                    $sourceName = $caseVariants[0]
                    if ($isExternal) {
                        & $script:ADB shell "mv '$target/$sourceName' '$target/$fname'" 2>&1 | Out-Null
                        foreach ($variant in @($caseVariants | Select-Object -Skip 1)) {
                            & $script:ADB shell "rm -f '$target/$variant'" 2>&1 | Out-Null
                        }
                    } else {
                        & $script:ADB shell "run-as $($script:PACKAGE) mv '$target/$sourceName' '$target/$fname'" 2>&1 | Out-Null
                        foreach ($variant in @($caseVariants | Select-Object -Skip 1)) {
                            & $script:ADB shell "run-as $($script:PACKAGE) rm -f '$target/$variant'" 2>&1 | Out-Null
                        }
                    }
                    $deviceOrigNames[$fname] = $fname
                    $deviceNamesByLower[$fname] = @($fname)
                }
                $ErrorActionPreference = $prevEAP
            }

            if (-not $deviceFiles.ContainsKey($fname)) {
                $needsPush = $true
            } elseif ($localFile -and (Test-Path $localFile)) {
                $localSize = (Get-Item $localFile).Length
                if ($deviceFiles[$fname] -ne $localSize) {
                    Write-Status "  Size mismatch: $($dep.file) (device=$($deviceFiles[$fname]), local=$localSize)" "Yellow"
                    $needsPush = $true
                }
            }

            if (-not $needsPush) {
                # Rename if case doesn't match (ext4 is case-sensitive)
                $origName = $deviceOrigNames[$fname]
                if ($origName -cne $fname) {
                    if ($isExternal) {
                        & $script:ADB shell "mv '$target/$origName' '$target/$fname'" 2>&1 | Out-Null
                    } else {
                        & $script:ADB shell "run-as $($script:PACKAGE) mv '$target/$origName' '$target/$fname'" 2>&1 | Out-Null
                    }
                }
                continue
            }

            if (-not $localFile -or -not (Test-Path $localFile)) {
                Write-Status "FAIL: Cannot resolve $($dep.file) (sha256: $($dep.sha256.Substring(0,12))...)" "Red"
                return $false
            }
            $prevEAP = $ErrorActionPreference
            $ErrorActionPreference = "Continue"
            if ($isExternal) {
                # Push directly to external storage (no run-as needed)
                & $script:ADB push $localFile "$target/$fname" 2>&1 | Out-Null
            } else {
                # Stage through /data/local/tmp then copy via run-as
                & $script:ADB push $localFile "/data/local/tmp/$fname" 2>&1 | Out-Null
                & $script:ADB shell "run-as $($script:PACKAGE) sh -c 'cat /data/local/tmp/$fname > /data/data/$($script:PACKAGE)/$target/$fname'" 2>&1 | Out-Null
                & $script:ADB shell "rm -f /data/local/tmp/$fname" 2>&1 | Out-Null
            }
            $ErrorActionPreference = $prevEAP
            $pushCount++
        }
    }
    Write-ResolvedGameDataAssetManifest -DepsByTarget $byTarget -Index $idx
    if (-not (Test-ResolvedGameDataCaseNormalized -DepsByTarget $byTarget)) {
        return $false
    }
    if ($pushCount -gt 0) {
        Write-Status "Game data: $pushCount files pushed via deps" "Green"
    }
    return $true
}

function Ensure-GameDataOnDevice {
    # Ensure required game files for $Game are present on the device.
    # Always pushes both D1 and D2 core files -- D2's mission list skips the
    # selection dialog when num_missions<=1, and having D1 files ensures
    # num_missions>=2 so the dialog appears (matching test expectations).
    # If missing, find them locally and push them. Returns $true on success.
    param([ValidateSet("d1", "d2")][string]$Game = "d2")

    $repoRoot = $script:REPO_ROOT
    $setDir = $script:DEFAULT_SET_DIR

    # Required files per game (lowercase). Must match SetupActivity.kt D2_FILES/D1_FILES.
    $d2Required = @("descent2.hog", "descent2.ham", "groupa.pig", "descent2.s22",
        "alien1.pig", "alien2.pig", "fire.pig", "ice.pig", "water.pig")
    $d1Required = @("descent.hog", "descent.pig")
    # Always push both sets so D2 mission-select dialog appears (needs num_missions>1)
    $needed = ($d2Required + $d1Required) | Sort-Object -Unique

    # List what's already on device
    $listing = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "ls", "$setDir/") -Seconds 5
    $onDevice = @()
    if ($listing) { $onDevice = ($listing -split "`n" | ForEach-Object { $_.Trim().ToLower() }) }

    $missing = $needed | Where-Object { $_ -notin $onDevice }
    if (-not $missing -or $missing.Count -eq 0) { return $true }

    Write-Status "Game data: $($missing.Count) files missing, pushing..." "Yellow"

    # Search paths for local game files (order = preference)
    $searchDirs = @(
        (Join-RegressionPath $repoRoot "game_data_to_copy_to_emulator" "temp"),
        (Join-RegressionPath $repoRoot "game_data_to_copy_to_emulator" "data"),
        (Join-RegressionPath $repoRoot "game_data" "extracted" "VERTIGO"),
        (Join-RegressionPath $repoRoot "game_data" "extracted" "d1 mac extracted")
    )

    # Ensure set dir exists
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "mkdir", "-p", $setDir) | Out-Null

    foreach ($fname in $missing) {
        $localFile = $null
        foreach ($dir in $searchDirs) {
            if (-not (Test-Path $dir)) { continue }
            # Case-insensitive match
            $match = Get-ChildItem $dir -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -ieq $fname } | Select-Object -First 1
            if ($match) { $localFile = $match.FullName; break }
        }
        if (-not $localFile) {
            Write-Status "FAIL: Cannot find $fname locally" "Red"
            return $false
        }
        $prevEAP = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        & $script:ADB push $localFile "/data/local/tmp/$fname" 2>&1 | Out-Null
        & $script:ADB shell "run-as $($script:PACKAGE) sh -c 'cat /data/local/tmp/$fname > /data/data/$($script:PACKAGE)/$setDir/$fname'" 2>&1 | Out-Null
        & $script:ADB shell "rm -f /data/local/tmp/$fname" 2>&1 | Out-Null
        $ErrorActionPreference = $prevEAP
    }
    Write-Status "Game data: $($missing.Count) files pushed" "Green"
    return $true
}

function Start-GameWithRetry {
    # Full launch flow: force-stop -> SetupActivity -> verify ready -> launch command -> verify game started.
    # Retries up to $MaxAttempts times.
    # $ExtraLaunchArgs: additional broadcast extras, e.g. @("--es", "game", "d1")
    # $PreLaunchScript: optional scriptblock to run after force-stop (e.g. delete pilot files)
    # $Game: "d1" or "d2" -- used to ensure game data is on device before launch.
    # $SkipGameData: skip Ensure-GameDataOnDevice (caller already resolved deps).
    # Returns $true on success, $false on failure (caller should exit).
    param(
        [string[]]$ExtraLaunchArgs = @(),
        [scriptblock]$PreLaunchScript = $null,
        [int]$MaxAttempts = 3,
        [string]$Game = "d2",
        [switch]$SkipGameData
    )

    $launchArgs = @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND", "--es", "command", "launch")
    $launchArgs += $ExtraLaunchArgs

    # Ensure game data is on device before attempting launch
    if (-not $SkipGameData -and -not (Ensure-GameDataOnDevice -Game $Game)) {
        Write-Status "FAIL: Could not ensure game data for $Game on device" "Red"
        return $false
    }

    for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
        if ($attempt -gt 1) {
            Write-Status "Game didn't start, retry $attempt/$MaxAttempts..." "Yellow"
        }

        Write-Status "Force-stopping app..."
        Stop-AppAndWait

        if ($PreLaunchScript) {
            & $PreLaunchScript
        }

        Adb -AdbArgs @("logcat", "-c") | Out-Null

        Write-Status "Launching SetupActivity..."
        Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null

        if (-not (Wait-SetupActivityReady)) {
            Write-Status "SetupActivity not responding after 30s" "Yellow"
            if ($attempt -lt $MaxAttempts) {
                if (-not (Invoke-LauncherStartupRecovery -Reason "SetupActivity launch timed out")) {
                    continue
                }
                if (-not $SkipGameData -and -not (Ensure-GameDataOnDevice -Game $Game)) {
                    Write-Status "FAIL: Could not restore game data for $Game after launcher recovery" "Red"
                    return $false
                }
            }
            continue
        }

        # Ensure "default" file set is active (a previous test may have switched it)
        Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
            "--es", "command", "switch_set", "--es", "name", "default") | Out-Null
        Start-Sleep -Milliseconds 500

        Write-Status "Sending launch command..."
        Adb -AdbArgs $launchArgs | Out-Null

        if (Wait-GameStarted) {
            Write-Status "Game started" "Green"
            return $true
        }
    }

    Write-Status "FAIL: Game never started after $MaxAttempts attempts" "Red"
    $diagLog = Adb-Timeout -AdbArgs @("logcat", "-d", "-t", "50") -Seconds 5
    if ($diagLog) { Write-Host $diagLog }
    return $false
}

function Send-AutomationScript {
    # Push a test script to the device and optionally send the AUTOMATE broadcast.
    # With -PushOnly, only copies the file (useful when pushing before game launch).
    param(
        [Parameter(Mandatory = $true, Position = 0)]
        [string]$ScriptName,
        [string]$ScriptDir = $script:ANDROID_ROOT,
        [switch]$PushOnly
    )
    $scriptPath = Join-RegressionPath $ScriptDir "game_scripts" $ScriptName
    if (-not (Test-Path $scriptPath)) {
        Write-Status "FAIL: Script not found: $scriptPath" "Red"
        return $false
    }
    Write-Status "Pushing test script: $ScriptName"
    Adb -AdbArgs @("push", $scriptPath, "/data/local/tmp/$ScriptName") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", "/data/local/tmp/$ScriptName", "files/$ScriptName") | Out-Null

    if ($PushOnly) { return $true }

    Start-Sleep -Seconds 1
    Adb -AdbArgs @("logcat", "-c") | Out-Null

    # Remove stale result file so prior run cannot confuse monitoring
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json") | Out-Null

    Write-Status "Sending automation broadcast for: $ScriptName"
    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE", "--es", "script", $ScriptName) | Out-Null
    return $true
}

function Test-AutomationResultRunId {
    # A correlated runner must never accept a result from an earlier invocation.
    param(
        [AllowNull()][object]$Result,
        [string]$ExpectedRunId
    )

    if (-not $ExpectedRunId) { return $true }
    if ($null -eq $Result) { return $false }
    $runIdProperty = $Result.PSObject.Properties["run_id"]
    if ($null -eq $runIdProperty) { return $false }
    return ([string]$runIdProperty.Value -ceq $ExpectedRunId)
}

function Get-TestStatusFromExitCode {
    param(
        [int]$ExitCode,
        [bool]$TimedOut = $false,
        [bool]$SkipDeclared = $false
    )

    if ($TimedOut) { return "TIMEOUT" }
    if ($ExitCode -eq 0) { return "PASS" }
    if ($ExitCode -eq 2 -and $SkipDeclared) { return "SKIP" }
    return "FAIL"
}

function Get-PowerShellTestSupportOwner {
    param([Parameter(Mandatory = $true)][string]$ScriptPath)

    $header = Get-Content -LiteralPath $ScriptPath -TotalCount 40 -ErrorAction Stop
    foreach ($line in $header) {
        if ($line -match '^\s*#\s*TEST-SUPPORT:\s*owner=([A-Za-z0-9_.-]+)\s*$') {
            return $matches[1]
        }
    }
    return $null
}

function Watch-AutomationResult {
    # Monitor for SCRIPT_RESULT via file-based result (primary) and logcat (fallback).
    # Handles SCRIPT_BACKGROUND markers by pressing HOME, waiting, then resuming.
    # Returns $true for PASS, $false for FAIL/timeout.
    param(
        [int]$TimeoutSeconds = 300,
        [switch]$IsLauncherScript,
        [string]$ExpectedRunId
    )

    $runLabel = if ($ExpectedRunId) { ", run_id=$ExpectedRunId" } else { "" }
    Write-Status "Monitoring test (timeout: ${TimeoutSeconds}s$runLabel)..."
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $lastHealthCheck = 0
    $finished = $false
    $passed = $false
    $backgroundMarkersHandled = [System.Collections.Generic.HashSet[string]]::new()
    $launcherChecked = $false
    $lastLauncherResumeStep = -1
    $cacheActive = $false
    $cacheStartSeconds = -1
    $cacheLastIndex = -1
    $cacheTotal = 0
    $lastCacheProgressSeconds = -1
    $postCacheGraceDeadlineSeconds = -1
    $cacheProgressGraceSeconds = 60
    $cachePhaseMaxSeconds = 900
    $lastAutomationSeq = -1
    $lastAutomationProgressSeconds = -1
    $automationProgressGraceSeconds = 120
    $mismatchedRunIds = [System.Collections.Generic.HashSet[string]]::new()

    while (-not $finished) {
        $elapsedNow = [int]$sw.Elapsed.TotalSeconds
        $cacheProgressGrace = $cacheActive -and $lastCacheProgressSeconds -ge 0 -and (($elapsedNow - $lastCacheProgressSeconds) -lt $cacheProgressGraceSeconds)
        $cachePhaseGrace = $cacheActive -and $cacheStartSeconds -ge 0 -and (($elapsedNow - $cacheStartSeconds) -lt $cachePhaseMaxSeconds)
        $postCacheGrace = $postCacheGraceDeadlineSeconds -ge 0 -and ($elapsedNow -lt $postCacheGraceDeadlineSeconds)
        $automationProgressGrace = $lastAutomationProgressSeconds -ge 0 -and (($elapsedNow - $lastAutomationProgressSeconds) -lt $automationProgressGraceSeconds)
        if ($elapsedNow -ge $TimeoutSeconds -and -not ($cacheProgressGrace -or $cachePhaseGrace -or $postCacheGrace -or $automationProgressGrace)) {
            break
        }

        Start-Sleep -Milliseconds 1500

        $elapsed = [int]$sw.Elapsed.TotalSeconds
        if ($elapsed - $lastHealthCheck -ge 15) {
            if (-not (Confirm-EmulatorHealthWithAdbRecovery)) {
                Write-Status "FAIL: Emulator or ADB transport unavailable during test (after ${elapsed}s)" "Red"
                Write-DeviceFailureDiagnostics -Reason "emulator health check failed after ADB reset at ${elapsed}s"
                return $false
            }
            if ($IsLauncherScript -and -not (Test-AppAutomationProcessRunning)) {
                Write-Status "FAIL: Launcher and game processes exited before automation produced a result (after ${elapsed}s)" "Red"
                Write-DeviceFailureDiagnostics -Reason "launcher and game processes exited during automation after ${elapsed}s"
                return $false
            }
            $lastHealthCheck = $elapsed

            $cacheLog = Adb-Timeout -AdbArgs @("logcat", "-d", "-t", "400", "-s", "DXX:*") -Seconds 10
            if ($cacheLog) {
                foreach ($line in ($cacheLog -split "`n")) {
                    if ($line -match 'ogl_cache: starting,\s+(\d+)\s+bitmaps') {
                        $cacheTotal = [int]$matches[1]
                        $cacheActive = $true
                        if ($cacheStartSeconds -lt 0) {
                            $cacheStartSeconds = $elapsed
                            $lastCacheProgressSeconds = $elapsed
                            if ($elapsed -ge $TimeoutSeconds) {
                                Write-Status "Extending timeout: ogl_cache started (0/$cacheTotal) after ${elapsed}s" "Yellow"
                            }
                        }
                    } elseif ($line -match 'ogl_cache: loading\s+(\d+)/(\d+)') {
                        $cacheIndex = [int]$matches[1]
                        $cacheTotal = [int]$matches[2]
                        if ($cacheStartSeconds -lt 0) {
                            $cacheStartSeconds = $elapsed
                        }
                        $cacheActive = $true
                        if ($cacheIndex -gt $cacheLastIndex) {
                            $cacheLastIndex = $cacheIndex
                            $lastCacheProgressSeconds = $elapsed
                            if ($elapsed -ge $TimeoutSeconds) {
                                Write-Status "Extending timeout: ogl_cache progress $cacheIndex/$cacheTotal after ${elapsed}s" "Yellow"
                            }
                        }
                    } elseif ($line -match 'ogl_cache: done') {
                        if ($cacheActive) {
                            $cacheActive = $false
                            $cacheStartSeconds = -1
                            $postCacheGraceDeadlineSeconds = $elapsed + 30
                            Write-Status "ogl_cache completed -- allowing 30s for automation to resume" "Yellow"
                        }
                    }
                }
            }

            # Stuck-at-launcher detection: after 15s, if no automation progress,
            # check if the game is actually stuck at the launcher/setup screen.
            # Skip for launcher scripts -- they're expected to be at SetupActivity.
            if (-not $launcherChecked -and $elapsed -ge 15 -and -not $IsLauncherScript) {
                $launcherChecked = $true
                $hasProgress = $false
                # Check automation_log.jsonl for any step progress
                $stepCheck = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE,
                    "ls", "files/automation_log.jsonl") -Seconds 3
                if ($stepCheck -and $stepCheck -notmatch 'No such file') { $hasProgress = $true }
                if (-not $hasProgress) {
                    # No automation progress -- check if launcher is still showing
                    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_INTROSPECT") | Out-Null
                    Start-Sleep -Milliseconds 800
                    $setupJson = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE,
                        "cat", "files/setup_introspect.json") -Seconds 3
                    if ($setupJson -and $setupJson -match '"screen"\s*:\s*"setup"') {
                        Write-Status "FAIL: Game stuck at launcher (SetupActivity still visible after ${elapsed}s)" "Red"
                        Write-Status "  The game engine may not have found its data files" "Yellow"
                        # Dump setup introspection for diagnostics
                        try {
                            $setupObj = $setupJson | ConvertFrom-Json
                            if ($setupObj.d2 -and -not $setupObj.d2.ready) {
                                Write-Status "  D2 files not ready on device" "Yellow"
                            }
                            if ($setupObj.d1 -and -not $setupObj.d1.ready) {
                                Write-Status "  D1 files not ready on device" "Yellow"
                            }
                            if ($setupObj.files_on_disk) {
                                Write-Status "  Files on disk: $($setupObj.files_on_disk -join ', ')" "Gray"
                            }
                        } catch {}
                        return $false
                    }
                }
            }
        }

        if ($elapsed -ge ($TimeoutSeconds - 60) -or $cacheActive -or $postCacheGraceDeadlineSeconds -ge 0) {
            $stepLog = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/automation_log.jsonl") -Seconds 3
            if ($stepLog) {
                $lastStepLine = ($stepLog -split "`n" | Where-Object { $_.Trim().StartsWith("{") } | Select-Object -Last 1)
                if ($lastStepLine) {
                    try {
                        $stepObj = $lastStepLine | ConvertFrom-Json
                        $stepSeq = [int]$stepObj.seq
                        if ($stepSeq -gt $lastAutomationSeq) {
                            $lastAutomationSeq = $stepSeq
                            $lastAutomationProgressSeconds = $elapsed
                            if ($elapsed -ge $TimeoutSeconds) {
                                Write-Status "Extending timeout: automation progress step $($stepObj.step)/$($stepObj.total) $($stepObj.action) $($stepObj.status) after ${elapsed}s" "Yellow"
                            }
                        }
                    } catch {
                        # Ignore malformed or partial log lines
                    }
                }
            }
        }

        # Primary: check file-based result (survives logcat issues)
        $resultJson = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/automation_result.json") -Seconds 3
        if ($resultJson -and $resultJson -match '"result"') {
            try {
                $resultObj = $resultJson | ConvertFrom-Json
                $resultRunId = if ($resultObj.run_id) { [string]$resultObj.run_id } else { "" }
                if (-not (Test-AutomationResultRunId -Result $resultObj -ExpectedRunId $ExpectedRunId)) {
                    if ($mismatchedRunIds.Add($resultRunId)) {
                        $displayRunId = if ($resultRunId) { $resultRunId } else { "<missing>" }
                        Write-Status "Ignoring automation result for run_id=$displayRunId" "Yellow"
                    }
                } elseif ($resultObj.result -eq "PASS") {
                    Write-Status "PASS (file-based, $($resultObj.steps_completed)/$($resultObj.total_steps) steps, $($resultObj.elapsed_ms)ms)" "Green"
                    $finished = $true
                    $passed = $true
                    continue
                } elseif ($resultObj.result -eq "FAIL") {
                    $reason = if ($resultObj.reason) { $resultObj.reason } else { "unknown" }
                    Write-Status "FAIL at step $($resultObj.steps_completed)/$($resultObj.total_steps): $reason" "Red"
                    $finished = $true
                    $passed = $false
                    continue
                } elseif ($resultObj.result -eq "LAUNCHER_CONTINUE") {
                    # Unified script: game exited, launcher resumes execution
                    # If SetupActivity already consumed the handoff, let the
                    # in-process executor keep running
                    # Otherwise restart the launcher so it can recreate the
                    # executor from the file
                    $nextStep = $resultObj.next_step
                    if ($IsLauncherScript -and $nextStep -ne $lastLauncherResumeStep) {
                        $lastLauncherResumeStep = $nextStep
                        Write-Status "LAUNCHER_CONTINUE at step $nextStep -- checking launcher handoff" "Yellow"
                        $handoffConsumed = $false
                        for ($handoffCheck = 0; $handoffCheck -lt 4; $handoffCheck++) {
                            Start-Sleep -Milliseconds 500
                            $handoffJson = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE,
                                "cat", "files/automation_result.json") -Seconds 3
                            if (-not ($handoffJson -and $handoffJson -match '"result"\s*:\s*"LAUNCHER_CONTINUE"')) {
                                $handoffConsumed = $true
                                break
                            }
                        }
                        if ($handoffConsumed) {
                            Write-Status "Launcher consumed LAUNCHER_CONTINUE -- continuing test monitoring" "Green"
                        } else {
                            Write-Status "LAUNCHER_CONTINUE at step $nextStep -- restarting launcher cleanly" "Yellow"
                            Stop-AppAndWait
                            Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
                            if (Wait-SetupActivityReady -TimeoutSeconds 15) {
                                Write-Status "Launcher resumed -- continuing test monitoring" "Green"
                            } else {
                                Write-Status "Launcher did not become ready after LAUNCHER_CONTINUE" "Yellow"
                            }
                        }
                    } else {
                        Write-Status "LAUNCHER_CONTINUE at step $nextStep -- waiting for launcher to resume" "Yellow"
                    }
                }
            } catch {
                # Parse failed -- fall through to logcat
            }
        }

        # Fallback: check logcat (include launcher/setup tags for launcher tests)
        $logArgs = if ($IsLauncherScript) {
            @("logcat", "-d", "-s", "DXX-Automate:*", "DXX-LauncherScript:*", "DXX-Setup:*")
        } else {
            @("logcat", "-d", "-s", "DXX-Automate:*")
        }
        $log = Adb-Timeout -AdbArgs $logArgs -Seconds 5
        if ($null -eq $log) {
            Write-Status "Warning: logcat not responding" "Yellow"
            continue
        }

        if ($log.Length -gt 0) {
            $lines = $log -split "`n"
            foreach ($line in $lines) {
                if (-not $ExpectedRunId -and $line -match 'SCRIPT_RESULT:\s*PASS') {
                    Write-Status "PASS (logcat)" "Green"
                    $finished = $true
                    $passed = $true
                } elseif (-not $ExpectedRunId -and $line -match 'SCRIPT_RESULT:\s*FAIL') {
                    Write-Status "FAIL (logcat): $line" "Red"
                    $finished = $true
                    $passed = $false
                } elseif ($line -match 'SCRIPT_BACKGROUND:\s*([^\s]+)' -and $backgroundMarkersHandled.Add($matches[1])) {
                    $backgroundMarker = $matches[1]
                    $backgroundSeconds = 3
                    $lockScreen = $false
                    if ($line -match 'duration_s=(\d+)') {
                        $backgroundSeconds = [Math]::Min([Math]::Max([int]$matches[1], 1), 300)
                    }
                    if ($line -match 'lock_screen=(true|1)') {
                        $lockScreen = $true
                    }
                    Write-Status "Background marker $backgroundMarker detected -- cycling app to background" "Yellow"
                    Start-Sleep -Seconds 1
                    # Press HOME to send app to background
                    Adb -AdbArgs @("shell", "input", "keyevent", "KEYCODE_HOME") | Out-Null
                    if ($lockScreen) {
                        Adb -AdbArgs @("shell", "input", "keyevent", "KEYCODE_SLEEP") | Out-Null
                        Write-Status "HOME pressed and screen locked -- waiting ${backgroundSeconds}s in background..."
                    } else {
                        Write-Status "HOME pressed -- waiting ${backgroundSeconds}s in background..."
                    }
                    Start-Sleep -Seconds $backgroundSeconds
                    if ($lockScreen) {
                        Adb -AdbArgs @("shell", "input", "keyevent", "KEYCODE_WAKEUP") | Out-Null
                        Adb -AdbArgs @("shell", "wm", "dismiss-keyguard") | Out-Null
                        Start-Sleep -Seconds 1
                    }
                    # Bring app back to foreground via launcher intent (re-opens
                    # existing task with SetupActivity on top), then press BACK
                    # to get back to the running game's MainActivity, triggering
                    # onResume and surface recreation.
                    Write-Status "Resuming app..."
                    Adb -AdbArgs @("shell", "monkey", "-p", $script:PACKAGE,
                        "-c", "android.intent.category.LAUNCHER", "1") | Out-Null
                    Start-Sleep -Seconds 1
                    # BACK dismisses the new SetupActivity, revealing the
                    # running game's MainActivity underneath
                    Adb -AdbArgs @("shell", "input", "keyevent", "KEYCODE_BACK") | Out-Null
                    Start-Sleep -Seconds 2
                    Write-Status "App resumed -- continuing test monitoring" "Green"
                } elseif (
                    $line.Trim().Length -gt 0 -and (
                        $line -match 'DXX-Automate' -or
                        ($IsLauncherScript -and ($line -match 'DXX-LauncherScript' -or $line -match 'DXX-Setup'))
                    )
                ) {
                    Write-Host "  $($line.Trim())" -ForegroundColor Gray
                }
            }
        }
    }

    if (-not $finished) {
        Write-Status "FAIL: Test timed out after ${TimeoutSeconds}s" "Red"
        # Dump diagnostics from files (more reliable than logcat)
        Write-Status "--- automation_log.jsonl (last 30 lines) ---" "Yellow"
        $stepLog = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/automation_log.jsonl") -Seconds 3
        if ($stepLog) {
            ($stepLog -split "`n") | Select-Object -Last 30 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
        } else {
            Write-Host "  (not available)" -ForegroundColor Gray
        }
        Write-Status "--- debug log files (last 30 lines each) ---" "Yellow"
        $debugLogs = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "ls", "files/debuglogs/") -Seconds 3
        if ($debugLogs) {
            foreach ($logFile in ($debugLogs -split "`n" | Where-Object { $_.Trim() })) {
                Write-Host "  -- $logFile --" -ForegroundColor DarkYellow
                $content = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/debuglogs/$($logFile.Trim())") -Seconds 3
                if ($content) {
                    ($content -split "`n") | Select-Object -Last 30 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
                }
            }
        } else {
            Write-Host "  (no debug log files)" -ForegroundColor Gray
        }
        Write-Status "--- logcat DXX-Automate (last 30 lines) ---" "Yellow"
        $log = Adb-Timeout -AdbArgs @("logcat", "-d", "-s", "DXX-Automate:*") -Seconds 5
        if ($log) {
            ($log -split "`n") | Select-Object -Last 30 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
        }
        return $false
    }

    # On failure, dump step log for context
    if (-not $passed) {
        Write-Status "--- automation_log.jsonl (last 20 lines) ---" "Yellow"
        $stepLog = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/automation_log.jsonl") -Seconds 3
        if ($stepLog) {
            ($stepLog -split "`n") | Select-Object -Last 20 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
        }
    }

    return $passed
}

function Get-GameIntrospection {
    # Request and return parsed game introspection JSON, or $null on failure.
    # If -Serial is provided, targets a specific device (multi-emulator tests).
    param([string]$Serial)
    if ($Serial) {
        Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
            "shell", "am", "broadcast", "-a", "com.dxxredux.INTROSPECT"
        ) -Seconds 10 | Out-Null
        Start-Sleep -Milliseconds 800
        $json = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
            "shell", "run-as", $script:PACKAGE, "cat", "files/introspect.json"
        ) -Seconds 5
    } else {
        Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.INTROSPECT") | Out-Null
        Start-Sleep -Milliseconds 800
        $json = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/introspect.json") -Seconds 5
    }
    if (-not $json -or $json -notmatch '^\s*\{') { return $null }
    try { return ($json | ConvertFrom-Json) } catch { return $null }
}

function Get-TestScriptInfo {
    # Parse only the first object so templates may contain unresolved placeholders later
    param([Parameter(Mandatory = $true)][string]$ScriptPath)
    if (-not (Test-Path $ScriptPath)) { return $null }
    $raw = Get-Content $ScriptPath -Raw

    $objectStart = -1
    $depth = 0
    $inString = $false
    $escaped = $false
    $lineComment = $false
    $blockComment = $false
    for ($i = $raw.IndexOf('[') + 1; $i -gt 0 -and $i -lt $raw.Length; $i++) {
        $current = $raw[$i]
        $next = if ($i + 1 -lt $raw.Length) { $raw[$i + 1] } else { [char]0 }
        if ($lineComment) {
            if ($current -in "`r", "`n") { $lineComment = $false }
            continue
        }
        if ($blockComment) {
            if ($current -eq '*' -and $next -eq '/') {
                $blockComment = $false
                $i++
            }
            continue
        }
        if ($inString) {
            if ($escaped) { $escaped = $false }
            elseif ($current -eq '\') { $escaped = $true }
            elseif ($current -eq '"') { $inString = $false }
            continue
        }
        if ($current -eq '/' -and $next -eq '/') {
            $lineComment = $true
            $i++
        } elseif ($current -eq '/' -and $next -eq '*') {
            $blockComment = $true
            $i++
        } elseif ($current -eq '"') {
            $inString = $true
        } elseif ($current -eq '{') {
            if ($objectStart -lt 0) { $objectStart = $i }
            $depth++
        } elseif ($current -eq '}' -and $objectStart -ge 0) {
            $depth--
            if ($depth -eq 0) {
                try {
                    $first = $raw.Substring($objectStart, $i - $objectStart + 1) | ConvertFrom-Json
                    if ($first._info) { return $first._info }
                } catch {}
                return $null
            }
        }
    }
    return $null
}

function Get-ScriptParams {
    # Read a .json5 test script and return the params dict from its _info element.
    # Returns hashtable of {ParamName -> {label, options -> {value -> {var_overrides}}}}
    # or $null if no params defined.
    param([Parameter(Mandatory = $true)][string]$ScriptPath)
    $info = Get-TestScriptInfo -ScriptPath $ScriptPath
    if ($info -and $info.params) { return $info.params }
    return $null
}

function Get-ScriptGameInfo {
    # Read a .json5 test script and return the games array from its _info element.
    # Returns @("d1","d2"), @("d1"), @("d2"), or $null if no _info element.
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptPath
    )
    $info = Get-TestScriptInfo -ScriptPath $ScriptPath
    if ($info -and $info.games) { return @($info.games) }
    return $null
}

function Get-ScriptStandalone {
    # Return $false if the script's _info has "_standalone": false, else $true.
    param([Parameter(Mandatory = $true)] [string]$ScriptPath)
    $info = Get-TestScriptInfo -ScriptPath $ScriptPath
    if ($info -and $info._standalone -eq $false) { return $false }
    return $true
}

function Get-ScriptIsLauncher {
    # Return $true if the script's first non-_info action is "enter_launcher".
    # These scripts start in SetupActivity and may transition to the game engine.
    param([Parameter(Mandatory = $true)] [string]$ScriptPath)
    if (-not (Test-Path $ScriptPath)) { return $false }
    $raw = Get-Content $ScriptPath -Raw
    $raw = [regex]::Replace($raw, '//.*', '')
    $raw = [regex]::Replace($raw, ',\s*([}\]])', '$1')
    try {
        $arr = $raw | ConvertFrom-Json
        foreach ($step in $arr) {
            if ($step._info) { continue }
            return ($step.action -eq "enter_launcher")
        }
    } catch {}
    return $false
}

function Resolve-TestScript {
    # Preprocess a .json5 test script for a specific game:
    #   1. Read _info.vars.$GameId for variable substitution
    #   2. Merge -Params option vars (from _info.params) into the vars dict
    #   3. Filter out steps where "when" (after param substitution) doesn't
    #      match $GameId
    #   4. Replace ${VAR} placeholders in all string values
    #   5. Write resolved script to a temp file
    # Returns the path to the resolved temp file, or $ScriptPath if no processing needed.
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptPath,
        [Parameter(Mandatory = $true)]
        [string]$GameId,
        [hashtable]$Params = @{}
    )
    if (-not (Test-Path $ScriptPath)) { return $ScriptPath }

    $raw = Get-Content $ScriptPath -Raw
    $cleaned = [regex]::Replace($raw, '//.*', '')
    $cleaned = [regex]::Replace($cleaned, ',\s*([}\]])', '$1')

    try {
        $arr = $cleaned | ConvertFrom-Json
    } catch {
        return $ScriptPath
    }

    # Extract vars from _info element
    $vars = @{}
    if ($arr.Count -gt 0 -and $arr[0]._info) {
        $info = $arr[0]._info
        if ($info.vars -and $info.vars.$GameId) {
            $gameVars = $info.vars.$GameId
            foreach ($prop in $gameVars.PSObject.Properties) {
                $vars[$prop.Name] = $prop.Value
            }
        }
        # Merge param selections: add param value itself + per-option var overrides
        if ($info.params -and $Params.Count -gt 0) {
            foreach ($pName in $Params.Keys) {
                $pValue = $Params[$pName]
                $vars[$pName] = $pValue
                $paramDef = $info.params.$pName
                if ($paramDef -and $paramDef.options -and $paramDef.options.$pValue) {
                    $optVars = $paramDef.options.$pValue
                    foreach ($prop in $optVars.PSObject.Properties) {
                        $vars[$prop.Name] = $prop.Value
                    }
                }
            }
        }
    }

    # No vars and no "when" fields? No processing needed.
    $hasWhen = $raw -match '"when"'
    if ($vars.Count -eq 0 -and -not $hasWhen) {
        return $ScriptPath
    }

    # Filter by "when" and remove _info elements
    $filtered = @()
    foreach ($step in $arr) {
        if ($step._info) { continue }
        $whenVal = $step.when
        if ($whenVal -is [string] -and $vars.Count -gt 0) {
            foreach ($k in $vars.Keys) {
                $whenVal = $whenVal.Replace("`${$k}", $vars[$k])
            }
        }
        if ($whenVal -and $whenVal -ne $GameId) { continue }
        # Remove the "when" property from output
        if ($whenVal) {
            $step.PSObject.Properties.Remove('when')
        }
        $filtered += $step
    }

    # Serialize to JSON and do variable substitution
    $jsonOut = ConvertTo-Json $filtered -Depth 10 -Compress
    foreach ($k in $vars.Keys) {
        $jsonOut = $jsonOut.Replace("`${$k}", $vars[$k])
    }

    # Write to temp file
    $tempDir = Join-Path (Split-Path $ScriptPath) ".resolved"
    if (-not (Test-Path $tempDir)) { New-Item -ItemType Directory -Path $tempDir -Force | Out-Null }
    $tempFile = Join-Path $tempDir ([System.IO.Path]::GetFileName($ScriptPath))
    Set-Content -Path $tempFile -Value $jsonOut -Encoding UTF8
    return $tempFile
}

function Get-ScriptTimeoutSeconds {
    # Calculate a timeout from the sum of all timing fields in a .json5 script.
    # Sums ms, timeout_ms, and post_delay_ms from every step, converts to
    # seconds, and adds a buffer. Returns an integer.
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptPath,
        [int]$BufferSeconds = 15
    )
    if (-not (Test-Path $ScriptPath)) { return 60 + $BufferSeconds }
    $raw = Get-Content $ScriptPath -Raw
    $totalMs = 0
    # Match all numeric values for timing keys (works on raw json5 text)
    foreach ($m in [regex]::Matches($raw, '"(?:ms|timeout_ms|post_delay_ms)"\s*:\s*(\d+)')) {
        $totalMs += [int]$m.Groups[1].Value
    }
    $seconds = [Math]::Ceiling($totalMs / 1000.0)
    return [Math]::Max(30, $seconds + $BufferSeconds)
}

function Get-SetupIntrospection {
    # Request and return parsed setup introspection JSON, or $null on failure.
    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_INTROSPECT") | Out-Null
    Start-Sleep -Milliseconds 800
    $json = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_introspect.json") -Seconds 5
    if (-not $json -or $json -notmatch '^\s*\{') { return $null }
    try { return ($json | ConvertFrom-Json) } catch { return $null }
}

# ── Infrastructure management ────────────────────────────────────────────
# Used by run_all_tests.ps1 to automatically bring up/tear down test infra.

$script:EMULATOR_EXE = Resolve-RegressionAndroidSdkTool -DepBase $script:DEP_BASE -Subdir "emulator" -ToolName "emulator"

function Get-ManagedAvdNames {
    if (-not (Test-Path -LiteralPath $script:EMULATOR_EXE -PathType Leaf)) {
        return @()
    }

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $output = & $script:EMULATOR_EXE -list-avds 2>&1
    $ErrorActionPreference = $previousErrorActionPreference

    return @(
        $output |
            ForEach-Object { "$_".Trim() } |
            Where-Object { $_ } |
            Sort-Object -Unique
    )
}

function Test-ManagedAvdExists {
    param([Parameter(Mandatory)][string]$AvdName)

    return (@(Get-ManagedAvdNames) -contains $AvdName)
}

function Ensure-ManagedAvdExists {
    param([Parameter(Mandatory)][string]$AvdName)

    if (Test-ManagedAvdExists -AvdName $AvdName) {
        return $true
    }

    $createScript = Join-RegressionPath $script:REPO_ROOT "android" "get_deps" "helpers" "create_light_avds.ps1"
    if (-not (Test-Path -LiteralPath $createScript -PathType Leaf)) {
        Write-Status "FAIL: AVD '$AvdName' is missing and create_light_avds.ps1 was not found at $createScript" "Red"
        return $false
    }

    Write-Status "AVD '$AvdName' is missing -- creating lightweight test AVDs..." "Yellow"
    $pwsh = Get-RegressionCurrentPwshPath
    & $pwsh -NoProfile -ExecutionPolicy Bypass -File $createScript |
        ForEach-Object { Write-Status "  $_" "DarkGray" }

    if ($LASTEXITCODE -ne 0) {
        Write-Status "FAIL: create_light_avds.ps1 failed with exit code $LASTEXITCODE" "Red"
        return $false
    }

    if (-not (Test-ManagedAvdExists -AvdName $AvdName)) {
        $available = @(Get-ManagedAvdNames)
        $availableText = if ($available.Count -gt 0) { $available -join ", " } else { "(none)" }
        Write-Status "FAIL: AVD '$AvdName' still missing after creation. Available AVDs: $availableText" "Red"
        return $false
    }

    return $true
}

function Get-ManagedEmulatorLaunchLogPaths {
    param([string]$AvdName)

    $logDir = Join-RegressionPath $script:REPO_ROOT "temp"
    New-Item -ItemType Directory -Force -Path $logDir | Out-Null
    $safeAvdName = $AvdName -replace '[^A-Za-z0-9_.-]', '_'
    return @{
        Stdout = Join-Path $logDir "emulator_${safeAvdName}.out.log"
        Stderr = Join-Path $logDir "emulator_${safeAvdName}.err.log"
    }
}

function Write-ManagedEmulatorLogTail {
    param([string]$AvdName)

    $logs = Get-ManagedEmulatorLaunchLogPaths -AvdName $AvdName
    foreach ($entry in @(
            @{ Label = "stderr"; Path = $logs.Stderr },
            @{ Label = "stdout"; Path = $logs.Stdout }
        )) {
        if (-not (Test-Path -LiteralPath $entry.Path)) {
            continue
        }

        $content = @(Get-Content -LiteralPath $entry.Path -Tail 20 -ErrorAction SilentlyContinue)
        if ($content.Count -eq 0) {
            continue
        }

        Write-Status "  Emulator $($entry.Label) tail ($($entry.Path))" "DarkGray"
        foreach ($line in $content) {
            Write-Status "    $line" "DarkGray"
        }
    }
}

function Start-ManagedEmulatorProcess {
    param(
        [Parameter(Mandatory)][string]$AvdName,
        [string]$GpuRenderer = "host",
        [switch]$Headless
    )

    $logs = Get-ManagedEmulatorLaunchLogPaths -AvdName $AvdName
    Remove-Item -LiteralPath $logs.Stdout, $logs.Stderr -ErrorAction SilentlyContinue
    $arguments = @("-avd", $AvdName, "-no-snapshot-load", "-no-snapshot-save", "-gpu", $GpuRenderer, "-crash-report-mode", "disabled")
    if ($Headless) {
        $arguments += "-no-window"
    }

    try {
        if (Test-RegressionWindowsHost) {
            # ShellExecute avoids inheriting redirected test-runner handles
            $processInfo = [System.Diagnostics.ProcessStartInfo]::new()
            $processInfo.FileName = $script:EMULATOR_EXE
            $processInfo.UseShellExecute = $true
            $processInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
            foreach ($argument in $arguments) {
                $null = $processInfo.ArgumentList.Add($argument)
            }

            $process = [System.Diagnostics.Process]::Start($processInfo)
            if ($process) {
                $process.Dispose()
                return $true
            }
            return $false
        }

        Start-Process -FilePath $script:EMULATOR_EXE `
            -ArgumentList $arguments `
            -RedirectStandardOutput $logs.Stdout `
            -RedirectStandardError $logs.Stderr
        return $true
    } catch {
        Write-Status "FAIL: could not start ${AvdName}: $_" "Red"
        return $false
    }
}

function Invoke-StaleEmulatorCleanupHelper {
    $cleanupScript = Join-Path $PSScriptRoot "kill-stale-emulators.ps1"
    if (-not (Test-Path -LiteralPath $cleanupScript)) {
        return
    }

    & $cleanupScript -Kill | ForEach-Object { Write-Status "  $_" "DarkGray" }
}

function Test-EmulatorAccelerationAvailable {
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $output = (& $script:EMULATOR_EXE -accel-check 2>&1) | Out-String
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference

    if ($exitCode -eq 0) {
        return $true
    }

    Write-Status "FAIL: Android emulator CPU acceleration is unavailable" "Red"
    foreach ($line in ($output -split "`n" | Where-Object { $_.Trim() })) {
        Write-Status "  $($line.TrimEnd())" "Red"
    }
    Write-Status "Install or enable Windows Hypervisor Platform or Android Emulator Hypervisor Driver, then rerun" "Yellow"
    return $false
}

function Wait-EmulatorBootComplete {
    param(
        [Parameter(Mandatory)][string]$Serial,
        [int]$TimeoutSeconds = 240
    )

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        $boot = Adb-Dev-Timeout -Serial $Serial -AdbArgs @("shell", "getprop", "sys.boot_completed") -Seconds 5
        if ($boot -and $boot.Trim() -eq "1") {
            $packageService = Adb-Dev-Timeout -Serial $Serial -AdbArgs @("shell", "cmd", "package", "list", "packages", "android") -Seconds 5
            if ($packageService -and $packageService -notmatch "Can't find service: package" -and $packageService -match "package:android") {
                return $true
            }
        }
        Start-Sleep -Seconds 1
    }

    return $false
}

function Start-ManagedEmulator {
    param(
        [Parameter(Mandatory)][string]$AvdName,
        [Parameter(Mandatory)][string]$Serial,
        [int]$AppearTimeoutSeconds = 90,
        [int]$BootTimeoutSeconds = 240
    )

    if (-not (Test-Path $script:EMULATOR_EXE)) {
        Write-Status "FAIL: emulator not found at $script:EMULATOR_EXE" "Red"
        return $false
    }
    if (-not (Ensure-ManagedAvdExists -AvdName $AvdName)) {
        return $false
    }
    if (-not (Test-EmulatorAccelerationAvailable)) {
        return $false
    }

    $launches = @(
        @{ GpuRenderer = "host"; Headless = $false; Label = "host gpu" },
        @{ GpuRenderer = "swiftshader_indirect"; Headless = $true; Label = "swiftshader_indirect no-window" }
    )

    foreach ($launch in $launches) {
        Write-Status "  Starting $AvdName ($Serial, $($launch.Label))..." "Yellow"
        if (-not (Start-ManagedEmulatorProcess -AvdName $AvdName -GpuRenderer $launch.GpuRenderer -Headless:$launch.Headless)) {
            Write-ManagedEmulatorLogTail -AvdName $AvdName
            continue
        }

        $appearStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        while ($appearStopwatch.Elapsed.TotalSeconds -lt $AppearTimeoutSeconds) {
            if (Test-DeviceOnline -Serial $Serial) {
                break
            }
            Start-Sleep -Seconds 1
        }

        if (-not (Test-DeviceOnline -Serial $Serial)) {
            Write-Status "  $Serial did not appear in adb after ${AppearTimeoutSeconds}s" "Yellow"
            Write-ManagedEmulatorLogTail -AvdName $AvdName
            Invoke-StaleEmulatorCleanupHelper
            continue
        }

        Write-Status "  Waiting for $Serial to boot..."
        if (Wait-EmulatorBootComplete -Serial $Serial -TimeoutSeconds $BootTimeoutSeconds) {
            Write-Status "  $Serial booted" "Green"
            return $true
        }

        Write-Status "  $Serial did not boot within ${BootTimeoutSeconds}s" "Yellow"
        Write-ManagedEmulatorLogTail -AvdName $AvdName
        Invoke-StaleEmulatorCleanupHelper
    }

    Write-Status "FAIL: $Serial did not start from $AvdName" "Red"
    return $false
}

function Start-SingleEmulator {
    # Start EMU1 (Nexus5X_Light_1) if not already running. Returns $true on success.
    if (Test-DeviceOnline -Serial $script:PRIMARY_EMULATOR_SERIAL) { return $true }
    Write-Status "Starting emulator ($script:PRIMARY_AVD_NAME)..." "Yellow"
    $healthScript = Join-Path $PSScriptRoot "emu_health.ps1"
    & $healthScript -Restart -Wait -ForceRestart -TimeoutSeconds 180 -AvdName $script:PRIMARY_AVD_NAME
    $ec = $LASTEXITCODE
    if (($ec -eq 0 -or $ec -eq 2) -and (Test-DeviceOnline -Serial $script:PRIMARY_EMULATOR_SERIAL)) { return $true }
    Write-Status "FAIL: Could not start emulator" "Red"
    return $false
}

function Start-SecondEmulator {
    # Start EMU2 (Nexus5X_Light_2 on emulator-5556) if not already running.
    # Returns $true on success.
    if (Test-DeviceOnline -Serial $script:SECONDARY_EMULATOR_SERIAL) { return $true }
    return Start-ManagedEmulator -AvdName $script:SECONDARY_AVD_NAME -Serial $script:SECONDARY_EMULATOR_SERIAL -AppearTimeoutSeconds 120 -BootTimeoutSeconds 240
}

function Ensure-FirewallRules {
    # Check if Windows Firewall rules exist for test server/relay ports.
    # If missing, warn and offer a one-liner the user can run as admin.
    if (-not (Test-RegressionWindowsHost)) {
        return
    }

    $rules = @(
        @{ Name = "DXX-Redux Matchmaking (TCP-in 9000)"; Protocol = "TCP"; Port = 9000 },
        @{ Name = "DXX-Redux UDP Relay (UDP-in 42600)"; Protocol = "UDP"; Port = 42600 },
        @{ Name = "DXX-Redux UDP Redir (UDP-in 42500)"; Protocol = "UDP"; Port = 42500 }
    )
    $missing = @()
    foreach ($r in $rules) {
        $check = netsh advfirewall firewall show rule name="$($r.Name)" 2>&1 | Out-String
        if ($check -notmatch "Rule Name") { $missing += $r }
    }
    if ($missing.Count -eq 0) { return }
    # Not fatal -- tests bind to 127.0.0.1 to avoid prompts, but warn in case
    # something uses 0.0.0.0 or the user runs the server manually.
    $cmds = $missing | ForEach-Object {
        "netsh advfirewall firewall add rule name='$($_.Name)' dir=in action=allow protocol=$($_.Protocol) localport=$($_.Port) enable=yes"
    }
    Write-Status "WARN: $($missing.Count) firewall rule(s) missing. Run as admin to fix:" "Yellow"
    foreach ($c in $cmds) { Write-Status "  $c" "Yellow" }
}

function Start-MatchmakingServer {
    # Build (if needed) and start the matchmaking server. Returns the Process object
    # or $null on failure. Caller is responsible for stopping it.
    Ensure-FirewallRules
    $serverDir = Join-Path $script:REPO_ROOT "server"
    $serverBin = Resolve-RegressionBuildTool -Directory (Join-RegressionPath $serverDir "target" "release") -BaseName "dxx-matchmaking"
    if (-not $serverBin) { $serverBin = Resolve-RegressionBuildTool -Directory (Join-RegressionPath $serverDir "target" "debug") -BaseName "dxx-matchmaking" }
    if (-not $serverBin -or -not (Test-Path $serverBin)) {
        Write-Status "Building matchmaking server..." "Yellow"
        $serverManifest = Join-RegressionPath $serverDir "Cargo.toml"
        $buildOut = & cargo build --release --manifest-path $serverManifest 2>&1
        $buildExitCode = $LASTEXITCODE
        if ($buildExitCode -ne 0 -and ($buildOut -match "lock file version 4 requires" -or $buildOut -match "rustc [0-9.]+ is not supported")) {
            $rustHelper = Join-RegressionPath $script:REPO_ROOT "android" "get_deps" "helpers" "get_rust.sh"
            if (Test-Path -LiteralPath $rustHelper -PathType Leaf) {
                Write-Status "Cargo is too old for server/Cargo.lock; installing/updating rustup stable..." "Yellow"
                $bash = Get-Command bash -ErrorAction SilentlyContinue
                if ($bash) {
                    & $bash.Source $rustHelper | ForEach-Object { Write-Status "  $_" "DarkGray" }
                    $cargoDir = Join-RegressionPath (Get-RegressionHomeDirectory) ".cargo" "bin"
                    if (Test-Path -LiteralPath (Join-RegressionPath $cargoDir "cargo")) {
                        $env:PATH = "$cargoDir$([System.IO.Path]::PathSeparator)$env:PATH"
                    }
                    $buildOut = & cargo build --release --manifest-path $serverManifest 2>&1
                    $buildExitCode = $LASTEXITCODE
                }
            }
        }
        if ($buildExitCode -ne 0) {
            Write-Status "FAIL: matchmaking server build failed" "Red"
            $buildOut | Select-Object -Last 40 | ForEach-Object { Write-Status "  $_" "Red" }
            return $null
        }
        $serverBin = Resolve-RegressionBuildTool -Directory (Join-RegressionPath $serverDir "target" "release") -BaseName "dxx-matchmaking"
    }
    if (-not $serverBin -or -not (Test-Path $serverBin)) {
        Write-Status "FAIL: matchmaking server binary not found" "Red"
        return $null
    }
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $serverBin
    $psi.WorkingDirectory = $serverDir
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.EnvironmentVariables["SKIP_GPGS_VERIFY"] = "true"
    $psi.EnvironmentVariables["RUST_LOG"] = "info"
    # Bind to localhost to avoid Windows Firewall prompts (emulator
    # reaches host via 10.0.2.2 which maps to loopback)
    $psi.EnvironmentVariables["WS_LISTEN_ADDR"] = "127.0.0.1:9000"
    $psi.EnvironmentVariables["HTTP_LISTEN_ADDR"] = "127.0.0.1:8080"
    # Configure built-in UDP relay so LocalhostProxy can route game traffic
    $psi.EnvironmentVariables["RELAY_LISTEN_ADDR"] = "127.0.0.1:9001"
    $psi.EnvironmentVariables["RELAY_PUBLIC_ADDR"] = "10.0.2.2:9001"
    $proc = [System.Diagnostics.Process]::Start($psi)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 15) {
        try {
            $tcp = [System.Net.Sockets.TcpClient]::new()
            $tcp.Connect("127.0.0.1", 9000)
            $tcp.Close()
            Write-Status "Matchmaking server ready (PID $($proc.Id))" "Green"
            return $proc
        } catch { Start-Sleep -Seconds 1 }
    }
    Write-Status "FAIL: matchmaking server did not start on port 9000" "Red"
    try { $proc.Kill() } catch {}
    return $null
}

function Start-DockerNat {
    # Start Docker NAT containers. Returns $true on success.
    param(
        [string]$NatA = "full-cone",
        [string]$NatB = "symmetric"
    )
    $composeDir = Join-RegressionPath $script:REPO_ROOT "android" "docker" "nat-testbed"
    if (-not (Test-Path (Join-Path $composeDir "docker-compose.yml"))) {
        Write-Status "SKIP: android/docker/nat-testbed/docker-compose.yml not found" "Yellow"
        return $false
    }
    $null = docker version --format '{{.Server.Version}}' 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Status "SKIP: Docker not running" "Yellow"
        return $false
    }
    Write-Status "Starting Docker NAT ($NatA / $NatB)..." "Yellow"
    $env:NAT_A = $NatA
    $env:NAT_B = $NatB
    $composeFile = Join-RegressionPath $composeDir "docker-compose.yml"
    docker compose --project-directory $composeDir -f $composeFile down 2>&1 | Out-Null
    docker compose --project-directory $composeDir -f $composeFile up -d --build 2>&1 | Out-Null
    $rc = $LASTEXITCODE
    Remove-Item Env:\NAT_A -ErrorAction SilentlyContinue
    Remove-Item Env:\NAT_B -ErrorAction SilentlyContinue
    if ($rc -ne 0) {
        Write-Status "FAIL: docker compose up failed" "Red"
        return $false
    }
    Start-Sleep -Seconds 2
    Write-Status "Docker NAT containers started" "Green"
    return $true
}

function Stop-DockerNat {
    $composeDir = Join-RegressionPath $script:REPO_ROOT "android" "docker" "nat-testbed"
    $composeFile = Join-Path $composeDir "docker-compose.yml"
    if (Test-Path $composeFile) {
        docker compose -f $composeFile down 2>&1 | Out-Null
    }
}

function Install-ApkOnDevice {
    # Install the debug APK on a specific emulator serial, or default.
    param([string]$Serial)
    $apk = Join-RegressionPath $script:ANDROID_ROOT "app" "build" "outputs" "apk" "debug" "app-debug.apk"
    if (-not (Test-Path $apk)) {
        Write-Status "WARN: No APK at $apk" "Yellow"
        return $false
    }
    $args_ = if ($Serial) { @("-s", $Serial, "install", "-r", $apk) } else { @("install", "-r", $apk) }
    $result = Adb-Timeout -AdbArgs $args_ -Seconds 180
    if (-not $result) {
        Write-Status "WARN: APK install timed out" "Yellow"
        return $false
    }
    if ($result -notmatch "Success") {
        Write-Host $result
        return $false
    }
    return $true
}

function Push-GameDataToDevice {
    # Push game data to a specific emulator serial using push_game_data.sh.
    param([string]$Serial)
    $pushScript = Join-Path $PSScriptRoot "push_game_data.sh"
    if (-not (Test-Path $pushScript)) { return }
    $gameDataDir = Join-Path $script:REPO_ROOT "game_data_to_copy_to_emulator"
    $hasData = (Test-Path (Join-Path $gameDataDir "data")) -or (Test-Path (Join-Path $gameDataDir "download"))
    if (-not $hasData) { return }
    $bashExe = "bash"
    if (Test-RegressionWindowsHost) {
        $gitBash = Join-RegressionPath $script:DEP_BASE "git" "bin" "bash.exe"
        if (Test-Path $gitBash) { $bashExe = $gitBash }
    }
    $prevSerial = $env:ANDROID_SERIAL
    if ($Serial) { $env:ANDROID_SERIAL = $Serial }
    $env:CALLED_FROM_SCRIPT = "1"
    & $bashExe $pushScript 2>&1 | Out-Null
    Remove-Item Env:\CALLED_FROM_SCRIPT -ErrorAction SilentlyContinue
    if ($Serial) {
        if ($prevSerial) { $env:ANDROID_SERIAL = $prevSerial }
        else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}

#!/usr/bin/env pwsh
# test_helpers.ps1 -- Shared helper functions for Android emulator test scripts.
#
# Usage (dot-source from another script):
#   . "$PSScriptRoot\test_helpers.ps1"
#
# Provides:
#   $ADB, $PACKAGE, $ACTIVITY  -- standard paths/constants
#   Adb [-AdbArgs ...]         -- run adb with stderr tolerance
#   Adb-Timeout [-AdbArgs ...] -- run adb with a timeout (returns $null on hang)
#   Test-EmulatorHealthy       -- check emulator process + adb + shell
#   Ensure-EmulatorHealthy     -- check + restart via emu_health.ps1 if needed
#   Write-Status               -- timestamped colored output
#   Start-GameWithRetry        -- full launch flow: SetupActivity -> verify -> game -> verify

$_depBaseFile = Join-Path (Split-Path $PSScriptRoot) "dependency_base.txt"
if (-not (Test-Path $_depBaseFile)) {
    Write-Error "dependency_base.txt not found at $_depBaseFile. Create it with a single line containing the path to your dependency directory (e.g. C:\local)."
    exit 1
}
$script:DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
$script:ADB = "$script:DEP_BASE\android-sdk\platform-tools\adb.exe"
$script:PACKAGE = "com.dxxredux.app"
$script:ACTIVITY = "com.dxxredux.app.SetupActivity"

function Adb {
    param([string[]]$AdbArgs)
    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $output = & $script:ADB @AdbArgs 2>&1 | Out-String
    $ErrorActionPreference = $prevEAP
    return $output.Trim()
}

function Adb-Timeout {
    # Run an adb command with a timeout; returns $null if it hangs.
    # Uses ProcessStartInfo instead of Start-Job because Start-Job with
    # adb.exe hangs on Windows PowerShell 5.1 (pipe/handle inheritance issue).
    param([string[]]$AdbArgs, [int]$Seconds = 8)
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
        if ([string]::IsNullOrEmpty($out)) { return "" }
        return $out.Trim()
    } catch {
        return $null
    }
}

function Test-EmulatorHealthy {
    # Check if emulator process is running, adb sees it, and shell responds.
    $emuProc = Get-Process | Where-Object {
        $_.ProcessName -match 'qemu-system|emulator' -and $_.Path -like "*android*"
    }
    if (-not $emuProc) { return $false }
    $devices = Adb -AdbArgs "devices"
    if ($devices -notmatch 'emulator-\d+\s+device') { return $false }
    $boot = Adb-Timeout -AdbArgs @("shell", "getprop", "sys.boot_completed") -Seconds 5
    if ($null -eq $boot -or $boot -ne "1") { return $false }
    return $true
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
    Start-Sleep -Seconds 3
    Write-Status "Emulator healthy" "Green"
    return $true
}

function Write-Status {
    param([string]$Msg, [string]$Color = "Cyan")
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Msg" -ForegroundColor $Color
}

function Wait-SetupActivityReady {
    # Poll until SetupActivity's broadcast receiver is alive (writes setup_introspect.json).
    # Returns $true if ready, $false if timed out.
    param([int]$TimeoutSeconds = 30)
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/setup_introspect.json") | Out-Null
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Start-Sleep -Seconds 2
        Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_INTROSPECT") | Out-Null
        Start-Sleep -Seconds 1
        $json = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_introspect.json") -Seconds 5
        if ($null -ne $json -and $json -match '"screen"') {
            return $true
        }
    }
    return $false
}

function Wait-GameStarted {
    # Poll logcat for "gameStarted=true" (logged by MainActivity.surfaceCreated).
    # Returns $true if detected, $false if timed out.
    param([int]$TimeoutSeconds = 30)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Start-Sleep -Seconds 2
        $log = Adb-Timeout -AdbArgs @("logcat", "-d", "-s", "DXX-Automate:*") -Seconds 5
        if ($null -ne $log -and $log -match 'gameStarted=true') {
            return $true
        }
    }
    return $false
}

function Start-GameWithRetry {
    # Full launch flow: force-stop -> SetupActivity -> verify ready -> launch command -> verify game started.
    # Retries up to $MaxAttempts times.
    # $ExtraLaunchArgs: additional broadcast extras, e.g. @("--es", "game", "d1")
    # $PreLaunchScript: optional scriptblock to run after force-stop (e.g. delete pilot files)
    # Returns $true on success, $false on failure (caller should exit).
    param(
        [string[]]$ExtraLaunchArgs = @(),
        [scriptblock]$PreLaunchScript = $null,
        [int]$MaxAttempts = 3
    )

    $launchArgs = @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND", "--es", "command", "launch")
    $launchArgs += $ExtraLaunchArgs

    for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
        if ($attempt -gt 1) {
            Write-Status "Game didn't start, retry $attempt/$MaxAttempts..." "Yellow"
        }

        Write-Status "Force-stopping app..."
        Adb -AdbArgs @("shell", "am", "force-stop", $script:PACKAGE) | Out-Null
        Start-Sleep -Seconds 2

        if ($PreLaunchScript) {
            & $PreLaunchScript
        }

        Adb -AdbArgs @("logcat", "-c") | Out-Null

        Write-Status "Launching SetupActivity..."
        Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null

        if (-not (Wait-SetupActivityReady)) {
            Write-Status "SetupActivity not responding after 30s" "Yellow"
            continue
        }

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
        [Parameter(Mandatory=$true, Position=0)]
        [string]$ScriptName,
        [string]$ScriptDir = $PSScriptRoot,
        [switch]$PushOnly
    )
    $scriptPath = Join-Path $ScriptDir "game_scripts\$ScriptName"
    if (-not (Test-Path $scriptPath)) {
        Write-Status "FAIL: Script not found: $scriptPath" "Red"
        return $false
    }
    Write-Status "Pushing test script: $ScriptName"
    Adb -AdbArgs @("push", $scriptPath, "/data/local/tmp/$ScriptName") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", "/data/local/tmp/$ScriptName", "files/$ScriptName") | Out-Null

    if ($PushOnly) { return $true }

    Start-Sleep -Seconds 2
    Adb -AdbArgs @("logcat", "-c") | Out-Null

    Write-Status "Sending automation broadcast for: $ScriptName"
    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE", "--es", "script", $ScriptName) | Out-Null
    return $true
}

function Watch-AutomationResult {
    # Monitor logcat for SCRIPT_RESULT, with periodic health checks.
    # Handles SCRIPT_BACKGROUND markers by pressing HOME, waiting, then resuming.
    # Returns $true for PASS, $false for FAIL/timeout.
    param([int]$TimeoutSeconds = 300)

    Write-Status "Monitoring test (timeout: ${TimeoutSeconds}s)..."
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $lastHealthCheck = 0
    $finished = $false
    $passed = $false
    $backgroundHandled = $false

    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds -and -not $finished) {
        Start-Sleep -Seconds 3

        $elapsed = [int]$sw.Elapsed.TotalSeconds
        if ($elapsed - $lastHealthCheck -ge 15) {
            if (-not (Test-EmulatorHealthy)) {
                Write-Status "FAIL: Emulator crashed during test (after ${elapsed}s)" "Red"
                return $false
            }
            $lastHealthCheck = $elapsed
        }

        $log = Adb-Timeout -AdbArgs @("logcat", "-d", "-s", "DXX-Automate:*") -Seconds 5
        if ($null -eq $log) {
            Write-Status "Warning: logcat not responding" "Yellow"
            continue
        }

        if ($log.Length -gt 0) {
            $lines = $log -split "`n"
            foreach ($line in $lines) {
                if ($line -match 'SCRIPT_RESULT:\s*PASS') {
                    Write-Status "PASS" "Green"
                    $finished = $true
                    $passed = $true
                }
                elseif ($line -match 'SCRIPT_RESULT:\s*FAIL') {
                    Write-Status "FAIL: $line" "Red"
                    $finished = $true
                    $passed = $false
                }
                elseif ($line -match 'SCRIPT_BACKGROUND:' -and -not $backgroundHandled) {
                    $backgroundHandled = $true
                    Write-Status "Background marker detected -- cycling app to background" "Yellow"
                    Start-Sleep -Seconds 2
                    # Press HOME to send app to background
                    Adb -AdbArgs @("shell", "input", "keyevent", "KEYCODE_HOME") | Out-Null
                    Write-Status "HOME pressed -- waiting 5s in background..."
                    Start-Sleep -Seconds 5
                    # Bring app back to foreground via launcher intent (re-opens
                    # existing task with SetupActivity on top), then press BACK
                    # to get back to the running game's MainActivity, triggering
                    # onResume and surface recreation.
                    Write-Status "Resuming app..."
                    Adb -AdbArgs @("shell", "monkey", "-p", $script:PACKAGE,
                        "-c", "android.intent.category.LAUNCHER", "1") | Out-Null
                    Start-Sleep -Seconds 2
                    # BACK dismisses the new SetupActivity, revealing the
                    # running game's MainActivity underneath
                    Adb -AdbArgs @("shell", "input", "keyevent", "KEYCODE_BACK") | Out-Null
                    Start-Sleep -Seconds 3
                    Write-Status "App resumed -- continuing test monitoring" "Green"
                }
                elseif ($line -match 'DXX-Automate' -and $line.Trim().Length -gt 0) {
                    Write-Host "  $($line.Trim())" -ForegroundColor Gray
                }
            }
        }
    }

    if (-not $finished) {
        Write-Status "FAIL: Test timed out after ${TimeoutSeconds}s" "Red"
        $log = Adb-Timeout -AdbArgs @("logcat", "-d", "-s", "DXX-Automate:*") -Seconds 5
        if ($log) { Write-Host $log }
        return $false
    }

    return $passed
}

function Get-GameIntrospection {
    # Request and return parsed game introspection JSON, or $null on failure.
    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.INTROSPECT") | Out-Null
    Start-Sleep -Seconds 2
    $json = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/introspect.json") -Seconds 5
    if (-not $json -or $json -notmatch '^\s*\{') { return $null }
    try { return ($json | ConvertFrom-Json) } catch { return $null }
}

function Get-ScriptGameInfo {
    # Read a .json5 test script and return the games array from its _info element.
    # Returns @("d1","d2"), @("d1"), @("d2"), or $null if no _info element.
    param(
        [Parameter(Mandatory=$true)]
        [string]$ScriptPath
    )
    if (-not (Test-Path $ScriptPath)) { return $null }
    $raw = Get-Content $ScriptPath -Raw
    # Strip single-line comments, trailing commas
    $raw = [regex]::Replace($raw, '//.*', '')
    $raw = [regex]::Replace($raw, ',\s*([}\]])', '$1')
    try {
        $arr = $raw | ConvertFrom-Json
        if ($arr.Count -gt 0 -and $arr[0]._info -and $arr[0]._info.games) {
            return @($arr[0]._info.games)
        }
    } catch {}
    return $null
}

function Resolve-TestScript {
    # Preprocess a .json5 test script for a specific game:
    #   1. Read _info.vars.$GameId for variable substitution
    #   2. Filter out steps where "when" doesn't match $GameId
    #   3. Replace ${VAR} placeholders in all string values
    #   4. Write resolved script to a temp file
    # Returns the path to the resolved temp file, or $ScriptPath if no processing needed.
    param(
        [Parameter(Mandatory=$true)]
        [string]$ScriptPath,
        [Parameter(Mandatory=$true)]
        [string]$GameId
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
        [Parameter(Mandatory=$true)]
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
    Start-Sleep -Seconds 2
    $json = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_introspect.json") -Seconds 5
    if (-not $json -or $json -notmatch '^\s*\{') { return $null }
    try { return ($json | ConvertFrom-Json) } catch { return $null }
}

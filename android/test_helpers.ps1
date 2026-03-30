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
#   Adb-Dev [-Serial ...]      -- run adb targeting a specific device
#   Adb-Dev-Timeout [...]      -- device-targeted adb with timeout
#   Test-EmulatorHealthy       -- check emulator process + adb + shell
#   Ensure-EmulatorHealthy     -- check + restart via emu_health.ps1 if needed
#   Test-DeviceOnline          -- check if a specific serial is online
#   Start-EmulatorIfNeeded     -- boot + provision an emulator if not running
#   Install-AppAndData         -- install APK + push game data to a device
#   Write-Status               -- timestamped colored output
#   Start-GameWithRetry        -- full launch flow: SetupActivity -> verify -> game -> verify

# Source shared environment setup (JAVA_HOME, cmake, cargo)
. "$PSScriptRoot\test_env.ps1"

$_depBaseFile = Join-Path (Split-Path $PSScriptRoot) "dependency_base.txt"
if (-not (Test-Path $_depBaseFile)) {
    Write-Host "FAIL: dependency_base.txt not found at $_depBaseFile" -ForegroundColor Red
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
    $boot = Adb-Timeout -AdbArgs @("shell", "getprop", "sys.boot_completed") -Seconds 10
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
    Write-Status "Emulator healthy" "Green"
    return $true
}

function Wait-ProcessDead {
    # Poll until the app process is gone (after force-stop). Returns $true
    # when the process is confirmed dead, $false on timeout.
    param([int]$TimeoutMs = 5000)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $TimeoutMs) {
        $procId = Adb-Timeout -AdbArgs @("shell", "pidof", $script:PACKAGE) -Seconds 3
        if (-not $procId -or $procId -notmatch '^\d+') { return $true }
        Start-Sleep -Milliseconds 200
    }
    return $false
}

function Stop-AppAndWait {
    # Force-stop the app and wait for the process to actually die.
    Adb -AdbArgs @("shell", "am", "force-stop", $script:PACKAGE) | Out-Null
    Wait-ProcessDead | Out-Null
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
        Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_INTROSPECT") | Out-Null
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
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Msg" -ForegroundColor $Color
}

# -- Multi-device helpers (for dual-emulator tests like test_mp, test_lan) --

function Adb-Dev {
    # Run adb targeting a specific device serial.
    param([string]$Serial, [string[]]$AdbArgs)
    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $fullArgs = @("-s", $Serial) + $AdbArgs
    $output = & $script:ADB @fullArgs 2>&1 | Out-String
    $ErrorActionPreference = $prevEAP
    return $output.Trim()
}

function Adb-Dev-Timeout {
    # Run adb targeting a specific device serial, with a timeout.
    param([string]$Serial, [string[]]$AdbArgs, [int]$Seconds = 10)
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
        if ([string]::IsNullOrEmpty($out)) { return "" }
        return $out.Trim()
    } catch {
        return $null
    }
}

function Test-DeviceOnline {
    param([string]$Serial)
    $devices = & $script:ADB devices 2>&1 | Out-String
    return $devices -match "$Serial\s+device"
}

function Install-AppAndData {
    # Install APK and push game data to a specific device. Called after
    # a fresh emulator boot or whenever a device needs provisioning.
    # Uses Resolve-GameDataDeps (SHA256 index) so it works even when
    # game_data_to_copy_to_emulator/data/ is empty.
    param([Parameter(Mandatory)][string]$Serial)
    $repoRoot = Split-Path $PSScriptRoot
    $apk = Join-Path $repoRoot "android\app\build\outputs\apk\debug\app-debug.apk"
    if (Test-Path $apk) {
        Write-Status "  Installing APK on $Serial..."
        Adb-Dev-Timeout -Serial $Serial -AdbArgs @("install", "-r", $apk) -Seconds 60 | Out-Null
    }
    # Set ANDROID_SERIAL so Adb/Adb-Timeout target the right device
    $prevSerial = $env:ANDROID_SERIAL
    $env:ANDROID_SERIAL = $Serial
    Write-Status "  Pushing game data to $Serial..."
    $deps = Get-StandardGameDataDeps
    $ok = Resolve-GameDataDeps -Deps $deps
    if (-not $ok) {
        Write-Status "  WARN: Could not push game data to $Serial" "Yellow"
    }
    if ($prevSerial) { $env:ANDROID_SERIAL = $prevSerial }
    else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
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
    $emulatorExe = "$script:DEP_BASE\android-sdk\emulator\emulator.exe"
    Write-Status "  Starting $avd ($Serial)..." "Yellow"
    Start-Process $emulatorExe -ArgumentList "-avd", $avd, "-no-snapshot-save", "-gpu", "host"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 60 -and -not (Test-DeviceOnline -Serial $Serial)) {
        Start-Sleep -Seconds 1
    }
    if (-not (Test-DeviceOnline -Serial $Serial)) {
        Write-Status "FAIL: $Serial did not appear in adb after 60s" "Red"; exit 1
    }
    Write-Status "  Waiting for $Serial to boot..."
    $sw2 = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw2.Elapsed.TotalSeconds -lt 240) {
        $r = Adb-Dev-Timeout -Serial $Serial -AdbArgs @("shell", "getprop", "sys.boot_completed") -Seconds 5
        if ($r -and $r.Trim() -eq "1") {
            Write-Status "  $Serial booted" "Green"
            Install-AppAndData -Serial $Serial
            return
        }
        Start-Sleep -Seconds 1
    }
    Write-Status "FAIL: $Serial did not boot within 240s" "Red"; exit 1
}

function Reset-GameState {
    # Delete pilot files, config, and controller config for fresh test state.
    # Also reset the active file set to "default" so the game finds its HOGs.
    # Centralized here so every test uses the same cleanup logic.
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE,
        "find", "files", "-name", "'*.plr'", "-delete") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE,
        "find", "files", "-name", "'*.plx'", "-delete") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE,
        "find", "files", "-name", "'descent.cfg'", "-delete") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE,
        "rm", "-f", "files/controller_config.json") | Out-Null
    # Reset active file set to "default" -- previous tests may have
    # created/switched to a different set that is now empty.
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE,
        "rm", "-f", "files/file_sets.json") | Out-Null
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
    $repoRoot = Split-Path $PSScriptRoot
    $indexFile = Join-Path $repoRoot "game_data\game_data_index.txt"
    if (-not (Test-Path $indexFile)) {
        Write-Status "WARN: game_data_index.txt not found -- run generate_game_data_index.ps1" "Yellow"
        return $null
    }
    $ht = @{}
    foreach ($line in (Get-Content $indexFile)) {
        if ($line -match '^\s*#' -or $line -match '^\s*$') { continue }
        $parts = $line -split '\s{2}', 2
        if ($parts.Count -eq 2) {
            $ht[$parts[0]] = Join-Path $repoRoot $parts[1]
        }
    }
    $script:GameDataIndex = $ht
    return $ht
}

function Get-ScriptDeps {
    # Read a .json5 test script and return the _deps array from its _info element.
    # Returns array of dep objects ({file, sha256, target?}) or $null.
    param([Parameter(Mandatory = $true)][string]$ScriptPath)
    if (-not (Test-Path $ScriptPath)) { return $null }
    $raw = Get-Content $ScriptPath -Raw
    $raw = [regex]::Replace($raw, '//.*', '')
    $raw = [regex]::Replace($raw, ',\s*([}\]])', '$1')
    try {
        $arr = $raw | ConvertFrom-Json
        if ($arr.Count -gt 0 -and $arr[0]._info -and $arr[0]._info._deps) {
            return @($arr[0]._info._deps)
        }
    } catch {}
    return $null
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

    $defaultTarget = "files/sets/default"
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "mkdir", "-p", $defaultTarget) | Out-Null

    # Group deps by target dir, list what's on device per target
    $byTarget = @{}
    foreach ($dep in $Deps) {
        # Support both hashtable (from Get-StandardGameDataDeps) and
        # PSCustomObject (from ConvertFrom-Json) -- use .target for both.
        $depTarget = $dep.target
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
        if ($listing) {
            foreach ($line in ($listing -split "`n")) {
                # ls -la: perms links owner group SIZE date time filename
                if ($line.Trim() -match '^\S+\s+\S+\s+\S+\s+\S+\s+(\d+)\s+\S+\s+\S+\s+(\S+)$') {
                    $origName = $matches[2].Trim()
                    $deviceFiles[$origName.ToLower()] = [long]$matches[1]
                    $deviceOrigNames[$origName.ToLower()] = $origName
                }
            }
        }

        foreach ($dep in $byTarget[$target]) {
            $fname = $dep.file.ToLower()
            $localFile = $idx[$dep.sha256]
            $needsPush = $false

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
    if ($pushCount -gt 0) {
        Write-Status "Game data: $pushCount files pushed via deps" "Green"
    }
    return $true
}

function Get-StandardGameDataDeps {
    # Returns the standard 11-file D2+D1 game data deps array used by most tests.
    return @(
        @{file = "descent2.hog"; sha256 = "f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703" }
        @{file = "descent2.ham"; sha256 = "5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d" }
        @{file = "groupa.pig"; sha256 = "facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b" }
        @{file = "descent2.s22"; sha256 = "4f10632dd4efcbffe532c35b6763edd22817135442bbcc4171381706f3893728" }
        @{file = "alien1.pig"; sha256 = "811fc58caa3e2a72cdfa07d7530b2bb0ca71836a6a2d8a3cb401e4284949c233" }
        @{file = "alien2.pig"; sha256 = "75ef8fa0cba03410c61ad1b58f57dcb1481f1f302985828aab0af90639926055" }
        @{file = "fire.pig"; sha256 = "26a5a5f4e91456abf31f79578d0922e7bc3348b6aa92489a84033de83f358156" }
        @{file = "ice.pig"; sha256 = "ae6152ef69502b00e51a98d8f04b21f2855a332cd2988ecceb3b909a49fa26a1" }
        @{file = "water.pig"; sha256 = "de88ead87dcb32f16936b3e2a08b81a2248440f29e6f8be0c4c3a5f9fe4b63c1" }
        @{file = "descent.hog"; sha256 = "83d76ff0c46bb2e7348a49bdd287ad764abeda0d851bfb16b42c1ede93b21052" }
        @{file = "descent.pig"; sha256 = "093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe" }
    )
}

function Ensure-GameDataOnDevice {
    # Ensure required game files for $Game are present on the device.
    # Always pushes both D1 and D2 core files -- D2's mission list skips the
    # selection dialog when num_missions<=1, and having D1 files ensures
    # num_missions>=2 so the dialog appears (matching test expectations).
    # If missing, find them locally and push them. Returns $true on success.
    param([ValidateSet("d1", "d2")][string]$Game = "d2")

    $repoRoot = Split-Path $PSScriptRoot
    $setDir = "files/sets/default"

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
        (Join-Path $repoRoot "game_data_to_copy_to_emulator\temp"),
        (Join-Path $repoRoot "game_data_to_copy_to_emulator\data"),
        (Join-Path $repoRoot "game_data\extracted\VERTIGO"),
        (Join-Path $repoRoot "game_data\extracted\d1 mac extracted")
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

    Start-Sleep -Seconds 1
    Adb -AdbArgs @("logcat", "-c") | Out-Null

    # Remove stale result file so prior run cannot confuse monitoring
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json") | Out-Null

    Write-Status "Sending automation broadcast for: $ScriptName"
    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE", "--es", "script", $ScriptName) | Out-Null
    return $true
}

function Watch-AutomationResult {
    # Monitor for SCRIPT_RESULT via file-based result (primary) and logcat (fallback).
    # Handles SCRIPT_BACKGROUND markers by pressing HOME, waiting, then resuming.
    # Returns $true for PASS, $false for FAIL/timeout.
    param([int]$TimeoutSeconds = 300, [switch]$IsLauncherScript)

    Write-Status "Monitoring test (timeout: ${TimeoutSeconds}s)..."
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $lastHealthCheck = 0
    $finished = $false
    $passed = $false
    $backgroundHandled = $false
    $launcherChecked = $false

    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds -and -not $finished) {
        Start-Sleep -Milliseconds 1500

        $elapsed = [int]$sw.Elapsed.TotalSeconds
        if ($elapsed - $lastHealthCheck -ge 15) {
            if (-not (Test-EmulatorHealthy)) {
                # Retry once -- adb can be slow under heavy game load (level loading,
                # title screens) without the emulator actually being dead.
                Write-Status "Health check failed, retrying in 2s..." "Yellow"
                Start-Sleep -Seconds 2
                if (-not (Test-EmulatorHealthy)) {
                    Write-Status "FAIL: Emulator crashed during test (after ${elapsed}s)" "Red"
                    return $false
                }
            }
            $lastHealthCheck = $elapsed

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

        # Primary: check file-based result (survives logcat issues)
        $resultJson = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/automation_result.json") -Seconds 3
        if ($resultJson -and $resultJson -match '"result"') {
            try {
                $resultObj = $resultJson | ConvertFrom-Json
                if ($resultObj.result -eq "PASS") {
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
                    # Unified script: game exited, launcher resumes execution.
                    # The launcher's onResume will pick this up, delete the file,
                    # and continue running steps. Just keep polling.
                    $nextStep = $resultObj.next_step
                    Write-Status "LAUNCHER_CONTINUE at step $nextStep -- waiting for launcher to resume" "Yellow"
                }
            } catch {
                # Parse failed -- fall through to logcat
            }
        }

        # Fallback: check logcat
        $log = Adb-Timeout -AdbArgs @("logcat", "-d", "-s", "DXX-Automate:*") -Seconds 5
        if ($null -eq $log) {
            Write-Status "Warning: logcat not responding" "Yellow"
            continue
        }

        if ($log.Length -gt 0) {
            $lines = $log -split "`n"
            foreach ($line in $lines) {
                if ($line -match 'SCRIPT_RESULT:\s*PASS') {
                    Write-Status "PASS (logcat)" "Green"
                    $finished = $true
                    $passed = $true
                } elseif ($line -match 'SCRIPT_RESULT:\s*FAIL') {
                    Write-Status "FAIL (logcat): $line" "Red"
                    $finished = $true
                    $passed = $false
                } elseif ($line -match 'SCRIPT_BACKGROUND:' -and -not $backgroundHandled) {
                    $backgroundHandled = $true
                    Write-Status "Background marker detected -- cycling app to background" "Yellow"
                    Start-Sleep -Seconds 1
                    # Press HOME to send app to background
                    Adb -AdbArgs @("shell", "input", "keyevent", "KEYCODE_HOME") | Out-Null
                    Write-Status "HOME pressed -- waiting 3s in background..."
                    Start-Sleep -Seconds 3
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
                } elseif ($line -match 'DXX-Automate' -and $line.Trim().Length -gt 0) {
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
        Write-Status "--- gamelog.txt (last 30 lines) ---" "Yellow"
        $gameLog = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/gamelog.txt") -Seconds 3
        if ($gameLog) {
            ($gameLog -split "`n") | Select-Object -Last 30 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
        } else {
            Write-Host "  (not available)" -ForegroundColor Gray
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
    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.INTROSPECT") | Out-Null
    Start-Sleep -Milliseconds 800
    $json = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/introspect.json") -Seconds 5
    if (-not $json -or $json -notmatch '^\s*\{') { return $null }
    try { return ($json | ConvertFrom-Json) } catch { return $null }
}

function Get-ScriptGameInfo {
    # Read a .json5 test script and return the games array from its _info element.
    # Returns @("d1","d2"), @("d1"), @("d2"), or $null if no _info element.
    param(
        [Parameter(Mandatory = $true)]
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

function Get-ScriptStandalone {
    # Return $false if the script's _info has "_standalone": false, else $true.
    param([Parameter(Mandatory = $true)] [string]$ScriptPath)
    if (-not (Test-Path $ScriptPath)) { return $true }
    $raw = Get-Content $ScriptPath -Raw
    $raw = [regex]::Replace($raw, '//.*', '')
    $raw = [regex]::Replace($raw, ',\s*([}\]])', '$1')
    try {
        $arr = $raw | ConvertFrom-Json
        if ($arr.Count -gt 0 -and $arr[0]._info -and $arr[0]._info._standalone -eq $false) {
            return $false
        }
    } catch {}
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
    #   2. Filter out steps where "when" doesn't match $GameId
    #   3. Replace ${VAR} placeholders in all string values
    #   4. Write resolved script to a temp file
    # Returns the path to the resolved temp file, or $ScriptPath if no processing needed.
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptPath,
        [Parameter(Mandatory = $true)]
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

$script:EMULATOR_EXE = "$script:DEP_BASE\android-sdk\emulator\emulator.exe"
$script:REPO_ROOT = Split-Path $PSScriptRoot

function Start-SingleEmulator {
    # Start EMU1 (Nexus5X_Light_1) if not already running. Returns $true on success.
    $devices = Adb-Timeout -AdbArgs @("devices") -Seconds 5
    if ($devices -and $devices -match "emulator-\d+\s+device") { return $true }
    Write-Status "Starting emulator (Nexus5X_Light_1)..." "Yellow"
    $healthScript = Join-Path $PSScriptRoot "emu_health.ps1"
    & $healthScript -Restart -Wait -TimeoutSeconds 180
    $ec = $LASTEXITCODE
    if ($ec -eq 0 -or $ec -eq 2) { return $true }
    Write-Status "FAIL: Could not start emulator" "Red"
    return $false
}

function Start-SecondEmulator {
    # Start EMU2 (Nexus5X_Light_2 on emulator-5556) if not already running.
    # Returns $true on success.
    $devices = Adb-Timeout -AdbArgs @("devices") -Seconds 5
    if ($devices) {
        $m = [regex]::Matches($devices, "emulator-\d+\s+device")
        if ($m.Count -ge 2) { return $true }
    }
    Write-Status "Starting second emulator (Nexus5X_Light_2)..." "Yellow"
    if (-not (Test-Path $script:EMULATOR_EXE)) {
        Write-Status "FAIL: emulator.exe not found at $script:EMULATOR_EXE" "Red"
        return $false
    }
    Start-Process -FilePath $script:EMULATOR_EXE `
        -ArgumentList "-avd", "Nexus5X_Light_2", "-no-snapshot-save", "-gpu", "host" `
        -WindowStyle Minimized
    # Wait for it to appear in adb and boot
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 180) {
        Start-Sleep -Seconds 2
        $devices = Adb-Timeout -AdbArgs @("devices") -Seconds 5
        if ($devices) {
            $m = [regex]::Matches($devices, "emulator-\d+\s+device")
            if ($m.Count -ge 2) {
                # Verify boot complete on the second one
                $serials = [regex]::Matches($devices, "(emulator-\d+)\s+device") |
                    ForEach-Object { $_.Groups[1].Value } | Sort-Object
                $emu2 = $serials | Select-Object -Last 1
                $boot = Adb-Timeout -AdbArgs @("-s", $emu2, "shell", "getprop", "sys.boot_completed") -Seconds 10
                if ($boot -and $boot.Trim() -eq "1") {
                    Write-Status "Second emulator ready ($emu2)" "Green"
                    return $true
                }
            }
        }
    }
    Write-Status "FAIL: Second emulator did not boot within 180s" "Red"
    return $false
}

function Ensure-FirewallRules {
    # Check if Windows Firewall rules exist for test server/relay ports.
    # If missing, warn and offer a one-liner the user can run as admin.
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
    $serverBin = Join-Path $serverDir "target\release\dxx-matchmaking.exe"
    if (-not (Test-Path $serverBin)) {
        $serverBin = Join-Path $serverDir "target\debug\dxx-matchmaking.exe"
    }
    if (-not (Test-Path $serverBin)) {
        Write-Status "Building matchmaking server..." "Yellow"
        Push-Location $serverDir
        & cargo build --release 2>&1 | Out-Null
        Pop-Location
        $serverBin = Join-Path $serverDir "target\release\dxx-matchmaking.exe"
    }
    if (-not (Test-Path $serverBin)) {
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
    $composeDir = Join-Path $script:REPO_ROOT "docker\nat-testbed"
    if (-not (Test-Path (Join-Path $composeDir "docker-compose.yml"))) {
        Write-Status "SKIP: docker/nat-testbed/docker-compose.yml not found" "Yellow"
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
    Push-Location $composeDir
    docker compose down 2>&1 | Out-Null
    docker compose up -d --build 2>&1 | Out-Null
    $rc = $LASTEXITCODE
    Pop-Location
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
    $composeDir = Join-Path $script:REPO_ROOT "docker\nat-testbed"
    $composeFile = Join-Path $composeDir "docker-compose.yml"
    if (Test-Path $composeFile) {
        docker compose -f $composeFile down 2>&1 | Out-Null
    }
}

function Install-ApkOnDevice {
    # Install the debug APK on a specific emulator serial, or default.
    param([string]$Serial)
    $apk = Join-Path $PSScriptRoot "app\build\outputs\apk\debug\app-debug.apk"
    if (-not (Test-Path $apk)) {
        Write-Status "WARN: No APK at $apk" "Yellow"
        return $false
    }
    $args_ = if ($Serial) { @("-s", $Serial, "install", "-r", $apk) } else { @("install", "-r", $apk) }
    $result = Adb-Timeout -AdbArgs $args_ -Seconds 60
    return ($result -and $result -match "Success")
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
    $gitBash = "$script:DEP_BASE\git\bin\bash.exe"
    if (Test-Path $gitBash) { $bashExe = $gitBash }
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

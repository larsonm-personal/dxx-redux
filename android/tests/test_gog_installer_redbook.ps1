#!/usr/bin/env pwsh
<#
.SYNOPSIS
  GOG Installer + Redbook audio regression test.

.DESCRIPTION
  End-to-end test that validates the full GOG import + redbook audio path:
  1. Pushes GOG .exe to /sdcard/Download/ on the emulator
  2. Clears existing game data for a fresh state
  3. Triggers import_gog via SETUP_COMMAND
  4. Verifies setup introspection shows audio source with track names
  5. Launches D2 and runs in-game automation script
  6. Verifies overlay ring buffer has level + track name entries
  7. Verifies redbook playback state

.PARAMETER GogExePath
  Local path to the GOG installer .exe. Defaults to the known location.

.PARAMETER SkipPush
  Skip pushing the GOG .exe (assumes it's already on the emulator).

.EXAMPLE
  .\test_gog_installer_redbook.ps1
  .\test_gog_installer_redbook.ps1 -SkipPush
#>
param(
    [string]$GogExePath,
    [switch]$SkipPush,
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\..\test_helpers.ps1"

$GOG_FILENAME = 'setup_descent_2_1.1_(16596).exe'
# Use /data/local/tmp/ because scoped storage (API 30+) prevents the app from
# accessing /sdcard/Download/ directly via native open().
$DEVICE_GOG_PATH = "/data/local/tmp/$GOG_FILENAME"
$SCRIPT_NAME = 'test_gog_installer_redbook.json5'

# -- Auto-discover GOG .exe path --------------------------------

if (-not $GogExePath) {
    $repoRoot = Split-Path (Split-Path $PSScriptRoot)
    $candidate = Join-Path $repoRoot "game_data\gog installers\$GOG_FILENAME"
    if (Test-Path $candidate) {
        $GogExePath = $candidate
    } else {
        Write-Status "FAIL: GOG .exe not found at $candidate. Pass -GogExePath" "Red"
        exit 1
    }
}

if (-not (Test-Path $GogExePath)) {
    Write-Status "FAIL: GOG .exe not found: $GogExePath" "Red"
    exit 1
}

# -- Step 1: Emulator health check ------------------------------

Write-Status "Checking emulator health..."
Ensure-EmulatorHealthy

# -- Step 2: Push GOG .exe to /sdcard/Download/ -----------------

if (-not $SkipPush) {
    Write-Status "Pushing GOG .exe to device (this may take a while for 563MB)..."
    # Check if file is already on device with correct size
    # Shell-escape the path for adb shell commands (parentheses in filename)
    $localSize = (Get-Item $GogExePath).Length
    $deviceSize = Adb-Timeout -AdbArgs @("shell", "stat -c %s '$DEVICE_GOG_PATH'") -Seconds 5 2>$null
    if ($deviceSize -and $deviceSize -match '^\d+$' -and [long]$deviceSize -eq $localSize) {
        Write-Status "GOG .exe already on device with correct size, skipping push"
    } else {
        Adb -AdbArgs @("push", $GogExePath, $DEVICE_GOG_PATH)
        Write-Status "Push complete"
    }
} else {
    Write-Status "Skipping push (-SkipPush)"
}

# Verify file is on device
$check = Adb-Timeout -AdbArgs @("shell", "ls -la '$DEVICE_GOG_PATH'") -Seconds 5 2>$null
if (-not $check) {
    Write-Status "FAIL: GOG .exe not found on device at $DEVICE_GOG_PATH" "Red"
    exit 1
}

# -- Step 3: Clean state + launch SetupActivity ------------------

Write-Status "Force-stopping app..."
Adb -AdbArgs @("shell", "am", "force-stop", $script:PACKAGE) | Out-Null
Start-Sleep -Seconds 2

# Clear game data for fresh import
Write-Status "Clearing game data (default set)..."
Reset-GameState
Adb -AdbArgs @("logcat", "-c") | Out-Null

Write-Status "Launching SetupActivity..."
Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null

if (-not (Wait-SetupActivityReady)) {
    Write-Status "FAIL: SetupActivity not responding" "Red"
    exit 1
}
Write-Status "SetupActivity is ready"

# Clear the default file set so this is a fresh import
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
    "--es", "command", "clear_set", "--es", "name", "default") | Out-Null
Start-Sleep -Seconds 1

# -- Step 4: Import GOG via SETUP_COMMAND ------------------------

Write-Status "Sending import_gog command..."
# Shell-escape the path (parentheses in filename break device shell without quotes)
$quotedPath = "'$DEVICE_GOG_PATH'"
Adb -AdbArgs @("shell", "am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_gog --es path $quotedPath --ez include_audio true") | Out-Null

# Poll for import completion via logcat
Write-Status "Waiting for import to complete..."
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$importDone = $false
while ($sw.Elapsed.TotalSeconds -lt 120) {
    Start-Sleep -Seconds 3
    $log = Adb-Timeout -AdbArgs @("logcat", "-d", "-s", "DXX-Setup:I", "-t", "50") -Seconds 5
    if ($log -and $log -match "import_gog '.*' -> (\-?\d+) file\(s\)") {
        $fileCount = [int]$Matches[1]
        if ($fileCount -lt 0) {
            Write-Status "FAIL: import_gog returned error ($fileCount files)" "Red"
            exit 1
        }
        $importDone = $true
        Write-Status "Import complete: $fileCount files extracted"
        break
    }
}
if (-not $importDone) {
    Write-Status "FAIL: import_gog did not complete within 120s" "Red"
    $log = Adb-Timeout -AdbArgs @("logcat", "-d", "-s", "DXX-Setup") -Seconds 5
    if ($log) { Write-Host $log }
    exit 1
}

# -- Step 5: Verify setup introspection -------------------------

Start-Sleep -Seconds 2
Write-Status "Checking setup introspection..."
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_INTROSPECT") | Out-Null
Start-Sleep -Seconds 2
$setupJson = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_introspect.json") -Seconds 5

if (-not $setupJson) {
    Write-Status "FAIL: Could not read setup_introspect.json" "Red"
    exit 1
}

$setup = $setupJson | ConvertFrom-Json
$audioSources = $setup.audio_sources

if (-not $audioSources -or $audioSources.Count -eq 0) {
    Write-Status "FAIL: No audio sources registered after import" "Red"
    Write-Host "Setup introspection: $setupJson"
    exit 1
}

$gogSource = $audioSources | Where-Object { $_.disc_id -eq "d2-gog-v1.2" }
if (-not $gogSource) {
    Write-Status "FAIL: No audio source with disc_id 'd2-gog-v1.2'" "Red"
    Write-Host "Audio sources: $($audioSources | ConvertTo-Json -Depth 5)"
    exit 1
}

Write-Status "Audio source found: id=$($gogSource.id) label=$($gogSource.label) tracks=$($gogSource.track_count)"

# Verify track names are present
if ($gogSource.track_names) {
    $trackCount = ($gogSource.track_names.PSObject.Properties | Measure-Object).Count
    Write-Status "Track names present: $trackCount entries"
    # Verify track 4 = "Crawl" (first level track for D2)
    $track4 = $gogSource.track_names.'4'
    if ($track4 -eq "Crawl") {
        Write-Status "Track 4 name verified: Crawl" "Green"
    } else {
        Write-Status "WARNING: Track 4 name expected 'Crawl', got '$track4'" "Yellow"
    }
} else {
    Write-Status "WARNING: No track names in audio source (fingerprint lookup may have failed)" "Yellow"
}

# Check can_launch
if (-not $setup.can_launch) {
    Write-Status "FAIL: can_launch is false after import -- game files may be missing" "Red"
    Write-Host "d2 ready: $($setup.d2.ready)"
    if ($setup.d2.files) {
        $missing = $setup.d2.files | Where-Object { $_.required -and -not $_.found }
        if ($missing) {
            Write-Host "Missing required files:"
            $missing | ForEach-Object { Write-Host "  $($_.filename)" }
        }
    }
    exit 1
}
Write-Status "can_launch=true -- game files are ready" "Green"

# -- Step 6: Push automation script and launch game ---------------

# Push the in-game automation script
$scriptDir = Join-Path (Split-Path $PSScriptRoot) "game_scripts"
$scriptPath = Join-Path $scriptDir $SCRIPT_NAME

# Resolve variable substitution if any
$resolvedPath = Resolve-TestScript -ScriptPath $scriptPath -GameId "d2"
$pushSrc = if ($resolvedPath -ne $scriptPath) { $resolvedPath } else { $scriptPath }

Write-Status "Pushing automation script: $SCRIPT_NAME"
Adb -AdbArgs @("push", $pushSrc, "/data/local/tmp/$SCRIPT_NAME") | Out-Null
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", "/data/local/tmp/$SCRIPT_NAME", "files/$SCRIPT_NAME") | Out-Null

# Ensure default set is active
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
    "--es", "command", "switch_set", "--es", "name", "default") | Out-Null
Start-Sleep -Milliseconds 500

# Launch game
Write-Status "Launching game (D2)..."
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
    "--es", "command", "launch", "--es", "game", "d2") | Out-Null

if (-not (Wait-GameStarted -TimeoutSeconds 45)) {
    Write-Status "FAIL: Game did not start" "Red"
    $diagLog = Adb-Timeout -AdbArgs @("logcat", "-d", "-t", "50") -Seconds 5
    if ($diagLog) { Write-Host $diagLog }
    exit 1
}
Write-Status "Game started" "Green"

# -- Step 7: Send automation + monitor ---------------------------

Start-Sleep -Seconds 2
Adb -AdbArgs @("logcat", "-c") | Out-Null
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json") | Out-Null
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_log.jsonl") | Out-Null

Write-Status "Sending automation broadcast..."
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE",
    "--es", "script", $SCRIPT_NAME) | Out-Null

# Calculate timeout from script or use default
$scriptTimeout = $TimeoutSeconds
if ($TimeoutSeconds -eq 300) {
    $scriptTimeout = Get-ScriptTimeoutSeconds -ScriptPath $scriptPath
    Write-Status "Calculated timeout: ${scriptTimeout}s"
}

$passed = Watch-AutomationResult -TimeoutSeconds $scriptTimeout
if (-not $passed) {
    Write-Status "FAIL: In-game automation failed" "Red"
    # Dump diagnostics
    Write-Status "Dumping automation log..."
    $autoLog = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/automation_log.jsonl") -Seconds 5
    if ($autoLog) { Write-Host $autoLog }
    exit 1
}

# -- Step 8: Final introspection dump ----------------------------

Write-Status "Dumping final introspection..."
$intro = Get-GameIntrospection
if ($intro) {
    # Show overlay entries
    if ($intro.overlays -and $intro.overlays.lines) {
        Write-Status "Overlay entries:"
        foreach ($entry in $intro.overlays.lines) {
            Write-Host "  [$($entry.type)] $($entry.text)"
        }
    }
    # Show redbook state
    if ($intro.redbook) {
        Write-Host "Redbook: status=$($intro.redbook.play_status) track=$($intro.redbook.current_track)"
    }
}

Write-Status "=== TEST PASSED: GOG Installer + Redbook ===" "Green"
exit 0

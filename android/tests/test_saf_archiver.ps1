<#
.SYNOPSIS
    Tests the SAF leave-in-place PhysFS archiver on an Android emulator.

.DESCRIPTION
    This script verifies that the custom SAF PhysFS archiver can serve game files
    from outside the app's internal storage. It:
      1. Installs the debug APK
      2. Moves descent2.ham from the app's files dir to /sdcard/test_saf/
      3. Creates a .saf_manifest.json pointing to the external copy (test mode: direct path)
      4. Launches the game
      5. Runs test_saf_basic.json to verify the game loads through to gameplay
      6. Restores the original file and cleans up

    The test proves the full SAF archiver pipeline:
      physfsx.c mounts manifest -> archiver parses it -> saf_open_file() opens via direct path
      -> pread()-based I/O serves data to the engine.

.PARAMETER NoBuild
    Skip the Gradle build step (use an already-built APK).

.PARAMETER NoCleanup
    Skip cleanup (leave test files in place for debugging).
#>
param(
    [switch]$NoBuild,
    [switch]$NoCleanup
)

$ErrorActionPreference = "Continue"

# Flush helper -- ensures progress lines appear in captured output
function Write-Progress-Flush {
    param([string]$Message, [string]$Color = "White")
    Write-Host $Message -ForegroundColor $Color
    [Console]::Out.Flush()
}

# -- Shared env setup (JAVA_HOME, cmake, cargo) ----------------------------
. "$PSScriptRoot\..\test_helpers.ps1"

# -- Paths --------------------------------------------------------------
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
# Handle running from repo root or android/ dir
if (!(Test-Path "$repoRoot\android")) {
    $repoRoot = Split-Path -Parent $PSScriptRoot
    if (!(Test-Path "$repoRoot\android")) {
        $repoRoot = (Get-Location).Path
    }
}
$androidDir = "$repoRoot\android"
$apkPath = "$androidDir\app\build\outputs\apk\debug\app-debug.apk"
$scriptSource = "$androidDir\game_scripts\test_saf_basic.json5"
$_depBaseFile = Join-Path $repoRoot "dependency_base.txt"
if (-not (Test-Path $_depBaseFile)) {
    Write-Host "FAIL: dependency_base.txt not found at $_depBaseFile" -ForegroundColor Red
    exit 1
}
$DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
$adb = "$DEP_BASE\android-sdk\platform-tools\adb.exe"

$PACKAGE = "com.dxxredux.app"
$TEST_FILE = "descent2.ham"
$SAF_DIR = "/data/local/tmp/test_saf"
$TIMEOUT_SEC = 60

if (!(Test-Path $adb)) {
    Write-Host "FAIL: adb not found at $adb" -ForegroundColor Red
    exit 1
}

function Adb {
    param([string[]]$AdbArgs)
    if ($args) { $AdbArgs = @($AdbArgs) + @($args) }
    & $adb @AdbArgs 2>&1
}

function Adb-WithTimeout {
    # Run an adb command with a timeout; returns $null if it hangs
    param([int]$Seconds = 10, [string[]]$AdbArgs)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $adb
    $psi.Arguments = ($AdbArgs | ForEach-Object {
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
        return $stdoutTask.GetAwaiter().GetResult()
    } catch {
        return $null
    }
}

# -- Preflight ----------------------------------------------------------
Write-Host "=== SAF Archiver Test ===" -ForegroundColor Cyan
Write-Host ""

# Check emulator -- auto-start if needed
$devices = (Adb devices) -join "`n"
if ($devices -notmatch "emulator.*device") {
    Write-Host "Emulator not found -- attempting start..." -ForegroundColor Yellow
    $healthScript = Join-Path $androidDir "emu_health.ps1"
    & $healthScript -Restart -Wait -TimeoutSeconds 120
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 2) {
        Write-Host "FAIL: Could not start emulator (exit $LASTEXITCODE)" -ForegroundColor Red
        exit 1
    }
    # Poll for emulator to come online (replaces fixed sleep)
    $emSw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($emSw.Elapsed.TotalSeconds -lt 30) {
        $devices = (Adb devices) -join "`n"
        if ($devices -match "emulator.*device") { break }
        Start-Sleep -Seconds 1
    }
    $devices = (Adb devices) -join "`n"
    if ($devices -notmatch "emulator.*device") {
        Write-Host "FAIL: Emulator still not available after restart" -ForegroundColor Red
        exit 1
    }
}
Write-Host "[OK] Emulator connected" -ForegroundColor Green

# Ensure game data is on device before running the SAF test
if (-not (Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps))) {
    Write-Host "FAIL: Could not resolve game data deps" -ForegroundColor Red
    exit 1
}

# -- Step 1: Build ------------------------------------------------------
if (!$NoBuild) {
    Write-Host ""
    Write-Host "Step 1: Building debug APK..." -ForegroundColor Yellow
    Push-Location $androidDir
    try {
        & .\gradlew.bat assembleDebug --console=plain 2>&1 |
            Where-Object { $_ -match "BUILD |FAIL|error:" } |
            ForEach-Object { Write-Host "  $_" }
        if ($LASTEXITCODE -ne 0) {
            Write-Host "FAIL: Build failed" -ForegroundColor Red
            exit 1
        }
        Write-Progress-Flush "[OK] Build successful" Green
    } finally {
        Pop-Location
    }
} else {
    Write-Host "Step 1: Skipping build (--NoBuild)" -ForegroundColor DarkGray
}

# -- Step 2: Stop the game if running -----------------------------------
Write-Host ""
Write-Host "Step 2: Stopping game..." -ForegroundColor Yellow
$stopResult = Adb-WithTimeout -Seconds 10 -AdbArgs "shell", "am force-stop $PACKAGE"
if ($null -eq $stopResult) {
    Write-Host "  [WARN] force-stop timed out, continuing anyway" -ForegroundColor Yellow
}
Start-Sleep -Seconds 2
Write-Progress-Flush "[OK] Game stopped" Green

# Clean stale pilot/config files so the game starts fresh
Adb shell "run-as $PACKAGE find files -name '*.plr' -delete" | Out-Null
Adb shell "run-as $PACKAGE find files -name '*.plx' -delete" | Out-Null
Adb shell "run-as $PACKAGE find files -name 'descent.cfg' -delete" | Out-Null
Adb shell "run-as $PACKAGE rm -f files/controller_config.json" | Out-Null
Adb shell "run-as $PACKAGE rm -f files/file_sets.json" | Out-Null

# -- Step 3: Install APK -----------------------------------------------
Write-Host ""
Write-Host "Step 3: Installing debug APK..." -ForegroundColor Yellow
$installResult = Adb -AdbArgs @("install", "-r", $apkPath)
if ($installResult -match "Success") {
    Write-Progress-Flush "[OK] APK installed" Green
} else {
    Write-Host "  $installResult"
    Write-Host "FAIL: APK install failed" -ForegroundColor Red
    exit 1
}

# -- Step 4: Get file size and move to SAF location ---------------------
Write-Host ""
Write-Progress-Flush "Step 4: Moving $TEST_FILE to SAF test location..." Yellow

# Get the file size (game data lives under sets/default/)
$GAME_DATA_DIR = "files/sets/default"
$sizeOutput = Adb shell "run-as $PACKAGE stat -c '%s' $GAME_DATA_DIR/$TEST_FILE"
if (-not $sizeOutput -or $sizeOutput -match 'No such file') {
    # File missing -- a previous timed-out run may not have cleaned up.
    # Re-push via the standard game data deps mechanism.
    Write-Host "  $TEST_FILE missing on device, re-pushing..." -ForegroundColor Yellow
    if (-not (Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps))) {
        Write-Host "FAIL: Could not re-push game data deps" -ForegroundColor Red
        exit 1
    }
    $sizeOutput = Adb shell "run-as $PACKAGE stat -c '%s' $GAME_DATA_DIR/$TEST_FILE"
    if (-not $sizeOutput -or $sizeOutput -match 'No such file') {
        # Diagnostic: list what's actually in the sets directory
        Write-Host "  Diagnostic: contents of $GAME_DATA_DIR/" -ForegroundColor Yellow
        Adb shell "run-as $PACKAGE ls -la $GAME_DATA_DIR/" | ForEach-Object { Write-Host "    $_" }
        Write-Host "FAIL: $TEST_FILE still not found after re-push" -ForegroundColor Red
        exit 1
    }
}
$fileSize = [long]($sizeOutput.ToString().Trim())
Write-Host "  File size: $fileSize bytes"

# Create target directory and push the file from local game_data/
# Note: /sdcard/ is not accessible from native code due to scoped storage.
# Use /data/local/tmp/ which is world-readable on emulators.
Adb shell "mkdir -p $SAF_DIR" | Out-Null
$localGameData = "$repoRoot\game_data_to_copy_to_emulator\data\$($TEST_FILE.ToUpper())"
if (-not (Test-Path $localGameData)) {
    # Fallback: check extracted game data directories
    $localGameData = "$repoRoot\game_data\extracted\VERTIGO\$($TEST_FILE.ToUpper())"
}
if (Test-Path $localGameData) {
    Adb push $localGameData "$SAF_DIR/$TEST_FILE" | Out-Null
} else {
    # Fallback: pull from device and push to new location
    $tmpLocal = "$env:TEMP\$TEST_FILE"
    Adb shell "run-as $PACKAGE cat $GAME_DATA_DIR/$TEST_FILE" | Set-Content -Path $tmpLocal -AsByteStream
    Adb push $tmpLocal "$SAF_DIR/$TEST_FILE" | Out-Null
    Remove-Item $tmpLocal -ErrorAction SilentlyContinue
}
Adb shell "chmod 644 $SAF_DIR/$TEST_FILE" | Out-Null
Start-Sleep -Seconds 1

# Verify copy
$copiedSize = Adb shell "stat -c '%s' $SAF_DIR/$TEST_FILE" 2>&1
$copiedSize = [long]($copiedSize.ToString().Trim())
if ($copiedSize -ne $fileSize) {
    Write-Host "FAIL: File copy failed: expected $fileSize bytes, got $copiedSize" -ForegroundColor Red
    exit 1
}
Write-Host "  Copied to $SAF_DIR/$TEST_FILE ($copiedSize bytes)"

# Remove from app's game data dir (force SAF archiver usage)
Adb shell "run-as $PACKAGE rm $GAME_DATA_DIR/$TEST_FILE" | Out-Null
Write-Host "  Removed from app game data dir"

# -- Step 5: Launch SetupActivity first ---------------------------------
# Must start SetupActivity BEFORE creating the SAF manifest.  SetupActivity's
# LaunchedEffect runs pruneStaleEntries() which would delete our test manifest
# entries (plain file paths aren't openable via ContentResolver).  By starting
# SetupActivity first and waiting for pruning to finish, we avoid the race.
Write-Host ""
Write-Progress-Flush "Step 5: Launching SetupActivity..." Yellow
Adb shell "am start -n $PACKAGE/.SetupActivity" | Out-Null
Write-Host "  Waiting for SetupActivity..."

# Poll until SetupActivity's broadcast receiver is alive
Adb shell "run-as $PACKAGE rm -f files/setup_introspect.json" | Out-Null
$setupReady = $false
$sw = [System.Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt 30) {
    Start-Sleep -Seconds 2
    Adb shell "am broadcast -a com.dxxredux.SETUP_INTROSPECT" | Out-Null
    Start-Sleep -Seconds 1
    $probe = (Adb shell "run-as $PACKAGE cat files/setup_introspect.json" 2>&1) -join "`n"
    if ($probe -match '"screen"') { $setupReady = $true; break }
}
if (-not $setupReady) {
    Write-Host "[FAIL] SetupActivity not responding after 30s" -ForegroundColor Red
    exit 1
}

# Give LaunchedEffect time to finish pruning stale entries
Start-Sleep -Seconds 2

# -- Step 6: Create .saf_manifest.json (after pruning has finished) -----
Write-Host ""
Write-Progress-Flush "Step 6: Creating .saf_manifest.json..." Yellow

$manifestJson = @"
{
  "files": [
    {
      "filename": "$($TEST_FILE.ToLower())",
      "content_uri": "$SAF_DIR/$TEST_FILE",
      "size_bytes": $fileSize
    }
  ]
}
"@

# Write manifest via adb push + run-as cp
$manifestTmp = "/data/local/tmp/.saf_manifest.json"
$localManifestTmp = "$env:TEMP\saf_manifest_test.json"
$manifestJson | Set-Content -Path $localManifestTmp -NoNewline -Encoding UTF8
Adb push $localManifestTmp $manifestTmp | Out-Null
Adb shell "run-as $PACKAGE cp $manifestTmp $GAME_DATA_DIR/.saf_manifest.json" | Out-Null
Adb shell "rm -f $manifestTmp" | Out-Null
Remove-Item $localManifestTmp -ErrorAction SilentlyContinue

# Verify manifest was created
$manifestCheck = Adb shell "run-as $PACKAGE cat $GAME_DATA_DIR/.saf_manifest.json"
if ($manifestCheck -match "descent2.ham") {
    Write-Host "[OK] Manifest created with $TEST_FILE entry" -ForegroundColor Green
} else {
    Write-Host "  Manifest content: $manifestCheck"
    Write-Host "FAIL: Manifest creation failed" -ForegroundColor Red
    exit 1
}

# -- Step 7: Launch the game -------------------------------------------
Write-Host ""
Write-Progress-Flush "Step 7: Launching game..." Yellow
Adb shell "am broadcast -a com.dxxredux.SETUP_COMMAND --es command launch" | Out-Null

# Wait for the game process to be up and the native engine to start
$retries = 0
$gameStarted = $false
while ($retries -lt 15) {
    $procId = (Adb shell "pidof $PACKAGE" 2>&1) -join ""
    if ($procId -match "\d+") {
        # Check if introspection works (means native engine is running)
        Adb shell "am broadcast -a com.dxxredux.INTROSPECT" | Out-Null
        Start-Sleep -Seconds 1
        $introState = (Adb shell "run-as $PACKAGE cat files/introspect.json" 2>&1) -join "`n"
        if ($introState -match "screen_mode") {
            $gameStarted = $true
            break
        }
    }
    $retries++
    Start-Sleep -Seconds 2
}

if (!$gameStarted) {
    Write-Host "[WARN] Game engine not detected via introspection, trying automation anyway..." -ForegroundColor Yellow
}

# -- Step 8: Push and run the test automation script --------------------
Write-Host ""
Write-Progress-Flush "Step 8: Running test_saf_basic automation..." Yellow

$scriptBasename = "test_saf_basic.json5"
$deviceTmp = "/data/local/tmp/$scriptBasename"

Adb push $scriptSource $deviceTmp | Out-Null
Adb shell "run-as $PACKAGE cp $deviceTmp files/$scriptBasename" | Out-Null
Adb shell "rm -f $deviceTmp" | Out-Null

# Clear logcat and send automation broadcast
Adb logcat -c | Out-Null
Adb shell "am broadcast -a com.dxxredux.AUTOMATE --es script $scriptBasename" | Out-Null

Write-Host "  Monitoring for result (timeout: ${TIMEOUT_SEC}s)..."

# Monitor logcat for SCRIPT_RESULT
$startTime = Get-Date
$result = $null
$failDetail = ""

while (((Get-Date) - $startTime).TotalSeconds -lt $TIMEOUT_SEC) {
    $logLines = (Adb logcat -d -s "DXX-Automate:*" 2>&1) -join "`n"

    if ($logLines -match "SCRIPT_RESULT: PASS") {
        $result = "PASS"
        break
    }
    if ($logLines -match "SCRIPT_RESULT: FAIL") {
        $result = "FAIL"
        $failDetail = ($logLines -split "`n" | Select-String "SCRIPT_RESULT: FAIL|ASSERT_FAIL|ASSERT_EXPECTED") -join "`n"
        break
    }

    # Check if game crashed
    $procId = (Adb shell "pidof $PACKAGE" 2>&1) -join ""
    if ($procId -notmatch "\d+") {
        $result = "CRASH"
        $failDetail = "Game process died during test"
        break
    }

    Start-Sleep -Seconds 2
}

if ($null -eq $result) {
    $result = "TIMEOUT"
    $failDetail = "Test did not complete within ${TIMEOUT_SEC}s"
}

# -- Step 8: Report result ---------------------------------------------
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan

switch ($result) {
    "PASS" {
        Write-Host "  RESULT: PASS" -ForegroundColor Green
        Write-Host "  SAF archiver successfully served $TEST_FILE to the game engine" -ForegroundColor Green
        Write-Host "  The game loaded to level 1 with HAM data served via PhysFS SAF archiver" -ForegroundColor Green
    }
    "FAIL" {
        Write-Host "  RESULT: FAIL" -ForegroundColor Red
        Write-Host $failDetail
    }
    "CRASH" {
        Write-Host "  RESULT: CRASH" -ForegroundColor Red
        Write-Host "  $failDetail"
        # Grab crash log
        $crashLog = Adb logcat -d -s "AndroidRuntime:E" 2>&1 | Select-Object -Last 10
        Write-Host "  Last crash lines:" -ForegroundColor Yellow
        $crashLog | ForEach-Object { Write-Host "    $_" }
    }
    "TIMEOUT" {
        Write-Host "  RESULT: TIMEOUT" -ForegroundColor Red
        Write-Host "  $failDetail"
        # Dump last automation log lines
        $lastLog = Adb logcat -d -s "DXX-Automate:*" 2>&1 | Select-Object -Last 10
        Write-Host "  Last automation log lines:" -ForegroundColor Yellow
        $lastLog | ForEach-Object { Write-Host "    $_" }
    }
}

Write-Host "========================================" -ForegroundColor Cyan

# -- Step 9: Cleanup ---------------------------------------------------
if (!$NoCleanup) {
    Write-Host ""
    Write-Host "Step 9: Cleaning up..." -ForegroundColor Yellow

    # Stop the game
    Adb-WithTimeout -Seconds 10 -AdbArgs "shell", "am force-stop $PACKAGE" | Out-Null
    Start-Sleep -Seconds 1

    # Restore the file to app's files dir
    # Note: run-as can't read /data/local/tmp/, so re-push from local game_data
    $restoreTmp = "/data/local/tmp/$TEST_FILE"
    if (Test-Path $localGameData) {
        Adb push $localGameData $restoreTmp | Out-Null
    }
    Adb shell "run-as $PACKAGE cp $restoreTmp $GAME_DATA_DIR/$TEST_FILE" | Out-Null
    Adb shell "rm -f $restoreTmp" | Out-Null

    # Remove SAF test artifacts
    Adb shell "run-as $PACKAGE rm -f $GAME_DATA_DIR/.saf_manifest.json" | Out-Null
    Adb shell "rm -rf $SAF_DIR" | Out-Null

    # Verify restore
    $restored = Adb shell "run-as $PACKAGE stat -c '%s' $GAME_DATA_DIR/$TEST_FILE" 2>&1
    if ($restored.ToString().Trim() -eq $fileSize.ToString()) {
        Write-Host "[OK] $TEST_FILE restored to app files dir" -ForegroundColor Green
    } else {
        Write-Host "[WARN] $TEST_FILE restore may have failed (size=$restored, expected=$fileSize)" -ForegroundColor Yellow
    }
} else {
    Write-Host ""
    Write-Host "Step 9: Skipping cleanup (--NoCleanup)" -ForegroundColor DarkGray
}

Write-Host ""
if ($result -eq "PASS") {
    Write-Host "=== SAF ARCHIVER TEST PASSED ===" -ForegroundColor Green
    exit 0
} else {
    Write-Host "=== SAF ARCHIVER TEST FAILED ===" -ForegroundColor Red
    exit 1
}

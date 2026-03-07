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
      physfsx.c mounts manifest → archiver parses it → saf_open_file() opens via direct path
      → pread()-based I/O serves data to the engine.

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

# ── Paths ──────────────────────────────────────────────────────────────
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
$scriptSource = "$androidDir\game_scripts\test_saf_basic.json"
$adb = "C:\local\android-sdk\platform-tools\adb.exe"

$PACKAGE = "com.dxxredux.app"
$TEST_FILE = "descent2.ham"
$SAF_DIR = "/data/local/tmp/test_saf"
$TIMEOUT_SEC = 120

if (!(Test-Path $adb)) {
    Write-Error "adb not found at $adb"
    exit 1
}

function Adb { & $adb @args 2>&1 }

# ── Preflight ──────────────────────────────────────────────────────────
Write-Host "=== SAF Archiver Test ===" -ForegroundColor Cyan
Write-Host ""

# Check emulator
$devices = (Adb devices) -join "`n"
if ($devices -notmatch "emulator.*device") {
    Write-Error "No emulator found. Start one with android/run_emulator.sh"
    exit 1
}
Write-Host "[OK] Emulator connected" -ForegroundColor Green

# ── Step 1: Build ──────────────────────────────────────────────────────
if (!$NoBuild) {
    Write-Host ""
    Write-Host "Step 1: Building debug APK..." -ForegroundColor Yellow
    Push-Location $androidDir
    try {
        & .\gradlew.bat assembleDebug --console=plain 2>&1 | 
            Where-Object { $_ -match "BUILD |FAIL|error:" } | 
            ForEach-Object { Write-Host "  $_" }
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Build failed"
            exit 1
        }
        Write-Host "[OK] Build successful" -ForegroundColor Green
    } finally {
        Pop-Location
    }
} else {
    Write-Host "Step 1: Skipping build (--NoBuild)" -ForegroundColor DarkGray
}

# ── Step 2: Stop the game if running ───────────────────────────────────
Write-Host ""
Write-Host "Step 2: Stopping game..." -ForegroundColor Yellow
Adb shell "am force-stop $PACKAGE" | Out-Null
Start-Sleep -Seconds 2
Write-Host "[OK] Game stopped" -ForegroundColor Green

# ── Step 3: Install APK ───────────────────────────────────────────────
Write-Host ""
Write-Host "Step 3: Installing debug APK..." -ForegroundColor Yellow
$installResult = Adb install -r $apkPath
if ($installResult -match "Success") {
    Write-Host "[OK] APK installed" -ForegroundColor Green
} else {
    Write-Host "  $installResult"
    Write-Error "APK install failed"
    exit 1
}

# ── Step 4: Get file size and move to SAF location ─────────────────────
Write-Host ""
Write-Host "Step 4: Moving $TEST_FILE to SAF test location..." -ForegroundColor Yellow

# Get the file size
$sizeOutput = Adb shell "run-as $PACKAGE stat -c '%s' files/$TEST_FILE"
$fileSize = [long]($sizeOutput.Trim())
Write-Host "  File size: $fileSize bytes"

# Create target directory and push the file from local game_data/
# Note: /sdcard/ is not accessible from native code due to scoped storage.
# Use /data/local/tmp/ which is world-readable on emulators.
Adb shell "mkdir -p $SAF_DIR" | Out-Null
$localGameData = "$repoRoot\game_data\$($TEST_FILE.ToUpper())"
if (Test-Path $localGameData) {
    Adb push $localGameData "$SAF_DIR/$TEST_FILE" | Out-Null
} else {
    # Fallback: pull from device and push to new location
    $tmpLocal = "$env:TEMP\$TEST_FILE"
    Adb shell "run-as $PACKAGE cat files/$TEST_FILE" | Set-Content -Path $tmpLocal -AsByteStream
    Adb push $tmpLocal "$SAF_DIR/$TEST_FILE" | Out-Null
    Remove-Item $tmpLocal -ErrorAction SilentlyContinue
}
Adb shell "chmod 644 $SAF_DIR/$TEST_FILE" | Out-Null
Start-Sleep -Seconds 1

# Verify copy
$copiedSize = Adb shell "stat -c '%s' $SAF_DIR/$TEST_FILE" 2>&1
$copiedSize = [long]($copiedSize.ToString().Trim())
if ($copiedSize -ne $fileSize) {
    Write-Error "File copy failed: expected $fileSize bytes, got $copiedSize"
    exit 1
}
Write-Host "  Copied to $SAF_DIR/$TEST_FILE ($copiedSize bytes)"

# Remove from app's files dir (force SAF archiver usage)
Adb shell "run-as $PACKAGE rm files/$TEST_FILE" | Out-Null
Write-Host "  Removed from app files dir"

# ── Step 5: Create .saf_manifest.json ──────────────────────────────────
Write-Host ""
Write-Host "Step 5: Creating .saf_manifest.json..." -ForegroundColor Yellow

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
Adb shell "run-as $PACKAGE cp $manifestTmp files/.saf_manifest.json" | Out-Null
Adb shell "rm -f $manifestTmp" | Out-Null
Remove-Item $localManifestTmp -ErrorAction SilentlyContinue

# Verify manifest was created
$manifestCheck = Adb shell "run-as $PACKAGE cat files/.saf_manifest.json"
if ($manifestCheck -match "descent2.ham") {
    Write-Host "[OK] Manifest created with $TEST_FILE entry" -ForegroundColor Green
} else {
    Write-Host "  Manifest content: $manifestCheck"
    Write-Error "Manifest creation failed"
    exit 1
}

# ── Step 6: Launch the game ────────────────────────────────────────────
Write-Host ""
Write-Host "Step 6: Launching game..." -ForegroundColor Yellow
Adb shell "am start -n $PACKAGE/.SetupActivity" | Out-Null
Write-Host "  Waiting for SetupActivity..."
Start-Sleep -Seconds 3

# Check if game can launch by introspecting SetupActivity
Adb shell "am broadcast -a com.dxxredux.SETUP_INTROSPECT" | Out-Null
Start-Sleep -Seconds 1
$setupState = (Adb shell "run-as $PACKAGE cat files/setup_introspect.json" 2>&1) -join "`n"
Write-Host "  Setup state: $($setupState.Substring(0, [Math]::Min(200, $setupState.Length)))..."

if ($setupState -match '"can_launch"\s*:\s*true') {
    Write-Host "[OK] Setup says can_launch=true (SAF file recognized)" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Setup says can_launch=false -- SAF manifest not recognized" -ForegroundColor Red
    # Cleanup and exit
    if (!$NoCleanup) {
        Write-Host "  Cleaning up..."
        Adb shell "run-as $PACKAGE rm -f files/.saf_manifest.json" | Out-Null
        Adb shell "cat $SAF_DIR/$TEST_FILE | run-as $PACKAGE sh -c 'cat > files/$TEST_FILE'" | Out-Null
        Adb shell "rm -rf $SAF_DIR" | Out-Null
    }
    exit 1
}

# Use correct broadcast extra key: "command" (not "action")
Adb shell "am broadcast -a com.dxxredux.SETUP_COMMAND --es command launch" | Out-Null
Start-Sleep -Seconds 5

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

# ── Step 7: Push and run the test automation script ────────────────────
Write-Host ""
Write-Host "Step 7: Running test_saf_basic automation..." -ForegroundColor Yellow

$scriptBasename = "test_saf_basic.json"
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

# ── Step 8: Report result ─────────────────────────────────────────────
Write-Host ""
Write-Host "════════════════════════════════════════" -ForegroundColor Cyan

switch ($result) {
    "PASS" {
        Write-Host "  RESULT: PASS" -ForegroundColor Green
        Write-Host "  SAF archiver successfully served $TEST_FILE to the game engine." -ForegroundColor Green
        Write-Host "  The game loaded to level 1 with HAM data served via PhysFS SAF archiver." -ForegroundColor Green
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

Write-Host "════════════════════════════════════════" -ForegroundColor Cyan

# ── Step 9: Cleanup ───────────────────────────────────────────────────
if (!$NoCleanup) {
    Write-Host ""
    Write-Host "Step 9: Cleaning up..." -ForegroundColor Yellow
    
    # Stop the game
    Adb shell "am force-stop $PACKAGE" | Out-Null
    Start-Sleep -Seconds 1
    
    # Restore the file to app's files dir
    Adb shell "run-as $PACKAGE cp $SAF_DIR/$TEST_FILE files/$TEST_FILE" | Out-Null
    
    # Remove SAF test artifacts
    Adb shell "run-as $PACKAGE rm -f files/.saf_manifest.json" | Out-Null
    Adb shell "rm -rf $SAF_DIR" | Out-Null
    
    # Verify restore
    $restored = Adb shell "run-as $PACKAGE stat -c '%s' files/$TEST_FILE" 2>&1
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

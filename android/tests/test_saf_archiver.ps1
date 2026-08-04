#!/usr/bin/env pwsh
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

.PARAMETER DescriptorOnly
    Stop after proving the successfully mounted manifest descriptor was released

.PARAMETER RejectIncompleteOnly
    Stop after proving that native startup rejects a manifest truncated after an entry size

.PARAMETER ProviderPipeOnly
    Use the debug ContentProvider pipe and stop after proving it was staged and mounted

.PARAMETER ProviderStaleSizeOnly
    Use a seekable provider with a stale manifest size and require the current descriptor size
#>
param(
    [switch]$NoBuild,
    [switch]$NoCleanup,
    [switch]$DescriptorOnly,
    [switch]$RejectIncompleteOnly,
    [switch]$ProviderPipeOnly,
    [switch]$ProviderStaleSizeOnly
)

$ErrorActionPreference = "Continue"

# Flush helper -- ensures progress lines appear in captured output
function Write-Progress-Flush {
    param([string]$Message, [string]$Color = "White")
    Write-Host $Message -ForegroundColor $Color
    [Console]::Out.Flush()
}

# -- Shared env setup (JAVA_HOME, cmake, cargo) ----------------------------
. "$PSScriptRoot\..\helpers\test_helpers.ps1"

# -- Paths --------------------------------------------------------------
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$androidDir = Join-RegressionPath $repoRoot "android"
$apkPath = Join-RegressionPath $androidDir "app" "build" "outputs" "apk" "debug" "app-debug.apk"
$scriptSource = Join-RegressionPath $androidDir "game_scripts" "test_saf_basic.json5"
$_depBaseFile = Join-Path $repoRoot "dependency_base.txt"
if (-not (Test-Path $_depBaseFile)) {
    Write-Host "FAIL: dependency_base.txt not found at $_depBaseFile" -ForegroundColor Red
    exit 1
}
$DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
$adb = Resolve-RegressionAndroidSdkTool -DepBase $DEP_BASE -Subdir "platform-tools" -ToolName "adb" -EnvironmentVariable "ADB"

$PACKAGE = "com.dxxredux.app"
$TEST_FILE = "descent2.ham"
$SAF_DIR = "/data/local/tmp/test_saf"
$TIMEOUT_SEC = 60

function Resolve-LocalSafTestFile {
    $dep = Get-StandardGameDataDeps | Where-Object { $_.file -eq $TEST_FILE } | Select-Object -First 1
    if ($dep) {
        $idx = Read-GameDataIndex
        if ($idx -and $idx.ContainsKey($dep.sha256) -and (Test-Path -LiteralPath $idx[$dep.sha256])) {
            return $idx[$dep.sha256]
        }
    }

    $candidates = @(
        (Join-RegressionPath $repoRoot "game_data_to_copy_to_emulator" "data" ($TEST_FILE.ToUpper())),
        (Join-RegressionPath $repoRoot "game_data" "extracted" "VERTIGO" ($TEST_FILE.ToUpper()))
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    return $null
}

if (!(Test-Path $adb)) {
    Write-Host "FAIL: adb not found at $adb" -ForegroundColor Red
    exit 1
}

function Adb {
    param([string[]]$AdbArgs)
    if ($args) { $AdbArgs = @($AdbArgs) + @($args) }
    # Inject -s $Serial when a target device is known, so commands
    # don't fail with "more than one device/emulator"
    if ($script:Serial -and $AdbArgs[0] -ne "-s") {
        $AdbArgs = @("-s", $script:Serial) + @($AdbArgs)
    }
    & $adb @AdbArgs 2>&1
}

function Invoke-AdbWithTimeout {
    # Run an adb command with a timeout; returns $null if it hangs
    param([int]$Seconds = 10, [string[]]$AdbArgs)
    # Inject -s $Serial when a target device is known
    if ($script:Serial -and $AdbArgs[0] -ne "-s") {
        $AdbArgs = @("-s", $script:Serial) + @($AdbArgs)
    }
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
        $null = $stderrTask.GetAwaiter().GetResult()
        return $stdoutTask.GetAwaiter().GetResult()
    } catch {
        return $null
    }
}

function Read-AutomationFile {
    param([string]$Name)

    $content = Invoke-AdbWithTimeout -Seconds 5 -AdbArgs "shell", "run-as $PACKAGE cat files/$Name"
    if ($null -eq $content -or $content -match "No such file") {
        return $null
    }
    return $content.Trim()
}

function Write-AutomationLogTail {
    param([int]$Lines = 20)

    $stepLog = Read-AutomationFile "automation_log.jsonl"
    if ($stepLog) {
        ($stepLog -split "`n") | Select-Object -Last $Lines | ForEach-Object { Write-Host "    $_" }
    } else {
        Write-Host "    (not available)"
    }
}

# -- Preflight ----------------------------------------------------------
Write-Host "=== SAF Archiver Test ===" -ForegroundColor Cyan
Write-Host ""

# Check emulator -- auto-start if needed
$script:Serial = $null
$devices = (Adb devices) -join "`n"
if ($devices -notmatch "emulator.*device") {
    Write-Host "Emulator not found -- attempting start..." -ForegroundColor Yellow
    $healthScript = Join-Path (Join-Path $androidDir "helpers") "emu_health.ps1"
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

# Extract the first emulator serial for -s targeting
$emuMatch = [regex]::Match($devices, "(emulator-\d+)\s+device")
if ($emuMatch.Success) {
    $script:Serial = $emuMatch.Groups[1].Value
    Write-Host "  Target device: $($script:Serial)"
}

# Ensure game data is on device before running the SAF test
if (-not (Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps))) {
    Write-Host "FAIL: Could not resolve game data deps" -ForegroundColor Red
    exit 1
}

# -- Step 1: Build ------------------------------------------------------
if (!$NoBuild) {
    Write-Host ""
    Write-Host "Step 1: Building debug APK..." -ForegroundColor Yellow
    $gradleWrapper = Resolve-RegressionGradleWrapper -AndroidDir $androidDir
    & $gradleWrapper -p $androidDir assembleDebug --console=plain 2>&1 |
        Where-Object { $_ -match "BUILD |FAIL|error:" } |
        ForEach-Object { Write-Host "  $_" }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL: Build failed" -ForegroundColor Red
        exit 1
    }
    Write-Progress-Flush "[OK] Build successful" Green
} else {
    Write-Host "Step 1: Skipping build (--NoBuild)" -ForegroundColor DarkGray
}

$localGameData = Resolve-LocalSafTestFile
$fileSize = 0
$GAME_DATA_DIR = $script:DEFAULT_SET_DIR

$script:safCleanupDone = $false
function Invoke-SafCleanup {
    if ($script:safCleanupDone) { return }
    $script:safCleanupDone = $true

    if ($NoCleanup) {
        Write-Host ""
        Write-Host "Step 9: Skipping cleanup (--NoCleanup)" -ForegroundColor DarkGray
        return
    }

    Write-Host ""
    Write-Host "Step 9: Cleaning up..." -ForegroundColor Yellow

    Invoke-AdbWithTimeout -Seconds 10 -AdbArgs "shell", "am force-stop $PACKAGE" | Out-Null
    Start-Sleep -Seconds 1

    # Restore the file to the app's files dir. run-as cannot read
    # /data/local/tmp on every image, so prefer the local test data.
    $restoreTmp = "/data/local/tmp/$TEST_FILE"
    if ($localGameData -and (Test-Path -LiteralPath $localGameData)) {
        Adb push $localGameData $restoreTmp | Out-Null
        Adb shell "run-as $PACKAGE cp $restoreTmp $GAME_DATA_DIR/$TEST_FILE" | Out-Null
    } else {
        Adb shell "run-as $PACKAGE cp $SAF_DIR/$TEST_FILE $GAME_DATA_DIR/$TEST_FILE" | Out-Null
    }
    Adb shell "rm -f $restoreTmp" | Out-Null

    Adb shell "run-as $PACKAGE rm -f $GAME_DATA_DIR/.saf_manifest.json" | Out-Null
    Adb shell "run-as $PACKAGE rm -rf files/saf_provider_source cache/saf_descriptor_stage" | Out-Null
    Adb shell "rm -rf $SAF_DIR" | Out-Null

    $restored = Adb shell "run-as $PACKAGE stat -c '%s' $GAME_DATA_DIR/$TEST_FILE" 2>&1
    if ($fileSize -gt 0 -and $restored.ToString().Trim() -eq $fileSize.ToString()) {
        Write-Host "[OK] $TEST_FILE restored to app files dir" -ForegroundColor Green
    } else {
        Write-Host "[WARN] $TEST_FILE restore may have failed (size=$restored, expected=$fileSize)" -ForegroundColor Yellow
    }
}

Register-EngineEvent PowerShell.Exiting -Action { Invoke-SafCleanup } | Out-Null

# -- Step 2: Stop the game if running -----------------------------------
Write-Host ""
Write-Host "Step 2: Stopping game..." -ForegroundColor Yellow
$stopResult = Invoke-AdbWithTimeout -Seconds 10 -AdbArgs "shell", "am force-stop $PACKAGE"
if ($null -eq $stopResult) {
    Write-Host "  [WARN] force-stop timed out, continuing anyway" -ForegroundColor Yellow
}
Start-Sleep -Seconds 2
Write-Progress-Flush "[OK] Game stopped" Green

# Clean state that may have survived an earlier test or interrupted SAF run.
Reset-GameState
Adb shell "run-as $PACKAGE find files -name '*.plr' -delete" | Out-Null
Adb shell "run-as $PACKAGE find files -name '*.plx' -delete" | Out-Null
Adb shell "run-as $PACKAGE rm -f $($script:DEFAULT_SET_DIR)/.saf_manifest.json" | Out-Null
Adb shell "rm -rf $SAF_DIR" | Out-Null

# -- Step 3: Install APK -----------------------------------------------
Write-Host ""
Write-Host "Step 3: Installing debug APK..." -ForegroundColor Yellow
$installResult = Adb -AdbArgs @("install", "-r", $apkPath)
if ($installResult -match "Success") {
    Write-Progress-Flush "[OK] APK installed" Green
} else {
    Write-Host "  $installResult"
    Write-Host "FAIL: APK install failed" -ForegroundColor Red
    Invoke-SafCleanup
    exit 1
}

# -- Step 4: Get file size and move to SAF location ---------------------
Write-Host ""
Write-Progress-Flush "Step 4: Moving $TEST_FILE to SAF test location..." Yellow

# Get the file size from the active default file set.
$sizeOutput = Adb shell "run-as $PACKAGE stat -c '%s' $GAME_DATA_DIR/$TEST_FILE"
if (-not $sizeOutput -or $sizeOutput -match 'No such file') {
    # File missing -- a previous timed-out run may not have cleaned up.
    # Re-push via the standard game data deps mechanism.
    Write-Host "  $TEST_FILE missing on device, re-pushing..." -ForegroundColor Yellow
    if (-not (Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps))) {
        Write-Host "FAIL: Could not re-push game data deps" -ForegroundColor Red
        Invoke-SafCleanup
        exit 1
    }
    $sizeOutput = Adb shell "run-as $PACKAGE stat -c '%s' $GAME_DATA_DIR/$TEST_FILE"
    if (-not $sizeOutput -or $sizeOutput -match 'No such file') {
        # Diagnostic: list what's actually in the sets directory
        Write-Host "  Diagnostic: contents of $GAME_DATA_DIR/" -ForegroundColor Yellow
        Adb shell "run-as $PACKAGE ls -la $GAME_DATA_DIR/" | ForEach-Object { Write-Host "    $_" }
        Write-Host "FAIL: $TEST_FILE still not found after re-push" -ForegroundColor Red
        Invoke-SafCleanup
        exit 1
    }
}
$fileSize = [long]($sizeOutput.ToString().Trim())
Write-Host "  File size: $fileSize bytes"

# Create target directory and push the file from local game_data.
# Note: /sdcard/ is not accessible from native code due to scoped storage.
# Use /data/local/tmp/ which is world-readable on emulators.
Adb shell "mkdir -p $SAF_DIR" | Out-Null
if ($localGameData) {
    Adb push $localGameData "$SAF_DIR/$TEST_FILE" | Out-Null
} else {
    # Fallback: copy on-device without piping binary data through PowerShell text streams.
    Adb shell "run-as $PACKAGE cat $GAME_DATA_DIR/$TEST_FILE > $SAF_DIR/$TEST_FILE" | Out-Null
}
Adb shell "chmod 644 $SAF_DIR/$TEST_FILE" | Out-Null
Start-Sleep -Seconds 1

# Verify copy
$copiedSize = Adb shell "stat -c '%s' $SAF_DIR/$TEST_FILE" 2>&1
$copiedSize = [long]($copiedSize.ToString().Trim())
if ($copiedSize -ne $fileSize) {
    Write-Host "FAIL: File copy failed: expected $fileSize bytes, got $copiedSize" -ForegroundColor Red
    Invoke-SafCleanup
    exit 1
}
Write-Host "  Copied to $SAF_DIR/$TEST_FILE ($copiedSize bytes)"

if ($ProviderPipeOnly -or $ProviderStaleSizeOnly) {
    Adb shell "run-as $PACKAGE mkdir -p files/saf_provider_source" | Out-Null
    Adb shell "run-as $PACKAGE cp $SAF_DIR/$TEST_FILE files/saf_provider_source/$TEST_FILE" | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL: Could not prepare the provider-backed SAF source" -ForegroundColor Red
        Invoke-SafCleanup
        exit 1
    }
}

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
$setupReady = Wait-SetupActivityReady -TimeoutSeconds 75
if (-not $setupReady) {
    Write-Host "[FAIL] SetupActivity not responding after bounded readiness polling" -ForegroundColor Red
    Invoke-SafCleanup
    exit 1
}

# Give LaunchedEffect time to finish pruning stale entries
Start-Sleep -Seconds 2

# -- Step 6: Create .saf_manifest.json (after pruning has finished) -----
Write-Host ""
Write-Progress-Flush "Step 6: Creating .saf_manifest.json..." Yellow

$manifestContentUri = "$SAF_DIR/$TEST_FILE"
if ($ProviderPipeOnly) {
    $manifestContentUri = "content://com.dxxredux.app.saf-test/pipe/$TEST_FILE"
} elseif ($ProviderStaleSizeOnly) {
    $manifestContentUri = "content://com.dxxredux.app.saf-test/seekable/$TEST_FILE"
}
$manifestDeclaredSize = if ($ProviderStaleSizeOnly) { $fileSize - 17 } else { $fileSize }

$manifestJson = @"
{
  "files": [
{
  "filename": "$($TEST_FILE.ToLower())",
  "content_uri": "$manifestContentUri",
  "size_bytes": $manifestDeclaredSize
}
  ]
}
"@
if ($RejectIncompleteOnly) {
    $manifestJson =
    '{"files":[{"filename":"' + $TEST_FILE.ToLower() +
    '","content_uri":"' + $SAF_DIR + '/' + $TEST_FILE +
    '","size_bytes":' + $fileSize
}

# Stage the complete bytes inside the destination directory, then rename them
# over the visible generation. This matches the production publication contract.
$manifestTmp = "/data/local/tmp/.saf_manifest.json"
$localManifestTmp = Join-Path ([System.IO.Path]::GetTempPath()) "saf_manifest_test.json"
$manifestJson | Set-Content -Path $localManifestTmp -NoNewline -Encoding UTF8
Adb push $localManifestTmp $manifestTmp | Out-Null
$manifestStage = "$GAME_DATA_DIR/.saf_manifest.json.$([Guid]::NewGuid().ToString('N')).tmp"
Adb shell "run-as $PACKAGE cp $manifestTmp $manifestStage" | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL: Could not stage the complete SAF manifest" -ForegroundColor Red
    Invoke-SafCleanup
    exit 1
}
Adb shell "run-as $PACKAGE mv -f -- $manifestStage $GAME_DATA_DIR/.saf_manifest.json" | Out-Null
if ($LASTEXITCODE -ne 0) {
    Adb shell "run-as $PACKAGE rm -f $manifestStage" | Out-Null
    Write-Host "FAIL: Could not atomically publish the SAF manifest" -ForegroundColor Red
    Invoke-SafCleanup
    exit 1
}
Adb shell "rm -f $manifestTmp" | Out-Null
Remove-Item $localManifestTmp -ErrorAction SilentlyContinue

# Verify the complete schema and exact entry, not just a filename substring.
$manifestCheck = (Adb shell "run-as $PACKAGE cat $GAME_DATA_DIR/.saf_manifest.json") -join "`n"
if ($RejectIncompleteOnly) {
    # Keep local readiness complete so startup reaches the native manifest mount.
    Adb shell "run-as $PACKAGE cp $SAF_DIR/$TEST_FILE $GAME_DATA_DIR/$TEST_FILE" | Out-Null
    Adb logcat -c | Out-Null
    Adb shell "am broadcast -a com.dxxredux.SETUP_COMMAND --es command launch" | Out-Null

    $nativeRejected = $false
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        Start-Sleep -Seconds 1
        $nativeLog = (Adb logcat -d -v brief -s DXX-SAF-Archiver:D '*:S') -join "`n"
        if ($nativeLog -match 'SAF_openArchive: invalid or incomplete manifest') {
            $nativeRejected = $true
            break
        }
    }
    if (-not $nativeRejected) {
        Write-Host "FAIL: Native startup did not reject the incomplete SAF manifest" -ForegroundColor Red
        Write-Host $nativeLog
        Invoke-SafCleanup
        exit 1
    }

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  RESULT: PASS" -ForegroundColor Green
    Write-Host "  Native startup rejected the incomplete SAF manifest" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Cyan
    Invoke-SafCleanup
    exit 0
}
try {
    $manifestObject = $manifestCheck | ConvertFrom-Json -ErrorAction Stop
    $manifestFiles = @($manifestObject.files)
    $manifestValid =
    $manifestObject.PSObject.Properties.Name.Count -eq 1 -and
    $manifestFiles.Count -eq 1 -and
    $manifestFiles[0].PSObject.Properties.Name.Count -eq 3 -and
    $manifestFiles[0].filename -ceq $TEST_FILE.ToLower() -and
    $manifestFiles[0].content_uri -ceq $manifestContentUri -and
    $manifestFiles[0].size_bytes -eq $manifestDeclaredSize
} catch {
    $manifestValid = $false
}
if ($manifestValid) {
    Write-Host "[OK] Manifest created with $TEST_FILE entry" -ForegroundColor Green
} else {
    Write-Host "  Manifest content: $manifestCheck"
    Write-Host "FAIL: Manifest creation failed" -ForegroundColor Red
    Invoke-SafCleanup
    exit 1
}

# -- Step 7: Launch the game -------------------------------------------
Write-Host ""
Write-Progress-Flush "Step 7: Launching game..." Yellow
Adb shell "run-as $PACKAGE rm -f files/introspect.json" | Out-Null
if ($ProviderPipeOnly -or $ProviderStaleSizeOnly) { Adb logcat -c | Out-Null }
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
} else {
    $gamePid = (($procId -split '\s+' | Where-Object { $_ -match '^\d+$' }) | Select-Object -First 1)
    $fdListing = (Adb shell "run-as $PACKAGE ls -l /proc/$gamePid/fd" 2>&1) -join "`n"
    if ($fdListing -notmatch ' -> ') {
        Write-Host "FAIL: Could not inspect game file descriptors after SAF mount" -ForegroundColor Red
        Write-Host $fdListing
        Invoke-SafCleanup
        exit 1
    }
    if ($fdListing -match '\.saf_manifest\.json') {
        Write-Host "FAIL: SAF manifest descriptor remains open after successful mount" -ForegroundColor Red
        Write-Host $fdListing
        Invoke-SafCleanup
        exit 1
    }
    Write-Host "[OK] SAF manifest descriptor released after mount" -ForegroundColor Green
}

if ($ProviderStaleSizeOnly) {
    $nativeLog = (Adb logcat -d -v brief -s DXX-SAF-Archiver:I '*:S') -join "`n"
    if ($nativeLog -notmatch "SAF source size changed.*manifest=$manifestDeclaredSize current=$fileSize") {
        Write-Host "FAIL: Native SAF I/O did not replace stale metadata with the current descriptor size" -ForegroundColor Red
        Write-Host $nativeLog
        Invoke-SafCleanup
        exit 1
    }
    $stageLog = (Adb logcat -d -v brief -s DXX-SAF-Stage:I '*:S') -join "`n"
    if ($stageLog -match 'Staged nonseekable SAF source') {
        Write-Host "FAIL: Seekable provider source was staged unnecessarily" -ForegroundColor Red
        Write-Host $stageLog
        Invoke-SafCleanup
        exit 1
    }
    Write-Host "[OK] Seekable provider used its current descriptor size" -ForegroundColor Green
}

if ($DescriptorOnly -or $ProviderStaleSizeOnly) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  RESULT: PASS" -ForegroundColor Green
    if ($ProviderStaleSizeOnly) {
        Write-Host "  Seekable provider mounted with its current descriptor size" -ForegroundColor Green
    } else {
        Write-Host "  SAF manifest descriptor was released after successful mount" -ForegroundColor Green
    }
    Write-Host "========================================" -ForegroundColor Cyan
    Invoke-SafCleanup
    exit 0
}

# -- Step 8: Push and run the test automation script --------------------
Write-Host ""
Write-Progress-Flush "Step 8: Running test_saf_basic automation..." Yellow

$scriptBasename = "test_saf_basic.json5"
$deviceTmp = "/data/local/tmp/$scriptBasename"

Adb push $scriptSource $deviceTmp | Out-Null
Adb shell "run-as $PACKAGE cp $deviceTmp files/$scriptBasename" | Out-Null
Adb shell "rm -f $deviceTmp" | Out-Null

# Preserve the provider launch log so the full gameplay result can also prove staging
if (-not $ProviderPipeOnly) { Adb logcat -c | Out-Null }
Adb shell "run-as $PACKAGE rm -f files/automation_result.json files/automation_log.jsonl" | Out-Null
Adb shell "am broadcast -a com.dxxredux.AUTOMATE --es script $scriptBasename" | Out-Null

Write-Host "  Monitoring for result (timeout: ${TIMEOUT_SEC}s)..."

# Monitor logcat for SCRIPT_RESULT
$startTime = Get-Date
$result = $null
$failDetail = ""

while (((Get-Date) - $startTime).TotalSeconds -lt $TIMEOUT_SEC) {
    if ($ProviderPipeOnly) {
        $stageLog = (Adb logcat -d -v brief -s DXX-SAF-Stage:I '*:S') -join "`n"
        $stepLog = Read-AutomationFile "automation_log.jsonl"
        if ($stageLog -match 'Staged nonseekable SAF source.*descent2\.ham' -and
            $stepLog -match '"step":14.*"action":"skip_briefing"') {
            $result = "PASS"
            break
        }
    }
    $resultJson = Read-AutomationFile "automation_result.json"
    if ($resultJson -and $resultJson -match '"result"') {
        try {
            $resultObj = $resultJson | ConvertFrom-Json
            if ($resultObj.result -eq "PASS") {
                $result = "PASS"
                break
            }
            if ($resultObj.result -eq "FAIL") {
                $result = "FAIL"
                $reason = if ($resultObj.reason) { $resultObj.reason } else { "unknown" }
                $failDetail = "Automation failed at step $($resultObj.steps_completed)/$($resultObj.total_steps): $reason"
                break
            }
        } catch {
            # Partial writes are possible while the game is closing the file
        }
    }

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

if ($ProviderPipeOnly -and $result -eq "PASS") {
    $stageLog = (Adb logcat -d -v brief -s DXX-SAF-Stage:I '*:S') -join "`n"
    if ($stageLog -notmatch 'Staged nonseekable SAF source.*descent2\.ham') {
        $result = "FAIL"
        $failDetail = "Gameplay passed without evidence that the provider pipe used seekable staging"
    } else {
        Write-Host "[OK] Provider pipe staged before the successful gameplay read" -ForegroundColor Green
    }
}

# -- Step 8: Report result ---------------------------------------------
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan

switch ($result) {
    "PASS" {
        Write-Host "  RESULT: PASS" -ForegroundColor Green
        if ($ProviderPipeOnly) {
            Write-Host "  Provider pipe was staged before level briefing through the production URI path" -ForegroundColor Green
        } else {
            Write-Host "  SAF archiver successfully served $TEST_FILE to the game engine" -ForegroundColor Green
            Write-Host "  The game loaded to level 1 with HAM data served via PhysFS SAF archiver" -ForegroundColor Green
        }
    }
    "FAIL" {
        Write-Host "  RESULT: FAIL" -ForegroundColor Red
        Write-Host $failDetail
        Write-Host "  Last automation file lines:" -ForegroundColor Yellow
        Write-AutomationLogTail -Lines 20
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
        Write-Host "  Last automation file lines:" -ForegroundColor Yellow
        Write-AutomationLogTail -Lines 20
    }
}

Write-Host "========================================" -ForegroundColor Cyan

# -- Step 9: Cleanup ---------------------------------------------------
Invoke-SafCleanup

Write-Host ""
if ($result -eq "PASS") {
    Write-Host "=== SAF ARCHIVER TEST PASSED ===" -ForegroundColor Green
    exit 0
} else {
    Write-Host "=== SAF ARCHIVER TEST FAILED ===" -ForegroundColor Red
    Invoke-SafCleanup
    exit 1
}

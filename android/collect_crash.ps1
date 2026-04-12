<#
.SYNOPSIS
    Collects crash diagnostics after reproducing a game crash.
.DESCRIPTION
    Clears logcat, waits for you to reproduce the crash, then collects:
    - logcat output
    - native tombstone (stack trace)
    - debug log files from the engine
    - last introspection dump
    - git commit hash
    All output goes to temp\crash_report\.
.EXAMPLE
    .\android\collect_crash.ps1
    # Then reproduce the crash in the emulator, then press Enter.
#>
$ErrorActionPreference = "Continue"
$reportDir = Join-Path $PSScriptRoot "..\temp\crash_report"
if (Test-Path $reportDir) { Remove-Item $reportDir -Recurse -Force }
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null

$PACKAGE = "com.dxxredux.app"

# Verify emulator
$devs = adb devices 2>&1 | Out-String
if ($devs -notmatch "emulator.*device") {
    Write-Host "ERROR: No emulator found. Start one first" -ForegroundColor Red
    Write-Host "  emulator -avd Nexus5X_Light_1 -no-snapshot-save -gpu swiftshader_indirect"
    exit 1
}

Write-Host "=== Crash Diagnostic Collector ===" -ForegroundColor Cyan
Write-Host ""

# Step 1: Clear logcat and start capturing
Write-Host "[1/5] Clearing logcat..." -ForegroundColor Yellow
adb logcat -c

# Start logcat capture in background
$logcatFile = Join-Path $reportDir "crash_logcat.txt"
$logcatJob = Start-Job -ScriptBlock {
    param($outFile)
    & adb logcat 2>&1 | Out-File $outFile -Encoding utf8
} -ArgumentList $logcatFile

Write-Host "[2/5] Logcat capture started" -ForegroundColor Green
Write-Host ""
Write-Host "Now reproduce the crash in the emulator" -ForegroundColor White
Write-Host "Steps:" -ForegroundColor White
Write-Host "  1. Launch D2, accept pilot"
Write-Host "  2. Close game (Quit from main menu)"
Write-Host "  3. Edit D2 autoselect in launcher, save"
Write-Host "  4. Launch game again"
Write-Host "  5. Options -> Secondary autoselect ordering -> Escape -> Escape"
Write-Host "  6. Wait for crash"
Write-Host ""
Write-Host "Press ENTER after the crash occurs (or after 'app stopped' dialog)..." -ForegroundColor Cyan
Read-Host | Out-Null

# Step 3: Stop logcat capture
Write-Host "[3/5] Stopping logcat capture..." -ForegroundColor Yellow
Stop-Job $logcatJob -ErrorAction SilentlyContinue
Remove-Job $logcatJob -Force -ErrorAction SilentlyContinue
# Also grab a fresh dump in case the job missed the tail
adb logcat -d >> $logcatFile 2>$null

# Step 4: Collect tombstone
Write-Host "[4/5] Collecting crash data..." -ForegroundColor Yellow

# Tombstone
$tombFile = Join-Path $reportDir "crash_tombstone.txt"
$latestTomb = adb shell "ls -t /data/tombstones/ 2>/dev/null | head -1" 2>$null | Out-String
$latestTomb = $latestTomb.Trim()
if ($latestTomb) {
    adb shell "cat /data/tombstones/$latestTomb" 2>$null | Out-File $tombFile -Encoding utf8
    Write-Host "  Tombstone: $latestTomb" -ForegroundColor Green
} else {
    Write-Host "  No tombstone found (try: adb bugreport temp\crash_report\bugreport.zip)" -ForegroundColor Yellow
    # Try bugreport as fallback
    adb bugreport (Join-Path $reportDir "bugreport.zip") 2>$null
}

# Debug log files
$debugLogDir = Join-Path $reportDir "debuglogs"
New-Item -ItemType Directory -Path $debugLogDir -Force | Out-Null
$debugLogList = adb shell "run-as $PACKAGE ls files/debuglogs/" 2>$null
if ($debugLogList) {
    foreach ($logFile in ($debugLogList -split "`n" | Where-Object { $_.Trim() })) {
        $trimmed = $logFile.Trim()
        adb shell "run-as $PACKAGE cat files/debuglogs/$trimmed" 2>$null | Out-File (Join-Path $debugLogDir $trimmed) -Encoding utf8
    }
}

# Introspection dump
$introFile = Join-Path $reportDir "crash_introspect.json"
adb shell "run-as $PACKAGE cat files/introspect.json" 2>$null | Out-File $introFile -Encoding utf8

# Automation log (if present)
$autoLogFile = Join-Path $reportDir "crash_automation_log.jsonl"
adb shell "run-as $PACKAGE cat files/automation_log.jsonl" 2>$null | Out-File $autoLogFile -Encoding utf8

# Step 5: Record metadata
Write-Host "[5/5] Recording metadata..." -ForegroundColor Yellow
$metaFile = Join-Path $reportDir "metadata.txt"
$commitHash = git rev-parse --short HEAD 2>$null
$commitFull = git rev-parse HEAD 2>$null
$branch = git branch --show-current 2>$null
$timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
@"
Crash report collected: $timestamp
Git commit: $commitHash ($commitFull)
Git branch: $branch
Emulator: $(adb shell getprop ro.product.model 2>$null)
Android: $(adb shell getprop ro.build.version.release 2>$null) (API $(adb shell getprop ro.build.version.sdk 2>$null))
ABI: $(adb shell getprop ro.product.cpu.abi 2>$null)
"@ | Out-File $metaFile -Encoding utf8

# Summary
Write-Host ""
Write-Host "=== Collection complete ===" -ForegroundColor Cyan
Write-Host "Files saved to: $reportDir" -ForegroundColor Green
Get-ChildItem $reportDir | ForEach-Object {
    $size = if ($_.Length -gt 1024) { "{0:N0} KB" -f ($_.Length / 1024) } else { "$($_.Length) B" }
    Write-Host "  $($_.Name)  ($size)"
}

# Quick check: was there actually a crash?
$logContent = Get-Content $logcatFile -Raw -ErrorAction SilentlyContinue
if ($logContent -match "Fatal signal") {
    Write-Host ""
    Write-Host "CRASH DETECTED in logcat:" -ForegroundColor Red
    $logContent -split "`n" | Where-Object { $_ -match "Fatal signal|backtrace:|#\d+ pc" } |
        Select-Object -First 15 | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
} else {
    Write-Host ""
    Write-Host "NOTE: No 'Fatal signal' found in logcat. The crash may not have" -ForegroundColor Yellow
    Write-Host "been a native SIGSEGV, or logcat capture may have missed it" -ForegroundColor Yellow
    Write-Host "Check the tombstone file for details" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "To share: zip temp\crash_report and provide along with these repro steps" -ForegroundColor Cyan

#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Build the APK, launch the emulator, install and run the app

.DESCRIPTION
    PowerShell equivalent of run_emulator.sh.
    Rebuilds the APK automatically when source files are newer than the
    existing APK (unless -NoBuild is specified).

.EXAMPLE
    .\Run-Emulator.ps1
    .\Run-Emulator.ps1 -NoBuild
    .\Run-Emulator.ps1 -NoData
    .\Run-Emulator.ps1 -Rebuild
    .\Run-Emulator.ps1 -NoBuild -NoData
#>

param(
    [switch]$NoBuild,
    [switch]$NoData,
    [switch]$Rebuild
)

$ErrorActionPreference = "Stop"

$AVD_NAME = "Nexus5X_Light_1"
$PACKAGE = "com.dxxredux.app"
$ACTIVITY = "com.dxxredux.app.SetupActivity"

$ScriptDir = Split-Path -Parent -Path $PSCommandPath
$RepoRoot = Split-Path -Parent -Path $ScriptDir
$HelpersDir = Join-Path $ScriptDir "helpers"
. (Join-Path $HelpersDir "test_host_platform.ps1")

# -- Resolve environment from dependency_base.txt ----------------------------
$depBaseFile = Join-Path $RepoRoot "dependency_base.txt"
if (-not (Test-Path $depBaseFile)) {
    Write-Host "ERROR: dependency_base.txt not found at $depBaseFile" -ForegroundColor Red
    exit 1
}
$DEP_BASE = (Get-Content $depBaseFile -First 1).Trim()

# Find newest folder matching a prefix
function Find-Newest {
    param([string]$Prefix)
    $matchDirs = Get-ChildItem -Path $DEP_BASE -Directory -Filter "${Prefix}*" -ErrorAction SilentlyContinue |
        Sort-Object Name
    if ($matchDirs) { return $matchDirs[-1].FullName }
    return $null
}

if (-not $env:JAVA_HOME) {
    $jdk = Find-Newest "jdk-"
    if ($jdk) { $env:JAVA_HOME = $jdk }
    else { Write-Host "WARNING: No jdk-* folder found in $DEP_BASE" -ForegroundColor Yellow }
}
if (-not $env:ANDROID_HOME) {
    $sdk = Find-Newest "android-sdk"
    if ($sdk) {
        $env:ANDROID_HOME = $sdk
        $env:ANDROID_SDK_ROOT = $sdk
    } else {
        Write-Host "WARNING: No android-sdk* folder found in $DEP_BASE" -ForegroundColor Yellow
    }
}

Write-Host "JAVA_HOME=$env:JAVA_HOME"
Write-Host "ANDROID_HOME=$env:ANDROID_HOME"
Write-Host ""

$ADB = Resolve-RegressionAndroidSdkTool -DepBase $DEP_BASE -Subdir "platform-tools" -ToolName "adb" -EnvironmentVariable "ADB"
$EMULATOR = Resolve-RegressionAndroidSdkTool -DepBase $DEP_BASE -Subdir "emulator" -ToolName "emulator"

if (-not (Test-Path $EMULATOR)) {
    Write-Host "ERROR: Emulator not found at $EMULATOR" -ForegroundColor Red
    exit 1
}

$APK = Join-RegressionPath $ScriptDir "app" "build" "outputs" "apk" "debug" "app-debug.apk"

# -- 1. Build APK (auto-rebuild if stale) ------------------------------------
if (-not $NoBuild) {
    $needsBuild = $true
    if (Test-Path $APK) {
        $apkTime = (Get-Item $APK).LastWriteTime
        # Check if any source or build-config file is newer than the APK
        $srcDirs = @(
            (Join-Path $ScriptDir "app\src"),
            (Join-Path $RepoRoot "d1"),
            (Join-Path $RepoRoot "d2"),
            (Join-Path $RepoRoot "cmake")
        )
        $buildFiles = @(
            (Join-Path $ScriptDir "build.gradle"),
            (Join-Path $ScriptDir "settings.gradle"),
            (Join-Path $ScriptDir "gradle.properties"),
            (Join-RegressionPath $ScriptDir "app" "build.gradle")
        )
        $newerFiles = foreach ($dir in $srcDirs) {
            if (Test-Path $dir) {
                Get-ChildItem -Path $dir -Recurse -File -ErrorAction SilentlyContinue |
                    Where-Object { $_.LastWriteTime -gt $apkTime } |
                    Select-Object -First 1
            }
        }
        if (-not $newerFiles) {
            # Also check individual build config files
            $newerFiles = foreach ($f in $buildFiles) {
                if ((Test-Path $f) -and (Get-Item $f).LastWriteTime -gt $apkTime) {
                    Get-Item $f
                }
            }
        }
        if (-not $newerFiles) {
            Write-Host "APK is up to date, skipping build (APK: $apkTime)"
            $needsBuild = $false
        }
    }

    if ($needsBuild) {
        Write-Host "=== Building APK ==="
        $gradleWrapper = Resolve-RegressionGradleWrapper -AndroidDir $ScriptDir
        & $gradleWrapper -p $ScriptDir assembleDebug --no-daemon 2>&1 | ForEach-Object { Write-Host $_ }
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: Build failed" -ForegroundColor Red
            exit 1
        }
        Write-Host ""
    }
}

if (-not (Test-Path $APK)) {
    Write-Host "ERROR: APK not found at $APK" -ForegroundColor Red
    Write-Host "Run without -NoBuild, or build first"
    exit 1
}

# -- 2. Launch emulator (if not already running) -----------------------------

# -- 2a. Rebuild AVD if requested -------------------------------------------
if (-not $Rebuild) {
    $answer = Read-Host "Delete and rebuild emulator AVD? (y/N)"
    if ($answer -eq 'y' -or $answer -eq 'Y') {
        $Rebuild = $true
    }
}

if ($Rebuild) {
    Write-Host "=== Rebuilding AVD ($AVD_NAME) ==="
    # Stop only the running instance backed by the AVD being rebuilt.
    $targetSerial = $null
    foreach ($deviceLine in @(& $ADB devices 2>$null)) {
        if ($deviceLine -match '^(emulator-\d+)\s+device$') {
            $serial = $Matches[1]
            $reportedName = @(& $ADB -s $serial emu avd name 2>$null) |
                Where-Object { $_ -and $_.Trim() -ne "OK" } |
                Select-Object -First 1
            if ($reportedName -and $reportedName.Trim() -eq $AVD_NAME) {
                $targetSerial = $serial
                break
            }
        }
    }
    if ($targetSerial) {
        Write-Host "Stopping $AVD_NAME on $targetSerial"
        & $ADB -s $targetSerial emu kill 2>$null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: Failed to stop $AVD_NAME on $targetSerial" -ForegroundColor Red
            exit 1
        }
        Start-Sleep -Seconds 3
    }
    $createScript = Join-Path $ScriptDir "get_deps\helpers\create_light_avds.ps1"
    if (Test-Path $createScript) {
        & $createScript -Force -AvdName $AVD_NAME
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: AVD recreation failed" -ForegroundColor Red
            exit 1
        }
        Write-Host "AVD rebuilt successfully"
    } else {
        Write-Host "ERROR: create_light_avds.ps1 not found at $createScript" -ForegroundColor Red
        exit 1
    }
}

Write-Host "=== Launching emulator ($AVD_NAME) ==="

$running = & $ADB devices 2>$null | Select-String "emulator-"
if ($running) {
    Write-Host "Emulator already running"
} else {
    $emulatorStart = @{
        FilePath     = $EMULATOR
        ArgumentList = @("-avd", $AVD_NAME, "-no-snapshot", "-gpu", "host")
    }
    if (Test-RegressionWindowsHost) {
        $emulatorStart.WindowStyle = "Normal"
    } else {
        $logDir = Join-Path $RepoRoot "temp"
        New-Item -ItemType Directory -Force -Path $logDir | Out-Null
        $emulatorStart.RedirectStandardOutput = Join-Path $logDir "emulator_${AVD_NAME}.out.log"
        $emulatorStart.RedirectStandardError = Join-Path $logDir "emulator_${AVD_NAME}.err.log"
    }
    Start-Process @emulatorStart
    Write-Host "Emulator started. Waiting for boot..."
}

# -- 3. Wait for device -----------------------------------------------------
Write-Host "Waiting for device to come online..."
& $ADB wait-for-device 2>$null

Write-Host "Waiting for boot to complete..."
$bootComplete = $false
for ($i = 0; $i -lt 120; $i++) {
    $prop = & $ADB shell getprop sys.boot_completed 2>$null
    if ($prop -and $prop.Trim() -eq "1") {
        $bootComplete = $true
        break
    }
    Start-Sleep -Seconds 2
}

if (-not $bootComplete) {
    Write-Host "ERROR: Emulator did not finish booting within 4 minutes" -ForegroundColor Red
    exit 1
}
Write-Host "Device booted"

# -- 4. Install APK ---------------------------------------------------------
Write-Host ""
Write-Host "=== Installing APK ==="
& $ADB install -r $APK
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: APK install failed" -ForegroundColor Red
    exit 1
}

# -- 4b. Add home screen icon (emulator only) --------------------------------
$LAUNCHER_DIR = "/data/data/com.google.android.apps.nexuslauncher/databases"
$ICON_INTENT = "#Intent;action=android.intent.action.MAIN;category=android.intent.category.LAUNCHER;launchFlags=0x10200000;component=${PACKAGE}/.SetupActivity;end"

& $ADB root 2>$null
if ($LASTEXITCODE -eq 0) {
    Start-Sleep -Seconds 1
    $launcherDb = (& $ADB shell "ls ${LAUNCHER_DIR}/launcher*.db 2>/dev/null | head -1" 2>$null)
    if ($launcherDb) {
        $launcherDb = $launcherDb.Trim()
    }
    if ($launcherDb) {
        $TMPSQL = "/data/local/tmp/_launcher_icon.sql"

        & $ADB shell "echo `"SELECT COUNT(*) FROM favorites WHERE intent LIKE '%${PACKAGE}%';`" > $TMPSQL" 2>$null
        $existing = (& $ADB shell "sqlite3 '$launcherDb' < $TMPSQL" 2>$null)
        if ($existing) { $existing = $existing.Trim() }

        if ($existing -eq "0" -or -not $existing) {
            & $ADB shell "echo 'SELECT COALESCE(MAX(_id),0) FROM favorites;' > $TMPSQL" 2>$null
            $maxId = (& $ADB shell "sqlite3 '$launcherDb' < $TMPSQL" 2>$null)
            if ($maxId) { $maxId = [int]$maxId.Trim() } else { $maxId = 0 }
            $nextId = $maxId + 1

            & $ADB shell "echo `"INSERT INTO favorites (_id, title, intent, container, screen, cellX, cellY, spanX, spanY, itemType, profileId) VALUES ($nextId, 'DXX-Redux', '$ICON_INTENT', -100, 0, 0, 3, 1, 1, 0, 0);`" > $TMPSQL" 2>$null
            & $ADB shell "sqlite3 '$launcherDb' < $TMPSQL" 2>$null
            & $ADB shell "rm -f $TMPSQL" 2>$null

            Write-Host "Home screen icon added"
            & $ADB shell am force-stop com.google.android.apps.nexuslauncher 2>$null
            Start-Sleep -Seconds 2
        } else {
            & $ADB shell "rm -f $TMPSQL" 2>$null
            Write-Host "Home screen icon already present"
        }
    } else {
        Write-Host "(Launcher database not found - skipping home screen icon)"
    }
    & $ADB unroot 2>$null
    Start-Sleep -Seconds 1
} else {
    Write-Host "(Root not available - skipping home screen icon. App is in the app drawer.)"
}

# -- 5. Push game data -------------------------------------------------------
if (-not $NoData) {
    $pushScript = Join-Path $HelpersDir "push_game_data.sh"
    $gameDataDir = Join-Path $RepoRoot "game_data_to_copy_to_emulator"
    if ((Test-Path $pushScript) -and (Test-Path $gameDataDir)) {
        $dataDir = Join-Path $gameDataDir "data"
        $downloadDir = Join-Path $gameDataDir "download"
        $hasData = (Test-Path $dataDir) -and @(Get-ChildItem $dataDir -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne ".gitkeep" }).Count -gt 0
        $hasDownload = (Test-Path $downloadDir) -and @(Get-ChildItem $downloadDir -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne ".gitkeep" }).Count -gt 0

        if ($hasData -or $hasDownload) {
            Write-Host ""
            $env:CALLED_FROM_SCRIPT = "1"
            & bash $pushScript
        } else {
            Write-Host ""
            Write-Host "(No game data files found - skipping push)"
        }
    }
}

# -- 6. Launch the app -------------------------------------------------------
Write-Host ""
Write-Host "=== Launching $PACKAGE ==="
& $ADB shell am force-stop $PACKAGE 2>$null
& $ADB shell am start -n "$PACKAGE/$ACTIVITY"

Write-Host ""
Write-Host "=== App launched. Tailing logcat (Ctrl+C to stop): ==="
Write-Host ""
& $ADB logcat -s "DXX-Redux:V" "DXX-Init:V" "DXX-Surface:V" "DXX-Input:V" "DXX-Msgbox:V" "AndroidRuntime:E" "DEBUG:V"

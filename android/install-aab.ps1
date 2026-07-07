#!/usr/bin/env pwsh
# install-aab.ps1 -- Convert the most recent .aab to a universal APK and install via adb
# Usage: .\install-aab.ps1 [-Aab <path>]
#   If -Aab is omitted, uses the newest .aab in build-outputs/ or app/build/outputs/bundle/

param(
    [string]$Aab
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot
if ($Aab -and -not [System.IO.Path]::IsPathRooted($Aab)) {
    $Aab = Join-Path $PSScriptRoot $Aab
}

# -- Resolve dependency base for JDK and bundletool --
$_depBaseFile = Join-Path $repoRoot "dependency_base.txt"
if (-not (Test-Path $_depBaseFile)) {
    Write-Error "dependency_base.txt not found. Create it with the path to your dependency directory"
    exit 1
}
$DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()

# -- JAVA_HOME --
if (-not $env:JAVA_HOME) {
    $jdk = Get-ChildItem "$DEP_BASE\jdk-*" -Directory | Sort-Object Name -Descending | Select-Object -First 1
    if ($jdk) {
        $env:JAVA_HOME = $jdk.FullName
    } else {
        Write-Error "No JDK found in $DEP_BASE\jdk-*. Set JAVA_HOME manually"
        exit 1
    }
}
$java = Join-Path $env:JAVA_HOME "bin\java.exe"
if (-not (Test-Path $java)) { Write-Error "java.exe not found at $java"; exit 1 }

# -- Locate bundletool --
# Pin to 1.17.2 -- update this line when upgrading
$bundletoolVersion = "1.17.2"
$bundletoolJar = Join-Path $DEP_BASE "bundletool-$bundletoolVersion.jar"
if (-not (Test-Path $bundletoolJar)) {
    $url = "https://github.com/google/bundletool/releases/download/$bundletoolVersion/bundletool-all-$bundletoolVersion.jar"
    Write-Host "Downloading bundletool $bundletoolVersion ..."
    Invoke-WebRequest -Uri $url -OutFile $bundletoolJar -UseBasicParsing
    Write-Host "Saved to $bundletoolJar"
}

# -- Find AAB --
if ($Aab) {
    if (-not (Test-Path $Aab)) { Write-Error "AAB not found: $Aab"; exit 1 }
    $aabFile = Get-Item $Aab
} else {
    # Try build-outputs/ first (timestamped copies), then gradle output
    $candidates = @()
    $boDir = Join-Path $PSScriptRoot "build-outputs"
    if (Test-Path $boDir) {
        $candidates += Get-ChildItem "$boDir\*.aab" -ErrorAction SilentlyContinue
    }
    $gradleRelease = Join-Path $PSScriptRoot "app\build\outputs\bundle\release"
    if (Test-Path $gradleRelease) {
        $candidates += Get-ChildItem "$gradleRelease\*.aab" -ErrorAction SilentlyContinue
    }
    $gradleDebug = Join-Path $PSScriptRoot "app\build\outputs\bundle\debug"
    if (Test-Path $gradleDebug) {
        $candidates += Get-ChildItem "$gradleDebug\*.aab" -ErrorAction SilentlyContinue
    }

    # Check if the newest AAB is stale compared to source files
    $needsBuild = $false
    if ($candidates.Count -eq 0) {
        $needsBuild = $true
    } else {
        $newest = $candidates | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        $aabTime = $newest.LastWriteTime
        $repoRoot = Split-Path $PSScriptRoot
        $srcDirs = @(
            (Join-Path $PSScriptRoot "app\src"),
            (Join-Path $repoRoot "d1"),
            (Join-Path $repoRoot "d2"),
            (Join-Path $repoRoot "cmake")
        )
        $buildFiles = @(
            (Join-Path $PSScriptRoot "build.gradle"),
            (Join-Path $PSScriptRoot "settings.gradle"),
            (Join-Path $PSScriptRoot "gradle.properties"),
            (Join-Path $PSScriptRoot "app\build.gradle")
        )
        $newerFiles = foreach ($dir in $srcDirs) {
            if (Test-Path $dir) {
                Get-ChildItem -Path $dir -Recurse -File -ErrorAction SilentlyContinue |
                    Where-Object { $_.LastWriteTime -gt $aabTime } |
                    Select-Object -First 1
            }
        }
        if (-not $newerFiles) {
            $newerFiles = foreach ($f in $buildFiles) {
                if ((Test-Path $f) -and (Get-Item $f).LastWriteTime -gt $aabTime) {
                    Get-Item $f
                }
            }
        }
        if ($newerFiles) {
            $needsBuild = $true
            Write-Host "AAB is stale (modified: $aabTime)"
        }
    }

    if ($needsBuild) {
        Write-Host "Building fresh AAB..."
        & (Join-Path $PSScriptRoot "1_build-aab.ps1") -BuildType "1"
        if ($LASTEXITCODE -ne 0) { throw "AAB build failed" }
        # Re-scan candidates after build
        $candidates = @()
        if (Test-Path $boDir) {
            $candidates += Get-ChildItem "$boDir\*.aab" -ErrorAction SilentlyContinue
        }
        if (Test-Path $gradleRelease) {
            $candidates += Get-ChildItem "$gradleRelease\*.aab" -ErrorAction SilentlyContinue
        }
        if (Test-Path $gradleDebug) {
            $candidates += Get-ChildItem "$gradleDebug\*.aab" -ErrorAction SilentlyContinue
        }
    }

    if ($candidates.Count -eq 0) {
        Write-Error "No .aab files found. Build first with .\1_build-aab.ps1"
        exit 1
    }
    $aabFile = $candidates | Sort-Object LastWriteTime -Descending | Select-Object -First 1
}
Write-Host "Using AAB: $($aabFile.FullName)"
Write-Host "  Size: $([math]::Round($aabFile.Length / 1MB, 1)) MB"
Write-Host "  Modified: $($aabFile.LastWriteTime)"

# -- Load keystore config --
$ksProps = Join-Path $PSScriptRoot "keystore.properties"
if (-not (Test-Path $ksProps)) {
    Write-Error "keystore.properties not found. Copy keystore.properties.example and fill in values"
    exit 1
}
$ksConfig = @{}
Get-Content $ksProps | ForEach-Object {
    $line = $_.Trim()
    if ($line -and -not $line.StartsWith('#') -and $line.Contains('=')) {
        $k, $v = $line -split '=', 2
        $ksConfig[$k.Trim()] = $v.Trim()
    }
}
$keystorePath = Join-Path $PSScriptRoot $ksConfig['storeFile']
if (-not (Test-Path $keystorePath)) {
    Write-Error "Keystore not found at $keystorePath"
    exit 1
}

# -- Convert AAB to universal APK --
$tempApks = Join-Path $PSScriptRoot "build-outputs\temp-universal.apks"
$apkOut = Join-Path $PSScriptRoot "build-outputs\app-universal.apk"

Write-Host ""
Write-Host "Converting AAB to universal APK..."
& $java -jar $bundletoolJar build-apks `
    --bundle="$($aabFile.FullName)" `
    --output="$tempApks" `
    --mode=universal `
    --overwrite `
    --ks="$keystorePath" `
    --ks-key-alias="$($ksConfig['keyAlias'])" `
    --ks-pass="pass:$($ksConfig['storePassword'])" `
    --key-pass="pass:$($ksConfig['keyPassword'])"
if ($LASTEXITCODE -ne 0) { throw "bundletool build-apks failed" }

# Extract universal.apk from the .apks zip
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($tempApks)
try {
    $entry = $zip.Entries | Where-Object { $_.Name -eq "universal.apk" } | Select-Object -First 1
    if (-not $entry) { throw "universal.apk not found inside $tempApks" }
    $stream = $entry.Open()
    $outStream = [System.IO.File]::Create($apkOut)
    try { $stream.CopyTo($outStream) } finally { $outStream.Close(); $stream.Close() }
} finally {
    $zip.Dispose()
}
Remove-Item $tempApks -Force -ErrorAction SilentlyContinue

$apkSize = [math]::Round((Get-Item $apkOut).Length / 1MB, 1)
Write-Host "APK: $apkOut ($apkSize MB)"

# -- Install via adb --
$adbExe = Join-Path $DEP_BASE "android-sdk\platform-tools\adb.exe"
if (-not (Test-Path $adbExe)) { Write-Error "adb not found at $adbExe"; exit 1 }
Write-Host ""
Write-Host "Installing APK via adb..."
& $adbExe install -r $apkOut
if ($LASTEXITCODE -ne 0) {
    Write-Host "Install failed -- uninstalling existing package and retrying..."
    & $adbExe uninstall com.dxxredux.app
    & $adbExe install $apkOut
    if ($LASTEXITCODE -ne 0) { throw "adb install failed" }
}

Write-Host ""
Write-Host "Done. APK installed on device"

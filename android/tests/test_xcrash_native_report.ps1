#!/usr/bin/env pwsh
# Verify that xCrash can execute its native dumper and write a useful tombstone

param(
    [switch]$NoBuild,
    [string]$Serial = "emulator-5554"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\..\helpers\test_env.ps1"
. "$PSScriptRoot\..\helpers\test_helpers.ps1"

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$androidDir = Join-Path $repoRoot "android"
$apk = Join-Path $androidDir "app\build\outputs\apk\debug\app-debug.apk"
$depBase = (Get-Content (Join-Path $repoRoot "dependency_base.txt") -First 1).Trim()
$adb = Resolve-RegressionAndroidSdkTool -DepBase $depBase -Subdir "platform-tools" -ToolName "adb" -EnvironmentVariable "ADB"
$sdkRoot = Split-Path (Split-Path $adb)
$aapt2 = Get-ChildItem (Join-Path $sdkRoot "build-tools") -Filter "aapt2.exe" -Recurse |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName
$package = "com.dxxredux.app"

function Fail {
    param([string]$Message)
    Write-Host "FAIL: $Message" -ForegroundColor Red
    exit 1
}

function Get-CrashFiles {
    $output = & $adb -s $Serial shell run-as $package ls files/tombstones 2>$null
    if ($LASTEXITCODE -ne 0) {
        return @()
    }
    return @($output | Where-Object { $_ })
}

if ($Serial -notlike "emulator-*") {
    Fail "Refusing to signal-crash a non-emulator device"
}
if (-not $NoBuild) {
    $env:JAVA_HOME = "C:\local\jdk-21"
    $env:Path = "$env:JAVA_HOME\bin;$env:Path"
    & (Join-Path $androidDir "gradlew.bat") -p $androidDir :app:assembleDebug
    if ($LASTEXITCODE -ne 0) {
        Fail "Android build failed"
    }
}
if (-not (Test-Path -LiteralPath $apk)) {
    Fail "APK not found at $apk"
}
if (-not $aapt2) {
    Fail "aapt2 not found under $sdkRoot"
}
if (-not (Test-DeviceOnline -Serial $Serial)) {
    Fail "$Serial is not online"
}

$manifest = (& $aapt2 dump xmltree $apk --file AndroidManifest.xml) -join "`n"
if ($manifest -notmatch "extractNativeLibs.*=true") {
    Fail "APK does not enable extracted native libraries"
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead($apk)
try {
    if (-not ($archive.Entries.FullName -contains "lib/x86_64/libxcrash_dumper.so")) {
        Fail "APK does not contain the x86_64 xCrash dumper"
    }
} finally {
    $archive.Dispose()
}

& $adb -s $Serial install -r $apk | Out-Null
if ($LASTEXITCODE -ne 0) {
    Fail "APK install failed"
}

$before = @(Get-CrashFiles)
& $adb -s $Serial shell am force-stop $package | Out-Null
& $adb -s $Serial shell monkey -p $package -c android.intent.category.LAUNCHER 1 | Out-Null
Start-Sleep -Seconds 3

$pidValue = (& $adb -s $Serial shell pidof $package).Trim()
if (-not $pidValue) {
    Fail "App process did not start"
}

$nativeDirLine = & $adb -s $Serial shell dumpsys package $package |
    Select-String "legacyNativeLibraryDir=" |
    Select-Object -First 1
$abiLine = & $adb -s $Serial shell dumpsys package $package |
    Select-String "primaryCpuAbi=" |
    Select-Object -First 1
$nativeDir = ($nativeDirLine -replace ".*legacyNativeLibraryDir=", "").Trim()
$abi = ($abiLine -replace ".*primaryCpuAbi=", "").Trim()
& $adb -s $Serial shell ls "$nativeDir/$abi/libxcrash_dumper.so" | Out-Null
if ($LASTEXITCODE -ne 0) {
    Fail "Installed xCrash dumper is missing"
}

& $adb -s $Serial shell run-as $package kill -11 $pidValue | Out-Null
Start-Sleep -Seconds 8

$after = @(Get-CrashFiles)
$newReport = $after |
    Where-Object { $_ -notin $before -and $_ -like "*.native.xcrash" } |
    Select-Object -First 1
if (-not $newReport) {
    Fail "Native xCrash report was not created"
}

$remotePath = "files/tombstones/$newReport"
$report = & $adb -s $Serial exec-out "run-as $package cat '$remotePath'" | Out-String
if ($report -match "exit status\(102\)") {
    Fail "xCrash dumper still failed with status 102"
}
if ($report -notmatch "signal 11 \(SIGSEGV\)") {
    Fail "Native report does not contain SIGSEGV details"
}
if ($report -notmatch "(?m)^backtrace:\r?$") {
    Fail "Native report does not contain a backtrace"
}

Write-Host "PASS: xCrash native report contains signal and backtrace" -ForegroundColor Green
Write-Host "Report: $newReport"

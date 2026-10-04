#!/usr/bin/env pwsh
# Inspect actual About text and log headers from an installed distribution
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$Apk = $(if ($env:DXX_TEST_APK) { $env:DXX_TEST_APK } else { Join-Path $PSScriptRoot '../app/build/outputs/apk/debug/app-debug.apk' })
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
if (-not $Serial) { throw 'Select a device with -Serial or ANDROID_SERIAL' }
$env:ANDROID_SERIAL = $Serial
$buildTools = ((Get-Content (Join-Path $PSScriptRoot '../get_deps/tool_versions.conf') | Where-Object { $_ -match '^BUILD_TOOLS_VERSION=' }) -split '=', 2)[1]
$aapt = Resolve-RegressionAndroidSdkTool -DepBase $script:DEP_BASE -Subdir "build-tools/$buildTools" -ToolName aapt2
$badging = (& $aapt dump badging $Apk) -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect APK' }
if ($badging -notmatch "package: name='([^']+)' versionCode='([^']+)' versionName='([^']+)'") { throw 'Missing APK identity' }
$script:PACKAGE = $Matches[1]
$versionCode = $Matches[2]
$versionName = $Matches[3]
$distribution = switch ($script:PACKAGE) {
    'com.dxxredux.app.github.legacy' { 'Sideload (GitHub, legacy)' }
    'com.dxxredux.app.github' { 'Sideload (GitHub)' }
    'com.dxxredux.app' { 'Play Store' }
    'com.dxxredux.app.nsdtest' { 'Play Store' }
    default { throw 'Unexpected application ID' }
}
if ($badging -notmatch "(?m)^(?:sdkVersion|minSdkVersion):'([0-9]+)'") { throw 'Missing minimum SDK' }
$minimum = $Matches[1]
if ($badging -notmatch "(?m)^targetSdkVersion:'([0-9]+)'") { throw 'Missing target SDK' }
$target = $Matches[1]
& $script:ADB -s $Serial install -r $Apk | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'APK install failed' }
$deviceApi = [int](Adb -AdbArgs @('shell', 'getprop', 'ro.build.version.sdk'))
$packageInfo = Adb -AdbArgs @('shell', 'dumpsys', 'package', $script:PACKAGE)
$needsSafeMode = $script:PACKAGE -eq 'com.dxxredux.app.github.legacy' -and $deviceApi -lt 24
if (($packageInfo -match 'VM_SAFE_MODE') -ne $needsSafeMode) { throw 'Incorrect legacy VM safeguard for this Android version' }
Stop-AppAndWait
if (-not (Start-PrimarySetupActivity)) { throw 'Launcher did not become ready' }
Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND', '--es', 'command', 'write_bool_pref', '--es', 'key', 'dlog_launcher_enabled', '--ez', 'value', 'true') | Out-Null
Stop-AppAndWait
if (-not (Start-PrimarySetupActivity)) { throw 'Launcher did not restart' }
$setup = Get-SetupIntrospection
if (-not $setup) { throw 'Launcher introspection failed' }
$about = $setup.about_build_info
if (-not $about.Contains("Distribution: $distribution") -or
    -not $about.Contains("Version: $versionName ($versionCode)") -or
    -not $about.Contains("minsdk: api $minimum, targetsdk: api $target") -or
    $about -notmatch 'Date: \d{4}-\d{2}-\d{2}') { throw "Incorrect About build information: $about" }
$files = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'ls', 'files/debuglogs')
$matched = $false
foreach ($file in ($files -split "`n")) {
    if (-not $file.Trim()) { continue }
    $log = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', "files/debuglogs/$($file.Trim())")
    if ($log.Contains("distribution=$distribution") -and
        $log.Contains("app=$versionName ($versionCode)") -and
        $log.Contains("minsdk=$minimum targetsdk=$target") -and
        $log -match 'built=\d{4}-\d{2}-\d{2}') { $matched = $true; break }
}
if (-not $matched) { throw 'No log header matches APK distribution, version, SDK and build date' }
Write-Host "PASS About/log metadata: $distribution, minimum API $minimum, target API $target"

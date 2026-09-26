# Build/install the current debug APK first, then test its codecs with Android key-name lookup
param([string]$Serial = $env:ANDROID_SERIAL)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_env.ps1')
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$depBase = (Get-Content (Join-Path $repoRoot 'dependency_base.txt') -First 1).Trim()
$adb = Resolve-RegressionAndroidSdkTool -DepBase $depBase -Subdir 'platform-tools' -ToolName 'adb'
$sdk = Split-Path (Split-Path $adb)
$versions = @{}
Get-Content (Join-Path $repoRoot 'android/get_deps/tool_versions.conf') | ForEach-Object {
    if ($_ -match '^(COMPILE_SDK|MIN_SDK|BUILD_TOOLS_VERSION)=(.+)$') { $versions[$Matches[1]] = $Matches[2] }
}
$platform = Get-ChildItem (Join-Path $sdk 'platforms') -Directory -Filter "android-$($versions.COMPILE_SDK)*" |
    Sort-Object Name -Descending | Select-Object -First 1
$androidJar = Join-Path $platform.FullName 'android.jar'
$d8Name = if (Test-RegressionWindowsHost) { 'd8.bat' } else { 'd8' }
$d8 = Join-Path $sdk "build-tools/$($versions.BUILD_TOOLS_VERSION)/$d8Name"
$classesJar = Join-Path $repoRoot 'android/app/build/intermediates/runtime_app_classes_jar/debug/bundleDebugClassesToRuntimeJar/classes.jar'
$output = Join-Path $repoRoot 'temp/touch-layout-format'
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output
New-Item -ItemType Directory -Path $output -Force | Out-Null
if (-not (Test-Path -LiteralPath $classesJar)) { throw 'Build the debug APK before running this probe' }

$classpath = $classesJar + [IO.Path]::PathSeparator + $androidJar
& javac --release 17 -cp $classpath -d $output (Join-Path $PSScriptRoot 'TouchLayoutFormatProbe.java')
if ($LASTEXITCODE -ne 0) { throw 'Touch layout probe compilation failed' }
$probe = Join-Path $output 'probe.jar'
& $d8 --min-api $versions.MIN_SDK --lib $androidJar --classpath $classesJar --output $probe (Join-Path $output 'com/dxxredux/app/TouchLayoutFormatProbe.class')
if ($LASTEXITCODE -ne 0) { throw 'Touch layout probe dex compilation failed' }

$device = if ($Serial) { @('-s', $Serial) } else { @() }
$apk = (& $adb @device shell pm path com.dxxredux.app | Where-Object { $_ -match '/base.apk$' } | Select-Object -First 1)
if (-not $apk) { throw 'Install the current debug APK before running this probe' }
$apk = $apk.Trim().Substring('package:'.Length)
$remote = '/data/local/tmp/dxx-touch-layout-format.jar'
& $adb @device push $probe $remote
if ($LASTEXITCODE -ne 0) { throw 'Could not push touch layout probe' }
try {
    & $adb @device shell "CLASSPATH=${remote}:$apk app_process / com.dxxredux.app.TouchLayoutFormatProbe $apk"
    if ($LASTEXITCODE -ne 0) { throw 'Touch layout format probe failed' }
} finally {
    & $adb @device shell rm -f $remote | Out-Null
}

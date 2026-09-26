param([string]$Serial = $env:ANDROID_SERIAL)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_env.ps1')
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$depBase = (Get-Content (Join-Path $repoRoot 'dependency_base.txt') -First 1).Trim()
$adb = Resolve-RegressionAndroidSdkTool -DepBase $depBase -Subdir 'platform-tools' -ToolName 'adb'
$sdk = Split-Path (Split-Path $adb)
$versions = @{}
Get-Content (Join-Path $repoRoot 'android/get_deps/tool_versions.conf') | ForEach-Object {
    if ($_ -match '^(NDK_VERSION|CMAKE_VERSION|MIN_SDK)=(.+)$') { $versions[$Matches[1]] = $Matches[2] }
}
$device = if ($Serial) { @('-s', $Serial) } else { @() }
$abi = (& $adb @device shell getprop ro.product.cpu.abi).Trim()
if ($LASTEXITCODE -ne 0 -or $abi -notin @('arm64-v8a', 'armeabi-v7a', 'x86_64')) { throw 'Select a supported online Android device' }
$cmakeDir = Join-Path $sdk "cmake/$($versions.CMAKE_VERSION)/bin"
$suffix = if (Test-RegressionWindowsHost) { '.exe' } else { '' }
$cmake = Join-Path $cmakeDir "cmake$suffix"
$ndk = Join-Path $depBase "android-ndk-$($versions.NDK_VERSION)"
$output = Join-Path $repoRoot "temp/pcm-ring-$abi"
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output
& $cmake -S (Join-Path $PSScriptRoot 'pcm_ring') -B $output -G Ninja "-DCMAKE_MAKE_PROGRAM=$cmakeDir/ninja$suffix" "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" "-DANDROID_ABI=$abi" "-DANDROID_PLATFORM=android-$($versions.MIN_SDK)" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'PCM ring test configuration failed' }
& $cmake --build $output
if ($LASTEXITCODE -ne 0) { throw 'PCM ring test build failed' }
$remote = '/data/local/tmp/dxx-test-pcm-ring'
& $adb @device push (Join-Path $output 'test_pcm_ring') $remote
if ($LASTEXITCODE -ne 0) { throw 'PCM ring test push failed' }
try {
    & $adb @device shell "chmod 700 $remote && $remote"
    if ($LASTEXITCODE -ne 0) { throw 'PCM ring test failed' }
} finally {
    & $adb @device shell rm -f $remote | Out-Null
}

#!/usr/bin/env pwsh
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Hog,
    [string]$Song = 'game07.hmp',
    [double]$Seconds = 20,
    [string]$Work = 'temp/fm-feasibility',
    [string]$AdlibWav,
    [string]$AndroidNdk,
    [string]$Adb,
    [string]$AdbSerial
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../../helpers/test_env.ps1"
$repo = (Resolve-Path "$PSScriptRoot/../../..").Path
Push-Location $repo
try {
    & "$PSScriptRoot/../../helpers/retain-recent-artifacts.ps1" -Artifacts $Work
    python "$PSScriptRoot/experiment.py" --work $Work --fetch-only
    if ($LASTEXITCODE -ne 0) { throw 'Dependency preparation failed' }
    $workPath = (Resolve-Path -LiteralPath $Work).Path
    $deps = Join-Path $workPath 'deps'
    $hostBuild = Join-Path $workPath 'host-vs'
    cmake -S $PSScriptRoot -B $hostBuild -G 'Visual Studio 17 2022' -A x64 "-DFM_DEPS=$deps"
    if ($LASTEXITCODE -ne 0) { throw 'Host configure failed' }
    cmake --build $hostBuild --config Release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Host build failed' }
    $arguments = @('--work', $workPath, '--hog', $Hog, '--song', $Song, '--seconds', $Seconds)
    if ($AdlibWav) { $arguments += @('--adlib-wav', $AdlibWav) }
    python "$PSScriptRoot/experiment.py" @arguments --probe "$hostBuild/Release/fm_probe.exe" --probe-fixed "$hostBuild/Release/fm_probe_live_fix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Host experiment failed' }
    if ($Song -eq 'game07.hmp' -and $Seconds -eq 20) {
        python "$PSScriptRoot/verify.py" "$workPath/game07-hmp/report.json"
        if ($LASTEXITCODE -ne 0) { throw 'Host verification failed' }
    }
    if ($AdbSerial -and -not $AndroidNdk) { throw 'AdbSerial requires AndroidNdk' }
    if ($AndroidNdk) {
        foreach ($abi in @('arm64-v8a', 'x86_64')) {
            $build = Join-Path $workPath "android-$abi"
            cmake -S $PSScriptRoot -B $build -G Ninja "-DFM_DEPS=$deps" "-DCMAKE_TOOLCHAIN_FILE=$AndroidNdk/build/cmake/android.toolchain.cmake" "-DANDROID_ABI=$abi" -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
            if ($LASTEXITCODE -ne 0) { throw "Android $abi configure failed" }
            cmake --build $build --parallel 4
            if ($LASTEXITCODE -ne 0) { throw "Android $abi build failed" }
        }
        if ($AdbSerial) {
            if (-not $Adb) {
                $adbCommand = Get-Command adb -ErrorAction SilentlyContinue
                if ($adbCommand) { $Adb = $adbCommand.Source }
                else { $Adb = Join-Path $script:_ENV_DEP_BASE 'android-sdk/platform-tools/adb.exe' }
            }
            $abi = (& $Adb -s $AdbSerial shell getprop ro.product.cpu.abi).Trim()
            if ($LASTEXITCODE -ne 0 -or $abi -notin @('arm64-v8a', 'x86_64')) { throw 'Unsupported/missing Android device ABI' }
            $build = Join-Path $workPath "android-$abi"
            python "$PSScriptRoot/experiment.py" @arguments --probe "$build/fm_probe" --probe-fixed "$build/fm_probe_live_fix" --adb $Adb --serial $AdbSerial
            if ($LASTEXITCODE -ne 0) { throw 'Android experiment failed' }
            if ($Song -eq 'game07.hmp' -and $Seconds -eq 20) {
                python "$PSScriptRoot/verify.py" "$workPath/game07-hmp-android/report.json"
                if ($LASTEXITCODE -ne 0) { throw 'Android verification failed' }
            }
        }
    }
} finally {
    Pop-Location
}

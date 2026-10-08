#!/usr/bin/env pwsh
# Run a maintained fixture linked to the actual Android engine libraries
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('config_policy', 'matcen_stations', 'texture_labels', 'automap_route_policy', 'music_completion', 'redbook_completion', 'cd_preview_completion', 'secret_origins')][string]$Fixture,
    [Parameter(Mandatory)][int]$ExpectedCases,
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$NativeLibraryDir,
    [ValidateSet('labels', 'batches', 'batches-baseline', 'merged-wrap', 'merged-wrap-baseline', 'merge-cache', 'bindings', 'bindings-baseline', 'mipmaps', 'mipmaps-baseline')][string]$Mode = 'labels',
    [string]$OutputDirectory,
    [string[]]$DataFiles = @(),
    [switch]$NoEngineBuild
)
$ErrorActionPreference = 'Stop'
$assetFiles = @($DataFiles | ForEach-Object { Get-Item -LiteralPath $_ -ErrorAction Stop })
$assetNames = @{}
foreach ($asset in $assetFiles) {
    if ($asset.PSIsContainer -or $asset.Name -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$' -or $assetNames.ContainsKey($asset.Name)) {
        throw 'Native fixture assets require unique plain filenames'
    }
    $assetNames[$asset.Name] = $true
}
. (Join-Path $PSScriptRoot 'test_env.ps1')
. (Join-Path $PSScriptRoot 'headless_process_pool.ps1')
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$depBase = (Get-Content (Join-Path $repoRoot 'dependency_base.txt') -First 1).Trim()
$adb = Resolve-RegressionAndroidSdkTool -DepBase $depBase -Subdir 'platform-tools' -ToolName 'adb'
$artifactName = $Fixture.Replace('_', '-')
$target = 'test_android_' + $Fixture
$device = if ($Serial) { @('-s', $Serial) } else { @() }
$abi = (& $adb @device shell getprop ro.product.cpu.abi).Trim()
if ($LASTEXITCODE -ne 0 -or $abi -notin @('arm64-v8a', 'armeabi-v7a', 'x86_64')) { throw 'Select a supported online Android device' }
if (-not $NativeBuildDir) {
    $cache = Get-ChildItem (Join-Path $repoRoot "android/app/.cxx/Debug/*/$abi/CMakeCache.txt") |
        Sort-Object LastWriteTime -Descending | Where-Object {
            $outputSetting = Select-String -LiteralPath $_.FullName -Pattern '^CMAKE_LIBRARY_OUTPUT_DIRECTORY:[^=]+='
            $outputPath = if ($outputSetting) { ($outputSetting.Line -split '=', 2)[1] }
            $outputPath -and (Test-Path (Join-Path $outputPath 'libdxx-redux-d1.so')) -and
            (Test-Path (Join-Path $outputPath 'libdxx-redux-d2.so'))
        } | Select-Object -First 1
    if (-not $cache) { throw 'Build Android Debug native libraries first, or provide NativeBuildDir' }
    $NativeBuildDir = $cache.DirectoryName
}
$NativeBuildDir = (Resolve-Path $NativeBuildDir).Path
$settings = @{}
Get-Content (Join-Path $NativeBuildDir 'CMakeCache.txt') | ForEach-Object {
    if ($_ -match '^([^/#][^:]*):[^=]+=(.*)$') { $settings[$Matches[1]] = $Matches[2] }
}
if ($settings.CMAKE_ANDROID_ARCH_ABI -ne $abi -and $settings.ANDROID_ABI -ne $abi) { throw 'Native build ABI does not match the selected device' }
$cmake = $settings.CMAKE_COMMAND
$libraries = $settings.CMAKE_LIBRARY_OUTPUT_DIRECTORY
if ($NativeLibraryDir) { $libraries = (Resolve-Path $NativeLibraryDir).Path }
if ($Fixture -ne 'texture_labels' -and $Mode -ne 'labels') { throw 'Rendering modes require the texture_labels fixture' }
$modeArgument = if ($Fixture -eq 'texture_labels') { " $Mode" } else { '' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot (('android/temp/' + $artifactName + '/run_') + [guid]::NewGuid().ToString('N')) }
if (-not [IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory = Join-Path $repoRoot $OutputDirectory }
$retention = @{ Artifacts = $OutputDirectory }
if ((Split-Path -Leaf $OutputDirectory).StartsWith('run_')) { $retention.DirectoryPrefix = 'run_' }
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') @retention
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
if (-not $NoEngineBuild) {
    & $cmake --build $NativeBuildDir --target dxx-redux-d1 dxx-redux-d2 --parallel 2
    if ($LASTEXITCODE -ne 0) { throw 'Android engine build failed' }
}
$remote = ('/data/local/tmp/dxx-' + $artifactName + '-') + [guid]::NewGuid().ToString('N')
try {
    & $adb @device shell mkdir -p "$remote/d1" "$remote/d2"
    if ($LASTEXITCODE -ne 0) { throw 'Create isolated native test directories failed' }
    foreach ($library in Get-ChildItem -LiteralPath $libraries -Filter '*.so' -File) {
        & $adb @device push $library.FullName "$remote/"
        if ($LASTEXITCODE -ne 0) { throw "Push failed: $($library.Name)" }
    }
    foreach ($game in @('d1', 'd2')) {
        foreach ($asset in $assetFiles) {
            & $adb @device push $asset.FullName "$remote/$game/$($asset.Name)"
            if ($LASTEXITCODE -ne 0) { throw "Push fixture asset failed: $($asset.Name)" }
        }
        $build = Join-Path $OutputDirectory "$game-build"
        & $cmake -S (Join-Path $repoRoot ('android/tests/' + $Fixture)) -B $build -G Ninja `
            "-DCMAKE_MAKE_PROGRAM=$($settings.CMAKE_MAKE_PROGRAM)" `
            "-DCMAKE_TOOLCHAIN_FILE=$($settings.CMAKE_TOOLCHAIN_FILE)" `
            "-DANDROID_ABI=$abi" "-DANDROID_PLATFORM=$($settings.ANDROID_PLATFORM)" `
            -DANDROID_STL=c++_shared "-DGAME=$game" "-DENGINE_BUILD_DIR=$NativeBuildDir" "-DENGINE_LIBRARY_DIR=$libraries"
        if ($LASTEXITCODE -ne 0) { throw "$game fixture configuration failed" }
        & $cmake --build $build
        if ($LASTEXITCODE -ne 0) { throw "$game fixture build failed" }
        & $adb @device push (Join-Path $build $target) "$remote/$game/"
        if ($LASTEXITCODE -ne 0) { throw "$game fixture push failed" }
        $task = [pscustomobject]@{
            FilePath = $adb
            Arguments = @($device) + @('shell', "cd $remote/$game && chmod 700 $target && LD_LIBRARY_PATH=.. ./$target trace.json$modeArgument")
            WorkingDirectory = $repoRoot
            TimeoutSeconds = 120
        }
        $execution = @{ Result = $null }
        Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
            param($task, $result)
            $execution.Result = $result
        }
        $result = $execution.Result
        if ($result) {
            [IO.File]::WriteAllText((Join-Path $OutputDirectory "$game.log"), $result.StandardOutput + $result.StandardError, [Text.UTF8Encoding]::new($false))
        }
        if (-not $result -or $result.TimedOut -or $result.ExitCode -ne 0) { throw "$game $Fixture fixture failed; see $OutputDirectory" }
        & $adb @device pull "$remote/$game/trace.json" (Join-Path $OutputDirectory "$game.json")
        if ($LASTEXITCODE -ne 0) { throw "$game trace pull failed" }
        $cases = @(Get-Content (Join-Path $OutputDirectory "$game.json") -Raw | ConvertFrom-Json)
        if ($cases.Count -ne $ExpectedCases) { throw "$game $Fixture trace case count is not $ExpectedCases" }
        Write-Host "$game $Fixture passed: $ExpectedCases actual engine cases"
    }
} finally {
    if ($remote -notmatch ('^/data/local/tmp/dxx-' + [regex]::Escape($artifactName) + '-[a-f0-9]{32}$')) { throw 'Invalid native test cleanup path' }
    & $adb @device shell rm -r $remote | Out-Null
}
Write-Host "Android $Fixture passed for both games; traces: $OutputDirectory"

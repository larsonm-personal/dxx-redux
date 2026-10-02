#!/usr/bin/env pwsh
# Build and inspect the three complete APKs without contacting release services
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_env.ps1')
$androidDir = Split-Path $PSScriptRoot
$output = Join-Path $androidDir 'temp/distribution-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$versions = @{}
foreach ($line in Get-Content (Join-Path $androidDir 'get_deps/tool_versions.conf')) {
    if ($line -match '^(BUILD_TOOLS_VERSION)=(.+)$') { $versions[$Matches[1]] = $Matches[2] }
}
foreach ($line in Get-Content (Join-Path $androidDir 'distribution_versions.conf')) {
    if ($line -match '^((?:CURRENT|LEGACY)_(?:MIN|TARGET)_SDK)=([0-9]+)$') { $versions[$Matches[1]] = $Matches[2] }
}
$aapt = Resolve-RegressionAndroidSdkTool -DepBase $script:_ENV_DEP_BASE -Subdir "build-tools/$($versions.BUILD_TOOLS_VERSION)" -ToolName aapt2
$gradle = Resolve-RegressionGradleWrapper -AndroidDir $androidDir
Add-Type -AssemblyName System.IO.Compression.FileSystem
$distributions = @(
    @{ Name = 'play'; Id = 'com.dxxredux.app'; Minimum = $versions.CURRENT_MIN_SDK; Target = $versions.CURRENT_TARGET_SDK; Properties = @('-PgithubRelease=false', '-PlegacyRelease=false') },
    @{ Name = 'github'; Id = 'com.dxxredux.app.github'; Minimum = $versions.CURRENT_MIN_SDK; Target = $versions.CURRENT_TARGET_SDK; Properties = @('-PgithubRelease=true', '-PlegacyRelease=false') },
    @{ Name = 'legacy'; Id = 'com.dxxredux.app.github.legacy'; Minimum = $versions.LEGACY_MIN_SDK; Target = $versions.LEGACY_TARGET_SDK; Properties = @('-PgithubRelease=true', '-PlegacyRelease=true') }
)
foreach ($distribution in $distributions) {
    $apk = Join-Path $output "$($distribution.Name).apk"
    if (-not $NoBuild) {
        & $gradle -p $androidDir :app:assembleDebug -PskipBuildInfo --console=plain @($distribution.Properties) *> (Join-Path $output "$($distribution.Name)-build.log")
        if ($LASTEXITCODE -ne 0) { throw "$($distribution.Name) build failed; inspect $output" }
        Copy-Item -LiteralPath (Join-Path $androidDir 'app/build/outputs/apk/debug/app-debug.apk') -Destination $apk -Force
    }
    $badging = (& $aapt dump badging $apk) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $apk" }
    if ($badging -notmatch "package: name='$([regex]::Escape($distribution.Id))'" -or
        $badging -notmatch "(?m)^(?:sdkVersion|minSdkVersion):'$($distribution.Minimum)'\s*$" -or
        $badging -notmatch "(?m)^targetSdkVersion:'$($distribution.Target)'\s*$") { throw "Incorrect package/SDK in $apk" }
    $archive = [IO.Compression.ZipFile]::OpenRead($apk)
    try {
        foreach ($abi in @('armeabi-v7a', 'arm64-v8a', 'x86_64')) {
            foreach ($library in @('libdxx-redux-d1.so', 'libdxx-redux-d2.so')) {
                if (-not $archive.GetEntry("lib/$abi/$library")) { throw "$apk lacks $abi/$library" }
            }
        }
        $hasPlaySdk = $false
        $hasJavaBackports = $false
        foreach ($entry in $archive.Entries | Where-Object FullName -Match '^classes[0-9]*\.dex$') {
            $stream = $entry.Open()
            $buffer = [IO.MemoryStream]::new()
            try {
                $stream.CopyTo($buffer)
                $dexText = [Text.Encoding]::ASCII.GetString($buffer.ToArray())
                if ($dexText.Contains('Lcom/google/android/gms/games/')) { $hasPlaySdk = $true }
                if ($dexText.Contains('Lj$/nio/file/')) { $hasJavaBackports = $true }
                if ($distribution.Name -ne 'play' -and ($dexText.Contains('Lcom/google/android/gms/') -or $dexText.Contains('Lcom/google/android/play/'))) {
                    throw "$apk still packages a Google Play runtime SDK"
                }
            } finally {
                $stream.Dispose()
                $buffer.Dispose()
            }
        }
        if (($distribution.Name -eq 'play') -ne $hasPlaySdk) { throw "Incorrect Play Games SDK packaging in $apk" }
        if (-not $hasJavaBackports) { throw "$apk lacks NIO Java API backports" }
    } finally { $archive.Dispose() }
    Write-Host "PASS $($distribution.Name): minimum API $($distribution.Minimum), target API $($distribution.Target), both engines/all ABIs"
}
exit 0

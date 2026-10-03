#!/usr/bin/env pwsh
# Inspect a Release/Internal combined build's actual AAB and APK without installing either
param(
    [Parameter(Mandatory)][string]$Aab,
    [Parameter(Mandatory)][string]$Apk
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_env.ps1')
$androidDir = Split-Path $PSScriptRoot
$toolVersions = @{}
foreach ($line in Get-Content (Join-Path $androidDir 'get_deps/tool_versions.conf')) {
    if ($line -match '^BUILD_TOOLS_VERSION=(.+)$') { $toolVersions.BuildTools = $Matches[1] }
}
$aapt = Resolve-RegressionAndroidSdkTool -DepBase $script:_ENV_DEP_BASE -Subdir "build-tools/$($toolVersions.BuildTools)" -ToolName aapt2
$signerName = if (Test-RegressionWindowsHost) { 'apksigner.bat' } else { 'apksigner' }
$signer = Resolve-RegressionAndroidSdkTool -DepBase $script:_ENV_DEP_BASE -Subdir "build-tools/$($toolVersions.BuildTools)" -ToolName $signerName
# Same bundletool pin as install-aab.ps1
$bundletool = Join-Path $script:_ENV_DEP_BASE 'bundletool-1.17.2.jar'
if (-not (Test-Path -LiteralPath $bundletool)) { throw "Missing $bundletool" }
$java = (Get-Command java -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$manifestText = (& $java -jar $bundletool dump manifest "--bundle=$Aab" --module=base) -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'Cannot read AAB manifest' }
$manifest = [xml]$manifestText
if ($manifest.manifest.package -ne 'com.dxxredux.app') { throw 'AAB must retain the Google Play application ID' }
$androidNamespace = 'http://schemas.android.com/apk/res/android'
$versionCode = $manifest.manifest.GetAttribute('versionCode', $androidNamespace)
$versionName = $manifest.manifest.GetAttribute('versionName', $androidNamespace)
$badging = (& $aapt dump badging $Apk) -join "`n"
if ($LASTEXITCODE -ne 0 -or $badging -notmatch "package: name='com\.dxxredux\.app\.github' versionCode='$versionCode' versionName='$([regex]::Escape($versionName))'") {
    throw 'APK must use the separate GitHub application ID and match the AAB version'
}
& $signer verify $Apk
if ($LASTEXITCODE -ne 0) { throw 'APK signature verification failed' }

Add-Type -AssemblyName System.IO.Compression.FileSystem
$bundle = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $Aab).Path)
$package = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $Apk).Path)
try {
    foreach ($abi in @('armeabi-v7a', 'arm64-v8a', 'x86_64')) {
        foreach ($library in @('libdxx-redux-d1.so', 'libdxx-redux-d2.so')) {
            $digests = foreach ($entry in @($bundle.GetEntry("base/lib/$abi/$library"), $package.GetEntry("lib/$abi/$library"))) {
                if (-not $entry) { throw "Missing $abi/$library" }
                $stream = $entry.Open()
                $hash = [Security.Cryptography.SHA256]::Create()
                try { [Convert]::ToBase64String($hash.ComputeHash($stream)) } finally { $stream.Dispose(); $hash.Dispose() }
            }
            if ($digests[0] -ne $digests[1]) { throw "Engine differs between AAB/APK: $abi/$library" }
        }
    }
    foreach ($entry in $package.Entries | Where-Object FullName -Match '^classes[0-9]*\.dex$') {
        $stream = $entry.Open()
        $buffer = [IO.MemoryStream]::new()
        try {
            $stream.CopyTo($buffer)
            $dex = [Text.Encoding]::ASCII.GetString($buffer.ToArray())
            if ($dex.Contains('Lcom/google/android/gms/') -or $dex.Contains('Lcom/google/android/play/')) {
                throw 'Direct-install APK must exclude Google Play runtime SDKs'
            }
        } finally { $stream.Dispose(); $buffer.Dispose() }
    }
} finally { $bundle.Dispose(); $package.Dispose() }
Write-Host "PASS: separate Play/APK identities, shared version $versionName ($versionCode), valid APK signature, identical engines/all ABIs, no Play runtime SDKs in APK"

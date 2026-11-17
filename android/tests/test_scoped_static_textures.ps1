#!/usr/bin/env pwsh
# Synthetic production candidate/decode/upload/readback and mission context isolation
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$NativeLibraryDir,
    [string]$OutputDirectory,
    [string]$OriginalTexture,
    [string]$OriginalAnimation,
    [string]$OriginalRleTexture,
    [string]$OriginalRuntime,
    [switch]$NoEngineBuild
)
$arguments = @{} + $PSBoundParameters
$arguments.Remove('OriginalTexture') | Out-Null
$arguments.Remove('OriginalAnimation') | Out-Null
$arguments.Remove('OriginalRleTexture') | Out-Null
$arguments.Remove('OriginalRuntime') | Out-Null
if ($OriginalTexture -or $OriginalAnimation) {
    . (Join-Path $PSScriptRoot '../helpers/test_host_platform.ps1')
    $python = Resolve-RegressionPythonCommand
    if (-not $python) { throw 'Python 3 is required to verify original texture GPU evidence' }
    $pythonPrefix = @($python.PrefixArguments)
}
if ($OriginalTexture) {
    if ((Get-FileHash -LiteralPath $OriginalTexture -Algorithm SHA256).Hash.ToLowerInvariant() -ne '150c3603749fc87d877c9a0c97218431605b845098a89338a632252402635c3b') { throw 'Original BoilingPoint texture hash differs' }
    if ((Split-Path -Leaf $OriginalTexture) -cne 'ceil002.tga') { throw 'Original texture must retain its authored filename' }
    $arguments.DataFiles = @($OriginalTexture)
}
if ($OriginalAnimation) {
    if (-not $OriginalRuntime) { throw 'Original animation requires the pinned retail resource banks' }
    if ((Get-FileHash -LiteralPath $OriginalAnimation -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'ce0ed3f3874c1e01fb27c46a2eb2cfba0034559fc3464052e40fa2fa4ac73dac') { throw 'Original BoilingPoint animation hash differs' }
    if ((Split-Path -Leaf $OriginalAnimation) -cne 'force01#0.tga') { throw 'Original animation must retain its authored filename' }
    $arguments.DataFiles = @($arguments.DataFiles) + @($OriginalAnimation) | Where-Object { $_ }
    . (Join-Path $PSScriptRoot '../helpers/standard_game_data.ps1')
    foreach ($bank in Get-StandardGameDataDeps | Where-Object file -In @('groupa.pig', 'descent2.s22')) {
        $source = Join-Path $OriginalRuntime $bank.file
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -ne $bank.sha256) { throw "Original native bank differs: $($bank.file)" }
        $arguments.DataFiles += $source
    }
} elseif ($OriginalRuntime) {
    throw 'OriginalRuntime requires OriginalAnimation'
}
if ($OriginalRleTexture) {
    if ((Get-FileHash -LiteralPath $OriginalRleTexture -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'ffbc219965339e0f079838b1919836adc2e1037ef67ab679d717d97adf01f064') { throw 'Original RLE texture hash differs' }
    if ((Split-Path -Leaf $OriginalRleTexture) -cne 'rock194.tga') { throw 'Original RLE texture must retain its authored filename' }
    $arguments.DataFiles = @($arguments.DataFiles) + @($OriginalRleTexture) | Where-Object { $_ }
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $PSScriptRoot ('../temp/scoped-textures/run_' + [guid]::NewGuid().ToString('N'))
    $arguments.OutputDirectory = $OutputDirectory
}
try {
    & (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture scoped_textures -ExpectedCases 8 @arguments
    if ($OriginalRleTexture) {
        $rows = @(Get-Content -LiteralPath (Join-Path $OutputDirectory 'd2.json') -Raw | ConvertFrom-Json)
        if ($rows.Count -ne 8 -or @($rows | Where-Object { -not $_.rle_image.correct -or $_.rle_image.sampled_pixels -ne 4096 -or $_.rle_image.rgba_sha256 -ne '370bf54a6cda458661dd8439c1ff749d38931b6dd005dcd0d69e6512b1a7b3e5' }).Count) { throw 'Original RLE GPU evidence is incomplete' }
    }
    if ($OriginalTexture) {
        & $python.Path @pythonPrefix (Join-Path $PSScriptRoot "xl_private_assets.py") --gpu-trace (Join-Path $OutputDirectory "d2.json")
        if ($LASTEXITCODE -ne 0) { throw "Required original texture GPU evidence is incomplete" }
    }
    if ($OriginalAnimation) {
        & $python.Path @pythonPrefix (Join-Path $PSScriptRoot "xl_private_assets.py") --animation-trace (Join-Path $OutputDirectory "d2.json")
        if ($LASTEXITCODE -ne 0) { throw "Required original animation GPU evidence is incomplete" }
    }
} catch {
    if (-not $OriginalTexture) {
        foreach ($game in @('d1', 'd2')) {
            $log = Join-Path $OutputDirectory "$game.log"
            if (Test-Path -LiteralPath $log -PathType Leaf) {
                Get-Content -LiteralPath $log -Tail 20 | ForEach-Object {
                    Write-Host ($_.Substring(0, [Math]::Min($_.Length, 512)))
                }
            }
        }
    }
    throw
}

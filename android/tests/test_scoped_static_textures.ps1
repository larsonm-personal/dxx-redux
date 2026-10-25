#!/usr/bin/env pwsh
# Synthetic production candidate/decode/upload/readback and mission context isolation
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$NativeLibraryDir,
    [string]$OutputDirectory,
    [string]$OriginalTexture,
    [switch]$NoEngineBuild
)
$arguments = @{} + $PSBoundParameters
$arguments.Remove('OriginalTexture') | Out-Null
if ($OriginalTexture) {
    if ((Get-FileHash -LiteralPath $OriginalTexture -Algorithm SHA256).Hash.ToLowerInvariant() -ne '150c3603749fc87d877c9a0c97218431605b845098a89338a632252402635c3b') { throw 'Original BoilingPoint texture hash differs' }
    if ((Split-Path -Leaf $OriginalTexture) -cne 'ceil002.tga') { throw 'Original texture must retain its authored filename' }
    $arguments.DataFiles = @($OriginalTexture)
}
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture scoped_textures -ExpectedCases 8 @arguments

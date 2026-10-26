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
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $PSScriptRoot ('../temp/scoped-textures/run_' + [guid]::NewGuid().ToString('N'))
    $arguments.OutputDirectory = $OutputDirectory
}
try {
    & (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture scoped_textures -ExpectedCases 8 @arguments
    if ($OriginalTexture) {
        & python3 (Join-Path $PSScriptRoot "xl_private_assets.py") --gpu-trace (Join-Path $OutputDirectory "d2.json")
        if ($LASTEXITCODE -ne 0) { throw "Required original texture GPU evidence is incomplete" }
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

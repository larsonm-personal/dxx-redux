#!/usr/bin/env pwsh
$script:D2xxlProjectReadmeEntryName = "README.dxx-redux.md"

function Get-D2xxlGameName {
    param([string]$GameId)

    if ($GameId -eq "d2") {
        return "Descent 2"
    }
    return "Descent 1"
}

function Get-D2xxlCreditText {
    param([string]$GameId)

    if ($GameId -eq "d2") {
        return "Aus-RED-5D, DizzyRox, MetalBeast, Novacron, Theftbot"
    }
    return "DizzyRox, Novacron, Aus-RED-5"
}

function Get-D2xxlProjectReadmeEntryName {
    return $script:D2xxlProjectReadmeEntryName
}

function Get-D2xxlProjectReadmeText {
    param([string]$ScriptDir)

    $readmePath = Join-Path $ScriptDir "README.md"
    if (-not (Test-Path -LiteralPath $readmePath)) {
        return ""
    }
    return [System.IO.File]::ReadAllText($readmePath)
}

function Get-D2xxlTextureReadmeText {
    param(
        [string]$GameId,
        [int]$Size,
        [bool]$Downscaled
    )

    $credits = Get-D2xxlCreditText -GameId $GameId
    $gameName = Get-D2xxlGameName -GameId $GameId
    $packName = "${GameId}-hires-${Size}-textures-ktx2"
    $mobileNote = "Textures are ETC2-compressed in KTX2 containers for mobile/Android"
    if ($Downscaled) {
        return @"
# ${packName}

${Size}x${Size} replacement textures for ${gameName}

Downscaled from the 512x512 d2x-xl texture pack using ImageMagick
with linear-light colorspace conversion, Lanczos resampling, and
micro-sharpening to preserve detail at reduced resolution

${mobileNote}

Original textures from d2x-xl by ${credits}
"@
    }
    return @"
# ${packName}

${Size}x${Size} high-resolution replacement textures for ${gameName}

${mobileNote}

Original textures from d2x-xl by ${credits}
"@
}

function Get-D2xxlSoundReadmeText {
    param([string]$GameId)

    $credits = Get-D2xxlCreditText -GameId $GameId
    $gameName = Get-D2xxlGameName -GameId $GameId
    $packName = "${GameId}-hires-sounds"
    return @"
# ${packName}

High-resolution replacement sounds for ${gameName}

Converted from the d2x-xl hires sound pack for DXX Redux on Android

Original sounds from d2x-xl by ${credits}
"@
}

function Get-D2xxlTextureEntryPath {
    param(
        [string]$GameId,
        [string]$BaseName,
        [string]$Extension = ".ktx2"
    )

    return "textures/$GameId/$BaseName$Extension"
}

function Get-D2xxlMaskEntryPath {
    param(
        [string]$GameId,
        [string]$BaseName
    )

    return (Get-D2xxlTextureEntryPath -GameId $GameId -BaseName "${BaseName}_mask" -Extension ".png")
}
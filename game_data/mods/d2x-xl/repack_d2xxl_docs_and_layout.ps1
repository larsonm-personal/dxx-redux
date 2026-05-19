param(
    [string[]]$Path,
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $OutputDir) {
    $OutputDir = $scriptDir
}

$packLibScript = Join-Path $scriptDir "d2xxl_pack_lib.ps1"
if (-not (Test-Path $packLibScript)) {
    throw "Missing helper script: $packLibScript"
}
. $packLibScript

if (-not (Test-Path -LiteralPath $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

if (-not $Path -or $Path.Count -eq 0) {
    $Path = @(
        Join-Path $scriptDir "d1-hires-128-textures-ktx2.dxa"
        Join-Path $scriptDir "d1-hires-256-textures-ktx2.dxa"
        Join-Path $scriptDir "d1-hires-512-textures-ktx2.dxa"
        Join-Path $scriptDir "d2-hires-128-textures-ktx2.dxa"
        Join-Path $scriptDir "d2-hires-256-textures-ktx2.dxa"
        Join-Path $scriptDir "d2-hires-512-textures-ktx2.dxa"
        Join-Path $scriptDir "d1-hires-sounds.dxa"
        Join-Path $scriptDir "d2-hires-sounds.dxa"
        Join-Path $scriptDir "d2xxl-hires-textures-d2-64.dxa"
    ) | Where-Object { Test-Path -LiteralPath $_ }
}

if (-not $Path -or $Path.Count -eq 0) {
    throw "No DXA archives found to repack"
}

$projectReadmeText = Get-D2xxlProjectReadmeText -ScriptDir $scriptDir

function Get-D2xxlRepackInfo {
    param([string]$ArchiveName)

    if ($ArchiveName -match '^(d1|d2)-hires-(\d+)-textures-ktx2\.dxa$') {
        $size = [int]$Matches[2]
        return [pscustomobject]@{
            game = $Matches[1]
            type = "texture"
            readmeText = Get-D2xxlTextureReadmeText -GameId $Matches[1] -Size $size -Downscaled ($size -eq 128)
        }
    }
    if ($ArchiveName -match '^(d1|d2)-hires-sounds\.dxa$') {
        return [pscustomobject]@{
            game = $Matches[1]
            type = "sound"
            readmeText = Get-D2xxlSoundReadmeText -GameId $Matches[1]
        }
    }
    if ($ArchiveName -eq 'd2xxl-hires-textures-d2-64.dxa') {
        return [pscustomobject]@{
            game = 'd2'
            type = 'texture'
            readmeText = ''
        }
    }
    return $null
}

function Copy-D2xxlZipEntry {
    param(
        [System.IO.Compression.ZipArchiveEntry]$SourceEntry,
        [System.IO.Compression.ZipArchive]$DestArchive,
        [string]$DestName
    )

    $compressionLevel = if ([System.IO.Path]::GetExtension($DestName).ToLowerInvariant() -eq '.ktx2') {
        [System.IO.Compression.CompressionLevel]::NoCompression
    } else {
        [System.IO.Compression.CompressionLevel]::Optimal
    }
    $destEntry = $DestArchive.CreateEntry($DestName, $compressionLevel)
    $sourceStream = $SourceEntry.Open()
    $destStream = $destEntry.Open()
    try {
        $sourceStream.CopyTo($destStream)
    } finally {
        $destStream.Dispose()
        $sourceStream.Dispose()
    }
}

function Add-D2xxlTextEntry {
    param(
        [System.IO.Compression.ZipArchive]$DestArchive,
        [string]$EntryName,
        [string]$Text
    )

    if (-not $Text) {
        return
    }
    $entry = $DestArchive.CreateEntry($EntryName, [System.IO.Compression.CompressionLevel]::Optimal)
    $stream = $entry.Open()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
        $stream.Write($bytes, 0, $bytes.Length)
    } finally {
        $stream.Dispose()
    }
}

$results = @()
foreach ($archivePath in $Path) {
    $resolvedArchivePath = (Resolve-Path -LiteralPath $archivePath).Path
    $archiveName = Split-Path -Leaf $resolvedArchivePath
    $info = Get-D2xxlRepackInfo -ArchiveName $archiveName
    if (-not $info) {
        Write-Host "Skipping unrecognized archive: $archiveName"
        continue
    }

    $targetPath = Join-Path $OutputDir $archiveName
    $tempTargetPath = if ([System.IO.Path]::GetFullPath($targetPath) -eq [System.IO.Path]::GetFullPath($resolvedArchivePath)) {
        Join-Path $OutputDir ($archiveName + ".repack.tmp")
    } else {
        $targetPath
    }

    if (Test-Path -LiteralPath $tempTargetPath) {
        Remove-Item -LiteralPath $tempTargetPath -Force
    }

    $sourceArchive = [System.IO.Compression.ZipFile]::OpenRead($resolvedArchivePath)
    $destArchive = [System.IO.Compression.ZipFile]::Open($tempTargetPath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        $hasRootReadme = $false
        foreach ($entry in $sourceArchive.Entries) {
            if (-not $entry.Name) {
                continue
            }
            if ($entry.FullName -eq (Get-D2xxlProjectReadmeEntryName)) {
                continue
            }

            $destName = $entry.FullName
            if ($info.type -eq 'texture' -and $entry.FullName -notmatch '/') {
                if ($entry.Name -like '*.ktx2') {
                    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($entry.Name)
                    $destName = Get-D2xxlTextureEntryPath -GameId $info.game -BaseName $baseName
                } elseif ($entry.Name -like '*_mask.png') {
                    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($entry.Name) -replace '_mask$',''
                    $destName = Get-D2xxlMaskEntryPath -GameId $info.game -BaseName $baseName
                } elseif ($entry.Name -like '*.png' -or $entry.Name -like '*.jpg' -or $entry.Name -like '*.tga') {
                    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($entry.Name)
                    $destName = Get-D2xxlTextureEntryPath -GameId $info.game -BaseName $baseName -Extension ([System.IO.Path]::GetExtension($entry.Name).ToLowerInvariant())
                }
            }

            if ($destName -eq 'README.md') {
                $hasRootReadme = $true
            }
            Copy-D2xxlZipEntry -SourceEntry $entry -DestArchive $destArchive -DestName $destName
        }

        if (-not $hasRootReadme -and $info.readmeText) {
            Add-D2xxlTextEntry -DestArchive $destArchive -EntryName 'README.md' -Text $info.readmeText
        }
        if ($projectReadmeText) {
            Add-D2xxlTextEntry -DestArchive $destArchive -EntryName (Get-D2xxlProjectReadmeEntryName) -Text $projectReadmeText
        }
    } finally {
        $destArchive.Dispose()
        $sourceArchive.Dispose()
    }

    if ($tempTargetPath -ne $targetPath) {
        Move-Item -LiteralPath $tempTargetPath -Destination $targetPath -Force
    }

    $results += [pscustomobject]@{
        archive = $archiveName
        game = $info.game
        type = $info.type
        output = $targetPath
        sizeBytes = (Get-Item -LiteralPath $targetPath).Length
    }
}

$results | Format-Table -AutoSize
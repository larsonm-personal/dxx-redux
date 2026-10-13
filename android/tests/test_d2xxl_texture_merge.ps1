#!/usr/bin/env pwsh
# Actual source artwork exercises alpha and supertransparency through the production merge path
[CmdletBinding()]
param(
    [string]$Etc2Tool,
    [string]$Magick,
    [string]$SevenZip = 'C:/Program Files/7-Zip/7z.exe'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot '../helpers/test_env.ps1')
if (!$Etc2Tool) {
    $source = Join-Path $repo 'android/tools/etc2tool'
    $build = Join-Path $source 'build-regression'
    $cmake = Resolve-RegressionCMakePath -RepoRoot $repo
    $configure = @('-S', $source, '-B', $build, '-DCMAKE_BUILD_TYPE=Release')
    if (Test-RegressionWindowsHost) { $configure += @('-G', 'Visual Studio 17 2022', '-A', 'x64') }
    & $cmake @configure
    if ($LASTEXITCODE -ne 0) { throw 'etc2tool configuration failed' }
    & $cmake --build $build --config Release --target etc2tool --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'etc2tool build failed' }
    $Etc2Tool = Join-Path $build $(if (Test-RegressionWindowsHost) { 'Release/etc2tool.exe' } else { 'etc2tool' })
}
if (!(Test-Path -LiteralPath $Etc2Tool)) { throw 'Build etc2tool or provide -Etc2Tool' }
$root = Join-Path $repo ('android/temp/d2xxl-merge-test/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix run_
New-Item -ItemType Directory -Path (Join-Path $root 'textures/d2') -Force | Out-Null
$sourceDir = Join-Path $repo 'game_data/mods/d2x-xl'
$fixtureDir = Join-Path $root 'textures/d2'
& $SevenZip e (Join-Path $sourceDir 'd2x-xl/D2-textures-512x512.7z') 'textures/d2/exp13#0.tga' "-o$fixtureDir" -y | Out-Null
if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath (Join-Path $fixtureDir 'exp13#0.tga'))) { throw 'Cannot extract original explosion strip' }
& $SevenZip e (Join-Path $sourceDir 'd2x-xl/D2-textures-512x512.7z') 'textures/d2/metl135.tga' "-o$fixtureDir" -y | Out-Null
if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath (Join-Path $fixtureDir 'metl135.tga'))) { throw 'Cannot extract original legacy-alpha artwork' }
& $SevenZip e (Join-Path $sourceDir 'd2x-xl/D2-textures-256x256.7z') 'textures/d2/misc037.tga' "-o$fixtureDir" -y | Out-Null
if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath (Join-Path $fixtureDir 'misc037.tga'))) { throw 'Cannot extract original cutout artwork' }
foreach ($rock in @('rock156', 'rock178', 'rock260')) {
    & $SevenZip e (Join-Path $sourceDir 'd2x-xl/D2-textures-512x512.7z') "textures/d2/$rock.tga" "-o$fixtureDir" -y | Out-Null
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath (Join-Path $fixtureDir "$rock.tga"))) { throw "Cannot extract original opaque rock: $rock" }
}
& $SevenZip e (Join-Path $sourceDir 'd2x-xl/D2-textures-256x256.7z') 'textures/d2/muzl02#0.tga' "-o$fixtureDir" -y | Out-Null
if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath (Join-Path $fixtureDir 'muzl02#0.tga'))) { throw 'Cannot extract original additive muzzle artwork' }
$fixture = Join-Path $root 'original-artwork.7z'
Push-Location $root
try { & $SevenZip a $fixture 'textures' | Out-Null } finally { Pop-Location }
if ($LASTEXITCODE -ne 0) { throw 'Cannot create source fixture' }
. (Join-Path $sourceDir 'convert_all.ps1') -OutputDir $root -Etc2Tool $Etc2Tool -Magick $Magick -SevenZip $SevenZip
if (!$Magick -or !(Test-Path -LiteralPath $Magick)) { throw 'Install ImageMagick or provide -Magick' }
$target = Join-Path $root 'merged.dxa'
$zip = [IO.Compression.ZipFile]::Open($target, [IO.Compression.ZipArchiveMode]::Create)
$zip.Dispose()
$names = @('exp13#0', 'misc037', 'metl135', 'rock156', 'rock178', 'rock260', 'muzl02#0', 'muzl02#1', 'muzl02#2')
Convert-AndAdd -GameId d2 -ArchivePath $fixture -MissingNames $names -Targets @(@{ DxaPath = $target; MaxDim = 128 })
$zip = [IO.Compression.ZipFile]::OpenRead($target)
try {
    foreach ($name in $names) {
        $entry = $zip.GetEntry("textures/d2/$name.ktx2")
        if (!$entry) { throw "Missing converted artwork: $name" }
        $stream = $entry.Open()
        try {
            $header = [byte[]]::new(80)
            $reader = [IO.BinaryReader]::new($stream)
            $header = $reader.ReadBytes(80)
            $expectedFormat = if ($name -like 'muzl*') { 147 } else { 151 }
            if ($header.Length -ne 80 -or [BitConverter]::ToUInt32($header, 12) -ne $expectedFormat) { throw "Lost source alpha: $name" }
            $expectedSize = if ($name -eq 'exp13#0') { 64 } else { 128 }
            if ([BitConverter]::ToUInt32($header, 20) -ne $expectedSize -or [BitConverter]::ToUInt32($header, 24) -ne $expectedSize) { throw "Incorrect merged dimensions or unwanted upscaling: $name" }
        } finally { $stream.Dispose() }
    }
    foreach ($name in $names) {
        $marker = $zip.GetEntry("textures/d2/$name.ktx2.blend")
        if ($name -like 'muzl*') {
            if (!$marker) { throw "Missing additive material: $name" }
            $reader = [IO.StreamReader]::new($marker.Open())
            try { if ($reader.ReadToEnd() -cne "additive`n") { throw "Invalid additive material: $name" } } finally { $reader.Dispose() }
        } elseif ($marker) { throw "Unexpected additive material: $name" }
    }
    $maskEntry = $zip.GetEntry('textures/d2/misc037_mask.png')
    if (!$maskEntry) { throw 'Merge dropped the supertransparency mask' }
    $maskStream = $maskEntry.Open()
    try {
        $mask = [Drawing.Bitmap]::new($maskStream)
        try {
            if ($mask.Width -ne 128 -or $mask.Height -ne 128) { throw 'Mask dimensions differ from image' }
            $cutout = 0
            $keep = 0
            for ($y = 0; $y -lt $mask.Height; $y++) {
                for ($x = 0; $x -lt $mask.Width; $x++) {
                    $pixel = $mask.GetPixel($x, $y)
                    if ($pixel.R -eq 0) { $cutout++ } elseif ($pixel.R -eq 255) { $keep++ } else { throw 'Mask lost its binary edges' }
                }
            }
            if (!$cutout -or !$keep) { throw 'Mask lost source cutouts or solid areas' }
        } finally { $mask.Dispose() }
    } finally { $maskStream.Dispose() }
} finally { $zip.Dispose() }
$before = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
$beforeTime = (Get-Item -LiteralPath $target).LastWriteTimeUtc
# Re-extraction changes file timestamps; PNG metadata must not change archive bytes
$repeatTarget = Join-Path $root 'merged-repeat.dxa'
$zip = [IO.Compression.ZipFile]::Open($repeatTarget, [IO.Compression.ZipArchiveMode]::Create)
$zip.Dispose()
Convert-AndAdd -GameId d2 -ArchivePath $fixture -MissingNames $names -Targets @(@{ DxaPath = $repeatTarget; MaxDim = 128 })
if ((Get-FileHash -LiteralPath $repeatTarget).Hash -ne $before) { throw 'Repeated source merge changed archive bytes' }
$primary = Join-Path $root 'primary'
. (Join-Path $sourceDir 'convert_d2xxl_textures.ps1') -SevenZip $SevenZip -Magick $Magick -Etc2Tool $Etc2Tool
Convert-GameTextures -GameId d2 -ArchivePath $fixture -OutDir $primary -MaxDim 128 -Readme 'Source regression' -ProjectReadme '' -MagickPath $Magick -Etc2ToolPath $Etc2Tool
$primaryPack = Join-Path $primary 'd2-hires-128-textures-ktx2.dxa'
$primaryHash = (Get-FileHash -LiteralPath $primaryPack).Hash
$primaryTime = (Get-Item -LiteralPath $primaryPack).LastWriteTimeUtc
Convert-GameTextures -GameId d2 -ArchivePath $fixture -OutDir $primary -MaxDim 128 -Readme 'Source regression' -ProjectReadme '' -MagickPath $Magick -Etc2ToolPath $Etc2Tool
if ((Get-FileHash -LiteralPath $primaryPack).Hash -ne $primaryHash -or (Get-Item -LiteralPath $primaryPack).LastWriteTimeUtc -ne $primaryTime) {
    throw 'Repeated source conversion changed archive bytes or modification time'
}
# Failure after an earlier addition must not publish a partial or duplicate ZIP
$duplicate = Join-Path $root 'muzl02#0.ktx2'
$fresh = Join-Path $root 'new-frame.ktx2'
$zip = [IO.Compression.ZipFile]::OpenRead($target)
try {
    [IO.Compression.ZipFileExtensions]::ExtractToFile($zip.GetEntry('textures/d2/muzl02#0.ktx2'), $duplicate)
} finally { $zip.Dispose() }
Copy-Item -LiteralPath $duplicate -Destination $fresh
foreach ($last in @($duplicate, (Join-Path $root 'missing.ktx2'))) {
    $failed = $false
    try {
        Add-Etc2ToDxa -GameId d2 -DxaPath $target -Etc2Files @($fresh, $last)
    } catch {
        $failed = $true
    }
    if (!$failed -or (Get-FileHash -LiteralPath $target).Hash -ne $before -or (Get-Item -LiteralPath $target).LastWriteTimeUtc -ne $beforeTime) {
        throw 'Failed merge changed the previous archive'
    }
    if (Get-ChildItem -LiteralPath $root -Filter '*.building-*') { throw 'Failed merge left a candidate archive' }
}
Add-Etc2ToDxa -GameId d2 -DxaPath $target -Etc2Files @()
if ((Get-FileHash -LiteralPath $target).Hash -ne $before -or (Get-Item -LiteralPath $target).LastWriteTimeUtc -ne $beforeTime) {
    throw 'Empty merge changed the previous archive'
}
$rejected = $false
try {
    Convert-AndAdd -GameId d2 -ArchivePath $fixture -MissingNames @('absent-frame#99') -Targets @(@{ DxaPath = $target; MaxDim = 64 })
} catch {
    if ($_.Exception.Message -notmatch 'merge conversion errors') { throw }
    $rejected = $true
}
if (!$rejected -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $before) {
    throw 'Missing merge artwork was not rejected without changing the existing archive'
}
Write-Host "Real-source merge alpha/mask regression passed: $target"

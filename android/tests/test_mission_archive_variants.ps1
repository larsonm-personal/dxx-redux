#!/usr/bin/env pwsh
param()

$ErrorActionPreference = 'Stop'
$androidRoot = Split-Path -Parent $PSScriptRoot
$NoBuild = $true
. (Join-Path $androidRoot 'helpers/mission_archive_variants.ps1')
$script:missionArchiveVariantCli = Initialize-MetadataKotlinCli
$testRoot = Join-Path $androidRoot ('temp/mission-archive-variants-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem

function New-FixtureZip {
    param([string]$Path, [hashtable]$Entries)
    $zip = [IO.Compression.ZipFile]::Open($Path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $Entries.Keys) {
            $stream = $zip.CreateEntry($name).Open()
            try {
                $bytes = [byte[]]$Entries[$name]
                $stream.Write($bytes, 0, $bytes.Length)
            } finally { $stream.Dispose() }
        }
    } finally { $zip.Dispose() }
}

$child = Join-Path $testRoot 'child.zip'
New-FixtureZip -Path $child -Entries @{
    'missions/ewithin.mn2' = [Text.Encoding]::UTF8.GetBytes("name = Enemy Within`nnum_levels = 1`newith01.rl2`n")
    'missions/ewithin.hog' = [Text.Encoding]::UTF8.GetBytes('DHF')
}
$parent = Join-Path $testRoot 'ewithin-versions.zip'
New-FixtureZip -Path $parent -Entries @{
    'ewithin-rebirth.zip' = [IO.File]::ReadAllBytes($child)
    'ewithin-xl.zip' = [Text.Encoding]::UTF8.GetBytes('Unused XL must not be opened')
}

# Execute both generators' actual extraction functions against the same combo fixture
foreach ($runner in @(
        @{ Script = 'regenerate_all_mission_metadata_host.ps1'; Function = 'Expand-MissionArchive'; Destination = 'RawArchiveDir' },
        @{ Script = 'regenerate_all_guidebot_simulations.ps1'; Function = 'Expand-GuidebotMissionArchive'; Destination = 'Destination' }
    )) {
    $ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $androidRoot ('helpers/' + $runner.Script)), [ref]$null, [ref]$null)
    $function = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $runner.Function }, $true)
    . ([scriptblock]::Create($function.Extent.Text))
    $destination = Join-Path $testRoot $runner.Function
    $arguments = @{ Archive = Get-Item -LiteralPath $parent }
    $arguments[$runner.Destination] = $destination
    & $runner.Function @arguments
    if (-not (Test-Path -LiteralPath (Join-Path $destination 'missions/ewithin.mn2'))) {
        throw "$($runner.Function) did not extract the Rebirth mission"
    }
    if (@(Get-ChildItem -LiteralPath $destination -Recurse -Filter '*.zip').Count -ne 0) {
        throw "$($runner.Function) retained nested archives"
    }
}
foreach ($names in @(
        @('first-rebirth.zip', 'second-rebirth.zip', 'fallback-dos.zip'),
        @('mission-xl.zip'),
        @('unrelated.zip')
    )) {
    if (Get-PreferredMissionChildArchive -Names $names) { throw "Unexpected selection for $names" }
}
if ((Get-PreferredMissionChildArchive -Names @('mission-dos.zip', 'mission-d2x.zip')) -ne 'mission-dos.zip') {
    throw 'Shared DOS fallback precedence was not preserved'
}
$direct = Join-Path $testRoot 'direct.zip'
New-FixtureZip -Path $direct -Entries @{
    'direct.mn2' = [Text.Encoding]::UTF8.GetBytes('name = Direct')
    'mission-rebirth.zip' = [Text.Encoding]::UTF8.GetBytes('Do not select children when a descriptor exists')
}
$directDestination = Join-Path $testRoot 'direct'
New-Item -ItemType Directory -Path $directDestination | Out-Null
Expand-MissionZipContent -ArchivePath $direct -Destination $directDestination
if (-not (Test-Path -LiteralPath (Join-Path $directDestination 'direct.mn2'))) {
    throw 'Direct mission archives must retain their existing extraction behavior'
}
Write-Output 'PASS: both generators select Rebirth and preserve shared ambiguity/support rules'

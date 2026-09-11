#!/usr/bin/env pwsh
param([string]$ArchivePath)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$helpers = Join-Path $repoRoot 'android/helpers'
Set-StrictMode -Off
. (Join-Path $helpers 'mission_rar_archive.ps1')
function Assert-CallerOptionalProperties {
    $source = [pscustomobject]@{ discover = $true }
    if ($null -ne $source.descriptor) { throw 'An absent optional descriptor must remain null' }
    $missingFiles = @('present') | Where-Object { $_ -eq 'missing' }
    if ($missingFiles.Count -ne 0) { throw 'An empty missing-file result must have count zero' }
}
Assert-CallerOptionalProperties
if (-not $ArchivePath) { $ArchivePath = Join-Path $repoRoot 'game_data/mission_files/FFYL.rar' }
$archive = Get-Item -LiteralPath $ArchivePath
$OutDir = Join-Path $repoRoot 'android/temp/test-mission-rar'
& (Join-Path $helpers 'retain-recent-artifacts.ps1') -Artifacts $OutDir
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
function Import-RunnerFunction {
    param([string]$Script, [string[]]$Names)
    $ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $helpers $Script), [ref]$null, [ref]$null)
    foreach ($name in $Names) {
        $definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
        if (-not $definition) { throw "Missing runner function $name" }
        [scriptblock]::Create($definition.Extent.Text)
    }
}
foreach ($definition in @(Import-RunnerFunction 'run_mission_zip_batch.ps1' @('Get-MissionArchiveEntryNames', 'Add-MissionZipGameHints', 'Get-MissionZipGameHint'))) { . $definition }
$entries = @(Get-MissionArchiveEntryNames -ArchivePath $archive.FullName)
if ($entries -notcontains 'ffyl.MN2' -or $entries -notcontains 'ffyl.HOG') { throw 'FFYL listing is missing its mission files' }
$hint = Get-MissionZipGameHint -ZipPath $archive.FullName
if ($hint.Game -ne 'd2') { throw 'FFYL should be detected as D2' }
foreach ($runner in @(
        @{ Script = 'regenerate_all_mission_metadata_host.ps1'; Function = 'Expand-MissionArchive'; Destination = 'RawArchiveDir' },
        @{ Script = 'regenerate_all_guidebot_simulations.ps1'; Function = 'Expand-GuidebotMissionArchive'; Destination = 'Destination' }
    )) {
    foreach ($definition in @(Import-RunnerFunction $runner.Script @($runner.Function))) { . $definition }
    $destination = Join-Path $OutDir ($runner.Function + '-' + [guid]::NewGuid().ToString('N'))
    $arguments = @{ Archive = $archive }
    $arguments[$runner.Destination] = $destination
    & $runner.Function @arguments
    if ((Get-Content -LiteralPath (Join-Path $destination 'ffyl.MN2') -Raw) -notmatch '(?i)name\s*=') { throw 'Missing mission descriptor' }
    $hog = [IO.File]::ReadAllBytes((Join-Path $destination 'ffyl.HOG'))
    if ([Text.Encoding]::ASCII.GetString($hog, 0, 3) -ne 'DHF') { throw 'Invalid extracted HOG' }
}
Assert-CallerOptionalProperties
Write-Host 'RAR regression listing, game detection, both extraction paths, and caller scope passed'

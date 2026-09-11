#!/usr/bin/env pwsh

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runnerPath = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$runnerAst = [Management.Automation.Language.Parser]::ParseFile($runnerPath, [ref]$null, [ref]$null)
$publisher = $runnerAst.Find({
        param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Publish-GuidebotResult'
    }, $true)
. ([scriptblock]::Create($publisher.Extent.Text))

$Mode = 'Headless'
$WriteRegression = $true
$resultsByIdentity = @{}
$changedFiles = [Collections.Generic.List[string]]::new()
$headedComparisons = [Collections.Generic.List[object]]::new()
$publications = [Collections.Generic.List[object]]::new()
function Write-GuidebotSimulationFile {
    param($MetadataFile, $ResultsByIdentity, $Destination, $HeadedComparisons, $ComparisonIdentity)
    $publications.Add([pscustomobject]@{ Count = $ResultsByIdentity.Count; Identity = $ComparisonIdentity })
}

$metadataFile = [IO.FileInfo]::new((Join-Path $repoRoot 'android/temp/collection.json'))
$publicationState = @{ $metadataFile.FullName = @{ Remaining = 65; Pending = 0; LastWrite = [DateTime]::UtcNow } }
foreach ($index in 1..65) {
    $identity = "level-$index"
    $resultsByIdentity[$identity] = $index
    Publish-GuidebotResult -Item ([pscustomobject]@{ MetadataFile = $metadataFile; Identity = $identity })
}
if (($publications.Count -ne 3) -or (($publications.Count -eq 3) -and
        (($publications | ForEach-Object Count) -join ',') -ne '32,64,65')) {
    throw 'Collection publication must batch results and flush its final partial batch'
}
if ($changedFiles.Count -ne 1) { throw 'Changed files must remain unique across checkpoints' }

$publicationState[$metadataFile.FullName] = @{ Remaining = 10; Pending = 1; LastWrite = [DateTime]::UtcNow.AddSeconds(-31) }
Publish-GuidebotResult -Item ([pscustomobject]@{ MetadataFile = $metadataFile; Identity = 'elapsed' })
if ($publications.Count -ne 4) { throw 'A completed result must checkpoint an elapsed publication interval' }

$Mode = 'Headed'
Publish-GuidebotResult -Item ([pscustomobject]@{ MetadataFile = $metadataFile; Identity = 'headed' })
if ($publications.Count -ne 5 -or $publications[4].Identity -ne 'headed') {
    throw 'Headed results must retain immediate publication and comparison'
}
Write-Host 'Guidebot publication batching tests passed'

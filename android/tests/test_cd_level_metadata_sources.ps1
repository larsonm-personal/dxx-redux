#!/usr/bin/env pwsh

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$tempRoot = Join-Path $repoRoot 'android\temp\cd_level_metadata_sources_test'
. (Join-Path $repoRoot 'android\helpers\cd_level_metadata_sources.ps1')

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

if (Test-Path -LiteralPath $tempRoot) {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}

try {
    $sourceDir = Join-Path $tempRoot 'source'
    $outputDir = Join-Path $tempRoot 'output'
    New-Item -ItemType Directory -Path $sourceDir, $outputDir -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $sourceDir 'mission.hog'), 'hog', [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $sourceDir 'mission.mn2'), 'name = Test', [Text.UTF8Encoding]::new($false))

    $manifestPath = Join-Path $tempRoot 'sources.jsonc'
    $manifest = @{
        sources = @(
            @{
                id = 'test'
                source_dir = 'source'
                files = @('mission.hog', 'mission.mn2')
                descriptor = 'mission.mn2'
                output = 'CD test.json'
            },
            @{
                id = 'discovery'
                source_dir = 'source'
                discover = $true
                exclude_descriptors = @('BROKEN.MN2')
                output = 'CD discovery.json'
            }
        )
    } | ConvertTo-Json -Depth 5
    [IO.File]::WriteAllText($manifestPath, $manifest, [Text.UTF8Encoding]::new($false))

    $literalManifest = @'
// Manifest comments and trailing commas are supported
{
    "sources": [],
    "url": "https://example.invalid/cd//tracks?literal=,]",
    "label": "comma ,} and ,] plus /* literal */ and // literal",
    "escaped": "quote \" // still inside string; slash \\ and ,}",
    /* Actual block comment */
}
'@
    $literalPath = Join-Path $tempRoot 'literal-strings.jsonc'
    [IO.File]::WriteAllText($literalPath, $literalManifest, [Text.UTF8Encoding]::new($false))
    $literals = Read-CdLevelMetadataSourceManifest -Path $literalPath
    Assert-True ($literals.url -ceq 'https://example.invalid/cd//tracks?literal=,]') `
        'A quoted URL and comma/bracket sequence should remain unchanged'
    Assert-True ($literals.label -ceq 'comma ,} and ,] plus /* literal */ and // literal') `
        'Quoted comment markers and comma/bracket sequences should remain unchanged'
    Assert-True ($literals.escaped -ceq 'quote " // still inside string; slash \ and ,}') `
        'Escaped quotes and backslashes should preserve string boundaries'
    Assert-True (@($literals.sources).Count -eq 0) 'The empty source array should remain empty'

    $checkedInPath = Join-Path $repoRoot 'game_data/mission_files/cd_level_metadata_sources.jsonc'
    $checkedIn = Read-CdLevelMetadataSourceManifest -Path $checkedInPath
    $expectedCheckedIn = [IO.File]::ReadAllText($checkedInPath) | ConvertFrom-Json
    Assert-True (($checkedIn | ConvertTo-Json -Depth 20 -Compress) -ceq
        ($expectedCheckedIn | ConvertTo-Json -Depth 20 -Compress)) `
        'The checked-in source manifest should retain every field and exclusion'

    $sources = @(Resolve-CdLevelMetadataSources -RepoRoot $tempRoot -ManifestPath $manifestPath -OutputDir $outputDir)
    Assert-True ($sources.Count -eq 2) 'Explicit and discovery CD metadata sources should resolve'
    Assert-True ($sources[0].Descriptor.Name -eq 'mission.mn2') 'The configured descriptor should resolve'
    Assert-True ($sources[0].Files.Count -eq 2) 'Only explicitly configured source files should resolve'
    Assert-True ($sources[0].OutputPath -eq (Join-Path $outputDir 'CD test.json')) 'The output should resolve under the metadata directory'
    Assert-True ($sources[1].Discover -and $sources[1].Files.Count -eq 0 -and $null -eq $sources[1].Descriptor) `
        'A discovery source should defer file and descriptor selection to the scanner'
    Assert-True ($sources[1].ExcludeDescriptors.Count -eq 1 -and $sources[1].ExcludeDescriptors[0] -eq 'broken.mn2') `
        'Descriptor exclusions should resolve as normalized leaf filenames'

    $firstDescriptor = Get-CdLevelMetadataSourceDescriptors -Source $sources[0] | Select-Object -First 1
    $duplicateDir = Join-Path $tempRoot 'source2'
    New-Item -ItemType Directory -Path $duplicateDir | Out-Null
    Copy-Item -LiteralPath $firstDescriptor.FullName -Destination (Join-Path $duplicateDir $firstDescriptor.Name)
    $duplicateSource = [pscustomobject]@{ Discover = $true; SourceDir = $duplicateDir }
    $duplicateDescriptor = Get-CdLevelMetadataSourceDescriptors -Source $duplicateSource | Select-Object -First 1
    Assert-True ((Get-CdLevelMetadataDescriptorHash -Descriptor $firstDescriptor) -eq
        (Get-CdLevelMetadataDescriptorHash -Descriptor $duplicateDescriptor)) `
        'Descriptor hashing should preserve cross-source duplicate detection'
    $seenHashes = @{}
    Add-CdLevelMetadataSourceDescriptorHashes -Source $sources[0] -SeenHashes $seenHashes
    Assert-True $seenHashes.ContainsKey((Get-CdLevelMetadataDescriptorHash -Descriptor $duplicateDescriptor)) `
        'An earlier unselected source should seed duplicate detection for a later sampled source'

    foreach ($required in @('id', 'source_dir', 'output')) {
        $incomplete = $manifest | ConvertFrom-Json
        $incomplete.sources[0].PSObject.Properties.Remove($required)
        [IO.File]::WriteAllText($manifestPath, ($incomplete | ConvertTo-Json -Depth 5))
        $rejected = $false
        try {
            Resolve-CdLevelMetadataSources -RepoRoot $tempRoot -ManifestPath $manifestPath -OutputDir $outputDir | Out-Null
        } catch {
            $rejected = $_.Exception.Message -like '*require id, source_dir, and output*'
        }
        Assert-True $rejected "Missing $required should produce a manifest validation error"
    }

    $badManifest = $manifest | ConvertFrom-Json
    $badManifest.sources[0].output = '..\escaped.json'
    [IO.File]::WriteAllText($manifestPath, ($badManifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
    $rejected = $false
    try {
        Resolve-CdLevelMetadataSources -RepoRoot $tempRoot -ManifestPath $manifestPath -OutputDir $outputDir | Out-Null
    } catch {
        $rejected = $_.Exception.Message -like '*outside the metadata directory*'
    }
    Assert-True $rejected 'An output outside the metadata directory should be rejected'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'CD level metadata source tests passed'

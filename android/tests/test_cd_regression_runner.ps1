#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$runnerPath = Join-Path $repoRoot 'game_data\run_all_cd_regressions.ps1'
. $runnerPath
. (Join-Path $repoRoot 'game_data\disc_track_manifest.ps1')
& git -C $repoRoot check-ignore --quiet --no-index -- game_data/disc_track_manifest.ps1
if ($LASTEXITCODE -ne 1) { throw 'Disc track manifest source must remain visible to Git' }

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

$defaultStages = @(Get-CdRegressionStages -RepoRoot $repoRoot)
Assert-True ($defaultStages.Count -eq 4) 'Default workflow should contain four stages'
Assert-True ($defaultStages[0].Arguments -contains '-Force') 'CD extraction should be forced by default'
Assert-True ($defaultStages[1].Script -eq (Join-Path $repoRoot 'game_data\extract_all_gog.ps1')) `
    'GOG extraction should refresh before validating shared extraction specs'
Assert-True ($defaultStages[1].Arguments -contains '-Force') 'GOG extraction should be forced by default'
Assert-True ($defaultStages.Name -notcontains 'Refresh regression specs') 'Default workflow must not rewrite its oracle'
Assert-True ($defaultStages[3].Arguments -contains '-All') 'Regression suite should run all specs'
Assert-True ($defaultStages[3].Arguments -contains '-BuildAndInstall') 'Regression suite should build and install the current APK'
Assert-True ($defaultStages[3].Arguments -contains '-RestartDevice') 'Regression suite should start from a clean emulator'
Assert-True ($defaultStages[3].Arguments -notcontains '-SkipLaunch') 'Regression suite should default to full launch mode'

$fileOnlyStages = @(Get-CdRegressionStages -RepoRoot $repoRoot -SkipLaunch)
Assert-True ($fileOnlyStages[3].Arguments -contains '-SkipLaunch') 'Explicit file-only mode should skip game launches'

$refreshStages = @(Get-CdRegressionStages -RepoRoot $repoRoot -RefreshOracle)
Assert-True ($refreshStages.Count -eq 6) 'Oracle refresh workflow should contain six stages'
Assert-True ($refreshStages[2].Name -eq 'Refresh disc track catalog') 'Track catalog must refresh before generating specs'
Assert-True ($refreshStages[3].Name -eq 'Refresh regression specs') 'Oracle refresh stage should be explicit'
Assert-True ($refreshStages[3].Arguments -contains '-Force') 'Explicit oracle refresh should regenerate all specs'

$sampleListPath = Join-Path $repoRoot 'android\temp\cd_sample_specs.txt'
$sampleStages = @(Get-CdRegressionStages -RepoRoot $repoRoot -RefreshOracle -SpecListPath $sampleListPath)
Assert-True ($sampleStages[0].Arguments -contains $sampleListPath -and
    $sampleStages[1].Arguments -contains $sampleListPath -and
    $sampleStages[2].Arguments -contains $sampleListPath -and
    $sampleStages[3].Arguments -contains $sampleListPath -and
    $sampleStages[5].Arguments -contains $sampleListPath) `
    'Sampled CD workflow should use one spec list for extraction, oracle refresh, and tests'
$sampledSpecs = @(New-CdRegressionSampleList -RepoRoot $repoRoot -Fraction 0.1 -Seed 123)
$sampledTypes = @($sampledSpecs | ForEach-Object { (Read-JsoncFile $_).source_type } | Sort-Object -Unique)
Assert-True ($sampledTypes -contains 'cd' -and $sampledTypes -contains 'gog' -and $sampledTypes -contains 'combined') `
    'CD sampling should preserve every source type'
Assert-True (($sampledSpecs -join ',') -eq ((New-CdRegressionSampleList `
                -RepoRoot $repoRoot -Fraction 0.1 -Seed 123) -join ',')) `
    'CD sampling should be reproducible'

$extractSuiteText = Get-Content -LiteralPath (Join-Path $repoRoot 'android\tests\test_all_extracts.ps1') -Raw
Assert-True ($extractSuiteText.Contains("Join-Path `$ReportDir 'summary.json'")) `
    'Extraction suite should save a machine-readable result summary'
Assert-True ($extractSuiteText.Contains("'logcat', '-d', '-v', 'time'")) `
    'Extraction suite should preserve logcat for failed sources'

$tempRoot = Join-Path $repoRoot ('android/temp/test_cd_regression_runner/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $tempRoot -DirectoryPrefix run_
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
$producerLock = [IO.File]::Open((Join-Path $tempRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)

try {
    $cdRoot = Join-Path $tempRoot 'CD images'
    $discRoot = Join-Path $cdRoot 'Descent II (USA) (v1.1)'
    New-Item -ItemType Directory -Path $discRoot -Force | Out-Null
    $catalogPath = Join-Path $tempRoot 'known_discs.jsonc'
    $oldDataHash = '1' * 40
    $oldAudioHash = '2' * 40
    $newDataHash = '3' * 40
    $newAudioHash = '4' * 40
    $catalog = @{
        discs = @(@{
                id = 'descent-ii-usa-v11'; label = 'Descent II (USA) (v1.1)'; game = 'd2'
                track_mapping = @{ title = 2 }
                tracks = @(
                    @{ track = 1; type = 'data'; sha1 = $oldDataHash }
                    @{ track = 2; type = 'audio'; sha1 = $oldAudioHash; name = 'Title'; chromaprint = 'fixture'; duration_ms = 1000 }
                )
            })
    }
    [IO.File]::WriteAllText($catalogPath, "// Preserve curated comments`n" + ($catalog | ConvertTo-Json -Depth 20))
    [IO.File]::WriteAllText((Join-Path $discRoot 'disc.cue'), "FILE `"disc.bin`" BINARY`n TRACK 01 MODE1/2352`n TRACK 02 AUDIO`n")
    $manifestPath = Join-Path $discRoot 'track_hashes.json'
    $tracks = @(@{ track = 1; type = 'data'; sha1 = $newDataHash }, @{ track = 2; type = 'audio'; sha1 = $newAudioHash })
    [IO.File]::WriteAllText($manifestPath, (ConvertTo-Json -InputObject @($tracks + @{ sow = 'descent2.sow'; files_extracted = 12 })))
    $publisher = Join-Path $repoRoot 'game_data/hash_disc_tracks.ps1'
    $pwsh = (Get-Process -Id $PID).Path
    & $pwsh -NoProfile -File $publisher -Force -CdImageDir $cdRoot -CatalogPath $catalogPath
    Assert-True ($LASTEXITCODE -eq 0) 'Catalog refresh should succeed'
    $published = (Read-JsoncFile $catalogPath).discs[0]
    Assert-True ($published.id -ceq 'descent-ii-usa-v11' -and $published.game -ceq 'd2') 'Refresh must preserve existing disc identity'
    Assert-True ($published.tracks[0].sha1 -ceq $newDataHash -and $published.tracks[1].sha1 -ceq $newAudioHash) 'Refresh must update data and audio hashes'
    Assert-True ($published.track_mapping.title -eq 2 -and $published.tracks[1].name -ceq 'Title' -and
        $published.tracks[1].chromaprint -ceq 'fixture' -and $published.tracks[1].duration_ms -eq 1000) 'Refresh must preserve track metadata'
    $publishedText = [IO.File]::ReadAllText($catalogPath)
    Assert-True ($publishedText.Contains('// Preserve curated comments')) 'Refresh must preserve comments'
    $writeTime = ([datetime]'2001-01-01T00:00:00Z').ToUniversalTime()
    [IO.File]::SetLastWriteTimeUtc($catalogPath, $writeTime)
    & $pwsh -NoProfile -File $publisher -Force -CdImageDir $cdRoot -CatalogPath $catalogPath
    Assert-True ($LASTEXITCODE -eq 0 -and [IO.File]::ReadAllText($catalogPath) -ceq $publishedText -and
        [IO.File]::GetLastWriteTimeUtc($catalogPath) -eq $writeTime) 'Unchanged catalog must not be written'
    [IO.File]::WriteAllText($manifestPath, (ConvertTo-Json -InputObject @($tracks[0])))
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $pwsh -NoProfile -File $publisher -Force -CdImageDir $cdRoot -CatalogPath $catalogPath 2>&1 | Out-Null
    $failedExit = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    Assert-True ($failedExit -ne 0 -and [IO.File]::ReadAllText($catalogPath) -ceq $publishedText) 'Incomplete tracks must fail without changing the catalog'

    $newDiscRoot = Join-Path $cdRoot 'D1 new disc'
    New-Item -ItemType Directory -Path $newDiscRoot | Out-Null
    [IO.File]::WriteAllText((Join-Path $newDiscRoot 'disc.iso'), 'standalone ISO fixture')
    [IO.File]::WriteAllText((Join-Path $newDiscRoot 'track_hashes.json'), (ConvertTo-Json -InputObject @(@{ track = 1; type = 'data'; sha1 = ('5' * 40) })))
    $selectedPath = Join-Path $tempRoot 'selected-specs.txt'
    [IO.File]::WriteAllText($selectedPath, (Join-Path $newDiscRoot 'extract_regression.jsonc'))
    & $pwsh -NoProfile -File $publisher -Force -CdImageDir $cdRoot -CatalogPath $catalogPath -SpecListPath $selectedPath
    Assert-True ($LASTEXITCODE -eq 0) 'Selected catalog refresh should not read unselected incomplete manifests'
    $addedCatalog = Read-JsoncFile $catalogPath
    Assert-True ($addedCatalog.discs.Count -eq 2 -and $addedCatalog.discs[1].id -ceq 'd1-new-disc' -and
        $addedCatalog.discs[1].game -ceq 'd1' -and $addedCatalog.discs[1].tracks.Count -eq 1) 'Catalog should append a valid new disc without replacing existing entries'

    # The repository formatter expands track objects over multiple lines
    $fingerprintPath = Join-Path $discRoot 'track_fingerprints.json'
    $fingerprints = @(@{
            track = 2
            type = 'audio'
            sha1 = $newAudioHash
            chromaprint = 'new-fingerprint'
            duration_ms = 2000
            acoustid_name = 'Title { "quoted" }'
            acoustid_album = 'Album // comment-like text'
        })
    [IO.File]::WriteAllText($fingerprintPath, (ConvertTo-Json -InputObject $fingerprints))
    $merger = Join-Path $repoRoot 'game_data/update_known_discs_fingerprints.ps1'
    & $pwsh -NoProfile -File $merger -CdImageDir $cdRoot -CatalogPath $catalogPath
    Assert-True ($LASTEXITCODE -eq 0) 'Multiline fingerprint merge should succeed'
    $merged = (Read-JsoncFile $catalogPath).discs[0]
    Assert-True ($merged.tracks[1].chromaprint -ceq 'new-fingerprint' -and $merged.tracks[1].duration_ms -eq 2000 -and
        $merged.tracks[1].acoustid_name -ceq 'Title { "quoted" }' -and $merged.tracks[1].acoustid_album -ceq 'Album // comment-like text') `
        'Fingerprint merger must handle multiline objects and punctuation inside strings'
    Assert-True ($merged.tracks[1].name -ceq 'Title' -and $merged.track_mapping.title -eq 2) 'Fingerprint merge must preserve curated metadata'
    $mergedText = [IO.File]::ReadAllText($catalogPath)
    Assert-True ($mergedText.Contains('// Preserve curated comments')) 'Fingerprint merge must preserve catalog comments'
    [IO.File]::SetLastWriteTimeUtc($catalogPath, $writeTime)
    & $pwsh -NoProfile -File $merger -CdImageDir $cdRoot -CatalogPath $catalogPath
    Assert-True ($LASTEXITCODE -eq 0 -and [IO.File]::ReadAllText($catalogPath) -ceq $mergedText -and
        [IO.File]::GetLastWriteTimeUtc($catalogPath) -eq $writeTime) 'Unchanged fingerprints must not rewrite the catalog'

    $logPath = Join-Path $tempRoot 'stages.log'
    $scripts = @()
    foreach ($definition in @(
            @{ Name = 'first'; ExitCode = 0 },
            @{ Name = 'second'; ExitCode = 7 },
            @{ Name = 'third'; ExitCode = 0 }
        )) {
        $scriptPath = Join-Path $tempRoot "$($definition.Name).ps1"
        $content = @"
param([string]`$LogPath)
Add-Content -LiteralPath `$LogPath -Value '$($definition.Name)'
exit $($definition.ExitCode)
"@
        [System.IO.File]::WriteAllText($scriptPath, $content, [System.Text.UTF8Encoding]::new($false))
        $scripts += [pscustomobject]@{
            Name = $definition.Name
            Script = $scriptPath
            Arguments = @($logPath)
        }
    }

    $failed = $false
    try {
        Invoke-CdRegressionStages -Stages $scripts
    } catch {
        $failed = $_.Exception.Message -match "stage 'second' failed with exit code 7"
    }
    Assert-True $failed 'Runner should report the failing stage and exit code'
    $executed = @(Get-Content -LiteralPath $logPath)
    Assert-True (($executed -join ',') -eq 'first,second') 'Runner should stop before the stage after a failure'
} finally {
    $producerLock.Dispose()
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

$manifestTemp = Join-Path $repoRoot ('android/temp/disc_track_manifest_test/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $manifestTemp -DirectoryPrefix run_
New-Item -ItemType Directory -Path $manifestTemp -Force | Out-Null
$manifestLock = [IO.File]::Open((Join-Path $manifestTemp 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    $cue = Join-Path $manifestTemp 'disc.cue'
    $cueText = @'
FILE "disc.bin" BINARY
  TRACK 01 MODE1/2352
  TRACK 02 AUDIO
'@
    [IO.File]::WriteAllText($cue, $cueText + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
    $valid = @(
        [pscustomobject]@{ track = 2; type = 'audio'; sha1 = ('b' * 40); source_format = 'flac' },
        [pscustomobject]@{ track = 1; type = 'data'; sha1 = ('a' * 40) }
    )
    $manifest = @(Get-ValidatedDiscTrackManifest -Manifest $valid -CuePath $cue)
    Assert-True ($manifest.Count -eq 2 -and $manifest[0].track -eq 1 -and $manifest[1].track -eq 2) `
        'Valid manifest should be returned in physical track order'
    Assert-True ($manifest[1].Contains('source_format') -and $manifest[1].source_format -ceq 'flac') `
        'Hash catalog publication must receive ordered records preserving source format'

    foreach ($case in @(
            @(),
            @($valid[0]),
            @([pscustomobject]@{ track = 1.5; type = 'data'; sha1 = ('a' * 40) }, $valid[0]),
            @([pscustomobject]@{ track = 3; type = 'data'; sha1 = ('a' * 40) }, $valid[0]),
            @($valid[0], $valid[0]),
            @([pscustomobject]@{ track = 1; type = 'data'; sha1 = ('A' * 40) }, $valid[0]),
            @([pscustomobject]@{ track = 1; type = 'audio'; sha1 = ('a' * 40) }, $valid[0]),
            @([pscustomobject]@{ track = 1; type = 'data'; sha1 = ('a' * 40); error = 'failed' }, $valid[0])
        )) {
        $failed = $false
        try { Get-ValidatedDiscTrackManifest -Manifest $case -CuePath $cue | Out-Null } catch { $failed = $true }
        Assert-True $failed 'Invalid track manifest was accepted'
    }
} finally {
    $manifestLock.Dispose()
    Remove-Item -LiteralPath $manifestTemp -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'CD regression runner tests passed' -ForegroundColor Green
exit 0

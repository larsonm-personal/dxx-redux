#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path "$PSScriptRoot/../..").Path
. "$repoRoot/android/helpers/acoustid_title_match.ps1"
. "$repoRoot/game_data/fingerprint_mission_zip_music.ps1" -BudgetTestOnly -SkipAcoustId

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

Assert-True (Test-DxxAcoustIdTitleMatch `
        "04 Time for the Big Guns (PSX Mix).mp3" "Allister Brimble - Time for the Big Guns") `
    "PSX mix qualifier should not hide a matching maintained title"
Assert-True (Test-DxxAcoustIdTitleMatch `
        "08 (Level 3) Lunar Military Base.mp3" "Composer - Lunar Military Base") `
    "Level qualifier should not hide a matching maintained title"
Assert-True (Test-DxxAcoustIdTitleMatch `
        "14 (Level 9) Mars Military Dig.mp3" "Composer - Stage 09 ~ Mars Military Dig MN0101") `
    "Stage and catalog qualifiers should not hide a matching maintained title"
Assert-True (Test-DxxAcoustIdTitleMatch `
        "03 Haunted - Remix.flac" "Artist - Haunted - Remix") `
    "A title separator must not be mistaken for the artist separator"
Assert-True (-not (Test-DxxAcoustIdTitleMatch `
            "05 Ratzez (Short Remix).mp3" "Ogre, Mark Walk - Ratzez (extended remix)")) `
    "Different remix variants must remain distinct"
Assert-True (-not (Test-DxxAcoustIdTitleMatch `
            "01 Vampyro Briefing (Remastered).mp3" "Torche - Vampyro")) `
    "Unrelated partial titles must remain rejected"

$reviewed = [PSCustomObject]@{
    chromaprint = "fingerprint"
    acoustid_name = "Allister Brimble - Time for the Big Guns"
    acoustid_album = "Descent"
    acoustid_score = 0.95
    acoustid_recording_id = "recording-id"
}
$reviewedMetadata = Get-DxxReusableAcoustIdMetadata -Existing $reviewed `
    -Chromaprint "fingerprint" -MaintainedLabel "Time for the Big Guns.mp3" -RequireReviewedFields
Assert-True ($reviewedMetadata.acoustid_recording_id -eq "recording-id") `
    "Reviewed cached AcoustID metadata should remain reusable after a failed refresh"
Assert-True (-not (Get-DxxReusableAcoustIdMetadata -Existing $reviewed `
            -Chromaprint "changed-fingerprint" -MaintainedLabel "Time for the Big Guns.mp3" `
            -RequireReviewedFields)) `
    "Cached AcoustID metadata must not cross a fingerprint change"

$legacy = [PSCustomObject]@{
    chromaprint = "fingerprint"
    acoustid_name = "Existing historical label"
    acoustid_album = "Existing historical album"
}
$legacyMetadata = Get-DxxReusableAcoustIdMetadata -Existing $legacy -Chromaprint "fingerprint"
Assert-True ($legacyMetadata.acoustid_name -eq "Existing historical label") `
    "A failed refresh should preserve legacy metadata that predates reviewed fields"
$tracklistOnly = [PSCustomObject]@{
    chromaprint = "fingerprint"
    acoustid_name = "Maintained fallback"
    name_source = "tracklist"
}
Assert-True (-not (Get-DxxReusableAcoustIdMetadata -Existing $tracklistOnly `
            -Chromaprint "fingerprint")) `
    "Tracklist fallback labels must not be treated as cached AcoustID results"

$tempDir = Join-Path $repoRoot "android/temp/acoustid_regeneration_test"
if (Test-Path -LiteralPath $tempDir) {
    Remove-Item -LiteralPath $tempDir -Recurse -Force
}
New-Item -ItemType Directory -Path $tempDir | Out-Null
try {
    $zipPath = Join-Path $tempDir "sample.zip"
    [System.IO.File]::WriteAllBytes($zipPath, [byte[]]@())
    $tracklistPath = Join-Path $tempDir "sample.tracklist.json"
    [System.IO.File]::WriteAllText(
        $tracklistPath,
        '{"schema":"dxx-mission-tracklist-v1","tracks":[' +
        '{"title":"Level One","slot":"level1"},{"name":"Level Two","level":2}]}',
        [System.Text.UTF8Encoding]::new($false))

    $lookup = Read-MissionTracklist -ZipFile (Get-Item -LiteralPath $zipPath)
    Assert-True ($lookup["slot:level1"] -eq "Level One") `
        "Tracklist entries without filename fields should remain valid"
    Assert-True ($lookup["slot:level2"] -eq "Level Two") `
        "Tracklist entries without source path fields should remain valid"

    $track = [PSCustomObject]@{
        filename = "game01.ogg"
        chromaprint = "fingerprint"
        duration_ms = 1000
    }
    $tracks = Add-AcoustIdResults -Tracks @($track) -ExistingTracks @{} -TracklistLookup $lookup
    Assert-True ($tracks[0].tracklist_name -eq "Level One") `
        "Tracks without optional source fields should use slot metadata"

    $script:lookupCalls = 0
    function Invoke-AcoustIdLookup {
        $script:lookupCalls++
        return $null
    }
    $SkipAcoustId = $false
    $existingTrack = [PSCustomObject]@{
        filename = "game01.ogg"
        chromaprint = "fingerprint"
        duration_ms = 1000
        acoustid_name = "Existing AcoustID Label"
        acoustid_album = "Existing Album"
        name_source = "acoustid"
    }
    $refreshed = Add-AcoustIdResults -Tracks @($track) `
        -ExistingTracks @{ "game01.ogg" = $existingTrack } `
        -TracklistLookup $lookup -RefreshAcoustId
    Assert-True ($script:lookupCalls -eq 1) `
        "Forced AcoustID refresh should still perform the network lookup"
    Assert-True ($refreshed[0].acoustid_name -eq "Existing AcoustID Label" -and
        $refreshed[0].name_source -eq "acoustid") `
        "A failed forced lookup should preserve existing AcoustID metadata"

    $outputPath = Join-Path $tempDir "chromaprint_info.jsonc"
    Write-ChromaprintInfo -Path $outputPath -AlbumName "Test" -SourceZip "sample.zip" `
        -SourceSha1 ('1' * 40) -SourceSha256 ('2' * 64) -Tracks @([PSCustomObject]@{
            filename = "plain.ogg"
            chromaprint = "fingerprint"
            duration_ms = 1000
        })
    Assert-True (Test-Path -LiteralPath $outputPath) `
        "Tracks without optional label fields should serialize"
    $written = Read-JsoncFile $outputPath
    Assert-True ($written.complete -ceq $true) `
        "Published mission fingerprint metadata should declare completeness"
} finally {
    Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "AcoustID regeneration regression tests passed"

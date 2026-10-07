#!/usr/bin/env pwsh
# Publish validated physical-track hashes without replacing curated disc metadata
param(
    [switch]$Force,
    [string]$CdImageDir = (Join-Path $PSScriptRoot 'CD images'),
    [string]$CatalogPath = (Join-Path $PSScriptRoot '../android/app/src/main/assets/known_discs.jsonc'),
    [string]$SpecListPath
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../android/helpers/fingerprint_source_identity.ps1')
. (Join-Path $PSScriptRoot '../android/helpers/jsonc.ps1')
. (Join-Path $PSScriptRoot '../android/helpers/normalized_json_text.ps1')
. (Join-Path $PSScriptRoot '../android/helpers/atomic_text_file.ps1')
. (Join-Path $PSScriptRoot '../android/helpers/powershell_compat.ps1')
. (Join-Path $PSScriptRoot 'disc_track_manifest.ps1')

$original = [IO.File]::ReadAllText($CatalogPath)
$catalog = Read-JsoncFile $CatalogPath
$byLabel = @{}
$byId = @{}
foreach ($disc in $catalog.discs) {
    $key = Get-DxxPortableSourceNameKey $disc.label
    if ($byLabel.ContainsKey($key) -or $byId.ContainsKey($disc.id)) { throw "Duplicate disc identity: $($disc.id)" }
    $byLabel[$key] = $disc
    $byId[$disc.id] = $disc
}
$selected = @(if ($SpecListPath) {
        Get-Content -LiteralPath $SpecListPath | ForEach-Object { [IO.Path]::GetFullPath($_) }
    })
$hashUpdates = @{}
$newDiscs = @()
$updated = 0
foreach ($folder in (Get-ChildItem -LiteralPath $CdImageDir -Directory | Sort-Object Name)) {
    $specPath = [IO.Path]::GetFullPath((Join-Path $folder.FullName 'extract_regression.jsonc'))
    if ($SpecListPath -and $selected -notcontains $specPath) { continue }
    $hashPath = Join-Path $folder.FullName 'track_hashes.json'
    if (-not (Test-Path -LiteralPath $hashPath)) { throw "Missing track manifest: $hashPath" }
    $key = Get-DxxPortableSourceNameKey $folder.Name
    $existing = $byLabel[$key]
    if ($existing -and -not $Force) { continue }
    $cues = @(Get-ChildItem -LiteralPath $folder.FullName -Filter '*.cue' -File)
    $isos = @(Get-ChildItem -LiteralPath $folder.FullName -Filter '*.iso' -File)
    $sourceArgs = if ($cues.Count -eq 1) { @{ CuePath = $cues[0].FullName } }
    elseif ($cues.Count -eq 0 -and $isos.Count -eq 1) { @{ IsoPath = $isos[0].FullName } }
    else { throw "Disc source must contain one CUE descriptor or standalone ISO: $($folder.FullName)" }
    $manifest = @(ConvertFrom-CompatibleJsonItems -Json (Get-Content -LiteralPath $hashPath -Raw))
    # Native extraction also reports unpacked SOW archives, which are not CD tracks
    $manifest = @($manifest | Where-Object { -not ($_.PSObject.Properties['sow'] -and -not $_.PSObject.Properties['track']) })
    $tracks = @(Get-ValidatedDiscTrackManifest -Manifest $manifest @sourceArgs)
    if ($existing) {
        # Labels identify the original source even when slug rules have changed
        # Updating only SHA-1 values preserves IDs, names, mappings and fingerprints
        if (@($existing.tracks).Count -ne $tracks.Count) { throw "Track layout changed for $($existing.id)" }
        foreach ($track in $tracks) {
            $prior = @($existing.tracks | Where-Object { $_.track -eq $track.track -and $_.type -ceq $track.type })
            if ($prior.Count -ne 1) { throw "Track layout changed for $($existing.id), track $($track.track)" }
            $oldHash = [string]$prior[0].sha1
            if ($oldHash -cnotmatch '^[0-9a-f]{40}$') { throw "Invalid catalog SHA-1 for $($existing.id)" }
            if ($hashUpdates.ContainsKey($oldHash) -and $hashUpdates[$oldHash] -cne $track.sha1) {
                throw "Conflicting source hashes for catalog SHA-1 $oldHash"
            }
            $hashUpdates[$oldHash] = $track.sha1
            if ($oldHash -cne $track.sha1) { $updated++ }
        }
    } else {
        $id = ConvertTo-DxxFingerprintSourceId $folder.Name
        if ($byId.ContainsKey($id)) { throw "Fingerprint source ID collision: $id" }
        $game = if ($folder.Name -match 'Descent I and II|Definitive Collection') { 'd1d2' }
        elseif ($folder.Name -match 'Descent[\s\-]II|Descent 2|D2') { 'd2' }
        elseif ($folder.Name -match 'Descent|Dimensions|D1') { 'd1' }
        else { 'unknown' }
        $disc = [ordered]@{ id = $id; label = $folder.Name; game = $game; tracks = $tracks }
        $byId[$id] = $disc
        $newDiscs += $disc
    }
}

# Replace in one pass so old/new hash overlaps cannot cause cascading edits
# Equal hashes in catalog aliases describe the same physical track
$content = [regex]::Replace($original, '("sha1"\s*:\s*")([0-9a-f]{40})(")', {
        param($match)
        $oldHash = $match.Groups[2].Value
        $hash = if ($hashUpdates.ContainsKey($oldHash)) { $hashUpdates[$oldHash] } else { $oldHash }
        return $match.Groups[1].Value + $hash + $match.Groups[3].Value
    })
if ($newDiscs.Count) {
    $closing = [regex]::Match($content, '\]\s*,?\s*\}\s*$')
    if (-not $closing.Success) { throw 'Cannot locate the catalog discs array closing bracket' }
    $prefix = $content.Substring(0, $closing.Index).TrimEnd()
    if (@($catalog.discs).Count -and -not $prefix.EndsWith(',')) { $prefix += ',' }
    $entries = @($newDiscs | ForEach-Object { $_ | ConvertTo-Json -Depth 20 })
    $content = $prefix + "`n" + ($entries -join ",`n") + "`n]}" + "`n"
}
if ($content -ceq $original) {
    Write-Host 'Disc catalog unchanged'
    exit 0
}
$content = ConvertTo-NormalizedJsonText -Text $content -RepositoryJsonc
$null = ConvertFrom-JsoncText -Text $content | ConvertFrom-Json
Write-Utf8NoBomTextAtomically -Path $CatalogPath -Text $content
Write-Host "Updated $updated track hashes; added $($newDiscs.Count) discs"

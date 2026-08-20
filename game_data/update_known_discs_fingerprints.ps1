#!/usr/bin/env pwsh
# update_known_discs_fingerprints.ps1 -- Merge chromaprint fingerprints into known_discs.jsonc.
#
# Reads track_fingerprints.json from each CD image folder, matches tracks to
# existing known_discs.jsonc entries by SHA-1, and adds "chromaprint",
# "duration_ms", "acoustid_name", and "acoustid_album" fields to audio tracks.
#
# The merge is done via text manipulation to preserve comments and formatting.
#
# Usage: .\update_known_discs_fingerprints.ps1 [-DryRun]
param([switch]$DryRun)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$CdImgDir  = Join-Path $ScriptDir "CD images"
$JsoncPath = Join-Path $ScriptDir "..\android\app\src\main\assets\known_discs.jsonc"

if (-not (Test-Path $JsoncPath)) {
    Write-Error "known_discs.jsonc not found: $JsoncPath"
    exit 1
}

# -- Build fingerprint lookup by SHA-1 --------------------------------

$fpBySha1 = @{}
$fpFolders = 0

$folders = Get-ChildItem -Path $CdImgDir -Directory | Sort-Object Name
foreach ($folder in $folders) {
    $fpFile = Join-Path $folder.FullName "track_fingerprints.json"
    if (-not (Test-Path $fpFile)) { continue }

    $fpFolders++
    $tracks = Get-Content $fpFile -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($t in $tracks) {
        if ($t.type -eq "audio" -and $t.sha1 -and $t.chromaprint) {
            $entry = @{
                chromaprint = $t.chromaprint
                duration_ms = $t.duration_ms
            }
            if ($t.PSObject.Properties["acoustid_name"] -and $t.acoustid_name) {
                $entry.acoustid_name = $t.acoustid_name
            }
            if ($t.PSObject.Properties["acoustid_album"] -and $t.acoustid_album) {
                $entry.acoustid_album = $t.acoustid_album
            }
            $fpBySha1[$t.sha1] = $entry
        }
    }
}

Write-Host "Loaded fingerprints from $fpFolders folders ($($fpBySha1.Count) unique audio tracks)"

if ($fpBySha1.Count -eq 0) {
    Write-Host "No fingerprints found -- run fingerprint_disc_tracks.ps1 first"
    exit 0
}

# -- Helper: build the fields string from a fingerprint entry ----------

function Get-FpFieldsString {
    param($fp)
    $parts = @("`"chromaprint`": `"$($fp.chromaprint)`"", "`"duration_ms`": $($fp.duration_ms)")
    if ($fp.acoustid_name) {
        $escaped = $fp.acoustid_name -replace '"', '\"'
        $parts += "`"acoustid_name`": `"$escaped`""
    }
    if ($fp.acoustid_album) {
        $escaped = $fp.acoustid_album -replace '"', '\"'
        $parts += "`"acoustid_album`": `"$escaped`""
    }
    return $parts -join ", "
}

# -- Process known_discs.jsonc line by line ----------------------------
# For each audio track line that has a sha1 we have a fingerprint for,
# add/update chromaprint, duration_ms, acoustid_name, acoustid_album.

$content = Get-Content $JsoncPath -Raw -Encoding UTF8
$lines = $content -split "`n"
$modified = 0
$alreadyPresent = 0
$newLines = @()

foreach ($line in $lines) {
    # Match track lines: {"track": N, "type": "audio", "sha1": "..."}
    # possibly with trailing name, chromaprint, etc
    if ($line -match '"type"\s*:\s*"audio"' -and $line -match '"sha1"\s*:\s*"([0-9a-f]{40})"') {
        $sha1 = $Matches[1]
        if ($fpBySha1.ContainsKey($sha1)) {
            $fp = $fpBySha1[$sha1]
            $fieldsStr = Get-FpFieldsString $fp

            # Strip existing fields we're about to re-add
            $workLine = $line
            $workLine = $workLine -replace ',\s*"chromaprint"\s*:\s*"[^"]*"', ''
            $workLine = $workLine -replace ',\s*"duration_ms"\s*:\s*\d+', ''
            $workLine = $workLine -replace ',\s*"acoustid_name"\s*:\s*"[^"]*"', ''
            $workLine = $workLine -replace ',\s*"acoustid_album"\s*:\s*"[^"]*"', ''

            # Insert new values before closing }
            $trailingComma = ""
            $trimmed = $workLine.TrimEnd()
            if ($trimmed.EndsWith(",")) {
                $trailingComma = ","
                $trimmed = $trimmed.Substring(0, $trimmed.Length - 1).TrimEnd()
            }
            if ($trimmed.EndsWith("}")) {
                $trimmed = $trimmed.Substring(0, $trimmed.Length - 1).TrimEnd()
            }

            $newLine = "$trimmed, $fieldsStr}$trailingComma"
            if ($newLine -ne $line) {
                $modified++
            } else {
                $alreadyPresent++
            }
            $newLines += $newLine
            continue
        }
    }
    $newLines += $line
}

Write-Host "Tracks updated: $modified"
Write-Host "Already had chromaprint: $alreadyPresent"

if ($modified -eq 0) {
    Write-Host "Nothing to update"
    exit 0
}

if ($DryRun) {
    Write-Host "(dry run -- no file written)"
    exit 0
}

$result = $newLines -join "`n"
$result | Set-Content -NoNewline $JsoncPath -Encoding UTF8
Write-Host "Wrote $JsoncPath"

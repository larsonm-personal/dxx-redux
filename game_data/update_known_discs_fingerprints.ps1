# update_known_discs_fingerprints.ps1 -- Merge chromaprint fingerprints into known_discs.json5.
#
# Reads track_fingerprints.json from each CD image folder, matches tracks to
# existing known_discs.json5 entries by SHA-1, and adds "chromaprint" and
# "duration_ms" fields to audio tracks that have them.
#
# The merge is done via text manipulation to preserve comments and formatting.
#
# Usage: .\update_known_discs_fingerprints.ps1 [-DryRun]
param([switch]$DryRun)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$CdImgDir  = Join-Path $ScriptDir "CD images"
$Json5Path = Join-Path $ScriptDir "..\android\app\src\main\assets\known_discs.json5"

if (-not (Test-Path $Json5Path)) {
    Write-Error "known_discs.json5 not found: $Json5Path"
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
            $fpBySha1[$t.sha1] = @{
                chromaprint = $t.chromaprint
                duration_ms = $t.duration_ms
            }
        }
    }
}

Write-Host "Loaded fingerprints from $fpFolders folders ($($fpBySha1.Count) unique audio tracks)"

if ($fpBySha1.Count -eq 0) {
    Write-Host "No fingerprints found -- run fingerprint_disc_tracks.ps1 first"
    exit 0
}

# -- Process known_discs.json5 line by line ----------------------------
# For each audio track line that has a sha1 we have a fingerprint for,
# add chromaprint and duration_ms fields (or update them).

$content = Get-Content $Json5Path -Raw -Encoding UTF8
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

            if ($line -match '"chromaprint"') {
                # Already has chromaprint -- replace it with the new value
                # Strip existing chromaprint and duration_ms fields, then re-add
                $workLine = $line
                $workLine = $workLine -replace ',\s*"chromaprint"\s*:\s*"[^"]*"', ''
                $workLine = $workLine -replace ',\s*"duration_ms"\s*:\s*\d+', ''

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

                $newLine = "$trimmed, `"chromaprint`": `"$($fp.chromaprint)`", `"duration_ms`": $($fp.duration_ms)}$trailingComma"
                if ($newLine -ne $line) {
                    $modified++
                    $newLines += $newLine
                } else {
                    $alreadyPresent++
                    $newLines += $line
                }
                continue
            }

            # Insert chromaprint and duration_ms before the closing }
            # Preserve trailing comma if present
            $trailingComma = ""
            $workLine = $line.TrimEnd()
            if ($workLine.EndsWith(",")) {
                $trailingComma = ","
                $workLine = $workLine.Substring(0, $workLine.Length - 1).TrimEnd()
            }

            # Remove the closing }
            if ($workLine.EndsWith("}")) {
                $workLine = $workLine.Substring(0, $workLine.Length - 1).TrimEnd()
            }

            $newLine = "$workLine, `"chromaprint`": `"$($fp.chromaprint)`", `"duration_ms`": $($fp.duration_ms)}$trailingComma"
            $newLines += $newLine
            $modified++
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
$result | Set-Content -NoNewline $Json5Path -Encoding UTF8
Write-Host "Wrote $Json5Path"

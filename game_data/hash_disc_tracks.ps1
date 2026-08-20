#!/usr/bin/env pwsh
# hash_disc_tracks.ps1 -- Collect track hashes from all CD images and update known_discs.jsonc.
#
# Reads track_hashes.json from each subfolder in game_data/CD images/,
# generates disc entries (id + label + tracks), and merges into known_discs.jsonc.
# Skips discs already present (by id). Idempotent.
#
# Usage: .\hash_disc_tracks.ps1 [-Force]
param([switch]$Force)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$CdImgDir  = Join-Path $ScriptDir "CD images"
$JsoncPath = Join-Path $ScriptDir "..\android\app\src\main\assets\known_discs.jsonc"
. (Join-Path $ScriptDir "..\android\helpers\fingerprint_source_identity.ps1")
. (Join-Path $ScriptDir "disc_track_manifest.ps1")

if (-not (Test-Path $JsoncPath)) {
    Write-Error "known_discs.jsonc not found: $JsoncPath"
    exit 1
}

# -- Read existing known_discs.jsonc ----------------------------------

$jsoncRaw = Get-Content $JsoncPath -Raw -Encoding UTF8

# Strip comments for JSON parsing
$stripped = $jsoncRaw -replace '//[^\r\n]*', '' -replace '/\*[\s\S]*?\*/', ''
# Remove trailing commas before } or ]
$stripped = $stripped -replace ',(\s*[}\]])', '$1'
$existing = $stripped | ConvertFrom-Json
$existingById = @{}
foreach ($d in $existing.discs) {
    $existingById[$d.id] = $d
}
Write-Host "Existing disc IDs: $($existingById.Keys -join ', ')"

# -- Map folder names to game + id ------------------------------------

function Get-DiscMeta($folderName) {
    $id = ConvertTo-DxxFingerprintSourceId -Name $folderName

    $game = "unknown"
    if ($folderName -match "Descent[\s\-]II|Descent 2|D2") { $game = "d2" }
    elseif ($folderName -match "Descent I and II|Definitive Collection") { $game = "d1d2" }
    elseif ($folderName -match "Descent(?! II)") { $game = "d1" }
    elseif ($folderName -match "Dimensions") { $game = "d1" }

    return @{ Id = $id; Label = $folderName; Game = $game }
}

# -- Scan all CD image folders ----------------------------------------

$newDiscs = @()
$skipped = @()
$replaced = @()

$folders = Get-ChildItem -Path $CdImgDir -Directory | Sort-Object Name
$folderSources = @($folders | ForEach-Object {
    $meta = Get-DiscMeta $_.Name
    [PSCustomObject]@{ Id = $meta.Id; Label = $meta.Label }
})
$reservedSources = @($existing.discs | ForEach-Object {
    [PSCustomObject]@{ Id = [string]$_.id; Label = [string]$_.label }
})
$folderLabelsById = @{}
foreach ($source in $folderSources) { $folderLabelsById[$source.Id.ToUpperInvariant()] = $source.Label }
$nonReplacingSources = @($reservedSources | Where-Object {
    $key = $_.Id.ToUpperInvariant()
    -not ($folderLabelsById.ContainsKey($key) -and
        [StringComparer]::Ordinal.Equals([string]$folderLabelsById[$key], [string]$_.Label))
})
Assert-DxxUniqueFingerprintSourceIds -Sources $folderSources -ReservedSources $nonReplacingSources
foreach ($folder in $folders) {
    $hashFile = Join-Path $folder.FullName "track_hashes.json"
    if (-not (Test-Path $hashFile)) {
        Write-Host "  SKIP (no track_hashes.json): $($folder.Name)" -ForegroundColor Yellow
        continue
    }

    $meta = Get-DiscMeta $folder.Name

    if ($existingById.ContainsKey($meta.Id) -and -not $Force) {
        $skipped += $meta.Id
        continue
    }
    if ($existingById.ContainsKey($meta.Id) -and $Force) {
        $replaced += $meta.Id
    }

    $tracks = @(Get-Content $hashFile -Raw -Encoding UTF8 | ConvertFrom-Json)
    $cueFiles = @(Get-ChildItem -LiteralPath $folder.FullName -Filter '*.cue' -File)
    if ($cueFiles.Count -ne 1) {
        throw "Disc source must contain exactly one CUE descriptor: $($folder.FullName)"
    }
    $trackEntries = @(Get-ValidatedDiscTrackManifest -Manifest $tracks -CuePath $cueFiles[0].FullName)

    $newDiscs += [ordered]@{
        id     = $meta.Id
        label  = $meta.Label
        game   = $meta.Game
        tracks = $trackEntries
    }
    Write-Host "  NEW: $($meta.Id) ($($trackEntries.Count) tracks)" -ForegroundColor Green
}

if ($newDiscs.Count -eq 0) {
    Write-Host "`nNo new discs to add ($($skipped.Count) already present)."
    exit 0
}

# -- Regenerate known_discs.jsonc -------------------------------------
# Strategy: preserve the hand-crafted header and existing entries verbatim,
# then append new entries with proper JSON commas.

# When -Force, split file into entry blocks and remove replaced entries
if ($replaced.Count -gt 0) {
    $replacedSet = @{}
    foreach ($rid in $replaced) { $replacedSet[$rid] = $true }

    # Parse the file into: header + array of entry blocks + footer
    $lines = $jsoncRaw -split "`n"

    # Find the "discs": [ line
    $arrayStartLine = -1
    for ($li = 0; $li -lt $lines.Count; $li++) {
        if ($lines[$li] -match '"discs"\s*:\s*\[') {
            $arrayStartLine = $li
            break
        }
    }

    # Collect entry blocks: each block is a set of lines from comment/blank to closing }
    $blocks = @()
    $currentBlock = @()
    $inEntry = $false
    $braceDepth = 0

    for ($li = $arrayStartLine + 1; $li -lt $lines.Count; $li++) {
        $line = $lines[$li]

        # Detect end of discs array (only when not inside an entry)
        if (-not $inEntry) {
            if ($line -match '^\s*\]\s*$' -or $line -match '^\s*\]\s*\}\s*$') { break }
        }

        # Start collecting a new block on comment or opening brace
        if (-not $inEntry) {
            if ($line -match '^\s*$' -or $line -match '^\s*//') {
                $currentBlock += $line
                continue
            }
            if ($line -match '^\s*\{') {
                $inEntry = $true
                $braceDepth = 0
            }
        }

        if ($inEntry) {
            $currentBlock += $line
            foreach ($ch in $line.ToCharArray()) {
                if ($ch -eq '{') { $braceDepth++ }
                elseif ($ch -eq '}') { $braceDepth-- }
            }
            if ($braceDepth -le 0) {
                # End of entry -- strip trailing comma from closing line
                $lastLine = $currentBlock[-1] -replace ',\s*$', ''
                $currentBlock[-1] = $lastLine

                $blockText = $currentBlock -join "`n"
                $entryId = ""
                if ($blockText -match '"id"\s*:\s*"([^"]+)"') { $entryId = $Matches[1] }

                if (-not $replacedSet.ContainsKey($entryId)) {
                    $blocks += ,@($currentBlock)
                }
                $currentBlock = @()
                $inEntry = $false
            }
        }
    }

    # Rebuild jsoncRaw from header + kept blocks + footer
    $header = ($lines[0..$arrayStartLine] -join "`n")
    $body = ""
    for ($bi = 0; $bi -lt $blocks.Count; $bi++) {
        if ($bi -gt 0) { $body += "," }
        $body += "`n" + ($blocks[$bi] -join "`n")
    }
    $jsoncRaw = $header + $body
    # Will be closed by the append logic below
}

# Find the insertion point.
# After -Force removal, jsoncRaw may not end with ]\n}\n (it's been truncated).
# Normal case: find the last } before the ]\n}\n closing.
$forceMode = ($replaced.Count -gt 0)
if ($forceMode) {
    # jsoncRaw was already truncated to header + kept blocks
    $beforeClosing = $jsoncRaw
} elseif ($jsoncRaw -match '(?s)^(.*\})\s*\]\s*\}\s*$') {
    $beforeClosing = $Matches[1]  # everything up to and including last entry's }
} else {
    Write-Error "Cannot find disc array structure in known_discs.jsonc"
    exit 1
}

# Format a single disc entry as JSONC
function Format-DiscEntry($disc) {
    $lines = @()
    $lines += "    {"
    $lines += "      `"id`": `"$($disc.id)`","
    $lines += "      `"label`": `"$($disc.label)`","
    $lines += "      `"game`": `"$($disc.game)`","
    $lines += "      `"tracks`": ["
    for ($i = 0; $i -lt $disc.tracks.Count; $i++) {
        $t = $disc.tracks[$i]
        $comma = if ($i -lt $disc.tracks.Count - 1) { "," } else { "" }
        $fields = @(
            "`"track`": $($t['track'])",
            "`"type`": `"$($t['type'])`"",
            "`"sha1`": `"$($t['sha1'])`""
        )
        if ($t.Contains('source_format')) {
            $fields += "`"source_format`": `"$($t['source_format'])`""
        }
        $lines += "        {$($fields -join ', ')}$comma"
    }
    $lines += "      ]"
    $lines += "    }"
    return ($lines -join "`n")
}

# Build the new entries block
$newBlock = ""
# If forceMode with no kept entries, first entry doesn't need a comma separator
$needsComma = -not ($forceMode -and $beforeClosing -match '"discs"\s*:\s*\[\s*$')
foreach ($disc in $newDiscs) {
    if ($needsComma) {
        $newBlock += ",`n`n    // -- $($disc.label)`n"
    } else {
        $newBlock += "`n`n    // -- $($disc.label)`n"
        $needsComma = $true
    }
    $newBlock += (Format-DiscEntry $disc)
}

$newContent = ($beforeClosing + $newBlock + "`n  ]`n}`n") -replace "`r`n", "`n"
$newContent | Set-Content -NoNewline $JsoncPath -Encoding UTF8
Write-Host "`nAdded $($newDiscs.Count) new disc entries to known_discs.jsonc"

# -- Report -----------------------------------------------------------

Write-Host "`nSummary:"
Write-Host "  New:      $($newDiscs.Count)"
Write-Host "  Replaced: $($replaced.Count)"
Write-Host "  Skipped:  $($skipped.Count) (already present)"
Write-Host "  Total:    $($existingById.Count + $newDiscs.Count - $replaced.Count) discs in database"

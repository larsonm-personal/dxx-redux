# hash_disc_tracks.ps1 -- Collect track hashes from all CD images and update known_discs.json5.
#
# Reads track_hashes.json from each subfolder in game_data/CD images/,
# generates disc entries (id + label + tracks), and merges into known_discs.json5.
# Skips discs already present (by id). Idempotent.
#
# Usage: .\hash_disc_tracks.ps1 [-Force]
param([switch]$Force)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$CdImgDir  = Join-Path $ScriptDir "CD images"
$Json5Path = Join-Path $ScriptDir "..\android\app\src\main\assets\known_discs.json5"

if (-not (Test-Path $Json5Path)) {
    Write-Error "known_discs.json5 not found: $Json5Path"
    exit 1
}

# -- Read existing known_discs.json5 ----------------------------------

$json5Raw = Get-Content $Json5Path -Raw -Encoding UTF8

# Strip comments for JSON parsing
$stripped = $json5Raw -replace '//[^\r\n]*', '' -replace '/\*[\s\S]*?\*/', ''
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
    $id = $folderName.ToLower() -replace '\s+', '-' -replace '[^a-z0-9\-]', ''
    $id = $id -replace '-+', '-' -replace '^-|-$', ''

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

    $tracks = Get-Content $hashFile -Raw -Encoding UTF8 | ConvertFrom-Json

    # Filter out SOW entries (they have "sow" key, not track/type/sha1)
    $trackEntries = @()
    foreach ($t in $tracks) {
        if ($null -ne $t.track -and $null -ne $t.type -and $null -ne $t.sha1) {
            $trackEntry = [ordered]@{ track = $t.track; type = $t.type; sha1 = $t.sha1 }
            if ($null -ne $t.PSObject.Properties['source_format'] -and $t.source_format) {
                $trackEntry.source_format = $t.source_format
            }
            $trackEntries += $trackEntry
        }
    }

    $newDiscs += [ordered]@{
        id     = $meta.Id
        label  = $meta.Label
        game   = $meta.Game
        tracks = $trackEntries
    }
    Write-Host "  NEW: $($meta.Id) ($($tracks.Count) tracks)" -ForegroundColor Green
}

if ($newDiscs.Count -eq 0) {
    Write-Host "`nNo new discs to add ($($skipped.Count) already present)."
    exit 0
}

# -- Regenerate known_discs.json5 -------------------------------------
# Strategy: preserve the hand-crafted header and existing entries verbatim,
# then append new entries with proper JSON commas.

# When -Force, split file into entry blocks and remove replaced entries
if ($replaced.Count -gt 0) {
    $replacedSet = @{}
    foreach ($rid in $replaced) { $replacedSet[$rid] = $true }

    # Parse the file into: header + array of entry blocks + footer
    $lines = $json5Raw -split "`n"

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

    # Rebuild json5Raw from header + kept blocks + footer
    $header = ($lines[0..$arrayStartLine] -join "`n")
    $body = ""
    for ($bi = 0; $bi -lt $blocks.Count; $bi++) {
        if ($bi -gt 0) { $body += "," }
        $body += "`n" + ($blocks[$bi] -join "`n")
    }
    $json5Raw = $header + $body
    # Will be closed by the append logic below
}

# Find the insertion point.
# After -Force removal, json5Raw may not end with ]\n}\n (it's been truncated).
# Normal case: find the last } before the ]\n}\n closing.
$forceMode = ($replaced.Count -gt 0)
if ($forceMode) {
    # json5Raw was already truncated to header + kept blocks
    $beforeClosing = $json5Raw
} elseif ($json5Raw -match '(?s)^(.*\})\s*\]\s*\}\s*$') {
    $beforeClosing = $Matches[1]  # everything up to and including last entry's }
} else {
    Write-Error "Cannot find disc array structure in known_discs.json5"
    exit 1
}

# Format a single disc entry as JSON5
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
$newContent | Set-Content -NoNewline $Json5Path -Encoding UTF8
Write-Host "`nAdded $($newDiscs.Count) new disc entries to known_discs.json5"

# -- Report -----------------------------------------------------------

Write-Host "`nSummary:"
Write-Host "  New:      $($newDiscs.Count)"
Write-Host "  Replaced: $($replaced.Count)"
Write-Host "  Skipped:  $($skipped.Count) (already present)"
Write-Host "  Total:    $($existingById.Count + $newDiscs.Count - $replaced.Count) discs in database"

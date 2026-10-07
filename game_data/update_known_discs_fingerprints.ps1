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
param(
    [switch]$DryRun,
    [string]$CdImageDir = (Join-Path $PSScriptRoot 'CD images'),
    [string]$CatalogPath = (Join-Path $PSScriptRoot '../android/app/src/main/assets/known_discs.jsonc')
)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$CdImgDir = $CdImageDir
$JsoncPath = $CatalogPath
. (Join-Path $ScriptDir '../android/helpers/jsonc.ps1')
. (Join-Path $ScriptDir '../android/helpers/normalized_json_text.ps1')
. (Join-Path $ScriptDir '../android/helpers/atomic_text_file.ps1')

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

# Tokenize strings and comments before braces so embedded punctuation cannot
# change object boundaries. Edit only track objects and preserve catalog comments
$content = [IO.File]::ReadAllText($JsoncPath)
$starts = [Collections.Generic.Stack[int]]::new()
$edits = [Collections.Generic.List[object]]::new()
$matched = 0
$tokens = [regex]::Matches($content, '"(?:\\.|[^"\\])*"|//[^\r\n]*|/\*[\s\S]*?\*/|[{}]')
foreach ($token in $tokens) {
    if ($token.Value -eq '{') {
        $starts.Push($token.Index)
    } elseif ($token.Value -eq '}') {
        if (-not $starts.Count) { throw 'Unbalanced catalog object' }
        $start = $starts.Pop()
        $length = $token.Index + 1 - $start
        $objectText = $content.Substring($start, $length)
        $record = ConvertFrom-JsoncText -Text $objectText | ConvertFrom-Json
        if (-not $record.PSObject.Properties['track'] -or
            -not $record.PSObject.Properties['type'] -or $record.type -cne 'audio' -or
            -not $record.PSObject.Properties['sha1'] -or -not $fpBySha1.ContainsKey($record.sha1)) { continue }
        $matched++
        $fingerprint = $fpBySha1[$record.sha1]
        $changed = $false
        foreach ($name in @('chromaprint', 'duration_ms', 'acoustid_name', 'acoustid_album')) {
            $property = $record.PSObject.Properties[$name]
            if ($fingerprint.ContainsKey($name)) {
                if (-not $property -or $property.Value -cne $fingerprint[$name]) {
                    $record | Add-Member -NotePropertyName $name -NotePropertyValue $fingerprint[$name] -Force
                    $changed = $true
                }
            } elseif ($property) {
                $record.PSObject.Properties.Remove($name)
                $changed = $true
            }
        }
        if ($changed) {
            $replacement = $record | ConvertTo-Json -Depth 20
            $comments = @([regex]::Matches($objectText, '"(?:\\.|[^"\\])*"|//[^\r\n]*|/\*[\s\S]*?\*/') |
                    Where-Object { $_.Value.StartsWith('/') } | ForEach-Object { $_.Value })
            if ($comments.Count) { $replacement = $replacement.Insert(1, "`n" + ($comments -join "`n") + "`n") }
            $edits.Add([pscustomobject]@{ Start = $start; Length = $length; Text = $replacement })
        }
    }
}
if ($starts.Count) { throw 'Unbalanced catalog object' }
Write-Host "Matched audio tracks: $matched; changed: $($edits.Count)"
if (-not $edits.Count) {
    Write-Host 'Disc fingerprints unchanged'
    exit 0
}
if ($DryRun) {
    Write-Host '(dry run -- no file written)'
    exit 0
}
foreach ($edit in ($edits | Sort-Object Start -Descending)) {
    $content = $content.Remove($edit.Start, $edit.Length).Insert($edit.Start, $edit.Text)
}
$content = ConvertTo-NormalizedJsonText -Text $content -RepositoryJsonc
$null = ConvertFrom-JsoncText -Text $content | ConvertFrom-Json
Write-Utf8NoBomTextAtomically -Path $JsoncPath -Text $content
Write-Host "Wrote $JsoncPath"

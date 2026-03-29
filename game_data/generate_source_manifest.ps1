# generate_source_manifest.ps1 - Generate game_data/SOURCE_FILES.txt
#
# Lists all original (non-derived) source files in game_data/ with SHA-256 hashes.
# Includes: CD images (.bin/.cue), GOG installers (.exe/.pkg/.zip),
#           demo installers (.zip), DESCENT2.SOW
# Excludes: extracted/, data_tracks/, scripts, derived/intermediate files
#
# Usage: .\generate_source_manifest.ps1

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
$OutFile   = Join-Path $ScriptDir "SOURCE_FILES.txt"

# Collect original source files
$files = Get-ChildItem -Path $ScriptDir -Recurse -File | Where-Object {
    ($_.FullName -match "\\CD images\\" -and $_.Extension -in '.bin','.cue','.ccd','.img','.sub') -or
    ($_.FullName -match "\\gog installers\\" -and $_.Extension -in '.exe','.pkg','.zip' -and $_.FullName -notmatch "\\extracted\\") -or
    ($_.FullName -match "\\demo installers\\" -and $_.Extension -eq '.zip') -or
    ($_.Name -eq 'DESCENT2.SOW' -and $_.DirectoryName -eq $ScriptDir)
} | Sort-Object { $_.FullName.Substring($ScriptDir.Length + 1) }

$total = $files.Count
$totalSize = ($files | Measure-Object Length -Sum).Sum
Write-Host "Hashing $total files ($([math]::Round($totalSize / 1GB, 2)) GB)..."

$lines = @()
$lines += "# game_data/ Source File Manifest"
$lines += "# Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$lines += "# Total: $total files, $([math]::Round($totalSize / 1GB, 2)) GB"
$lines += "#"
$lines += "# These are the original, non-derived source files needed to recreate the"
$lines += "# game_data/ directory. They are NOT committed to git due to size and copyright"
$lines += "# Derived files (extracted/, data_tracks/) are produced by the scripts"
$lines += "#"
$lines += "# Format: SHA256  SIZE  PATH"
$lines += ""

$currentGroup = ""
$i = 0

foreach ($f in $files) {
    $i++
    $rel = $f.FullName.Substring($ScriptDir.Length + 1)
    $group = $rel.Split('\')[0]
    if ($group -ne $currentGroup) {
        if ($currentGroup -ne "") { $lines += "" }
        $lines += "## $group"
        $currentGroup = $group
    }

    Write-Host "  [$i/$total] $rel" -NoNewline
    $hash = (Get-FileHash -Path $f.FullName -Algorithm SHA256).Hash.ToLower()
    Write-Host " -> $($hash.Substring(0,16))..."
    $lines += "$hash  $($f.Length.ToString().PadLeft(12))  $rel"
}

($lines -join "`n") + "`n" | Set-Content $OutFile -NoNewline -Encoding UTF8
Write-Host "`nManifest written to: $OutFile"

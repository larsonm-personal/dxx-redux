<#
.SYNOPSIS
  Generates extract_regression.json5 specs for each CD image and GOG installer.
.DESCRIPTION
  Walks game_data/CD images/ and game_data/gog installers/ to create regression
  test specs. Uses known_discs.json5 for disc identification and tracks extracted
  files to populate expected_files lists.
.PARAMETER Force
  Overwrite existing extract_regression.json5 files.
#>
param([switch]$Force)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent  # dxx-redux root
$gameDataDir = $PSScriptRoot              # game_data/
$knownDiscsPath = Join-Path $root 'android\app\src\main\assets\known_discs.json5'

# --- Parse known_discs.json5 ---
$rawText = [System.IO.File]::ReadAllText($knownDiscsPath, [System.Text.Encoding]::UTF8)
# Strip BOM if present
if ($rawText[0] -eq [char]0xFEFF) { $rawText = $rawText.Substring(1) }
# Strip // comments
$rawText = [regex]::Replace($rawText, '//.*', '')
# Strip trailing commas before } or ]
$rawText = [regex]::Replace($rawText, ',\s*([}\]])', '$1')
$knownDiscs = ($rawText | ConvertFrom-Json).discs

# Build SHA1->disc lookup (keyed by data track 1 SHA1)
$sha1ToDisc = @{}
foreach ($disc in $knownDiscs) {
    foreach ($track in $disc.tracks) {
        if ($track.type -eq 'data' -and $track.sha1) {
            $sha1ToDisc[$track.sha1] = $disc
        }
    }
}

# --- Classification tables ---
# Maps disc properties to classification. The disc's `game` field plus extracted
# file patterns determine what type of release this is.
# NOTE: These mission names are what appears in the game's mission select menu.
# expected_level1 is what introspection reports for level 1 of each mission.

# d2 full variants (any d2 disc with descent2.hog + groupa.pig)
# d2 oem  = Destination Quartzon (descent2.hog but NO groupa.pig... actually it HAS groupa.pig)
# d2 demo = 3-level preview (d2demo.hog)
# d2 vertigo = expansion only (d2x.hog but no groupa.pig)
# d1 full = any d1 disc with descent.hog + descent.pig
# d1 demo = test flight (descent.hog but different contents)
# d1 levels-only = Levels of the World, Dimensions (no descent.pig)
# d1 expansion = Destination Saturn (descent.hog + saturn.hog)
# d1d2 = Definitive Collection discs

function Get-DiscClassification($discId, $game, $extractedFiles) {
    $lower = $extractedFiles | ForEach-Object { $_.ToLower() }

    # D2 demo/preview
    if ($lower -contains 'd2demo.hog') {
        return @{
            type = 'd2_demo'
            mission = 'Descent 2 Demo'
            level1 = 'Corliss Steam Mine'
            min_files = @('d2demo.hog', 'd2demo.ham', 'd2demo.pig')
        }
    }

    # D2 Vertigo expansion (d2x.hog without full game PIGs)
    if ($lower -contains 'd2x.hog' -and $lower -notcontains 'groupa.pig') {
        return @{
            type = 'd2_vertigo'
            mission = 'D2: Vertigo!'
            level1 = 'Styx Level 1'  # placeholder - needs verification
            min_files = @('d2x.hog', 'd2x.mn2', 'descent2.hog', 'descent2.ham')
        }
    }

    # D2 OEM (Destination Quartzon) - has descent2.hog but disc id contains "quartzon"
    if ($discId -match 'quartzon') {
        return @{
            type = 'd2_oem'
            mission = 'D2 Destination:Quartzon'
            level1 = 'Drec Sphere'  # placeholder - needs verification
            min_files = @('descent2.hog', 'descent2.ham', 'groupa.pig')
        }
    }

    # D2 full
    if ($lower -contains 'descent2.hog' -and $lower -contains 'groupa.pig') {
        return @{
            type = 'd2_full'
            mission = 'Descent 2: Counterstrike!'
            level1 = 'Ahayweh Gate'
            min_files = @('descent2.hog', 'descent2.ham', 'descent2.s11', 'descent2.s22', 'groupa.pig')
        }
    }

    # D1 demo/test flight (descent.hog but no descent.pig of normal size? Actually test flight has descent.pig)
    if ($discId -match 'test-flight') {
        return @{
            type = 'd1_demo'
            mission = 'Descent Demo'
            level1 = 'Lunar Outpost'  # demo starts at level 1
            min_files = @('descent.hog', 'descent.pig')
        }
    }

    # D1 levels-only (no descent.pig - just add-on levels)
    if ($game -eq 'd1' -and $lower -notcontains 'descent.pig') {
        return @{
            type = 'd1_levels'
            mission = $null  # can't launch standalone
            level1 = $null
            min_files = @()
        }
    }

    # D1 Destination Saturn (has saturn.hog)
    if ($lower -contains 'saturn.hog') {
        return @{
            type = 'd1_expansion'
            mission = 'Descent: First Strike'
            level1 = 'Lunar Outpost'
            min_files = @('descent.hog', 'descent.pig')
        }
    }

    # D1 full (including Anniversary, Definitive Disc 1)
    if ($lower -contains 'descent.hog' -and $lower -contains 'descent.pig') {
        return @{
            type = 'd1_full'
            mission = 'Descent: First Strike'
            level1 = 'Lunar Outpost'
            min_files = @('descent.hog', 'descent.pig')
        }
    }

    # D1D2 multi-disc - classify by what's actually on this disc
    if ($game -eq 'd1d2') {
        if ($lower -contains 'descent2.hog' -and $lower -contains 'groupa.pig') {
            return @{
                type = 'd2_full'
                mission = 'Descent 2: Counterstrike!'
                level1 = 'Ahayweh Gate'
                min_files = @('descent2.hog', 'descent2.ham', 'groupa.pig')
            }
        }
        if ($lower -contains 'd2x.hog') {
            return @{
                type = 'd2_vertigo'
                mission = 'D2: Vertigo!'
                level1 = 'Styx Level 1'
                min_files = @('d2x.hog', 'd2x.mn2', 'descent2.hog', 'descent2.ham')
            }
        }
        if ($lower -contains 'descent.hog' -and $lower -contains 'descent.pig') {
            return @{
                type = 'd1_full'
                mission = 'Descent: First Strike'
                level1 = 'Lunar Outpost'
                min_files = @('descent.hog', 'descent.pig')
            }
        }
    }

    return @{
        type = 'unknown'
        mission = $null
        level1 = $null
        min_files = @()
    }
}

# --- Process CD images ---
$cdDir = Join-Path $gameDataDir 'CD images'
$gogDir = Join-Path $gameDataDir 'gog installers'
$specCount = 0
$skipped = 0

Write-Host "=== Generating extract_regression.json5 specs ===" -ForegroundColor Cyan

foreach ($dir in (Get-ChildItem $cdDir -Directory | Sort-Object Name)) {
    $specPath = Join-Path $dir.FullName 'extract_regression.json5'
    if ((Test-Path $specPath) -and -not $Force) {
        $skipped++
        continue
    }

    $hashFile = Join-Path $dir.FullName 'track_hashes.json'
    $dataTracksDir = Join-Path $dir.FullName 'data_tracks'

    # Find source .cue file
    $cueFiles = Get-ChildItem $dir.FullName -Filter '*.cue' -File
    if (-not $cueFiles) {
        Write-Host "  SKIP $($dir.Name): no .cue file" -ForegroundColor Yellow
        continue
    }

    # Get source file hashes
    $sourceFiles = @()
    foreach ($cue in $cueFiles) {
        $hash = (Get-FileHash $cue.FullName -Algorithm SHA256).Hash.ToLower()
        $sourceFiles += @{ name = $cue.Name; sha256 = $hash }
    }
    # Also hash .bin files
    foreach ($bin in (Get-ChildItem $dir.FullName -Filter '*.bin' -File)) {
        $hash = (Get-FileHash $bin.FullName -Algorithm SHA256).Hash.ToLower()
        $sourceFiles += @{ name = $bin.Name; sha256 = $hash }
    }

    # Look up disc in known_discs via track_hashes.json
    $discId = $null
    $game = $null
    $audioTracks = 0
    if (Test-Path $hashFile) {
        $tracks = Get-Content $hashFile -Raw | ConvertFrom-Json
        foreach ($track in $tracks) {
            if ($track.type -eq 'data' -and $track.sha1) {
                if ($sha1ToDisc.ContainsKey($track.sha1)) {
                    $disc = $sha1ToDisc[$track.sha1]
                    $discId = $disc.id
                    $game = $disc.game
                    $audioTracks = ($disc.tracks | Where-Object { $_.type -eq 'audio' }).Count
                }
            }
        }
    }
    if (-not $discId) {
        Write-Host "  WARN $($dir.Name): not found in known_discs" -ForegroundColor Yellow
    }

    # Get extracted files list
    $extractedFiles = @()
    if (Test-Path $dataTracksDir) {
        $extractedFiles = Get-ChildItem $dataTracksDir -Recurse -File |
            Where-Object { $_.Extension -match '\.(hog|pig|ham|s11|s22|mvl|dem|msn|mn2|sow|gog|inst)$' } |
            Select-Object -ExpandProperty Name | Sort-Object -Unique
    }

    # Classify
    $classification = Get-DiscClassification $discId $game $extractedFiles

    # Build spec
    $spec = [ordered]@{
        source_type = 'cd'
        disc_id = $discId
        source_files = $sourceFiles
        game = if ($game) { $game } else { 'unknown' }
        classification = $classification.type
        expected_mission = $classification.mission
        expected_level1 = $classification.level1
        expected_files = ($classification.min_files | Sort-Object)
        audio_tracks = $audioTracks
        total_extracted = $extractedFiles.Count
    }

    # Write as json5 (just JSON with a comment header)
    $json = $spec | ConvertTo-Json -Depth 4
    $content = "// Auto-generated regression spec for: $($dir.Name)`n// Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`n$json"
    [System.IO.File]::WriteAllText($specPath, $content, [System.Text.Encoding]::UTF8)
    $specCount++
    $status = if ($classification.type -eq 'unknown') { 'UNKNOWN' } elseif (-not $classification.mission) { 'NO-LAUNCH' } else { 'OK' }
    Write-Host "  $status $($dir.Name) -> $($classification.type)" -ForegroundColor $(if ($status -eq 'OK') { 'Green' } elseif ($status -eq 'NO-LAUNCH') { 'DarkYellow' } else { 'Red' })
}

# --- Process GOG installers ---
Write-Host ""
Write-Host "=== GOG installers ===" -ForegroundColor Cyan

$gogInstallers = @(
    @{ file = 'setup_descent_1.4a_(16596).exe'; game = 'd1'; type = 'd1_full';
       mission = 'Descent: First Strike'; level1 = 'Lunar Outpost';
       min_files = @('DESCENT.HOG', 'DESCENT.PIG') },
    @{ file = 'setup_descent_2_1.1_(16596).exe'; game = 'd2'; type = 'd2_full';
       mission = 'Descent 2: Counterstrike!'; level1 = 'Ahayweh Gate';
       min_files = @('DESCENT2.HOG', 'DESCENT2.HAM', 'DESCENT2.S11', 'DESCENT2.S22', 'GROUPA.PIG') },
    @{ file = 'descent_enUS_1_0_35122.pkg'; game = 'd1'; type = 'd1_full';
       mission = 'Descent: First Strike'; level1 = 'Lunar Outpost';
       min_files = @('DESCENT.HOG', 'DESCENT.PIG') },
    @{ file = 'descent_2_enUS_1_0_51877.pkg'; game = 'd2'; type = 'd2_full';
       mission = 'Descent 2: Counterstrike!'; level1 = 'Ahayweh Gate';
       min_files = @('DESCENT2.HOG', 'DESCENT2.HAM', 'DESCENT2.S11', 'DESCENT2.S22', 'GROUPA.PIG') }
)

foreach ($gog in $gogInstallers) {
    $installerPath = Join-Path $gogDir $gog.file
    if (-not (Test-Path $installerPath)) {
        Write-Host "  SKIP $($gog.file): not found" -ForegroundColor Yellow
        continue
    }

    # Output spec goes next to the installer
    $specPath = Join-Path $gogDir "$([System.IO.Path]::GetFileNameWithoutExtension($gog.file))_regression.json5"
    if ((Test-Path $specPath) -and -not $Force) {
        $skipped++
        continue
    }

    # Hash installer
    $hash = (Get-FileHash $installerPath -Algorithm SHA256).Hash.ToLower()

    # Get extracted files from the pre-extracted directory
    $extractedDir = Join-Path $gogDir ([System.IO.Path]::GetFileNameWithoutExtension($gog.file))
    $extractedFiles = @()
    $extractedCount = 0
    if (Test-Path $extractedDir) {
        $extractedFiles = Get-ChildItem $extractedDir -Recurse -File | Select-Object -ExpandProperty Name | Sort-Object
        $extractedCount = $extractedFiles.Count
    }

    # Determine audio tracks from GOG audio files
    $audioTracks = 0
    $lowerFiles = $extractedFiles | ForEach-Object { $_.ToLower() }
    if ($lowerFiles -contains 'descent_ii.gog' -or $lowerFiles -contains 'descent.gog') {
        # GOG audio is embedded in .gog file, track count comes from .inst file
        # D2 GOG has 8 audio tracks, D1 GOG has 0 (no redbook audio in D1)
        if ($gog.game -eq 'd2') { $audioTracks = 8 }
    }

    $spec = [ordered]@{
        source_type = 'gog'
        source_files = @(@{ name = $gog.file; sha256 = $hash })
        game = $gog.game
        classification = $gog.type
        expected_mission = $gog.mission
        expected_level1 = $gog.level1
        expected_files = ($gog.min_files | Sort-Object)
        audio_tracks = $audioTracks
        total_extracted = $extractedCount
    }

    $json = $spec | ConvertTo-Json -Depth 4
    $content = "// Auto-generated regression spec for: $($gog.file)`n// Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`n$json"
    [System.IO.File]::WriteAllText($specPath, $content, [System.Text.Encoding]::UTF8)
    $specCount++
    Write-Host "  OK $($gog.file) -> $($gog.type)" -ForegroundColor Green
}

Write-Host ""
Write-Host "Generated $specCount specs, skipped $skipped (use -Force to overwrite)" -ForegroundColor Cyan

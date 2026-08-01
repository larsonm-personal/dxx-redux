#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Generates extract_regression.json5 specs for each CD image, ISO image, and GOG installer.
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
$androidDir = Join-Path $root 'android'
$knownDiscsPath = Join-Path (Join-Path (Join-Path (Join-Path (Join-Path $androidDir 'app') 'src') 'main') 'assets') 'known_discs.json5'
$regressionSpecHelpersPath = Join-Path (Join-Path $androidDir 'tests') 'extract_regression_spec_helpers.ps1'
. $regressionSpecHelpersPath
. (Join-Path $androidDir 'helpers\bounded_extraction.ps1')

function Get-ExistingLastTestResult($path) {
    if (-not (Test-Path -LiteralPath $path)) {
        return $null
    }

    try {
        $existingSpec = Read-Json5File $path
        return $existingSpec.last_test_result
    } catch {
        Write-Host "  WARN $([System.IO.Path]::GetFileName((Split-Path $path -Parent))): could not preserve last_test_result: $($_.Exception.Message)" -ForegroundColor Yellow
        return $null
    }
}

function Resolve-GameForSpec($game, $classification) {
    if ($game -and $game -ne 'unknown') {
        return $game
    }
    if ($classification.type -match '^d1') {
        return 'd1'
    }
    if ($classification.type -match '^d2') {
        return 'd2'
    }
    return 'unknown'
}

# --- Parse known_discs.json5 ---
$knownDiscs = (Read-Json5File $knownDiscsPath).discs

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
            mission = $null
            level1 = $null
            min_files = @('d2x.hog', 'd2x.mn2', 'descent2.hog', 'descent2.ham')
        }
    }

    # Quartzon 3D exposes the eight A/B levels through a full-mission wrapper.
    if ($discId -eq 'descent-ii-destination-quartzon-3d-europe') {
        return @{
            type = 'd2_oem'
            mission = 'Descent 2: Counterstrike!'
            level1 = 'Ahayweh Gate'
            min_files = @(
                'descent2.hog',
                'descent2.ham',
                'descent2.s11',
                'descent2.s22',
                'groupa.pig',
                'water.pig'
            )
        }
    }

    # D2 OEM (Destination Quartzon) - has descent2.hog but disc id contains "quartzon"
    if ($discId -match 'quartzon') {
        return @{
            type = 'd2_oem'
            mission = 'D2 Destination:Quartzon'
            level1 = 'Ahayweh Gate'
            min_files = @(
                'descent2.hog',
                'descent2.ham',
                'descent2.s11',
                'descent2.s22',
                'groupa.pig',
                'water.pig'
            )
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
            mission = $null
            level1 = $null
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
                mission = $null
                level1 = $null
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
    $lastTestResult = Get-ExistingLastTestResult $specPath

    $dataTracksDir = Join-Path $dir.FullName 'data_tracks'
    $hashFile = Join-Path $dataTracksDir '.track_hashes.json'

    $discSource = Resolve-DiscExtractionSource -Directory $dir.FullName
    $discImageType = if ($discSource.Primary.Extension -ieq '.iso') { 'iso' } else { 'cue_bin' }
    $sourceIdentities = @($discSource.Files | ForEach-Object {
            Get-ExtractionPathIdentity -Path $_.FullName -Name $_.Name
        })
    $sourceFiles = @($sourceIdentities | ForEach-Object {
            @{ name = $_.name; sha256 = $_.sha256 }
        })
    $extractScriptIdentity = Get-ExtractionPathIdentity `
        -Path (Join-Path $gameDataDir 'extract_all_cds.ps1') -Name 'extract_all_cds.ps1'
    if (-not (Test-ExtractionCompletionSources -Directory $dataTracksDir `
            -ExpectedPolicy 'extract-all-cds-v1' -ExpectedSources $sourceIdentities `
            -RequiredTools @($extractScriptIdentity))) {
        throw "$($dir.FullName): data_tracks cache does not match the current source, extraction policy, or outputs"
    }

    # Look up disc in known_discs via track_hashes.json
    $discId = $null
    $game = $null
    $audioTracks = 0
    if (Test-Path $hashFile) {
        $tracks = Get-Content $hashFile -Raw | ConvertFrom-Json
        foreach ($track in $tracks) {
            $typeProperty = $track.PSObject.Properties['type']
            $sha1Property = $track.PSObject.Properties['sha1']
            if ($typeProperty -and $typeProperty.Value -eq 'data' -and $sha1Property -and $sha1Property.Value) {
                $dataTrackSha1 = [string]$sha1Property.Value
                if ($sha1ToDisc.ContainsKey($dataTrackSha1)) {
                    $disc = $sha1ToDisc[$dataTrackSha1]
                    $discId = $disc.id
                    $game = $disc.game
                    $audioTracks = @($disc.tracks | Where-Object { $_.type -eq 'audio' }).Count
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
        $extractedFiles = @(Get-ChildItem $dataTracksDir -Recurse -File |
            Where-Object { $_.Extension -match '\.(hog|pig|ham|s11|s22|mvl|dem|msn|mn2|sow|gog|inst)$' } |
            Select-Object -ExpandProperty Name | Sort-Object -Unique)
    }

    # Classify
    $classification = Get-DiscClassification $discId $game $extractedFiles
    $resolvedGame = Resolve-GameForSpec $game $classification
    $expectedFiles = @($classification.min_files | Sort-Object)
    if ($expectedFiles.Count -eq 0 -and $extractedFiles.Count -gt 0) {
        $expectedFiles = @($extractedFiles | Sort-Object)
    }

    # Build spec
    $spec = [ordered]@{
        source_type = 'cd'
        disc_image_type = $discImageType
        disc_id = $discId
        source_files = $sourceFiles
        game = $resolvedGame
        classification = $classification.type
        expected_mission = $classification.mission
        expected_level1 = $classification.level1
        expected_files = $expectedFiles
        audio_tracks = $audioTracks
        total_extracted = $extractedFiles.Count
    }
    if ($discImageType -eq 'iso') {
        $spec.import_mode = 'setup_iso'
    } elseif ($discImageType -eq 'cue_bin') {
        $spec.import_mode = 'setup_cd'
    }
    if ($null -ne $lastTestResult) {
        $spec.last_test_result = $lastTestResult
    }

    Write-CanonicalRegressionSpec `
        -path $specPath `
        -spec $spec `
        -sourceName $dir.Name `
        -generated (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
    $specCount++
    $status = if ($classification.type -eq 'unknown') { 'UNKNOWN' } elseif (-not $classification.mission) { 'NO-LAUNCH' } else { 'OK' }
    Write-Host "  $status $($dir.Name) -> $($classification.type)" -ForegroundColor $(if ($status -eq 'OK') { 'Green' } elseif ($status -eq 'NO-LAUNCH') { 'DarkYellow' } else { 'Red' })
}

# --- Process combined launch fixtures ---
$combinedLaunchDir = Join-Path $gameDataDir 'combined launches'
if (Test-Path -LiteralPath $combinedLaunchDir) {
    Write-Host ""
    Write-Host "=== Combined launches ===" -ForegroundColor Cyan

    foreach ($helperFile in (Get-ChildItem -LiteralPath $combinedLaunchDir -Recurse -Filter 'combined_launch.json5' -File | Sort-Object FullName)) {
        $dir = $helperFile.Directory
        $specPath = Join-Path $dir.FullName 'extract_regression.json5'
        if ((Test-Path -LiteralPath $specPath) -and -not $Force) {
            $skipped++
            continue
        }

        $helper = Read-Json5File $helperFile.FullName
        $sourceSpecs = @($helper.source_specs | Where-Object { $_ })
        $missionFiles = @($helper.mission_files | Where-Object { $_ } | ForEach-Object { $_.ToLowerInvariant() })
        if ($sourceSpecs.Count -lt 2) {
            throw "$($helperFile.FullName): source_specs must name at least two regression specs"
        }
        if (-not $helper.game -or -not $helper.classification -or
            -not $helper.expected_mission -or -not $helper.expected_level1) {
            throw "$($helperFile.FullName): game, classification, expected_mission, and expected_level1 are required"
        }

        $expectedFiles = @()
        foreach ($relativeSourceSpec in $sourceSpecs) {
            $sourceSpecPath = [System.IO.Path]::GetFullPath((Join-Path $dir.FullName $relativeSourceSpec))
            if (-not (Test-Path -LiteralPath $sourceSpecPath -PathType Leaf)) {
                throw "$($helperFile.FullName): source spec not found: $relativeSourceSpec"
            }
            $sourceSpec = Read-Json5File $sourceSpecPath
            if ($sourceSpec.game -ne $helper.game) {
                throw "$($helperFile.FullName): source spec game '$($sourceSpec.game)' does not match '$($helper.game)': $relativeSourceSpec"
            }
            $dataTracksDir = Join-Path (Split-Path $sourceSpecPath -Parent) 'data_tracks'
            if (-not (Test-Path -LiteralPath $dataTracksDir -PathType Container)) {
                throw "$($helperFile.FullName): source data_tracks directory not found: $relativeSourceSpec"
            }
            $expectedFiles += @($sourceSpec.expected_files | Where-Object { $_ })
        }
        $expectedFileSet = @{}
        foreach ($expectedFile in $expectedFiles) {
            $expectedFileSet[$expectedFile.ToLowerInvariant()] = $true
        }
        foreach ($missionFile in $missionFiles) {
            if (-not $expectedFileSet.ContainsKey($missionFile)) {
                throw "$($helperFile.FullName): mission file is not in a component expected_files oracle: $missionFile"
            }
        }
        $combinedExpectedFiles = @($expectedFiles | ForEach-Object {
                $name = $_.ToLowerInvariant()
                if ($missionFiles -contains $name) { "missions/$name" } else { $name }
            } | Sort-Object -Unique)

        $lastTestResult = Get-ExistingLastTestResult $specPath
        $spec = [ordered]@{
            source_type = 'combined'
            source_specs = $sourceSpecs
            game = $helper.game
            classification = $helper.classification
            expected_mission = $helper.expected_mission
            expected_level1 = $helper.expected_level1
            mission_selection_required = $true
            mission_files = $missionFiles
            expected_files = $combinedExpectedFiles
        }
        if ($null -ne $lastTestResult) {
            $spec.last_test_result = $lastTestResult
        }

        Write-CanonicalRegressionSpec `
            -path $specPath `
            -spec $spec `
            -sourceName $dir.Name `
            -generated (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
        $specCount++
        Write-Host "  OK $($dir.Name) -> $($helper.classification)" -ForegroundColor Green
    }
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
    if (-not (Test-Path -LiteralPath $installerPath)) {
        Write-Host "  SKIP $($gog.file): not found" -ForegroundColor Yellow
        continue
    }

    # Output spec goes next to the installer
    $specPath = Join-Path $gogDir "$([System.IO.Path]::GetFileNameWithoutExtension($gog.file))_regression.json5"
    if ((Test-Path $specPath) -and -not $Force) {
        $skipped++
        continue
    }
    $lastTestResult = Get-ExistingLastTestResult $specPath

    # Hash installer
    $hash = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash.ToLower()

    # Get extracted files from the pre-extracted directory
    $extractedDir = Join-Path (Join-Path $gogDir ([System.IO.Path]::GetFileNameWithoutExtension($gog.file))) 'extracted'
    $installerIdentity = Get-ExtractionPathIdentity -Path $installerPath -Name $gog.file
    $extractScriptIdentity = Get-ExtractionPathIdentity `
        -Path (Join-Path $gameDataDir 'extract_all_gog.ps1') -Name 'extract_all_gog.ps1'
    if (-not (Test-ExtractionCompletionSources -Directory $extractedDir `
            -ExpectedPolicy 'extract-all-gog-v1' -ExpectedSources @($installerIdentity) `
            -RequiredTools @($extractScriptIdentity))) {
        throw "${installerPath}: extracted cache does not match the current source, extraction policy, or outputs"
    }
    $extractedFiles = @()
    $extractedCount = 0
    if (Test-Path -LiteralPath $extractedDir) {
        $extractedFiles = @(Get-ChildItem $extractedDir -Recurse -File |
            Where-Object Name -ne '.extraction-complete.json' |
            Select-Object -ExpandProperty Name | Sort-Object)
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
    if ($null -ne $lastTestResult) {
        $spec.last_test_result = $lastTestResult
    }

    Write-CanonicalRegressionSpec `
        -path $specPath `
        -spec $spec `
        -sourceName $gog.file `
        -generated (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
    $specCount++
    Write-Host "  OK $($gog.file) -> $($gog.type)" -ForegroundColor Green
}

Write-Host ""
Write-Host "Generated $specCount specs, skipped $skipped (use -Force to overwrite)" -ForegroundColor Cyan

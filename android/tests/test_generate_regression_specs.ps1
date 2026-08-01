#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$tempRoot = Join-Path $repoRoot 'android\temp\generate_regression_specs_test'

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

if (Test-Path -LiteralPath $tempRoot) {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}

try {
    $gameDataDir = Join-Path $tempRoot 'game_data'
    $discDir = Join-Path $gameDataDir 'CD images\iso-with-fingerprint-cue'
    $vertigoDir = Join-Path $gameDataDir 'CD images\vertigo-expansion-only'
    $baseD2Dir = Join-Path $gameDataDir 'CD images\d2-base'
    $testFlightDir = Join-Path $gameDataDir 'CD images\test-flight'
    $combinedDir = Join-Path $gameDataDir 'combined launches\d2-plus-vertigo'
    $testsDir = Join-Path $tempRoot 'android\tests'
    $helpersDir = Join-Path $tempRoot 'android\helpers'
    $assetsDir = Join-Path $tempRoot 'android\app\src\main\assets'
    New-Item -ItemType Directory -Path $discDir, $vertigoDir, $baseD2Dir, $testFlightDir, $combinedDir, $testsDir, $helpersDir, $assetsDir -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $vertigoDir 'data_tracks'), `
    (Join-Path $baseD2Dir 'data_tracks'), `
    (Join-Path $testFlightDir 'data_tracks'), `
    (Join-Path $discDir 'data_tracks') | Out-Null

    Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data\generate_regression_specs.ps1') `
        -Destination (Join-Path $gameDataDir 'generate_regression_specs.ps1')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'android\tests\extract_regression_spec_helpers.ps1') `
        -Destination (Join-Path $testsDir 'extract_regression_spec_helpers.ps1')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'android\helpers\json5.ps1') `
        -Destination (Join-Path $helpersDir 'json5.ps1')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'android\helpers\bounded_extraction.ps1') `
        -Destination (Join-Path $helpersDir 'bounded_extraction.ps1')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data\extract_all_cds.ps1') `
        -Destination (Join-Path $gameDataDir 'extract_all_cds.ps1')
    [System.IO.File]::WriteAllText(
        (Join-Path $assetsDir 'known_discs.json5'),
        '{"discs":[{"id":"descent-test-flight","game":"d1","tracks":' +
        '[{"track":1,"type":"data","sha1":"test-flight-sha1"}]}]}',
        [System.Text.UTF8Encoding]::new($false)
    )
    [System.IO.File]::WriteAllText(
        (Join-Path $discDir 'disc.iso'),
        'fixture',
        [System.Text.UTF8Encoding]::new($false)
    )
    [System.IO.File]::WriteAllText(
        (Join-Path $discDir 'disc.cue'),
        "FILE `"disc.iso`" BINARY`n  TRACK 01 MODE1/2048`n    INDEX 01 00:00:00`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    foreach ($fixtureDir in @($vertigoDir, $baseD2Dir, $testFlightDir)) {
        [System.IO.File]::WriteAllText(
            (Join-Path $fixtureDir 'disc.cue'),
            "FILE `"disc.bin`" BINARY`n  TRACK 01 MODE1/2352`n    INDEX 01 00:00:00`n",
            [System.Text.UTF8Encoding]::new($false)
        )
        [System.IO.File]::WriteAllText(
            (Join-Path $fixtureDir 'disc.bin'),
            'disc payload',
            [System.Text.UTF8Encoding]::new($false)
        )
    }
    foreach ($name in @('d2x.hog', 'd2x.mn2', 'descent2.hog', 'descent2.ham')) {
        [System.IO.File]::WriteAllText(
            (Join-Path (Join-Path $vertigoDir 'data_tracks') $name),
            'fixture',
            [System.Text.UTF8Encoding]::new($false)
        )
    }
    foreach ($name in @('descent2.hog', 'descent2.ham', 'descent2.s11', 'descent2.s22', 'groupa.pig')) {
        [System.IO.File]::WriteAllText(
            (Join-Path (Join-Path $baseD2Dir 'data_tracks') $name),
            'fixture',
            [System.Text.UTF8Encoding]::new($false)
        )
    }
    foreach ($name in @('descent.hog', 'descent.pig')) {
        [System.IO.File]::WriteAllText(
            (Join-Path (Join-Path $testFlightDir 'data_tracks') $name),
            'fixture',
            [System.Text.UTF8Encoding]::new($false)
        )
    }
    [System.IO.File]::WriteAllText(
        (Join-Path $discDir 'data_tracks\fixture.hog'),
        'fixture',
        [System.Text.UTF8Encoding]::new($false)
    )
    [System.IO.File]::WriteAllText(
        (Join-Path $testFlightDir 'data_tracks\.track_hashes.json'),
        '[{"track":1,"type":"data","sha1":"test-flight-sha1"},' +
        '{"sow":"descent1.sow","files_extracted":4}]',
        [System.Text.UTF8Encoding]::new($false)
    )
    [System.IO.File]::WriteAllText(
        (Join-Path $combinedDir 'combined_launch.json5'),
        '{"source_specs":["../../CD images/d2-base/extract_regression.json5",' +
        '"../../CD images/vertigo-expansion-only/extract_regression.json5"],' +
        '"game":"d2","classification":"d2_vertigo_combined",' +
        '"expected_mission":"Descent 2: Vertigo","expected_level1":"deep kraeg tunnel system",' +
        '"mission_files":["d2x.hog","d2x.mn2"]}',
        [System.Text.UTF8Encoding]::new($false)
    )

    . (Join-Path $helpersDir 'bounded_extraction.ps1')
    $extractScriptIdentity = Get-ExtractionPathIdentity `
        -Path (Join-Path $gameDataDir 'extract_all_cds.ps1') -Name 'extract_all_cds.ps1'
    foreach ($fixtureDir in @($discDir, $vertigoDir, $baseD2Dir, $testFlightDir)) {
        $source = Resolve-DiscExtractionSource -Directory $fixtureDir
        $sourceIdentities = @($source.Files | ForEach-Object {
                Get-ExtractionPathIdentity -Path $_.FullName -Name $_.Name
            })
        $provenance = New-ExtractionProvenance -Policy 'extract-all-cds-v1' `
            -Sources $sourceIdentities -Tools @($extractScriptIdentity)
        Write-ExtractionCompletionManifest -Directory (Join-Path $fixtureDir 'data_tracks') `
            -Provenance $provenance
    }

    $powerShellPath = (Get-Process -Id $PID).Path
    $scriptPath = Join-Path $gameDataDir 'generate_regression_specs.ps1'
    $output = @(& $powerShellPath -NoProfile -NonInteractive -File $scriptPath -Force 2>&1)
    Assert-True ($LASTEXITCODE -eq 0) "Regression spec generation failed: $($output -join "`n")"

    . (Join-Path $testsDir 'extract_regression_spec_helpers.ps1')
    $spec = Read-Json5File (Join-Path $discDir 'extract_regression.json5')
    Assert-True ($spec.disc_image_type -eq 'iso') `
        'Regression spec generation should use an ISO referenced by its fingerprint CUE'
    Assert-True ($spec.import_mode -eq 'setup_iso') `
        'An ISO with a matching fingerprint CUE should retain setup_iso import mode'
    Assert-True (@($spec.source_files).Count -eq 2 -and
        @($spec.source_files.name) -contains 'disc.cue' -and
        @($spec.source_files.name) -contains 'disc.iso') `
        'An ISO fingerprint CUE source set should bind both descriptor files'

    $vertigoSpec = Read-Json5File (Join-Path $vertigoDir 'extract_regression.json5')
    Assert-True ($vertigoSpec.classification -eq 'd2_vertigo' -and
        $null -eq $vertigoSpec.expected_mission -and $null -eq $vertigoSpec.expected_level1) `
        'A Vertigo-only disc should be a non-launchable file-only regression'

    $testFlightSpec = Read-Json5File (Join-Path $testFlightDir 'extract_regression.json5')
    Assert-True ($testFlightSpec.classification -eq 'd1_demo' -and
        $null -eq $testFlightSpec.expected_mission -and $null -eq $testFlightSpec.expected_level1) `
        'The unsupported Test Flight demo should be a non-launchable file-only regression'

    $combinedSpec = Read-Json5File (Join-Path $combinedDir 'extract_regression.json5')
    Assert-True ($combinedSpec.source_type -eq 'combined' -and
        @($combinedSpec.source_specs).Count -eq 2 -and
        $combinedSpec.expected_mission -eq 'Descent 2: Vertigo' -and
        $combinedSpec.expected_level1 -eq 'deep kraeg tunnel system' -and
        $combinedSpec.mission_selection_required -eq $true) `
        'A combined launch helper should generate a launchable composite regression'
    Assert-True (@($combinedSpec.expected_files).Count -eq 7 -and
        @($combinedSpec.expected_files) -contains 'missions/d2x.hog' -and
        @($combinedSpec.expected_files) -contains 'groupa.pig') `
        'A combined regression should merge and deduplicate component extraction oracles'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'Regression spec generation tests passed' -ForegroundColor Green
exit 0

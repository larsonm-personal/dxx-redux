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
    $testFlightDir = Join-Path $gameDataDir 'CD images\test-flight'
    $testsDir = Join-Path $tempRoot 'android\tests'
    $assetsDir = Join-Path $tempRoot 'android\app\src\main\assets'
    New-Item -ItemType Directory -Path $discDir, $vertigoDir, $testFlightDir, $testsDir, $assetsDir -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $vertigoDir 'data_tracks'), `
    (Join-Path $testFlightDir 'data_tracks') | Out-Null

    Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data\generate_regression_specs.ps1') `
        -Destination (Join-Path $gameDataDir 'generate_regression_specs.ps1')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'android\tests\extract_regression_spec_helpers.ps1') `
        -Destination (Join-Path $testsDir 'extract_regression_spec_helpers.ps1')
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
    foreach ($fixtureDir in @($vertigoDir, $testFlightDir)) {
        [System.IO.File]::WriteAllText(
            (Join-Path $fixtureDir 'disc.cue'),
            "FILE `"disc.bin`" BINARY`n  TRACK 01 MODE1/2352`n    INDEX 01 00:00:00`n",
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
    foreach ($name in @('descent.hog', 'descent.pig')) {
        [System.IO.File]::WriteAllText(
            (Join-Path (Join-Path $testFlightDir 'data_tracks') $name),
            'fixture',
            [System.Text.UTF8Encoding]::new($false)
        )
    }
    [System.IO.File]::WriteAllText(
        (Join-Path $testFlightDir 'track_hashes.json'),
        '[{"track":1,"type":"data","sha1":"test-flight-sha1"}]',
        [System.Text.UTF8Encoding]::new($false)
    )

    $powerShellPath = (Get-Process -Id $PID).Path
    $scriptPath = Join-Path $gameDataDir 'generate_regression_specs.ps1'
    $output = @(& $powerShellPath -NoProfile -NonInteractive -File $scriptPath -Force 2>&1)
    Assert-True ($LASTEXITCODE -eq 0) "Regression spec generation failed: $($output -join "`n")"

    . (Join-Path $testsDir 'extract_regression_spec_helpers.ps1')
    $spec = Read-Json5File (Join-Path $discDir 'extract_regression.json5')
    Assert-True ($spec.disc_image_type -eq 'iso') `
        'Regression spec generation should prefer ISO when a fingerprint cue is also present'
    Assert-True ($spec.import_mode -eq 'setup_iso') `
        'An ISO with a fingerprint cue should retain setup_iso import mode'
    Assert-True ($spec.source_files.name -eq 'disc.iso') `
        'An ISO with a fingerprint cue should hash only the ISO regression source'

    $vertigoSpec = Read-Json5File (Join-Path $vertigoDir 'extract_regression.json5')
    Assert-True ($vertigoSpec.classification -eq 'd2_vertigo' -and
        $null -eq $vertigoSpec.expected_mission -and $null -eq $vertigoSpec.expected_level1) `
        'A Vertigo-only disc should be a non-launchable file-only regression'

    $testFlightSpec = Read-Json5File (Join-Path $testFlightDir 'extract_regression.json5')
    Assert-True ($testFlightSpec.classification -eq 'd1_demo' -and
        $null -eq $testFlightSpec.expected_mission -and $null -eq $testFlightSpec.expected_level1) `
        'The unsupported Test Flight demo should be a non-launchable file-only regression'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'Regression spec generation tests passed' -ForegroundColor Green
exit 0

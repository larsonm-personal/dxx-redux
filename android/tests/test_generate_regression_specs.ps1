#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$tempRoot = Join-Path $repoRoot ('android/temp/test_generate_regression_specs/run_' + [guid]::NewGuid().ToString('N'))

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $tempRoot -DirectoryPrefix run_
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
$producerLock = [IO.File]::Open((Join-Path $tempRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)

try {
    $gameDataDir = Join-Path $tempRoot 'game_data'
    $discDir = Join-Path $gameDataDir 'CD images\iso with fingerprint cue'
    $vertigoDir = Join-Path $gameDataDir 'CD images\vertigo-expansion-only'
    $baseD2Dir = Join-Path $gameDataDir 'CD images\d2-base'
    $previewDir = Join-Path $gameDataDir 'CD images/d2-preview'
    $testFlightDir = Join-Path $gameDataDir 'CD images\test-flight'
    $combinedDir = Join-Path $gameDataDir 'combined launches\d2-plus-vertigo'
    $testsDir = Join-Path $tempRoot 'android\tests'
    $helpersDir = Join-Path $tempRoot 'android\helpers'
    $assetsDir = Join-Path $tempRoot 'android\app\src\main\assets'
    New-Item -ItemType Directory -Path $discDir, $vertigoDir, $baseD2Dir, $testFlightDir, $previewDir, $combinedDir, $testsDir, $helpersDir, $assetsDir -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $vertigoDir 'data_tracks'), `
    (Join-Path $baseD2Dir 'data_tracks'), `
    (Join-Path $testFlightDir 'data_tracks'),
    (Join-Path $previewDir 'data_tracks'), `
    (Join-Path $discDir 'data_tracks') | Out-Null

    Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data\generate_regression_specs.ps1') `
        -Destination (Join-Path $gameDataDir 'generate_regression_specs.ps1')
    # Use the real writer and pinned formatter while keeping source data isolated
    $specHelper = (Join-Path $repoRoot 'android/tests/extract_regression_spec_helpers.ps1').Replace("'", "''")
    [IO.File]::WriteAllText((Join-Path $testsDir 'extract_regression_spec_helpers.ps1'), ". '$specHelper'", [Text.UTF8Encoding]::new($false))
    Copy-Item -LiteralPath (Join-Path $repoRoot 'android\helpers\jsonc.ps1') `
        -Destination (Join-Path $helpersDir 'jsonc.ps1')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'android\helpers\bounded_extraction.ps1') `
        -Destination (Join-Path $helpersDir 'bounded_extraction.ps1')
    $depHelpersDir = Join-Path $tempRoot 'android/get_deps/helpers'
    New-Item -ItemType Directory -Path $depHelpersDir -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot 'android/get_deps/helpers/Get-DepPlatform.ps1') -Destination $depHelpersDir
    Copy-Item -LiteralPath (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1') -Destination $helpersDir
    Copy-Item -LiteralPath (Join-Path $repoRoot 'android/helpers/powershell_compat.ps1') -Destination $helpersDir
    Copy-Item -LiteralPath (Join-Path $repoRoot 'android\helpers\normalized_json_text.ps1') `
        -Destination (Join-Path $helpersDir 'normalized_json_text.ps1')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'android\helpers\normalize_json.py') `
        -Destination (Join-Path $helpersDir 'normalize_json.py')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data\extract_all_cds.ps1') `
        -Destination (Join-Path $gameDataDir 'extract_all_cds.ps1')
    [System.IO.File]::WriteAllText(
        (Join-Path $assetsDir 'known_discs.jsonc'),
        '{"discs":[{"id":"descent-test-flight","label":"test-flight","game":"d1","tracks":' +
        '[{"track":1,"type":"data","sha1":"test-flight-sha1"},{"track":2,"type":"audio","sha1":"audio-sha1"}]},' +
        '{"id":"same-tracks-other-release","label":"other","game":"d1","tracks":' +
        '[{"track":1,"type":"data","sha1":"test-flight-sha1"},{"track":2,"type":"audio","sha1":"audio-sha1"}]},' +
        '{"id":"same-data-other-audio","game":"d1","tracks":' +
        '[{"track":1,"type":"data","sha1":"test-flight-sha1"},{"track":2,"type":"audio","sha1":"different-audio"}]}]}',
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
    foreach ($fixtureDir in @($vertigoDir, $baseD2Dir, $testFlightDir, $previewDir)) {
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
    foreach ($name in @('d2demo.hog', 'd2demo.ham', 'd2demo.pig')) {
        [IO.File]::WriteAllText((Join-Path (Join-Path $previewDir 'data_tracks') $name), 'fixture')
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
    foreach ($name in @('deepfrst.hog', 'deep-pit.msn', 'tartarus.msn', 't-zone.msn')) {
        [System.IO.File]::WriteAllText(
            (Join-Path (Join-Path $discDir 'data_tracks') $name),
            'fixture',
            [System.Text.UTF8Encoding]::new($false)
        )
    }
    [System.IO.File]::WriteAllText(
        (Join-Path $testFlightDir 'data_tracks\.track_hashes.json'),
        '[{"track":1,"type":"data","sha1":"test-flight-sha1"},' +
        '{"track":2,"type":"audio","sha1":"audio-sha1"},' +
        '{"sow":"descent1.sow","files_extracted":4}]',
        [System.Text.UTF8Encoding]::new($false)
    )
    [System.IO.File]::WriteAllText(
        (Join-Path $combinedDir 'combined_launch.jsonc'),
        '{"source_specs":["../../CD images/d2-base/extract_regression.jsonc",' +
        '"../../CD images/vertigo-expansion-only/extract_regression.jsonc"],' +
        '"game":"d2","classification":"d2_vertigo_combined",' +
        '"expected_mission":"Descent 2: Vertigo","expected_level1":"deep kraeg tunnel system",' +
        '"mission_files":["d2x.hog","d2x.mn2"]}',
        [System.Text.UTF8Encoding]::new($false)
    )

    . (Join-Path $helpersDir 'bounded_extraction.ps1')
    $extractScriptIdentity = Get-ExtractionPathIdentity `
        -Path (Join-Path $gameDataDir 'extract_all_cds.ps1') -Name 'extract_all_cds.ps1'
    foreach ($fixtureDir in @($discDir, $vertigoDir, $baseD2Dir, $testFlightDir, $previewDir)) {
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
    $spec = Read-JsoncFile (Join-Path $discDir 'extract_regression.jsonc')
    Assert-True ($spec.disc_image_type -eq 'iso') `
        'Regression spec generation should use an ISO referenced by its fingerprint CUE'
    Assert-True ($spec.import_mode -eq 'setup_iso') `
        'An ISO with a matching fingerprint CUE should retain setup_iso import mode'
    Assert-True ((@($spec.expected_files) -join ',') -eq `
            'deep-pit.msn,deepfrst.hog,fixture.hog,t-zone.msn,tartarus.msn') `
        'Expected filenames should use ordinal ordering independent of runtime culture'
    Assert-True (@($spec.source_files).Count -eq 2 -and
        @($spec.source_files.name) -contains 'disc.cue' -and
        @($spec.source_files.name) -contains 'disc.iso') `
        'An ISO fingerprint CUE source set should bind both descriptor files'

    $vertigoSpec = Read-JsoncFile (Join-Path $vertigoDir 'extract_regression.jsonc')
    Assert-True ($vertigoSpec.classification -eq 'd2_vertigo' -and
        $null -eq $vertigoSpec.expected_mission -and $null -eq $vertigoSpec.expected_level1) `
        'A Vertigo-only disc should be a non-launchable file-only regression'

    $testFlightSpec = Read-JsoncFile (Join-Path $testFlightDir 'extract_regression.jsonc')
    Assert-True ($testFlightSpec.disc_id -ceq 'descent-test-flight' -and $testFlightSpec.audio_tracks -eq 1) `
        'Disc lookup should compare audio tracks and disambiguate identical releases by source label'
    Assert-True ($testFlightSpec.classification -eq 'd1_demo' -and
        $null -eq $testFlightSpec.expected_mission -and $null -eq $testFlightSpec.expected_level1) `
        'The unsupported Test Flight demo should be a non-launchable file-only regression'

    $previewSpec = Read-JsoncFile (Join-Path $previewDir 'extract_regression.jsonc')
    Assert-True ($previewSpec.classification -eq 'd2_demo' -and $previewSpec.game -eq 'd2' -and
        $previewSpec.expected_mission -eq 'Descent 2 Demo' -and $previewSpec.expected_level1 -eq 'Ahayweh Gate') `
        'D2 Preview must generate a launchable demo spec with the name embedded in d2leva-1.sl2'

    $combinedSpec = Read-JsoncFile (Join-Path $combinedDir 'extract_regression.jsonc')
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

    $specPaths = @($discDir, $vertigoDir, $baseD2Dir, $testFlightDir, $previewDir, $combinedDir) |
        ForEach-Object { Join-Path $_ 'extract_regression.jsonc' }
    function Get-SpecSnapshots {
        $snapshots = @{}
        foreach ($path in $specPaths) {
            $snapshots[$path] = @((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash,
                [IO.File]::GetLastWriteTimeUtc($path))
        }
        return $snapshots
    }
    function Assert-SpecSnapshots($Before, $Except = '') {
        $after = Get-SpecSnapshots
        foreach ($path in $Before.Keys) {
            if ($path -eq $Except) { continue }
            Assert-True ($Before[$path][0] -ceq $after[$path][0] -and $Before[$path][1] -eq $after[$path][1]) `
                "Unselected or rejected spec changed bytes or modification time: $path"
        }
    }
    function Invoke-SelectedGenerator([string]$Selector, [switch]$Force) {
        $arguments = @('-NoProfile', '-NonInteractive', '-File', $scriptPath, '-SpecListPath', $Selector)
        if ($Force) { $arguments += '-Force' }
        $savedPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $output = @(& $powerShellPath @arguments 2>&1)
            return @{ Exit = $LASTEXITCODE; Text = $output -join "`n" }
        } finally { $ErrorActionPreference = $savedPreference }
    }

    $discSpecPath = Join-Path $discDir 'extract_regression.jsonc'
    $combinedSpecPath = Join-Path $combinedDir 'extract_regression.jsonc'
    $selectionPath = Join-Path $tempRoot 'selection.txt'
    $unknownPath = Join-Path $tempRoot 'unknown/extract_regression.jsonc'
    foreach ($contents in @('', '   ', "$discSpecPath`n$unknownPath", "$unknownPath`n$discSpecPath",
            $unknownPath, "$discSpecPath`n$discSpecPath", "$discSpecPath`n`n",
            ($discSpecPath + "`n" + (Join-Path $discDir '../iso with fingerprint cue/extract_regression.jsonc')),
            (Join-Path $gameDataDir 'gog installers/setup_descent_2_1.1_(16596)_regression.jsonc'))) {
        [IO.File]::WriteAllText($selectionPath, $contents, [Text.UTF8Encoding]::new($false))
        $before = Get-SpecSnapshots
        $result = Invoke-SelectedGenerator -Selector $selectionPath -Force
        Assert-True ($result.Exit -ne 0) "Invalid selection should fail before publication: $($result.Text)"
        Assert-SpecSnapshots $before
    }
    foreach ($selector in @('', '   ', (Join-Path $tempRoot 'missing-list.txt'))) {
        $before = Get-SpecSnapshots
        $result = Invoke-SelectedGenerator -Selector $selector -Force
        Assert-True ($result.Exit -ne 0) 'Blank or missing selection-list paths should fail'
        Assert-SpecSnapshots $before
    }
    [IO.File]::WriteAllText($selectionPath, (Join-Path $discDir './extract_regression.jsonc'))
    $before = Get-SpecSnapshots
    $result = Invoke-SelectedGenerator -Selector $selectionPath
    Assert-True ($result.Exit -eq 0 -and $result.Text -match 'Generated 0 specs, skipped 1') `
        'A normalized selected existing spec should count exactly one valid skip'
    Assert-SpecSnapshots $before

    $validSelectedText = [IO.File]::ReadAllText($discSpecPath)
    $validSelectedTime = [IO.File]::GetLastWriteTimeUtc($discSpecPath)
    foreach ($invalidSelectedText in @('{broken', '{"source_type":"cd","expected_files":"not an array"}')) {
        try {
            [IO.File]::WriteAllText($discSpecPath, $invalidSelectedText)
            $before = Get-SpecSnapshots
            $result = Invoke-SelectedGenerator -Selector $selectionPath
            Assert-True ($result.Exit -ne 0) 'An invalid selected existing spec must not count as a valid skip'
            Assert-SpecSnapshots $before
        } finally {
            [IO.File]::WriteAllText($discSpecPath, $validSelectedText, [Text.UTF8Encoding]::new($false))
            [IO.File]::SetLastWriteTimeUtc($discSpecPath, $validSelectedTime)
        }
    }

    $stale = Read-JsoncFile $discSpecPath
    $stale.expected_files = @('stale.hog')
    [IO.File]::WriteAllText($discSpecPath, ($stale | ConvertTo-Json -Depth 20))
    $before = Get-SpecSnapshots
    $result = Invoke-SelectedGenerator -Selector $selectionPath -Force
    Assert-True ($result.Exit -eq 0 -and $result.Text -match 'Generated 1 specs, skipped 0' -and
        (Read-JsoncFile $discSpecPath).expected_files -notcontains 'stale.hog') `
        'Selected Force should regenerate exactly the requested spec'
    Assert-SpecSnapshots $before -Except $discSpecPath
    [IO.File]::WriteAllText($selectionPath, "$discSpecPath`n$combinedSpecPath")
    $before = Get-SpecSnapshots
    $result = Invoke-SelectedGenerator -Selector $selectionPath -Force
    Assert-True ($result.Exit -eq 0 -and $result.Text -match 'Generated 2 specs, skipped 0') `
        'A multi-source selection should account exactly its two requested specs'
    Assert-SpecSnapshots $before

    $unavailableDir = Join-Path $gameDataDir 'CD images/unavailable source'
    New-Item -ItemType Directory -Path $unavailableDir | Out-Null
    $stale = Read-JsoncFile $discSpecPath
    $stale.expected_files = @('must-not-publish.hog')
    [IO.File]::WriteAllText($discSpecPath, ($stale | ConvertTo-Json -Depth 20))
    [IO.File]::WriteAllText($selectionPath, ($discSpecPath + "`n" + (Join-Path $unavailableDir 'extract_regression.jsonc')))
    $before = Get-SpecSnapshots
    $result = Invoke-SelectedGenerator -Selector $selectionPath -Force
    Assert-True ($result.Exit -ne 0) 'An unavailable source mixed with a valid request should fail'
    Assert-SpecSnapshots $before

    # Tiny pre-extracted installer fixtures exercise every supported GOG record
    Remove-Item -LiteralPath $unavailableDir
    $gogDir = Join-Path $gameDataDir 'gog installers'
    $gogTool = Join-Path $gameDataDir 'extract_all_gog.ps1'
    [IO.File]::WriteAllText($gogTool, '# fixture extraction identity')
    $gogToolIdentity = Get-ExtractionPathIdentity -Path $gogTool -Name 'extract_all_gog.ps1'
    $gogFixtures = @(
        @{ File = 'setup_descent_1.4a_(16596).exe'; Game = 'd1'; Files = @('DESCENT.HOG', 'DESCENT.PIG') },
        @{ File = 'setup_descent_2_1.1_(16596).exe'; Game = 'd2'; Files = @('DESCENT2.HOG', 'DESCENT2.HAM', 'DESCENT2.S11', 'DESCENT2.S22', 'GROUPA.PIG') },
        @{ File = 'descent_enUS_1_0_35122.pkg'; Game = 'd1'; Files = @('DESCENT.HOG', 'DESCENT.PIG') },
        @{ File = 'descent_2_enUS_1_0_51877.pkg'; Game = 'd2'; Files = @('DESCENT2.HOG', 'DESCENT2.HAM', 'DESCENT2.S11', 'DESCENT2.S22', 'GROUPA.PIG') }
    )
    $gogPaths = @()
    foreach ($fixture in $gogFixtures) {
        $base = [IO.Path]::GetFileNameWithoutExtension($fixture.File)
        $extracted = Join-Path $gogDir "$base/extracted"
        New-Item -ItemType Directory -Path $extracted -Force | Out-Null
        $installer = Join-Path $gogDir $fixture.File
        [IO.File]::WriteAllText($installer, 'installer identity only')
        foreach ($file in $fixture.Files) { [IO.File]::WriteAllText((Join-Path $extracted $file), 'fixture') }
        $provenance = New-ExtractionProvenance -Policy 'extract-all-gog-v1' `
            -Sources @((Get-ExtractionPathIdentity -Path $installer -Name $fixture.File)) -Tools @($gogToolIdentity)
        Write-ExtractionCompletionManifest -Directory $extracted -Provenance $provenance
        $gogPaths += Join-Path $gogDir "${base}_regression.jsonc"
    }
    [IO.File]::WriteAllText($selectionPath, ($gogPaths -join "`n"))
    $before = Get-SpecSnapshots
    $result = Invoke-SelectedGenerator -Selector $selectionPath -Force
    Assert-True ($result.Exit -eq 0 -and $result.Text -match 'Generated 4 specs, skipped 0') `
        'All four selected GOG records must be generated without touching CD/combined outputs'
    Assert-SpecSnapshots $before
    for ($i = 0; $i -lt $gogPaths.Count; $i++) {
        $gogSpec = Read-JsoncFile $gogPaths[$i]
        Assert-True ($gogSpec.source_type -eq 'gog' -and $gogSpec.game -eq $gogFixtures[$i].Game -and
            (@($gogSpec.expected_files) -join ',') -ceq (@(Get-OrdinalSortedUniqueStrings $gogFixtures[$i].Files) -join ',') -and
            @($gogSpec.source_files).Count -eq 1 -and $gogSpec.source_files[0].name -ceq $gogFixtures[$i].File) `
            'GOG output must retain the existing source identity and extraction oracle'
    }
    $specPaths += $gogPaths
    $before = Get-SpecSnapshots
    $result = Invoke-SelectedGenerator -Selector $selectionPath
    Assert-True ($result.Exit -eq 0 -and $result.Text -match 'Generated 0 specs, skipped 4') `
        'Selected existing GOG specs must be accounted as valid skips'
    Assert-SpecSnapshots $before
    $output = @(& $powerShellPath -NoProfile -NonInteractive -File $scriptPath -Force 2>&1)
    Assert-True ($LASTEXITCODE -eq 0 -and ($output -join "`n") -match 'Generated 10 specs, skipped 0') `
        'Absent selector must retain ordinary CD, combined and GOG generation'
    Assert-SpecSnapshots $before -Except $discSpecPath

    $testFlightPath = Join-Path $testFlightDir 'extract_regression.jsonc'
    $beforeLostIdentity = [IO.File]::ReadAllText($testFlightPath)
    [IO.File]::WriteAllText((Join-Path $assetsDir 'known_discs.jsonc'), '{"discs":[]}')
    $specList = Join-Path $tempRoot 'selected-spec.txt'
    [IO.File]::WriteAllText($specList, $testFlightPath)
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $output = @(& $powerShellPath -NoProfile -NonInteractive -File $scriptPath -Force -SpecListPath $specList 2>&1)
    $failedExit = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    Assert-True ($failedExit -ne 0 -and ($output -join "`n") -match 'known disc identity was lost') `
        'Regeneration must reject a lost known disc identity'
    Assert-True ([IO.File]::ReadAllText($testFlightPath) -ceq $beforeLostIdentity) `
        'A missing catalog identity must not downgrade the existing oracle'
} finally {
    $producerLock.Dispose()
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'Regression spec generation tests passed' -ForegroundColor Green
exit 0

#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$testRoot = Join-Path $repoRoot ('android/temp/fingerprint_manifest_publication/run_' + [guid]::NewGuid().ToString('N'))
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
$workflow = Join-Path $repoRoot 'game_data\fingerprint_disc_tracks.ps1'
$missionWorkflow = Join-Path $repoRoot 'game_data\fingerprint_mission_zip_music.ps1'
$powershellPath = (Get-Process -Id $PID).Path
. (Join-Path $repoRoot 'android\helpers\jsonc.ps1')
. (Join-Path $repoRoot 'android\helpers\atomic_text_file.ps1')
. (Join-Path $repoRoot 'android\helpers\powershell_compat.ps1')
. (Join-Path $repoRoot 'android\helpers\normalized_json_text.ps1')
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Invoke-Workflow {
    param([string]$Mode, [switch]$Force)

    $env:DXX_FINGERPRINT_MANIFEST_TEST_MODE = $Mode
    $arguments = @(
        '-NoProfile', '-File', $workflow,
        '-SkipBuild', '-SkipAcoustId',
        '-CdImageDir', $cdRoot,
        '-FingerprintExePath', $fakeCli
    )
    if ($Force) { $arguments += '-Force' }
    & $powershellPath @arguments | Out-Null
    return $LASTEXITCODE
}

function Invoke-MissionWorkflow {
    param([string]$Mode, [switch]$Force)

    $env:DXX_FINGERPRINT_MANIFEST_TEST_MODE = $Mode
    $arguments = @('-NoProfile', '-File', $missionWorkflow,
        '-SkipBuild', '-SkipAcoustId', '-MissionDir', $missionRoot,
        '-OutputRoot', $missionOutput, '-FingerprintExePath', $fakeAudio)
    if ($Force) { $arguments += '-Force' }
    $output = & $powershellPath @arguments 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0 -and $Mode -ne 'mission_partial') {
        $output | ForEach-Object { Write-Host $_ }
    }
    return $exitCode
}

function New-FingerprintFixtureLauncher {
    param([string]$Path, [string]$ScriptPath)
    if (Test-RegressionWindowsHost) {
        $scriptName = [IO.Path]::GetFileName($ScriptPath)
        $command = "@`"$powershellPath`" -NoProfile -File `"%~dp0$scriptName`" %*`r`n"
        [IO.File]::WriteAllText($Path, $command, [Text.ASCIIEncoding]::new())
    } else {
        # Single-quote executable and script paths; forward each argument unchanged
        $quotedPowerShell = "'" + $powershellPath.Replace("'", "'`"'`"'") + "'"
        $quotedScript = "'" + $ScriptPath.Replace("'", "'`"'`"'") + "'"
        $command = "#!/bin/sh`nexec $quotedPowerShell -NoProfile -File $quotedScript " + '"$@"' + "`n"
        [IO.File]::WriteAllText($Path, $command, [Text.UTF8Encoding]::new($false))
        & chmod +x -- $Path
        if ($LASTEXITCODE -ne 0) { throw 'Could not make fixture launcher executable' }
    }
}
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $testRoot -DirectoryPrefix run_
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$producerLock = [IO.File]::Open((Join-Path $testRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)

try {
    $atomicPath = Join-Path $testRoot 'atomic.txt'
    Write-Utf8NoBomTextAtomically -Path $atomicPath -Text 'first'
    Write-Utf8NoBomTextAtomically -Path $atomicPath -Text 'replacement'
    Assert-True ([IO.File]::ReadAllText($atomicPath) -ceq 'replacement') `
        'Atomic text publication should replace an existing file'
    Assert-True (@(Get-ChildItem -LiteralPath $testRoot -File -Force | Where-Object {
                $_.Name -match '^\.atomic\.txt\..*\.(tmp|bak)$'
            }).Count -eq 0) 'Atomic text publication should clean temporary and backup files'

    $cdRoot = Join-Path $testRoot 'CD images'
    $discDir = Join-Path $cdRoot 'fixture'
    $singleDir = Join-Path $cdRoot 'single disc'
    $fakeCli = Join-Path $testRoot $(if (Test-RegressionWindowsHost) { 'fake_fingerprint_cd.cmd' } else { 'fake_fingerprint_cd' })
    $fakeInner = Join-Path $testRoot 'fake_fingerprint_cd.ps1'
    $invocationMarker = Join-Path $testRoot 'invoked.txt'
    $manifest = Join-Path $discDir 'track_fingerprints.json'
    New-Item -ItemType Directory -Path $discDir, $singleDir | Out-Null
    [IO.File]::WriteAllText(
        (Join-Path $discDir 'fixture.cue'),
        "FILE `"fixture.bin`" BINARY`n  TRACK 01 MODE1/2352`n  TRACK 02 AUDIO`n",
        [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText(
        (Join-Path $singleDir 'single.cue'),
        "FILE `"single.bin`" BINARY`n  TRACK 01 MODE1/2352`n",
        [Text.UTF8Encoding]::new($false))

    $fakeBody = @'
param([string]$CuePath)
$sha1 = '1' * 40
$mode = $env:DXX_FINGERPRINT_MANIFEST_TEST_MODE
if ((Split-Path -Leaf $CuePath) -eq 'single.cue') {
    Write-Output "{`"track`":1,`"type`":`"data`",`"sha1`":`"$sha1`"}"
    exit 0
}
if ($mode -eq 'must_not_run') {
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'invoked.txt'), 'invoked')
    exit 91
}
Write-Output "{`"track`":1,`"type`":`"data`",`"sha1`":`"$sha1`"}"
if ($mode -eq 'partial') {
    Write-Output '{"track":2,"type":"audio","error":"fingerprint failed"}'
    exit 7
}
if ($mode -eq 'missing') { exit 0 }
if ($mode -eq 'error_zero') {
    Write-Output "{`"track`":2,`"type`":`"audio`",`"sha1`":`"$sha1`",`"error`":`"failed`"}"
    exit 0
}
$fingerprint = if ($mode -eq 'changed') { 'changed-fingerprint' } else { 'fingerprint' }
Write-Output "{`"track`":2,`"type`":`"audio`",`"sha1`":`"$sha1`",`"chromaprint`":`"$fingerprint`",`"duration_ms`":120000}"
exit 0
'@
    [IO.File]::WriteAllText($fakeInner, $fakeBody, [Text.UTF8Encoding]::new($false))
    New-FingerprintFixtureLauncher -Path $fakeCli -ScriptPath $fakeInner

    Assert-True ((Invoke-Workflow -Mode 'partial') -ne 0) `
        'A nonzero CLI with partial JSON should fail the workflow'
    Assert-True (-not (Test-Path -LiteralPath $manifest)) `
        'A partial CLI result should not publish a manifest'
    $singleManifest = Join-Path $singleDir 'track_fingerprints.json'
    Assert-True ([IO.File]::ReadAllText($singleManifest).TrimStart().StartsWith('[')) `
        'A complete one-track result should still publish as a JSON array'
    $expectedSingle = "[`n  {`n    `"track`": 1,`n    `"type`": `"data`",`n" +
    "    `"sha1`": `"$('1' * 40)`"`n  }`n]"
    Assert-True ([IO.File]::ReadAllText($singleManifest) -ceq $expectedSingle) `
        'Fingerprint manifest formatting should be canonical across PowerShell hosts'

    Assert-True ((Invoke-Workflow -Mode 'success') -eq 0) `
        'A complete CLI result should succeed'
    $published = @(ConvertFrom-CompatibleJsonItems -Json (Get-Content -LiteralPath $manifest -Raw))
    Assert-True ($published.Count -eq 2 -and $published[1].chromaprint -eq 'fingerprint') `
        'A complete result should publish every expected track'
    Assert-True (@(Get-ChildItem -LiteralPath $discDir -Filter '*.tmp').Count -eq 0) `
        'Atomic publication should not leave temporary files'

    # A real child wrapper preserves bound empty arrays that -File cannot express
    $selectorLauncher = Join-Path $testRoot 'selected_disc_workflow.ps1'
    $selectorFile = Join-Path $testRoot 'selection.json'
    [IO.File]::WriteAllText($selectorLauncher, @'
param([string]$Workflow, [string]$Selection, [string]$CdRoot, [string]$Exe)
$ErrorActionPreference = 'Stop'
$names = @(Get-Content -LiteralPath $Selection -Raw | ConvertFrom-Json)
& $Workflow -FolderNames $names -CdImageDir $CdRoot -FingerprintExePath $Exe -SkipBuild -SkipAcoustId -Force
exit $LASTEXITCODE
'@, [Text.UTF8Encoding]::new($false))
    function Invoke-SelectedDiscWorkflow {
        param([string[]]$Names, [string]$Exe = $fakeCli)
        [IO.File]::WriteAllText($selectorFile, (ConvertTo-Json -InputObject @($Names)), [Text.UTF8Encoding]::new($false))
        $output = & $powershellPath -NoProfile -File $selectorLauncher -Workflow $workflow `
            -Selection $selectorFile -CdRoot $cdRoot -Exe $Exe 2>&1
        return @{ Code = $LASTEXITCODE; Output = ($output -join "`n") }
    }
    $selectionPaths = @($manifest, $singleManifest)
    $selectionSnapshots = @($selectionPaths | ForEach-Object {
            @{ Text = [IO.File]::ReadAllText($_); Time = [IO.File]::GetLastWriteTimeUtc($_) }
        })
    $unavailableDir = Join-Path $cdRoot 'unavailable'
    New-Item -ItemType Directory -Path $unavailableDir | Out-Null
    foreach ($names in @(@(), @(''), @(' '), @('unknown'), @('fixture', 'unknown'),
            @('unknown', 'fixture'), @('fixture', 'FIXTURE'), @('fixture', 'unavailable'))) {
        $result = Invoke-SelectedDiscWorkflow -Names $names -Exe (Join-Path $testRoot 'missing-cli')
        Assert-True ($result.Code -ne 0 -and $result.Output -match 'Invalid disc selection') `
            "Invalid selection must fail before native tool resolution: $($names -join ',')"
        for ($i = 0; $i -lt $selectionPaths.Count; $i++) {
            Assert-True ([IO.File]::ReadAllText($selectionPaths[$i]) -ceq $selectionSnapshots[$i].Text -and
                [IO.File]::GetLastWriteTimeUtc($selectionPaths[$i]) -eq $selectionSnapshots[$i].Time) `
                'Invalid selection must preserve all manifest bytes and modification times'
        }
    }
    Remove-Item -LiteralPath $unavailableDir
    $env:DXX_FINGERPRINT_MANIFEST_TEST_MODE = 'changed'
    $result = Invoke-SelectedDiscWorkflow -Names @('fixture')
    Assert-True ($result.Code -eq 0 -and $result.Output -match 'OK:\s+1') 'Exactly one selected disc should succeed'
    Assert-True ([IO.File]::ReadAllText($singleManifest) -ceq $selectionSnapshots[1].Text -and
        [IO.File]::GetLastWriteTimeUtc($singleManifest) -eq $selectionSnapshots[1].Time) `
        'One selected disc must preserve the unselected manifest bytes and time'
    Assert-True ((@(ConvertFrom-CompatibleJsonItems -Json ([IO.File]::ReadAllText($manifest))))[1].chromaprint -eq 'changed-fingerprint') `
        'One selected disc must actually regenerate the requested manifest'
    $env:DXX_FINGERPRINT_MANIFEST_TEST_MODE = 'success'
    $result = Invoke-SelectedDiscWorkflow -Names @('fixture', 'single disc')
    Assert-True ($result.Code -eq 0 -and $result.Output -match 'OK:\s+2') `
        'Many selected discs must process exactly the requested set'
    Assert-True ([IO.File]::ReadAllText($manifest) -ceq $selectionSnapshots[0].Text) `
        'Selected regeneration must preserve the ordinary canonical result'

    $published[1] | Add-Member -NotePropertyName acoustid_name -NotePropertyValue 'Preserved artist - track'
    $published[1] | Add-Member -NotePropertyName acoustid_album -NotePropertyValue 'Preserved album'
    $legacyText = (ConvertTo-Json -InputObject $published -Depth 10) -replace '(?m)^  ', "`t"
    $legacyText = ($legacyText -replace "`r?`n", "`r`n") + "`r`n"
    [IO.File]::WriteAllText($manifest, $legacyText, [Text.UTF8Encoding]::new($false))
    $writeTime = ([datetime]'2024-01-02T03:04:05Z').ToUniversalTime()
    [IO.File]::SetLastWriteTimeUtc($manifest, $writeTime)
    Assert-True ((Invoke-Workflow -Mode 'success' -Force) -eq 0) `
        'Forced offline regeneration should preserve successful matching identifications'
    Assert-True ([IO.File]::ReadAllText($manifest) -ceq $legacyText -and
        [IO.File]::GetLastWriteTimeUtc($manifest) -eq $writeTime) `
        'Equivalent forced output must preserve cached labels, tabs/CRLF bytes and modification time'
    $published[1].sha1 = '2' * 40
    [IO.File]::WriteAllText($manifest, (ConvertTo-Json -InputObject $published -Depth 10), [Text.UTF8Encoding]::new($false))
    Assert-True ((Invoke-Workflow -Mode 'success' -Force) -eq 0) `
        'Corrected physical hashes should refresh successfully'
    $corrected = @(ConvertFrom-CompatibleJsonItems -Json ([IO.File]::ReadAllText($manifest)))
    Assert-True ($corrected[1].sha1 -eq ('1' * 40) -and
        $corrected[1].acoustid_name -eq 'Preserved artist - track') `
        'A corrected SHA-1 must retain identification for unchanged audio'
    Assert-True ((Invoke-Workflow -Mode 'changed' -Force) -eq 0) `
        'Changed audio should regenerate successfully'
    $changed = @(ConvertFrom-CompatibleJsonItems -Json ([IO.File]::ReadAllText($manifest)))
    Assert-True (-not $changed[1].PSObject.Properties['acoustid_name'] -and
        -not $changed[1].PSObject.Properties['acoustid_album']) `
        'Cached identifications must not cross a changed audio fingerprint'

    Assert-True ((Invoke-Workflow -Mode 'must_not_run') -eq 0) `
        'A complete existing manifest should be safely skipped'
    Assert-True (-not (Test-Path -LiteralPath $invocationMarker)) `
        'Skipping a complete manifest should not invoke the CLI'

    [IO.File]::WriteAllText(
        $manifest,
        '[{"track":1,"type":"data","sha1":"' + ('1' * 40) + '"}]',
        [Text.UTF8Encoding]::new($false))
    Assert-True ((Invoke-Workflow -Mode 'success') -eq 0) `
        'An incomplete existing manifest should be regenerated'
    Assert-True (@(ConvertFrom-CompatibleJsonItems -Json (Get-Content -LiteralPath $manifest -Raw)).Count -eq 2) `
        'Regeneration should replace the incomplete manifest'

    $knownGood = [IO.File]::ReadAllText($manifest)
    Assert-True ((Invoke-Workflow -Mode 'partial' -Force) -ne 0) `
        'A failed forced rerun should report failure'
    Assert-True ([IO.File]::ReadAllText($manifest) -ceq $knownGood) `
        'A failed rerun should not replace a previously complete manifest'

    Remove-Item -LiteralPath $manifest
    Assert-True ((Invoke-Workflow -Mode 'missing') -ne 0) `
        'A zero exit with a missing track should fail validation'
    Assert-True (-not (Test-Path -LiteralPath $manifest)) `
        'A count mismatch should not publish a manifest'
    Assert-True ((Invoke-Workflow -Mode 'error_zero') -ne 0) `
        'An explicit error record should fail even when the CLI exits zero'
    Assert-True (-not (Test-Path -LiteralPath $manifest)) `
        'An error record should not publish a manifest'

    $missionRoot = Join-Path $testRoot 'missions'
    $missionOutput = Join-Path $testRoot 'music'
    $missionZip = Join-Path $missionRoot 'partial.zip'
    $fakeAudio = Join-Path $testRoot $(if (Test-RegressionWindowsHost) { 'fake_fingerprint_audio.cmd' } else { 'fake_fingerprint_audio' })
    $fakeAudioInner = Join-Path $testRoot 'fake_fingerprint_audio.ps1'
    $audioMarker = Join-Path $testRoot 'audio_invoked.txt'
    New-Item -ItemType Directory -Path $missionRoot, $missionOutput | Out-Null
    $archive = [IO.Compression.ZipFile]::Open($missionZip, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in @("a's.ogg", 'b.ogg')) {
            $entry = $archive.CreateEntry($name)
            $stream = $entry.Open()
            try { $stream.WriteByte(1) } finally { $stream.Dispose() }
        }
    } finally {
        $archive.Dispose()
    }
    $fakeAudioBody = @'
param([string]$Directory)
$mode = $env:DXX_FINGERPRINT_MANIFEST_TEST_MODE
if ($mode -eq 'mission_must_not_run') {
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'audio_invoked.txt'), 'invoked')
    exit 92
}
$results = @(Get-ChildItem -LiteralPath $Directory -File | Where-Object {
        $_.Extension -in @('.ogg', '.mp3', '.flac')
    } | Sort-Object Name | ForEach-Object {
        [PSCustomObject]@{ filename = $_.Name; chromaprint = "fp-$($_.BaseName)"; duration_ms = 1000 }
    })
if ($mode -eq 'mission_partial') {
    Write-Output (ConvertTo-Json -InputObject @($results[0]) -Compress)
    exit 8
}
Write-Output (ConvertTo-Json -InputObject $results -Compress)
exit 0
'@
    [IO.File]::WriteAllText($fakeAudioInner, $fakeAudioBody, [Text.UTF8Encoding]::new($false))
    New-FingerprintFixtureLauncher -Path $fakeAudio -ScriptPath $fakeAudioInner

    $missionAlbum = Join-Path $missionOutput 'Mission ZIP - partial'
    $missionInfo = Join-Path $missionAlbum 'chromaprint_info.jsonc'
    Assert-True ((Invoke-MissionWorkflow -Mode 'mission_partial') -ne 0) `
        'A mission CLI partial result with nonzero status should fail'
    Assert-True (-not (Test-Path -LiteralPath $missionInfo)) `
        'A failed mission fingerprint batch should not publish a sidecar'
    Assert-True ((Invoke-MissionWorkflow -Mode 'mission_success') -eq 0) `
        'A complete mission fingerprint batch should succeed'
    $missionMetadata = Read-JsoncFile $missionInfo
    Assert-True ($missionMetadata.complete -ceq $true -and @($missionMetadata.tracks).Count -eq 2) `
        'A complete mission batch should publish every track with a completeness marker'
    $missionText = [IO.File]::ReadAllText($missionInfo)
    Assert-True ($missionText -ceq (ConvertTo-NormalizedJsonText -Text $missionText -RepositoryJsonc)) `
        'New soundtrack sidecars must use the repository formatter during generation'
    Assert-True ($missionText.Contains("a's.ogg") -and -not $missionText.Contains('\u0027')) `
        'Mission manifest apostrophe escaping should be canonical across PowerShell hosts'
    $incompleteText = [IO.File]::ReadAllText($missionInfo).Replace('"complete": true', '"complete": false')
    [IO.File]::WriteAllText($missionInfo, $incompleteText, [Text.UTF8Encoding]::new($false))
    Assert-True ((Invoke-MissionWorkflow -Mode 'mission_success') -eq 0) `
        'A matching mission sidecar without completeness should be regenerated'
    Assert-True ((Read-JsoncFile $missionInfo).complete -ceq $true) `
        'Mission regeneration should restore the completeness marker'
    Assert-True ((Invoke-MissionWorkflow -Mode 'mission_must_not_run') -eq 0) `
        'A complete mission sidecar should be safely skipped'
    Assert-True (-not (Test-Path -LiteralPath $audioMarker)) `
        'Skipping a complete mission sidecar should not invoke the CLI'

    $legacyMissionText = ([IO.File]::ReadAllText($missionInfo) -replace '(?m)^    ', "`t") -replace "`r?`n", "`r`n"
    [IO.File]::WriteAllText($missionInfo, $legacyMissionText, [Text.UTF8Encoding]::new($false))
    [IO.File]::SetLastWriteTimeUtc($missionInfo, $writeTime)
    Assert-True ((Invoke-MissionWorkflow -Mode 'mission_success' -Force) -eq 0) `
        'Forced soundtrack regeneration should succeed through directory publication'
    Assert-True ([IO.File]::ReadAllText($missionInfo) -ceq $legacyMissionText -and
        [IO.File]::GetLastWriteTimeUtc($missionInfo) -eq $writeTime) `
        'Whole-album publication must preserve equivalent sidecar bytes and modification time'
    Write-NormalizedJsoncFile -Path $missionInfo -Text $missionText
    Assert-True ([IO.File]::ReadAllText($missionInfo) -ceq $legacyMissionText -and
        [IO.File]::GetLastWriteTimeUtc($missionInfo) -eq $writeTime) `
        'In-place soundtrack publication must also preserve equivalent sidecars'
    [IO.File]::WriteAllText($missionInfo, '{broken', [Text.UTF8Encoding]::new($false))
    Write-NormalizedJsoncFile -Path $missionInfo -Text $missionText
    Assert-True ([IO.File]::ReadAllText($missionInfo) -ceq $missionText) `
        'A damaged prior sidecar must not prevent publication of valid formatted output'

    # Exercise real pipeline sampling and routing with recording child stages
    $pipelineRoot = Join-Path $testRoot 'pipeline repo'
    $pipelineData = Join-Path $pipelineRoot 'game_data'
    $pipelineHelpers = Join-Path $pipelineRoot 'android/helpers'
    $pipelineDiscs = Join-Path $pipelineData 'CD images'
    $pipelineMusic = Join-Path $pipelineData 'music'
    $pipelineMissions = Join-Path $pipelineData 'mission_files'
    New-Item -ItemType Directory -Path $pipelineHelpers, $pipelineDiscs, $pipelineMusic, $pipelineMissions -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data/update_all_fingerprints.ps1') -Destination $pipelineData
    foreach ($helper in @('runtime_targeted_sampling.ps1', 'powershell_compat.ps1', 'mission_archive_sources.ps1')) {
        Copy-Item -LiteralPath (Join-Path $repoRoot "android/helpers/$helper") -Destination $pipelineHelpers
    }
    $stageBody = @'
param([string[]]$FolderNames, [string[]]$Albums, [string[]]$Zips, [switch]$Force, [switch]$SkipAcoustId, [switch]$DryRun)
$record = @{ stage = [IO.Path]::GetFileNameWithoutExtension($PSCommandPath); parameters = $PSBoundParameters }
[IO.File]::AppendAllText((Join-Path $PSScriptRoot 'calls.jsonl'), ($record | ConvertTo-Json -Depth 5 -Compress) + "`n")
$global:LASTEXITCODE = 0
'@
    foreach ($stage in @('fingerprint_disc_tracks', 'fingerprint_music_packs', 'fingerprint_mission_zip_music',
            'update_known_discs_fingerprints', 'update_known_discs_albums')) {
        [IO.File]::WriteAllText((Join-Path $pipelineData "$stage.ps1"), $stageBody, [Text.UTF8Encoding]::new($false))
    }
    $pipelineCalls = Join-Path $pipelineData 'calls.jsonl'
    function Invoke-FingerprintPipeline {
        param([string]$Step = 'all', [double]$Fraction = 1)
        [IO.File]::WriteAllText($pipelineCalls, '')
        $output = & $powershellPath -NoProfile -File (Join-Path $pipelineData 'update_all_fingerprints.ps1') `
            -Step $Step -SampleFraction $Fraction -SampleSeed 123 -SkipAcoustId -Force 2>&1
        Assert-True ($LASTEXITCODE -eq 0) "Copied fingerprint pipeline failed: $($output -join ' ')"
        return @(Get-Content -LiteralPath $pipelineCalls | ForEach-Object { $_ | ConvertFrom-Json })
    }
    # Sampling inventories names only; these files are never decoded
    [IO.File]::WriteAllText((Join-Path $pipelineMissions 'one.zip'), 'inventory only')
    foreach ($step in @('discs', 'packs')) {
        $calls = @(Invoke-FingerprintPipeline -Step $step -Fraction 0.1)
        Assert-True ($calls.Count -eq 0) "An empty $step sample must skip fingerprinting and dependent merge"
    }
    $calls = @(Invoke-FingerprintPipeline -Fraction 0.1)
    Assert-True ($calls.Count -eq 2 -and $calls[0].stage -eq 'fingerprint_mission_zip_music' -and
        $calls[1].stage -eq 'update_known_discs_albums') 'Empty disc/pack samples must not broaden the all-stage request'
    $calls = @(Invoke-FingerprintPipeline -Step merge -Fraction 0.1)
    Assert-True ($calls.Count -eq 2 -and $calls[0].stage -eq 'update_known_discs_fingerprints' -and
        $calls[1].stage -eq 'update_known_discs_albums') 'Explicit merge must remain available with empty input samples'
    foreach ($name in @('one', 'two', 'three')) {
        New-Item -ItemType Directory -Path (Join-Path $pipelineDiscs $name) | Out-Null
        [IO.File]::WriteAllText((Join-Path $pipelineMusic "$name.zip"), 'inventory only')
        [IO.File]::WriteAllText((Join-Path $pipelineMissions "$name.zip"), 'inventory only')
    }
    foreach ($fraction in @(0.1, 0.5, 1.0)) {
        $calls = @(Invoke-FingerprintPipeline -Fraction $fraction)
        Assert-True ($calls.Count -eq 5) 'Nonempty/default pipeline must retain all five stages'
        foreach ($control in @(@{ Index = 0; Key = 'FolderNames'; Names = @('one', 'two', 'three') },
                @{ Index = 2; Key = 'Albums'; Names = @('one', 'two', 'three') },
                @{ Index = 3; Key = 'Zips'; Names = @('mission_files/one.zip', 'mission_files/two.zip', 'mission_files/three.zip') })) {
            $property = $calls[$control.Index].parameters.PSObject.Properties[$control.Key]
            if ($fraction -eq 1) {
                Assert-True ($null -eq $property) 'Default unsampled pipeline must omit selectors'
            } else {
                $names = @($property.Value)
                $expectedCount = if ($fraction -eq 0.1) { 1 } else { 2 }
                Assert-True ($names.Count -eq $expectedCount -and
                    @($names | Select-Object -Unique).Count -eq $expectedCount -and
                    @($names | Where-Object { $_ -notin $control.Names }).Count -eq 0) `
                    'Sampled pipeline must forward exactly one/many distinct available identities'
            }
        }
    }

    Write-Host 'fingerprint manifest publication tests passed' -ForegroundColor Green
} finally {
    $producerLock.Dispose()
    Remove-Item Env:DXX_FINGERPRINT_MANIFEST_TEST_MODE -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}

exit 0

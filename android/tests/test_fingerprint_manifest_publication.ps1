#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$testRoot = Join-Path $repoRoot 'android\temp\fingerprint_manifest_publication_test'
$workflow = Join-Path $repoRoot 'game_data\fingerprint_disc_tracks.ps1'
$missionWorkflow = Join-Path $repoRoot 'game_data\fingerprint_mission_zip_music.ps1'
. (Join-Path $repoRoot 'android\helpers\json5.ps1')
. (Join-Path $repoRoot 'android\helpers\atomic_text_file.ps1')
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
    & (Join-Path $PSHOME 'pwsh.exe') @arguments | Out-Null
    return $LASTEXITCODE
}

function Invoke-MissionWorkflow {
    param([string]$Mode)

    $env:DXX_FINGERPRINT_MANIFEST_TEST_MODE = $Mode
    $output = & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $missionWorkflow `
        -SkipBuild -SkipAcoustId -MissionDir $missionRoot -OutputRoot $missionOutput `
        -FingerprintExePath $fakeAudio 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0 -and $Mode -ne 'mission_partial') {
        $output | ForEach-Object { Write-Host $_ }
    }
    return $exitCode
}

if (Test-Path -LiteralPath $testRoot) {
    Remove-Item -LiteralPath $testRoot -Recurse -Force
}

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
    $singleDir = Join-Path $cdRoot 'single'
    $fakeCli = Join-Path $testRoot 'fake_fingerprint_cd.cmd'
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
Write-Output "{`"track`":2,`"type`":`"audio`",`"sha1`":`"$sha1`",`"chromaprint`":`"fingerprint`",`"duration_ms`":120000}"
exit 0
'@
    [IO.File]::WriteAllText($fakeInner, $fakeBody, [Text.UTF8Encoding]::new($false))
    $fakeCommand = "@`"$(Join-Path $PSHOME 'pwsh.exe')`" -NoProfile -File `"%~dp0fake_fingerprint_cd.ps1`" %*`r`n"
    [IO.File]::WriteAllText($fakeCli, $fakeCommand, [Text.ASCIIEncoding]::new())

    Assert-True ((Invoke-Workflow -Mode 'partial') -ne 0) `
        'A nonzero CLI with partial JSON should fail the workflow'
    Assert-True (-not (Test-Path -LiteralPath $manifest)) `
        'A partial CLI result should not publish a manifest'
    $singleManifest = Join-Path $singleDir 'track_fingerprints.json'
    Assert-True ([IO.File]::ReadAllText($singleManifest).TrimStart().StartsWith('[')) `
        'A complete one-track result should still publish as a JSON array'

    Assert-True ((Invoke-Workflow -Mode 'success') -eq 0) `
        'A complete CLI result should succeed'
    $published = @(Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json)
    Assert-True ($published.Count -eq 2 -and $published[1].chromaprint -eq 'fingerprint') `
        'A complete result should publish every expected track'
    Assert-True (@(Get-ChildItem -LiteralPath $discDir -Filter '*.tmp').Count -eq 0) `
        'Atomic publication should not leave temporary files'

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
    Assert-True (@(Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).Count -eq 2) `
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
    $fakeAudio = Join-Path $testRoot 'fake_fingerprint_audio.cmd'
    $fakeAudioInner = Join-Path $testRoot 'fake_fingerprint_audio.ps1'
    $audioMarker = Join-Path $testRoot 'audio_invoked.txt'
    New-Item -ItemType Directory -Path $missionRoot, $missionOutput | Out-Null
    $archive = [IO.Compression.ZipFile]::Open($missionZip, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in @('a.ogg', 'b.ogg')) {
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
    $fakeAudioCommand = "@`"$(Join-Path $PSHOME 'pwsh.exe')`" -NoProfile -File `"%~dp0fake_fingerprint_audio.ps1`" %*`r`n"
    [IO.File]::WriteAllText($fakeAudio, $fakeAudioCommand, [Text.ASCIIEncoding]::new())

    $missionAlbum = Join-Path $missionOutput 'Mission ZIP - partial'
    $missionInfo = Join-Path $missionAlbum 'chromaprint_info.json5'
    Assert-True ((Invoke-MissionWorkflow -Mode 'mission_partial') -ne 0) `
        'A mission CLI partial result with nonzero status should fail'
    Assert-True (-not (Test-Path -LiteralPath $missionInfo)) `
        'A failed mission fingerprint batch should not publish a sidecar'
    Assert-True ((Invoke-MissionWorkflow -Mode 'mission_success') -eq 0) `
        'A complete mission fingerprint batch should succeed'
    $missionMetadata = Read-Json5File $missionInfo
    Assert-True ($missionMetadata.complete -ceq $true -and @($missionMetadata.tracks).Count -eq 2) `
        'A complete mission batch should publish every track with a completeness marker'
    $incompleteText = [IO.File]::ReadAllText($missionInfo).Replace('"complete": true', '"complete": false')
    [IO.File]::WriteAllText($missionInfo, $incompleteText, [Text.UTF8Encoding]::new($false))
    Assert-True ((Invoke-MissionWorkflow -Mode 'mission_success') -eq 0) `
        'A matching mission sidecar without completeness should be regenerated'
    Assert-True ((Read-Json5File $missionInfo).complete -ceq $true) `
        'Mission regeneration should restore the completeness marker'
    Assert-True ((Invoke-MissionWorkflow -Mode 'mission_must_not_run') -eq 0) `
        'A complete mission sidecar should be safely skipped'
    Assert-True (-not (Test-Path -LiteralPath $audioMarker)) `
        'Skipping a complete mission sidecar should not invoke the CLI'

    Write-Host 'fingerprint manifest publication tests passed' -ForegroundColor Green
} finally {
    Remove-Item Env:DXX_FINGERPRINT_MANIFEST_TEST_MODE -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}

exit 0

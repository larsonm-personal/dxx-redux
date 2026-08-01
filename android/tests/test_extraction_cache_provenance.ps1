#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$tempRoot = Join-Path $repoRoot 'android\temp\extraction_cache_provenance_test'
. (Join-Path $repoRoot 'android\helpers\bounded_extraction.ps1')

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

if (Test-Path -LiteralPath $tempRoot) {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}

try {
    $discDir = Join-Path $tempRoot 'disc'
    $outputDir = Join-Path $tempRoot 'output'
    New-Item -ItemType Directory -Path $discDir, $outputDir | Out-Null
    Set-Content -LiteralPath (Join-Path $discDir 'disc.bin') -Value 'payload-one' -NoNewline
    Set-Content -LiteralPath (Join-Path $discDir 'disc.cue') `
        -Value "FILE `"disc.bin`" BINARY`n  TRACK 01 MODE1/2352" -NoNewline
    Set-Content -LiteralPath (Join-Path $tempRoot 'tool.exe') -Value 'tool-one' -NoNewline
    Set-Content -LiteralPath (Join-Path $outputDir 'game.hog') -Value 'output' -NoNewline

    $source = Resolve-DiscExtractionSource -Directory $discDir
    Assert-True ($source.Primary.Name -eq 'disc.cue' -and $source.Files.Count -eq 2) `
        'CUE identity should include its descriptor and referenced payload'
    $sources = @($source.Files | ForEach-Object {
            Get-ExtractionPathIdentity -Path $_.FullName -Name $_.Name
        })
    $tools = @(
        (Get-ExtractionPathIdentity -Path (Join-Path $tempRoot 'tool.exe') -Name 'tool'),
        (Get-ExtractionPathIdentity -Path $PSCommandPath -Name 'test-script')
    )
    $provenance = New-ExtractionProvenance -Policy 'test-policy-v1' -Sources $sources -Tools $tools
    Write-ExtractionCompletionManifest -Directory $outputDir -Provenance $provenance
    Assert-True (Test-ExtractionCompletionManifest -Directory $outputDir -ExpectedProvenance $provenance) `
        'An unchanged complete cache should be reusable'

    Set-Content -LiteralPath (Join-Path $discDir 'disc.bin') -Value 'payload-two' -NoNewline
    $changedSources = @((Resolve-DiscExtractionSource -Directory $discDir).Files | ForEach-Object {
            Get-ExtractionPathIdentity -Path $_.FullName -Name $_.Name
        })
    $changedSourceProvenance = New-ExtractionProvenance -Policy 'test-policy-v1' `
        -Sources $changedSources -Tools $tools
    Assert-True (-not (Test-ExtractionCompletionManifest -Directory $outputDir `
                -ExpectedProvenance $changedSourceProvenance)) `
        'Replacing a referenced payload under the same name should invalidate the cache'

    Set-Content -LiteralPath (Join-Path $tempRoot 'tool.exe') -Value 'tool-two' -NoNewline
    $changedTools = @(
        (Get-ExtractionPathIdentity -Path (Join-Path $tempRoot 'tool.exe') -Name 'tool'),
        $tools[1]
    )
    $changedToolProvenance = New-ExtractionProvenance -Policy 'test-policy-v1' `
        -Sources $sources -Tools $changedTools
    Assert-True (-not (Test-ExtractionCompletionManifest -Directory $outputDir `
                -ExpectedProvenance $changedToolProvenance)) 'Changing the tool should invalidate the cache'
    $changedPolicyProvenance = New-ExtractionProvenance -Policy 'test-policy-v2' `
        -Sources $sources -Tools $tools
    Assert-True (-not (Test-ExtractionCompletionManifest -Directory $outputDir `
                -ExpectedProvenance $changedPolicyProvenance)) 'Changing policy should invalidate the cache'

    Add-Content -LiteralPath (Join-Path $outputDir 'game.hog') -Value 'corrupt' -NoNewline
    Assert-True (-not (Test-ExtractionCompletionManifest -Directory $outputDir `
                -ExpectedProvenance $provenance)) 'Changing output should invalidate the cache'

    Set-Content -LiteralPath (Join-Path $discDir 'extra.iso') -Value 'iso' -NoNewline
    $ambiguousFailed = $false
    try { $null = Resolve-DiscExtractionSource -Directory $discDir } catch { $ambiguousFailed = $true }
    Assert-True $ambiguousFailed 'CUE plus ISO input should fail instead of selecting by enumeration order'
    Remove-Item -LiteralPath (Join-Path $discDir 'extra.iso')
    Copy-Item -LiteralPath (Join-Path $discDir 'disc.cue') -Destination (Join-Path $discDir 'extra.cue')
    $ambiguousFailed = $false
    try { $null = Resolve-DiscExtractionSource -Directory $discDir } catch { $ambiguousFailed = $true }
    Assert-True $ambiguousFailed 'Multiple CUE inputs should fail instead of selecting element zero'

    $pairedDir = Join-Path $tempRoot 'paired-disc'
    New-Item -ItemType Directory -Path $pairedDir | Out-Null
    Set-Content -LiteralPath (Join-Path $pairedDir 'disc.iso') -Value 'iso' -NoNewline
    Set-Content -LiteralPath (Join-Path $pairedDir 'disc.cue') `
        -Value 'FILE "disc.iso" BINARY' -NoNewline
    $pairedSource = Resolve-DiscExtractionSource -Directory $pairedDir
    Assert-True ($pairedSource.Primary.Name -eq 'disc.iso' -and $pairedSource.Files.Count -eq 2) `
        'A CUE whose sole payload is the neighboring ISO should bind one complete source set'
    $cueOnlyFailed = $false
    try { $null = Resolve-DiscExtractionSource -Directory $pairedDir -CueOnly } catch { $cueOnlyFailed = $true }
    Assert-True $cueOnlyFailed 'The legacy CUE-only extractor should reject a neighboring ISO descriptor'

    . (Join-Path $repoRoot 'game_data\fingerprint_mission_zip_music.ps1') `
        -BudgetTestOnly -SkipAcoustId
    $sha1 = '1' * 40
    $sha256 = '2' * 64
    $sidecar = [pscustomobject]@{
        complete = $true
        source_zip = 'mission.zip'
        source_sha1 = $sha1
        source_sha256 = $sha256
    }
    Assert-True (Test-MissionFingerprintCacheIdentity -ExistingInfo $sidecar `
            -SourceZip 'mission.zip' -SourceSha1 $sha1 -SourceSha256 $sha256) `
        'A complete matching mission sidecar identity should be reusable'
    foreach ($field in @('source_zip', 'source_sha1', 'source_sha256')) {
        $invalid = $sidecar | Select-Object *
        $invalid.$field = ''
        Assert-True (-not (Test-MissionFingerprintCacheIdentity -ExistingInfo $invalid `
                    -SourceZip 'mission.zip' -SourceSha1 $sha1 -SourceSha256 $sha256)) `
            "A mission sidecar missing $field should not be reusable"
    }
    $incomplete = $sidecar | Select-Object *
    $incomplete.complete = $false
    Assert-True (-not (Test-MissionFingerprintCacheIdentity -ExistingInfo $incomplete `
                -SourceZip 'mission.zip' -SourceSha1 $sha1 -SourceSha256 $sha256)) `
        'A matching but incomplete mission sidecar should not be reusable'

    Write-Host 'extraction cache provenance tests passed' -ForegroundColor Green
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

exit 0

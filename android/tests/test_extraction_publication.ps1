$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android\helpers\bounded_extraction.ps1')

$testRoot = Join-Path $repoRoot "android\temp\publication-$([Guid]::NewGuid().ToString('N'))"
$destination = Join-Path $testRoot 'published'
$staging = Join-Path $testRoot 'staging'

try {
    New-Item -ItemType Directory -Path $destination, $staging | Out-Null
    Set-Content -LiteralPath (Join-Path $destination 'old.txt') -Value 'old' -NoNewline
    $newFile = Join-Path $staging 'new.txt'
    Set-Content -LiteralPath $newFile -Value 'new' -NoNewline
    $provenance = New-ExtractionProvenance -Policy 'publication-test-v1' -Sources @(
        (Get-ExtractionPathIdentity -Path $newFile -Name 'source')
    ) -Tools @(
        (Get-ExtractionPathIdentity -Path $PSCommandPath -Name 'test_extraction_publication.ps1')
    )
    Write-ExtractionCompletionManifest -Directory $staging -Provenance $provenance

    Publish-ExtractionDirectory -StagingDirectory $staging -DestinationDirectory $destination

    if (Test-Path -LiteralPath (Join-Path $destination 'old.txt')) {
        throw 'old generation remained after publication'
    }
    if ((Get-Content -LiteralPath (Join-Path $destination 'new.txt') -Raw) -ne 'new') {
        throw 'staged generation was not published'
    }
    if (-not (Test-ExtractionCompletionManifest -Directory $destination -ExpectedProvenance $provenance)) {
        throw 'valid completion manifest was not accepted'
    }
    Add-Content -LiteralPath (Join-Path $destination 'new.txt') -Value 'corrupt' -NoNewline
    if (Test-ExtractionCompletionManifest -Directory $destination -ExpectedProvenance $provenance) {
        throw 'corrupt completion generation was accepted'
    }
    if (Get-ChildItem -LiteralPath $testRoot -Directory -Filter '*.rollback-*') {
        throw 'rollback directory remained after publication'
    }
    Write-Host 'extraction publication tests passed'
} finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}

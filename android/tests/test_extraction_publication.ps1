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
    [PSCustomObject]@{
        source = 'test'
        files = @(
            [PSCustomObject]@{
                name = 'new.txt'
                size = 3
                sha256 = (Get-FileHash -LiteralPath $newFile -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        )
    } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $staging '.extraction-complete.json') -NoNewline

    Publish-ExtractionDirectory -StagingDirectory $staging -DestinationDirectory $destination

    if (Test-Path -LiteralPath (Join-Path $destination 'old.txt')) {
        throw 'old generation remained after publication'
    }
    if ((Get-Content -LiteralPath (Join-Path $destination 'new.txt') -Raw) -ne 'new') {
        throw 'staged generation was not published'
    }
    if (-not (Test-ExtractionCompletionManifest -Directory $destination)) {
        throw 'valid completion manifest was not accepted'
    }
    Add-Content -LiteralPath (Join-Path $destination 'new.txt') -Value 'corrupt' -NoNewline
    if (Test-ExtractionCompletionManifest -Directory $destination) {
        throw 'corrupt completion generation was accepted'
    }
    if (Get-ChildItem -LiteralPath $testRoot -Directory -Filter '*.rollback-*') {
        throw 'rollback directory remained after publication'
    }
    Write-Host 'extraction publication tests passed'
} finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}

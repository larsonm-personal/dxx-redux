#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("dxx-hash-assets-" + [guid]::NewGuid().ToString('N'))

function Invoke-ForcedHash {
    param([string]$ScriptPath)

    & pwsh -NoProfile -File $ScriptPath -Force *> $null
    return $LASTEXITCODE
}

try {
    $gameData = New-Item -ItemType Directory -Force -Path (Join-Path $tempRoot 'game_data')
    $assetDir = New-Item -ItemType Directory -Force -Path (Join-Path $tempRoot 'android/app/src/main/assets')
    $helperDir = New-Item -ItemType Directory -Force -Path (Join-Path $tempRoot 'android/helpers')
    $sourceDir = New-Item -ItemType Directory -Force -Path (Join-Path $tempRoot 'game_data_to_copy_to_emulator')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data/hash_assets.ps1') -Destination $gameData.FullName
    Copy-Item -LiteralPath (Join-Path $repoRoot 'android/helpers/atomic_text_file.ps1') -Destination $helperDir.FullName

    $foo = Join-Path $sourceDir.FullName 'foo.hog'
    $bar = Join-Path $sourceDir.FullName 'bar.pig'
    [System.IO.File]::WriteAllBytes($foo, [byte[]](1, 2, 3))
    [System.IO.File]::WriteAllBytes($bar, [byte[]](4, 5, 6))
    $fooHash = (Get-FileHash -LiteralPath $foo -Algorithm SHA256).Hash.ToLowerInvariant()
    $barHash = (Get-FileHash -LiteralPath $bar -Algorithm SHA256).Hash.ToLowerInvariant()
    $knownVersions = Join-Path $assetDir.FullName 'known_versions.json5'
    $baseline = @"
{
  "versions": [
    { "file": "foo.hog", "sha256": "$fooHash", "version": "D2 v1.2" },
    { "file": "bar.pig", "sha256": "$barHash", "version": "D2 v1.2" }
  ]
}
"@.Trim()
    [System.IO.File]::WriteAllText($knownVersions, $baseline)
    $scriptPath = Join-Path $gameData.FullName 'hash_assets.ps1'

    Remove-Item -LiteralPath $bar
    if ((Invoke-ForcedHash $scriptPath) -eq 0) { throw 'Partial Force regeneration unexpectedly succeeded' }
    if ([System.IO.File]::ReadAllText($knownVersions) -ne $baseline) { throw 'Partial Force regeneration changed the baseline' }

    Remove-Item -LiteralPath $foo
    if ((Invoke-ForcedHash $scriptPath) -eq 0) { throw 'Zero-source Force regeneration unexpectedly succeeded' }
    if ([System.IO.File]::ReadAllText($knownVersions) -ne $baseline) { throw 'Zero-source Force regeneration changed the baseline' }

    [System.IO.File]::WriteAllBytes($foo, [byte[]](1, 2, 3))
    [System.IO.File]::WriteAllBytes($bar, [byte[]](4, 5, 6))
    if ((Invoke-ForcedHash $scriptPath) -ne 0) { throw 'Complete Force regeneration failed' }
    $generated = Get-Content -LiteralPath $knownVersions -Raw
    if ($generated -notmatch [regex]::Escape($fooHash) -or $generated -notmatch [regex]::Escape($barHash)) {
        throw 'Complete Force regeneration omitted a maintained identity'
    }

    Write-Host 'PASS'
} finally {
    if ($tempRoot.StartsWith([System.IO.Path]::GetTempPath(), [System.StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

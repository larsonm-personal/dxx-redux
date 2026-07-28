#!/usr/bin/env pwsh
# Verify BR-0157 dependency identity checks and fail-closed consumer contracts.

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android\helpers\verified_dependencies.ps1')

$tempRoot = Join-Path $repoRoot "temp\download-verification-$([Guid]::NewGuid().ToString('N'))"
$filePath = Join-Path $tempRoot 'source.bin'
$treePath = Join-Path $tempRoot 'tree'

function Assert-Throws {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Action,
        [Parameter(Mandatory = $true)][string]$Label
    )

    try {
        & $Action
    } catch {
        Write-Host "PASS: $Label"
        return
    }
    throw "Expected failure: $Label"
}

function Assert-Contains {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $content = Get-Content -LiteralPath $Path -Raw
    if ($content -notmatch $Pattern) {
        throw "$Label missing from $Path"
    }
    Write-Host "PASS: $Label"
}

try {
    New-Item -ItemType Directory -Path $treePath -Force | Out-Null
    [IO.File]::WriteAllBytes($filePath, [byte[]](1, 2, 3, 4))
    $fileHash = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash
    Assert-DxxFileSha256 -Path $filePath -ExpectedSha256 $fileHash -Label 'test source' | Out-Null
    Write-Host 'PASS: verified file is accepted'

    [IO.File]::WriteAllBytes($filePath, [byte[]](1, 2, 3, 5))
    Assert-Throws {
        Assert-DxxFileSha256 -Path $filePath -ExpectedSha256 $fileHash -Label 'mutated source'
    } 'same-size file mutation is rejected'

    Assert-Throws {
        Assert-DxxFileSha256 -Path (Join-Path $tempRoot 'missing') `
            -ExpectedSha256 $fileHash -Label 'missing source'
    } 'missing offline cache is rejected'

    [IO.File]::WriteAllText((Join-Path $treePath 'module.py'), 'trusted')
    $treeHash = Get-DxxTreeSha256 -Path $treePath
    Assert-DxxTreeSha256 -Path $treePath -ExpectedSha256 $treeHash -Label 'test tree' | Out-Null
    Write-Host 'PASS: verified extracted tree is accepted'

    [IO.File]::WriteAllText((Join-Path $treePath 'module.py'), 'altered')
    Assert-Throws {
        Assert-DxxTreeSha256 -Path $treePath -ExpectedSha256 $treeHash -Label 'mutated tree'
    } 'extracted tree mutation is rejected'

    $cmake = Join-Path $repoRoot 'android\app\src\main\cpp\extract\CMakeLists.txt'
    Assert-Contains $cmake 'URL_HASH\s+"SHA256=\$\{CHROMAPRINT_SHA256\}"' `
        'Chromaprint archive has a pinned hash'
    Assert-Contains $cmake 'EXPECTED_HASH\s+"SHA256=\$\{sha256\}"' `
        'single-file downloads have pinned hashes'
    Assert-Contains $cmake 'TLS_VERIFY\s+ON' 'CMake downloads explicitly verify TLS'
    Assert-Contains $cmake 'file\(SHA256\s+"\$\{destination\}"' `
        'cached decoder sources are reverified'
    Assert-Contains (Join-Path $repoRoot 'android\helpers\run_verified_python.py') `
        'sys\.dont_write_bytecode\s*=\s*True' `
        'verified Python execution preserves the hashed package tree'
    $updater = Join-Path $repoRoot 'android\get_deps\check-updates.ps1'
    Assert-Contains $updater 'ManualTargetUpdateHint' `
        'dependency updates require reviewed hash changes'
    Assert-Contains $updater 'review-hash' `
        'hash-coupled updates remain visible to maintainers'

    foreach ($relativePath in @(
            'game_data\extract_dos_demos.ps1',
            'game_data\extract_mac_demos.ps1',
            'game_data\fingerprint_mission_zip_music.ps1',
            'game_data\fingerprint_music_packs.ps1',
            'android\helpers\run_mission_zip_batch.ps1',
            'android\helpers\regenerate_all_mission_metadata_host.ps1'
        )) {
        $path = Join-Path $repoRoot $relativePath
        Assert-Contains $path 'get_7zip\.ps1|Resolve-DxxVerifiedDependencyExecutable' `
            "$relativePath uses a repository-verified executable"
        $content = Get-Content -LiteralPath $path -Raw
        if ($content -match 'Get-Command\s+["'']?(7z|7za|unar)') {
            throw "$relativePath still trusts a PATH-selected archive executable"
        }
    }

    $manifestPaths = Get-ChildItem -LiteralPath (
        Join-Path $repoRoot 'android\app\src\main\cpp\extract\test\data\stuffit_manifests'
    ) -Filter '*.json' -File
    foreach ($manifestPath in $manifestPaths) {
        $manifest = Get-Content -LiteralPath $manifestPath.FullName -Raw | ConvertFrom-Json
        if ($manifest.schema_version -ne 3 -or
            $manifest.unar_exe_sha256 -notmatch '^[0-9a-f]{64}$' -or
            $manifest.lsar_exe_sha256 -notmatch '^[0-9a-f]{64}$') {
            throw "Unverified StuffIt provenance in $($manifestPath.Name)"
        }
    }
    Write-Host 'PASS: StuffIt manifests record verified executable identities'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'All downloaded dependency verification tests passed'

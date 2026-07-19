#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
$androidRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $androidRoot "helpers\standard_game_data.ps1")

$tempParent = [IO.Path]::GetFullPath((Join-Path $androidRoot "temp"))
$tempRoot = [IO.Path]::GetFullPath((Join-Path $tempParent "standard_game_data_resolution_$PID"))
if (-not $tempRoot.StartsWith($tempParent + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Temporary test directory is outside android/temp"
}

try {
    $wrongDir = Join-Path $tempRoot "wrong"
    $validDir = Join-Path $tempRoot "valid"
    New-Item -ItemType Directory -Force -Path $wrongDir, $validDir | Out-Null
    Set-Content -LiteralPath (Join-Path $wrongDir "DESCENT.HOG") -Value "wrong hog" -NoNewline
    Set-Content -LiteralPath (Join-Path $wrongDir "DESCENT.PIG") -Value "wrong pig" -NoNewline
    Set-Content -LiteralPath (Join-Path $validDir "descent.hog") -Value "expected hog" -NoNewline
    Set-Content -LiteralPath (Join-Path $validDir "descent.pig") -Value "expected pig" -NoNewline

    $dependencies = @(
        @{file = "descent.hog"; sha256 = (Get-FileHash -LiteralPath (Join-Path $validDir "descent.hog") -Algorithm SHA256).Hash }
        @{file = "descent.pig"; sha256 = (Get-FileHash -LiteralPath (Join-Path $validDir "descent.pig") -Algorithm SHA256).Hash }
    )
    $selection = Resolve-StandardGameDataDirectory -Candidates @($wrongDir, $validDir) -Dependencies $dependencies -Label "test"
    if ($selection.Path -cne (Resolve-Path -LiteralPath $validDir).Path) {
        throw "Resolver accepted a filename-only candidate with mismatched hashes"
    }
    if ($selection.Hashes.Count -ne 2) {
        throw "Resolver did not report every validated hash"
    }

    $rejected = $false
    try {
        Resolve-StandardGameDataDirectory -Candidates @($wrongDir) -Dependencies $dependencies -Label "test" | Out-Null
    } catch {
        $rejected = $_.Exception.Message -like "*matching pinned hashes not found*"
    }
    if (-not $rejected) {
        throw "Resolver did not reject the mismatched candidate"
    }

    $standardD1 = @(Get-StandardGameDataDeps | Where-Object { $_.file -in @("descent.hog", "descent.pig") })
    if ($standardD1.Count -ne 2 -or $standardD1[1].sha256 -cne "093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe") {
        throw "Pinned DOS D1 data dependencies changed unexpectedly"
    }

    Write-Host "PASS: standard game data resolution validates hashes and skips mismatched candidates"
} finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

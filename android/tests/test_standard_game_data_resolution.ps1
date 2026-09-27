#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
$androidRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $androidRoot "helpers\standard_game_data.ps1")

$tempParent = [IO.Path]::GetFullPath((Join-Path $androidRoot "temp"))
$tempRoot = [IO.Path]::GetFullPath((Join-Path $tempParent "standard_game_data_resolution_$PID"))
if (-not $tempRoot.StartsWith($tempParent + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Temporary test directory is outside android/temp"
}

$fixtureLock = $null
& (Join-Path $androidRoot 'helpers/retain-recent-artifacts.ps1') -Artifacts $tempRoot -DirectoryPrefix standard_game_data_resolution_
New-Item -ItemType Directory -Path $tempRoot -ErrorAction Stop | Out-Null
try {
    $fixtureLock = [IO.File]::Open((Join-Path $tempRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $wrongDir = Join-Path $tempRoot "wrong"
    $validDir = Join-Path $tempRoot "game_data/CD images/retail disc/data_tracks/descent"
    New-Item -ItemType Directory -Force -Path $wrongDir, $validDir | Out-Null
    Set-Content -LiteralPath (Join-Path $wrongDir "DESCENT.HOG") -Value "wrong hog" -NoNewline
    Set-Content -LiteralPath (Join-Path $wrongDir "DESCENT.PIG") -Value "wrong pig" -NoNewline
    Set-Content -LiteralPath (Join-Path $validDir "DESCENT.HOG") -Value "expected hog" -NoNewline
    Set-Content -LiteralPath (Join-Path $validDir "DESCENT.PIG") -Value "expected pig" -NoNewline

    $dependencies = @(
        @{file = "descent.hog"; sha256 = (Get-FileHash -LiteralPath (Join-Path $validDir "DESCENT.HOG") -Algorithm SHA256).Hash }
        @{file = "descent.pig"; sha256 = (Get-FileHash -LiteralPath (Join-Path $validDir "DESCENT.PIG") -Algorithm SHA256).Hash }
    )
    $selection = Resolve-StandardGameDataDirectory -Candidates @($wrongDir, $validDir) -Dependencies $dependencies -Label "test"
    if ($selection.Path -cne (Resolve-Path -LiteralPath $validDir).Path) {
        throw "Resolver accepted a filename-only candidate with mismatched hashes"
    }
    if ($selection.Hashes.Count -ne 2) {
        throw "Resolver did not report every validated hash"
    }

    $discovered = @(Get-StandardGameDataCandidates -RepoRoot $tempRoot -Game d1)
    $fromMedia = Resolve-StandardGameDataDirectory -Candidates $discovered -Dependencies $dependencies -Label 'extracted media'
    if ($fromMedia.Path -cne $selection.Path) { throw 'Extracted uppercase CD data was not discovered and validated' }

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

    $otherDisc = Join-Path $tempRoot 'other disc'
    New-Item -ItemType Directory -Path $otherDisc | Out-Null
    Move-Item -LiteralPath (Join-Path $validDir 'DESCENT.PIG') -Destination $otherDisc
    $sources = @($wrongDir, $validDir, $otherDisc)
    $stagePath = Join-Path $tempRoot 'combined data'
    $stage = New-StandardGameDataStage -Destination $stagePath -Candidates $sources -Dependencies $dependencies
    if ($stage.Hashes.Count -ne 2 -or (Get-ChildItem -LiteralPath $stage.Path -File | Where-Object Name -ceq 'descent.hog').Count -ne 1) {
        throw 'Combined data stage did not validate and normalize filenames'
    }
    Set-Content -LiteralPath (Join-Path $stage.Path 'descent.hog') -Value 'changed copy'
    if ((Get-FileHash -LiteralPath (Join-Path $validDir 'DESCENT.HOG')).Hash -ne $dependencies[0].sha256) {
        throw 'Changing staged data modified its source'
    }
    $rejected = $false
    try { New-StandardGameDataStage -Destination $stagePath -Candidates $sources -Dependencies $dependencies | Out-Null }
    catch { $rejected = $_ -match 'already exists' }
    if (-not $rejected -or (Get-Content -LiteralPath (Join-Path $stagePath 'descent.hog') -Raw).Trim() -ne 'changed copy') {
        throw 'Staging replaced an existing directory'
    }

    $failedStage = Join-Path $tempRoot 'failed stage'
    $originalResolver = ${function:Resolve-StandardGameDataDirectory}
    try {
        function Resolve-StandardGameDataDirectory {
            param($Candidates, $Dependencies, $Label)
            if (-not (Test-Path -LiteralPath (Join-Path $Candidates[0] 'descent.hog'))) { throw 'Fixture failed before copying data' }
            throw 'Injected stage verification failure'
        }
        $rejected = $false
        try { New-StandardGameDataStage -Destination $failedStage -Candidates $sources -Dependencies $dependencies | Out-Null }
        catch { $rejected = $_ -match 'Injected stage verification failure' }
        if (-not $rejected -or (Test-Path -LiteralPath $failedStage)) { throw 'Failed verification left copied game data behind' }
    } finally { Set-Item Function:Resolve-StandardGameDataDirectory -Value $originalResolver }

    Write-Host 'PASS: pinned data discovery, mixed-source staging, source isolation, existing-directory protection and failure cleanup'
} finally {
    if ($fixtureLock) { $fixtureLock.Dispose() }
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

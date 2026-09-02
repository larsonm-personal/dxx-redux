#!/usr/bin/env pwsh

param(
    [Parameter(Mandatory = $true)][string]$ArchiveName,
    [switch]$KeepArtifacts
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $PSCommandPath
$androidRoot = Split-Path -Parent $scriptDir
$repoRoot = Split-Path -Parent $androidRoot
$archive = Get-ChildItem -LiteralPath (Join-Path $repoRoot 'game_data\mission_files') -Recurse -File |
    Where-Object { $_.Name.Equals($ArchiveName, [StringComparison]::OrdinalIgnoreCase) } |
    Select-Object -First 1
if (-not $archive) { throw "Mission archive not found: $ArchiveName" }

$regressionPath = Join-Path $archive.DirectoryName "$($archive.BaseName).json"
$tempRoot = Join-Path $androidRoot "temp\metadata_engine_parity\$([IO.Path]::GetRandomFileName())"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
$original = if (Test-Path -LiteralPath $regressionPath) { [IO.File]::ReadAllBytes($regressionPath) } else { $null }
try {
    & (Join-Path $scriptDir 'regenerate_all_mission_metadata_host.ps1') -ArchiveName $archive.Name
    if ($LASTEXITCODE -ne 0) { throw "Windows metadata runner failed with exit code $LASTEXITCODE" }
    $windows = [IO.File]::ReadAllBytes($regressionPath)
    [IO.File]::WriteAllBytes((Join-Path $tempRoot 'windows.json'), $windows)

    $jdkHome = 'C:\local\jdk-21'
    if (Test-Path -LiteralPath $jdkHome -PathType Container) { $env:JAVA_HOME = $jdkHome; $env:Path = "$jdkHome\bin;$env:Path" }
    & (Join-Path $androidRoot 'gradlew.bat') -p $androidRoot assembleDebug --console=plain
    if ($LASTEXITCODE -ne 0) { throw "Android APK build failed with exit code $LASTEXITCODE" }
    & (Join-Path $scriptDir 'run_mission_zip_batch.ps1') -MetadataOnly -Install -ZipDir $archive.DirectoryName -OutDir (Join-Path $tempRoot 'emulator') -Pattern $archive.Name -TimeoutSeconds 900
    if ($LASTEXITCODE -ne 0) { throw "Emulator metadata runner failed with exit code $LASTEXITCODE" }
    $emulator = [IO.File]::ReadAllBytes($regressionPath)
    [IO.File]::WriteAllBytes((Join-Path $tempRoot 'emulator.json'), $emulator)
    if (-not [Linq.Enumerable]::SequenceEqual[byte]($windows, $emulator)) {
        throw "Windows and emulator metadata differ for $ArchiveName"
    }
    Write-Host "Windows/emulator metadata parity passed: $ArchiveName" -ForegroundColor Green
} finally {
    if ($null -ne $original) { [IO.File]::WriteAllBytes($regressionPath, $original) }
    elseif (Test-Path -LiteralPath $regressionPath) { Remove-Item -LiteralPath $regressionPath -Force }
    if (-not $KeepArtifacts -and (Test-Path -LiteralPath $tempRoot)) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

#!/usr/bin/env pwsh
param([string]$Serial = "emulator-5554")
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
$fixtures = Join-Path $repoRoot 'android/temp/enemy-within-fixtures'
& (Join-Path $PSScriptRoot '../helpers/retain-recent-artifacts.ps1') -Artifacts $fixtures
New-Item -ItemType Directory -Force $fixtures | Out-Null
$enemy = Join-Path $fixtures 'ewithin-rebirth.zip'
if (-not (Test-Path -LiteralPath $enemy)) {
    $archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $repoRoot 'game_data/mission_files/ewithin-versions.zip'))
    try {
        $entry = @($archive.Entries | Where-Object { $_.Name -eq 'ewithin-rebirth.zip' })
        if ($entry.Count -ne 1) { throw 'Expected one rebirth campaign in fixture' }
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry[0], $enemy)
    } finally { $archive.Dispose() }
}
if ((Get-FileHash -LiteralPath $enemy -Algorithm SHA256).Hash.ToLowerInvariant() -ne
    'bf8e9f58ef6993f44df5b94f38efd0235bb407485267948a4d0eaaa0a6aa8c84') {
    throw 'Enemy Within fixture differs from the phone report'
}
Adb -AdbArgs @('push', $enemy, '/data/local/tmp/ewithin-rebirth.zip') -Seconds 180 | Out-Null
Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', 'files/mission_zip_batch_cache') | Out-Null
Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/ewithin-rebirth.zip', 'files/mission_zip_batch_cache/ewithin-rebirth.zip') -Seconds 180 | Out-Null
$stagedSize = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'stat', '-c', '%s', 'files/mission_zip_batch_cache/ewithin-rebirth.zip')
if ("$stagedSize".Trim() -ne "$((Get-Item -LiteralPath $enemy).Length)") { throw 'Could not stage complete mission fixture' }
try {
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    & (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -Serial $Serial -ScriptName test_enemy_within_level2.jsonc -Game d2 -TimeoutSeconds 180
    if ($LASTEXITCODE -ne 0) { throw 'Enemy Within Level 2 integration failed' }
} finally {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('shell', 'rm', '-f', '/data/local/tmp/ewithin-rebirth.zip') | Out-Null
}

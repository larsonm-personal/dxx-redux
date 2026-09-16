#!/usr/bin/env pwsh
# Real campaign isolation regression on the disposable repository emulator
param([switch]$Install)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$devices = & $ADB devices
if (@($devices | Where-Object { $_ -match '^emulator-\d+\s+device$' }).Count -ne 1 -or
    @($devices | Where-Object { $_ -match '^\S+\s+device$' }).Count -ne 1) {
    throw 'This test requires exactly one connected device, the disposable test emulator'
}
if ($Install) {
    # Drop generated test projections before installation on the small test AVD
    & $ADB shell am force-stop com.dxxredux.app
    & $ADB shell run-as com.dxxredux.app rm -rf files/d2x-redux/.mission_assets
    & $ADB install -r (Join-Path $repoRoot 'android/app/build/outputs/apk/debug/app-debug.apk')
    if ($LASTEXITCODE -ne 0) { throw 'Could not install the test APK' }
}
$fixtures = Join-Path $repoRoot 'temp/mission-isolation-fixtures'
New-Item -ItemType Directory -Force $fixtures | Out-Null
$enemy = Join-Path $fixtures 'ewithin-rebirth.zip'
if (!(Test-Path -LiteralPath $enemy)) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $repoRoot 'game_data/mission_files/ewithin-versions.zip'))
    try {
        $entry = @($archive.Entries | Where-Object { $_.Name -eq 'ewithin-rebirth.zip' })
        if ($entry.Count -ne 1) { throw 'Expected one rebirth campaign in the local fixture' }
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry[0], $enemy)
    } finally { $archive.Dispose() }
}
if ((Get-FileHash -LiteralPath $enemy -Algorithm SHA256).Hash.ToLowerInvariant() -ne
    'bf8e9f58ef6993f44df5b94f38efd0235bb407485267948a4d0eaaa0a6aa8c84') {
    throw 'Enemy Within fixture does not match the phone reproduction sample fixture'
}
$target = 'files/imported/sets/default/.content/mods'
& $ADB shell run-as com.dxxredux.app mkdir -p $target
foreach ($source in @($enemy, (Join-Path $repoRoot 'game_data/mission_files/descent_maximum_fixed.zip'))) {
    $name = Split-Path -Leaf $source
    & $ADB push $source "/data/local/tmp/$name"
    if ($LASTEXITCODE -ne 0) { throw "Could not stage $name" }
    & $ADB shell run-as com.dxxredux.app cp "/data/local/tmp/$name" "$target/$name"
    if ($LASTEXITCODE -ne 0) { throw "Could not publish $name" }
    & $ADB shell rm -f "/data/local/tmp/$name"
}
& $ADB logcat -c
& (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -ScriptName test_mission_asset_isolation.jsonc -Game d2 -TimeoutSeconds 600
$testResult = $LASTEXITCODE
$logPath = Join-Path $repoRoot 'temp/mission_isolation_logcat.txt'
& $ADB logcat -d | Set-Content -LiteralPath $logPath -Encoding utf8
if ($testResult -ne 0) { throw "Mission switching failed; see $logPath" }
$lines = Get-Content -LiteralPath $logPath
$marker = $lines | Where-Object { $_ -match 'DXX-Automate: SCRIPT: ISOLATION start Counterstrike' } | Select-Object -First 1
if (!$marker) { throw 'Missing game process marker' }
$gameProcess = ($marker -split '\s+')[2]
$lines = @($lines | Where-Object { ($_ -split '\s+')[2] -eq $gameProcess })
$activations = @($lines | Where-Object { $_ -match '\[ASSETS\] activated' })
$owners = @($activations | ForEach-Object { if ($_ -match "owner='([^']+)'" ) { $Matches[1] } })
$expected = @('base', 'mod/ewithin-rebirth.zip', 'base', 'mod/descent_maximum_fixed.zip', 'mod/ewithin-rebirth.zip', 'base', 'mod/ewithin-rebirth.zip')
if (($owners -join ',') -ne ($expected -join ',')) { throw "Wrong mission owners: $owners" }
$processes = @($activations | ForEach-Object { ($_ -split '\s+')[2] } | Select-Object -Unique)
if ($processes.Count -ne 1) { throw 'Mission switches restarted the game process' }
$trace = $lines -join "`n"
if ($trace -notmatch "file='descent2.s22' loaded_from='[^']*ewithin.dxa'") {
    throw 'Selected ewithin did not load its authored sound bank'
}
$samples = @($lines | Where-Object { $_ -match 'resident_sample=53 ' })
if ($samples.Count -ne $expected.Count) { throw "Expected $($expected.Count) resident robot sample checks, found $($samples.Count)" }
for ($index = 0; $index -lt $expected.Count; $index++) {
    $sample = if ($expected[$index] -eq 'mod/ewithin-rebirth.zip') {
        'loaded_hash=446c95505b42af56 loaded_bytes=16367 bank_match=1'
    } else {
        'loaded_hash=9a629f9713cdff17 loaded_bytes=24098 bank_match=1'
    }
    if ($samples[$index] -notmatch $sample) { throw "Wrong resident robot sound for $($expected[$index]): $($samples[$index])" }
}
Write-Host "Mission asset isolation passed in process $($processes[0]): $logPath"

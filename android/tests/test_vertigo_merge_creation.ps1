# Merge regression on the disposable test emulator; requires local retail Vertigo files
param([switch]$Install)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$devices = @(& $ADB devices | Where-Object { $_ -match '^\S+\s+device$' })
if ($devices.Count -ne 1 -or $devices[0] -notmatch '^emulator-') {
    throw 'Requires exactly one connected device, the disposable test emulator'
}
if ($Install) {
    & $ADB shell am force-stop com.dxxredux.app
    & $ADB shell run-as com.dxxredux.app rm -rf files/d2x-redux/.mission_assets
    & $ADB install -r (Join-Path $repoRoot 'android/app/build/outputs/apk/debug/app-debug.apk')
    if ($LASTEXITCODE -ne 0) { throw 'APK installation failed' }
}
$source = Join-Path $repoRoot 'game_data/extracted/VERTIGO/MISSIONS'
$fixture = Join-Path $repoRoot 'temp/vertigo-probe.zip'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stream = [IO.File]::Create($fixture)
$zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
try {
    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, (Join-Path $source 'D2X.HOG'), 'd2x.hog') | Out-Null
    $descriptor = [IO.File]::ReadAllText((Join-Path $source 'D2X.MN2')).Replace('Descent 2: Vertigo', 'Vertigo Probe')
    $writer = [IO.StreamWriter]::new($zip.CreateEntry('d2x.mn2').Open())
    try { $writer.Write($descriptor) } finally { $writer.Dispose() }
} finally { $zip.Dispose(); $stream.Dispose() }
& $ADB push $fixture /data/local/tmp/vertigo-probe.zip
if ($LASTEXITCODE -ne 0) { throw 'Fixture push failed' }
& $ADB shell run-as com.dxxredux.app mkdir -p files/imported/sets/default/.content/mods
& $ADB shell run-as com.dxxredux.app cp /data/local/tmp/vertigo-probe.zip files/imported/sets/default/.content/mods/vertigo-probe.zip
if ($LASTEXITCODE -ne 0) { throw 'Fixture staging failed' }
& $ADB logcat -c
& (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -ScriptName test_vertigo_merge_creation.jsonc -Game d2 -TimeoutSeconds 240
$testResult = $LASTEXITCODE
$logPath = Join-Path $repoRoot 'temp/vertigo-creation-logcat.txt'
& $ADB logcat -d | Set-Content -LiteralPath $logPath -Encoding utf8
if ($testResult -ne 0) { throw "Automation failed; see $logPath" }
$log = Get-Content -LiteralPath $logPath -Raw
foreach ($stage in @('event=begin', 'stage=before_draw', 'stage=created', 'stage=after_mipmap', 'stage=tap_direct')) {
    if ($log -notmatch ('\[mwall_create\].*' + [regex]::Escape($stage))) { throw "Missing capture: $stage" }
}
if ($log -match '\[mwall_create\].*(read_error|incomplete_fbo|stage=state_query gl_error)') {
    throw "Diagnostic failed; see $logPath"
}
if ($log -match '\[mwall_create\].*(requested_merge_program=0 |requested_is_program=0|stage=(setup|draw|mipmap) gl_error)') {
    throw "Merge shader setup failed; see $logPath"
}
$created = [regex]::Matches($log, '\[mwall_create\] serial=(\d+) stage=created .*?hash=(0x[0-9a-f]+) zero_alpha=(\d+) alpha=(\d+)/(\d+)')
if ($created.Count -lt 2) { throw 'Expected fresh merge captures before and after mission switching' }
foreach ($capture in $created) {
    if ($capture.Groups[3].Value -ne '0' -or $capture.Groups[4].Value -ne '255' -or $capture.Groups[5].Value -ne '255') {
        throw "Vertigo composite is not fully opaque: $($capture.Value)"
    }
    $hash = $capture.Groups[2].Value
    if ($hash -ne $created[0].Groups[2].Value) { throw 'Composite changed across mission switching' }
}
$taps = [regex]::Matches($log, '\[mwall_create\] serial=(\d+) stage=tap_direct .*?hash=(0x[0-9a-f]+) zero_alpha=(\d+) alpha=(\d+)/(\d+)')
$tappedSerials = @($taps | ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
if ($tappedSerials.Count -lt 2) { throw 'Expected taps of separate merges before and after mission switching' }
foreach ($tap in $taps) {
    $serial = $tap.Groups[1].Value
    if ($tap.Groups[2].Value -ne $created[0].Groups[2].Value -or
        $tap.Groups[3].Value -ne '0' -or $tap.Groups[4].Value -ne '255' -or $tap.Groups[5].Value -ne '255' -or
        !($created | Where-Object { $_.Groups[1].Value -eq $serial })) {
        throw "Tap does not match opaque creation: $($tap.Value)"
    }
}
Write-Host "Vertigo opaque creation and tap verified across mission switching: $logPath"

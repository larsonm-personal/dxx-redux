#!/usr/bin/env pwsh
# Install the local original-Pumo playtest bundle without replacing player data
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9._:-]+$')][string]$Serial,
    [Parameter(Mandatory)][string]$Bundle,
    [ValidatePattern('^files/[A-Za-z0-9_-]+(?:/[A-Za-z0-9_-]+)*$')]
    [string]$RetailDataDirectory = 'files/imported/sets/default',
    [string]$Adb = 'C:/local/android-sdk/platform-tools/adb.exe'
)
$ErrorActionPreference = 'Stop'
$Bundle = (Resolve-Path -LiteralPath $Bundle).Path
$manifest = Get-Content -LiteralPath (Join-Path $Bundle 'manifest.json') -Raw | ConvertFrom-Json
foreach ($name in 'app-debug.apk', 'pumo-runtime.tar') {
    $expected = $manifest.files.$name
    if (!$expected -or (Get-FileHash -LiteralPath (Join-Path $Bundle $name) -Algorithm SHA256).Hash -ne $expected) {
        throw "Playtest bundle hash mismatch: $name"
    }
}
function Invoke-PlaytestAdb {
    param([string[]]$Arguments)
    $output = & $Adb -s $Serial @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "adb failed: $output" }
    $output
}
function Invoke-PlaytestPrivate {
    param([string]$Command)
    Invoke-PlaytestAdb @('shell', 'run-as', 'com.dxxredux.app', 'sh', '-c', "'$Command'")
}
Invoke-PlaytestAdb @('install', '-r', (Join-Path $Bundle 'app-debug.apk'))
Invoke-PlaytestAdb @('shell', 'am', 'force-stop', 'com.dxxredux.app') | Out-Null
try {
    Invoke-PlaytestPrivate "test -f $RetailDataDirectory/descent2.ham" | Out-Null
} catch {
    throw "Retail Descent 2 HAM not found at $RetailDataDirectory. Pass -RetailDataDirectory for the selected app-private file set"
}
try {
    Invoke-PlaytestPrivate 'test -d files/d2x-redux/Players' | Out-Null
} catch {
    throw 'Launch Descent 2 with your retail data and create a pilot before installing this bundle'
}
$stamp = [DateTimeOffset]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')
$backup = "files/pumo-playtest-backup-$stamp"
# Back up existing mission resources only; saves, pilots and settings are untouched
Invoke-PlaytestPrivate "mkdir $backup && for p in files/d2x-redux/missions/pmines.hog files/d2x-redux/missions/pmines.mn2 files/d2x-redux/mods/pmines files/d2x-redux/missions/d2x-xl.hog files/d2x-redux/missions/d2x.ham $RetailDataDirectory/data/d2x-xl.hog $RetailDataDirectory/data/d2x.ham; do if test -e `$p; then echo `$p; fi; done > $backup/paths.txt && if test -s $backup/paths.txt; then tar -cf $backup/resources.tar -T $backup/paths.txt; else touch $backup/no-existing-resources; fi" | Out-Null
Invoke-PlaytestAdb @('push', (Join-Path $Bundle 'pumo-runtime.tar'), '/data/local/tmp/pumo-playtest-runtime.tar') | Out-Null
Invoke-PlaytestPrivate 'tar -xf /data/local/tmp/pumo-playtest-runtime.tar' | Out-Null
# Imported mission contexts cannot see the loose mission's sibling addons
# Stage the originals in the selected retail installation, an admitted resource owner
Invoke-PlaytestPrivate "mkdir -p $RetailDataDirectory/data && cp files/d2x-redux/missions/d2x-xl.hog $RetailDataDirectory/data/d2x-xl.hog && cp files/d2x-redux/missions/d2x.ham $RetailDataDirectory/data/d2x.ham && cmp files/d2x-redux/missions/d2x-xl.hog $RetailDataDirectory/data/d2x-xl.hog && cmp files/d2x-redux/missions/d2x.ham $RetailDataDirectory/data/d2x.ham" | Out-Null
Invoke-PlaytestAdb @('shell', 'rm', '/data/local/tmp/pumo-playtest-runtime.tar') | Out-Null
Write-Output "Installed build $($manifest.build). Existing mission resource backup or absence marker: $backup"
Write-Output "Verified camera/Vertigo addons in $RetailDataDirectory/data for imported and loose Pumo missions"
Write-Output 'The APK alone does not install these runtime assets; keep this bundle and its installer together'
Write-Output 'Launch Descent 2, select New Game, XL: [-Pumo Mines-], level 1, and your preferred difficulty'
if ($manifest.launch_notes) { Write-Output $manifest.launch_notes }
else { Write-Output 'Choose the Mineral Valley route in Temporal Hub; PMC Data Storage Center remains unsupported' }

#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. "$PSScriptRoot\..\helpers\test_helpers.ps1"

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$fixtureDir = Join-Path $repoRoot 'temp\game_data_asset_manifest_writer'
$fakeAdb = Join-Path $fixtureDir 'fake_adb.ps1'
$fakeAdbLog = Join-Path $fixtureDir 'fake_adb.log'
$pushedManifest = Join-Path $fixtureDir 'assets.pushed.json'

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Text
    )

    [System.IO.File]::WriteAllText($Path, $Text, [System.Text.UTF8Encoding]::new($false))
}

function New-FixtureFile {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][int]$Size
    )

    $path = Join-Path $fixtureDir $Name
    $bytes = New-Object byte[] $Size
    for ($i = 0; $i -lt $Size; $i++) {
        $bytes[$i] = [byte](($i * 13) % 251)
    }
    [System.IO.File]::WriteAllBytes($path, $bytes)
    return $path
}

try {
    New-Item -ItemType Directory -Path $fixtureDir -Force | Out-Null
    Remove-Item -LiteralPath $fakeAdbLog, $pushedManifest -Force -ErrorAction SilentlyContinue

    Write-Utf8NoBom -Path $fakeAdb -Text @'
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$AdbArgs)
Add-Content -LiteralPath $env:DXX_FAKE_ADB_LOG -Value ($AdbArgs -join "`t")
if ($AdbArgs.Count -ge 3 -and $AdbArgs[0] -eq "push") {
    Copy-Item -LiteralPath $AdbArgs[1] -Destination $env:DXX_FAKE_ADB_PUSHED_MANIFEST -Force
}
exit 0
'@

    $pigHash = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
    $hogHash = 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'
    $otherHash = 'cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc'
    $pigPath = New-FixtureFile -Name 'descent.pig' -Size 17
    $hogPath = New-FixtureFile -Name 'descent.hog' -Size 29

    $index = @{}
    $index[$pigHash] = $pigPath
    $index[$hogHash] = $hogPath
    $script:DEFAULT_SET_DIR = 'files/imported/sets/default'
    $script:PACKAGE = 'com.dxxredux.app'
    $script:ADB = $fakeAdb
    $env:DXX_FAKE_ADB_LOG = $fakeAdbLog
    $env:DXX_FAKE_ADB_PUSHED_MANIFEST = $pushedManifest

    $script:ExistingManifestJson = @(
        [ordered]@{
            filename = 'descent.pig'
            sha256 = $pigHash
            sizeBytes = 1
            importedAt = 123
            versionName = 'Existing D1 data'
            sourceUri = 'content://old-local-copy'
        },
        [ordered]@{
            filename = 'other.dat'
            sha256 = $otherHash
            sizeBytes = 2
            importedAt = 456
            sourceUri = 'content://unrelated'
        },
        [ordered]@{
            filename = 'descent.hog'
            sha256 = $hogHash
            sizeBytes = 3
            importedAt = 789
            sourceUri = 'content://old-hog-copy'
        }
    ) | ConvertTo-Json -Depth 5

    function Adb-Timeout {
        param([string[]]$AdbArgs, [int]$Seconds = 8)
        if (($AdbArgs -join ' ') -match 'cat files/imported/sets/default/assets\.json') {
            return $script:ExistingManifestJson
        }
        return ''
    }

    $jsonDep = [pscustomobject]@{
        file = 'Descent.HOG'
        sha256 = $hogHash
    }
    $hashDep = @{
        file = 'Descent.PIG'
        sha256 = $pigHash
    }
    $externalDep = @{
        file = 'ignored.hog'
        sha256 = $hogHash
        target = '/sdcard/Download'
    }

    $depsByTarget = @{}
    $depsByTarget[$script:DEFAULT_SET_DIR] = @($jsonDep, $hashDep)
    $depsByTarget['/sdcard/Download'] = @($externalDep)

    Write-ResolvedGameDataAssetManifest -DepsByTarget $depsByTarget -Index $index

    if (-not (Test-Path -LiteralPath $pushedManifest -PathType Leaf)) {
        throw 'fake adb did not receive a pushed assets.json'
    }

    $manifestEntries = Get-Content -LiteralPath $pushedManifest -Raw | ConvertFrom-Json
    $entries = @{}
    foreach ($entry in ($manifestEntries | ForEach-Object { $_ })) {
        if (-not $entry.filename) {
            throw "manifest entry missing filename: $(ConvertTo-Json -InputObject $entry -Depth 5)"
        }
        $entries[$entry.filename] = $entry
    }
    if (-not $entries.ContainsKey('descent.pig') -or -not $entries.ContainsKey('descent.hog')) {
        throw "dependency entries missing from manifest: $($entries.Keys -join ', ')"
    }
    $pushedManifestText = Get-Content -LiteralPath $pushedManifest -Raw
    if ($entries['descent.pig'].sizeBytes -ne (Get-Item -LiteralPath $pigPath).Length) {
        throw "descent.pig size was not refreshed from the resolved local file`n$pushedManifestText"
    }
    if ($entries['descent.pig'].importedAt -ne 123) {
        throw 'existing importedAt was not preserved'
    }
    if ($entries['descent.pig'].versionName -ne 'Existing D1 data') {
        throw 'existing versionName for the same SHA was not preserved'
    }
    if ($entries['descent.pig'].PSObject.Properties.Name -contains 'sourceUri') {
        throw 'resolved local dependency entry incorrectly kept sourceUri'
    }
    if ($entries['descent.hog'].sha256 -ne $hogHash -or
        $entries['descent.hog'].sizeBytes -ne (Get-Item -LiteralPath $hogPath).Length) {
        throw 'descent.hog entry does not match the resolved dependency'
    }
    if ($entries['descent.hog'].PSObject.Properties.Name -contains 'versionName') {
        throw 'missing existing versionName was written back as an explicit field'
    }
    if ($entries['other.dat'].sourceUri -ne 'content://unrelated') {
        throw 'unrelated manifest entry was not preserved'
    }

    $adbLog = if (Test-Path -LiteralPath $fakeAdbLog) {
        Get-Content -LiteralPath $fakeAdbLog -Raw
    } else {
        ''
    }
    if ($adbLog -match '/sdcard/Download') {
        throw 'external target unexpectedly received an assets.json push'
    }

    Write-Host 'PASS'
} finally {
    Remove-Item Env:\DXX_FAKE_ADB_LOG -ErrorAction SilentlyContinue
    Remove-Item Env:\DXX_FAKE_ADB_PUSHED_MANIFEST -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $fixtureDir) {
        Remove-Item -LiteralPath $fixtureDir -Recurse -Force
    }
}

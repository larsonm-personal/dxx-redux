#!/usr/bin/env pwsh
# Seeded serial mod campaign with replayable cases and bounded device operations
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [ValidateSet('smoke', 'overnight', 'shared')][string]$Profile = 'smoke',
    [int]$Seed = 20261011,
    [double]$Hours = 0,
    [string]$OutputDirectory,
    [string]$ReplayCase,
    [switch]$Resume,
    [switch]$PrepareOnly,
    [switch]$ControlledFailure
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
. (Join-Path $PSScriptRoot '../helpers/shareware_test_data.ps1')
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
Ensure-EmulatorHealthy | Out-Null
$repo = Split-Path (Split-Path $PSScriptRoot)
& python (Join-Path $PSScriptRoot 'test_mod_fuzz_controls.py')
if ($LASTEXITCODE -ne 0) { throw 'Fuzz controller checks failed' }
if (!$OutputDirectory) {
    $campaignRoot = Join-Path $repo "android/temp/mod-fuzz-$Profile"
    if ($Resume) {
        $latestCampaign = if (Test-Path -LiteralPath $campaignRoot) {
            Get-ChildItem -LiteralPath $campaignRoot -Directory -Filter 'run_*' |
                Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'journal.jsonl') } |
                Sort-Object Name -Descending | Select-Object -First 1
        }
        $OutputDirectory = if ($latestCampaign) { $latestCampaign.FullName } else { $campaignRoot }
    } else { $OutputDirectory = Join-Path $campaignRoot "run_$(Get-Date -Format 'yyyyMMdd_HHmmssfff')" }
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (!$Resume) {
    if (Test-Path -LiteralPath (Join-Path $OutputDirectory 'journal.jsonl')) { throw 'Existing campaign requires -Resume or a new output directory' }
    & (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $OutputDirectory
    New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
}
$configPath = Join-Path $OutputDirectory 'inventory.json'
if (!$Resume) {
    $template = Read-JsoncFile -Path (Join-Path $repo 'android/game_scripts/test_uud2sp.jsonc')
    $mac = Read-JsoncFile -Path (Join-Path $repo 'android/game_scripts/test_dxa_asset_usage.jsonc')
    $vertigo = Read-JsoncFile -Path (Join-Path $repo 'android/game_scripts/test_vertigo_level_metadata.jsonc')
    $index = Read-GameDataIndex
    $fixtures = @{}
    foreach ($mode in @('d1', 'd2', 'd1-in-d2', 'vertigo', 'mac-d1', 'demo-d1', 'demo-d2')) {
        $set = "mod-fuzz-$mode"
        $game = if ($mode -in @('d1', 'mac-d1', 'demo-d1')) { 'd1' } else { 'd2' }
        $mission = if ($mode -eq 'vertigo') { 'Vertigo' } elseif ($mode -in @('d2', 'demo-d2')) { 'Counterstrike' } else { 'First Strike' }
        $deps = @($template[0]._info._deps | ConvertTo-Json -Depth 10 | ConvertFrom-Json)
        if ($mode -eq 'vertigo') { $deps += @($vertigo[0]._info._deps | Where-Object file -Like 'd2x.*') }
        if ($mode -eq 'mac-d1') { $deps = @($mac[0]._info._deps | Where-Object target -Like '*mac*') }
        if ($mode -like 'demo-*') {
            $demo = Get-SharewareTestGameConfig -RepoRoot $repo -Game $game
            $demoDir = Resolve-SharewareTestData -RepoRoot $repo -Game $game
            $mission = $demo.MissionTitle
            $deps = @($demo.RequiredHashes | ForEach-Object {
                    $index[$_.Sha256] = Join-Path $demoDir $_.File
                    [pscustomobject]@{ file = $_.File; sha256 = $_.Sha256; target = '' }
                })
        }
        $assets = @()
        foreach ($dep in $deps) {
            $dep | Add-Member NoteProperty target "files/imported/sets/$set" -Force
            $path = $index[$dep.sha256]
            if (!$path -or !(Test-Path -LiteralPath $path)) { throw "Missing required fixture: $mode $($dep.file) $($dep.sha256)" }
            if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $dep.sha256) { throw "Fixture hash mismatch: $path" }
            $assets += @{ file = $dep.file; path = $path; sha256 = $dep.sha256; size = (Get-Item -LiteralPath $path).Length }
        }
        if (!(Resolve-GameDataDeps -Deps $deps)) { throw "Unable to provision $mode" }
        $fixtures[$mode] = @{ set = $set; game = $game; mission = $mission; deps = $deps; assets = $assets }
    }
    # Child run_test processes resolve the same verified demo files by content hash
    $indexPath = Join-Path $repo 'game_data/game_data_index.txt'
    $indexText = Get-Content -LiteralPath $indexPath -Raw
    foreach ($fixture in $fixtures.Values) {
        foreach ($asset in $fixture.assets) {
            if ($indexText -notmatch "(?m)^$($asset.sha256)  ") {
                $relative = [IO.Path]::GetRelativePath($repo, $asset.path).Replace('\', '/')
                Add-Content -LiteralPath $indexPath -Value "$($asset.sha256)  $relative" -Encoding utf8
                $indexText += "`n$($asset.sha256)  $relative"
            }
        }
    }
    $catalog = Get-Content -LiteralPath (Join-Path $repo 'android/app/src/main/java/com/dxxredux/app/RecommendedModsDialog.kt') -Raw
    $recommended = @([regex]::Matches($catalog, 'filename = "([^"]+\.dxa)"') | ForEach-Object { $_.Groups[1].Value })
    $names = $recommended + @('d1-hires-128-textures-ktx2.dxa', 'd1-hires-512-textures-ktx2.dxa', 'd2-hires-128-textures-ktx2.dxa', 'd2-hires-512-textures-ktx2.dxa')
    $packs = @()
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', 'files/mod-fuzz-import') | Out-Null
    foreach ($name in $names) {
        $path = Join-Path $repo "game_data/mods/d2x-xl/$name"
        if ($name -like 'uud*tp*') { $path = Join-Path $repo "game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/$name" }
        if ($name -eq 'uud2sp-sounds.dxa') { $path = Join-Path $repo "game_data/mods/xfing/d2x_sp/$name" }
        if (!(Test-Path -LiteralPath $path)) { throw "Missing recommended mod or resolution variant: $path" }
        $packs += @{ filename = $name; path = $path; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant(); size = (Get-Item -LiteralPath $path).Length; recommended = $name -in $recommended }
        Adb -AdbArgs @('push', $path, "/data/local/tmp/$name") -Seconds 120 | Out-Null
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', "/data/local/tmp/$name", "files/mod-fuzz-import/$name") -Seconds 60 | Out-Null
        Adb -AdbArgs @('shell', 'rm', '-f', "/data/local/tmp/$name") | Out-Null
    }
    @{ schema = 1; seed = $Seed; serial = $env:ANDROID_SERIAL; package = $script:PACKAGE; fixtures = $fixtures; packs = $packs; template = $template; source_commit = (& git -C $repo rev-parse HEAD); device = (Adb -AdbArgs @('shell', 'getprop')); installed_package = (Adb -AdbArgs @('shell', 'dumpsys', 'package', $script:PACKAGE)) } | ConvertTo-Json -Depth 40 | Set-Content -LiteralPath $configPath -Encoding utf8
}
if ($PrepareOnly) { Write-Host "Prepared $configPath"; exit 0 }
if ($Hours -le 0) { $Hours = if ($Profile -eq 'overnight') { 8 } else { 0.5 } }
$arguments = @((Join-Path $repo 'android/helpers/mod_fuzz.py'), '--inventory', $configPath, '--profile', $Profile, '--hours', $Hours, '--seed', $Seed, '--adb', $script:ADB)
if ($ReplayCase) { $arguments += @('--replay', [IO.Path]::GetFullPath($ReplayCase)) }
if ($Resume) { $arguments += '--resume' }
if ($ControlledFailure) { $arguments += '--controlled-failure' }
& python @arguments
exit $LASTEXITCODE

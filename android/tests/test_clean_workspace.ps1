#!/usr/bin/env pwsh
# Exercise cleanup against a synthetic Git repository, never real build outputs
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$helper = Join-Path $repoRoot 'android/clean-workspace.ps1'
$fixture = Join-Path $repoRoot "temp/cleanup-fixture-$([guid]::NewGuid().ToString('N'))"
$old = [DateTime]::UtcNow.AddDays(-90)
$state = [pscustomobject]@{ Answers = [Collections.Generic.Queue[string]]::new(); Prompts = 0; Busy = $false; OnAnswer = $null; OnIdle = $null; IdleChecks = 0 }
$heldLock = $null

${function:Read-Host} = {
    param([string]$Prompt)
    $state.Prompts++
    if (-not $state.Answers.Count) { throw "Unexpected prompt: $Prompt" }
    if ($state.OnAnswer) { & $state.OnAnswer }
    return $state.Answers.Dequeue()
}.GetNewClosure()

${function:Get-CimInstance} = {
    param([string]$ClassName)
    if ($ClassName -ne 'Win32_Process') { throw "Unexpected inventory: $ClassName" }
    $state.IdleChecks++
    if ($state.OnIdle) { & $state.OnIdle }
    if ($state.Busy) {
        [pscustomobject]@{ ProcessId = -10; Name = 'ninja.exe'; CommandLine = 'synthetic active build' }
    }
}.GetNewClosure()

function New-CleanupFixtureFile {
    param([string]$Relative)
    $path = Join-Path $fixture $Relative
    New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null
    [IO.File]::WriteAllText($path, 'synthetic cleanup test')
}

function Assert-CleanupExists {
    param([string]$Relative, [bool]$Expected = $true)
    if ((Test-Path -LiteralPath (Join-Path $fixture $Relative)) -ne $Expected) {
        throw "Unexpected existence for $Relative, expected $Expected"
    }
}

try {
    New-Item -ItemType Directory -Path $fixture -Force | Out-Null
    & git -C $fixture init --quiet
    if ($LASTEXITCODE -ne 0) { throw 'Fixture git init failed' }
    [IO.File]::WriteAllText((Join-Path $fixture '.gitignore'), "temp/`nandroid/temp/`nbuild*/`ndownloads/`nserver/target/`n")
    foreach ($path in @('temp/old.log', 'temp/recent.log', 'temp/tracked.log',
            'temp/copy [1].tmp', 'temp/review.json', 'temp/runs/old/result.json',
            'temp/runs/new/result.json', 'temp/nested-repo/.git/config',
            'temp/locked/result.log', 'temp/locked/owner.lock',
            'build-old/CMakeCache.txt', 'build-old/output.obj', 'build-old/released.lock',
            'build-protected/keep.cpp', 'downloads/tool.zip',
            'server/target/old/output.o', 'game_data/precious.log',
            'android/regression_demos/precious.dximdemo', 'outside/precious.log')) {
        New-CleanupFixtureFile $path
    }
    & git -C $fixture add -f temp/tracked.log build-protected/keep.cpp
    if ($LASTEXITCODE -ne 0) { throw 'Fixture git add failed' }
    # Untracked, non-ignored files under an otherwise ignored tree are protected
    Add-Content -LiteralPath (Join-Path $fixture '.gitignore') -Value "!temp/`ntemp/*`n!temp/user-notes.txt"
    New-CleanupFixtureFile 'temp/user-notes.txt'
    # Junctions are created and removed by native PowerShell without following them
    $link = Join-Path $fixture 'android/temp/link'
    New-Item -ItemType Directory -Path (Split-Path $link) -Force | Out-Null
    New-Item -ItemType Junction -Path $link -Target (Join-Path $fixture 'outside') | Out-Null
    foreach ($item in Get-ChildItem -LiteralPath $fixture -Recurse -Force | Where-Object {
            -not ($_.Attributes -band [IO.FileAttributes]::ReparsePoint)
        }) { $item.LastWriteTimeUtc = $old }
    (Get-Item -LiteralPath (Join-Path $fixture 'temp/recent.log')).LastWriteTimeUtc = [DateTime]::UtcNow
    (Get-Item -LiteralPath (Join-Path $fixture 'temp/runs/new/result.json')).LastWriteTimeUtc = [DateTime]::UtcNow
    $heldLock = [IO.File]::Open((Join-Path $fixture 'temp/locked/owner.lock'), 'Open', 'Read', 'None')

    & $helper -RepositoryRoot $fixture -Preview
    & $helper -RepositoryRoot $fixture -WhatIf
    Assert-CleanupExists 'temp/old.log'
    if ($state.Prompts) { throw 'Preview prompted' }

    $state.Busy = $true
    $blocked = $false
    try { & $helper -RepositoryRoot $fixture -AutoOnly -BusyWaitSeconds 0 } catch {
        $blocked = $_.Exception.Message -match 'processes are active'
        if (-not $blocked) { throw }
    }
    if (-not $blocked) { throw 'Active build was not blocked' }
    $state.Busy = $false
    # A transient producer is waited out without asking or deleting while busy
    $state.Busy = $true
    $state.IdleChecks = 0
    $state.OnIdle = { if ($state.IdleChecks -ge 2) { $state.Busy = $false } }
    & $helper -RepositoryRoot $fixture -TemporaryOnly -TempDays 3650 -BusyWaitSeconds 2
    $state.OnIdle = $null
    Assert-CleanupExists 'temp/old.log'

    # Fresh and arbitrary-name outputs are disposable without an age limit
    Add-Content -LiteralPath (Join-Path $fixture '.gitignore') -Value "tmp/`nandroid/temp/`n*.tmp`n*.log"
    foreach ($path in @('android/tools/deep/tmp/custom_run/engine/game.exe',
            'android/tools/deep/tmp/custom_run/raw/assets.hog',
            'android/temp/custom_name/results/report.json', 'temp/backup.cpp',
            'temp/emulator/Test.avd/config.ini',
            'temp/generated-build/CMakeCache.txt', 'temp/generated-build/_deps/vendor-src/.git/config',
            'temp/mixed/keep.txt', 'temp/mixed/disposable.bin', 'misc/copy [2].tmp')) {
        New-CleanupFixtureFile $path
    }
    New-Item -ItemType Directory -Path (Join-Path $fixture 'temp/emulator/Test.avd/hardware-qemu.ini.lock') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $fixture 'temp/emulator/Test.avd/snapshot.lock.lock') -Force | Out-Null
    & git -C $fixture add -f temp/mixed/keep.txt outside/precious.log
    & $helper -RepositoryRoot $fixture -TemporaryOnly -AutoOnly
    foreach ($path in @('temp/old.log', 'temp/copy [1].tmp', 'temp/recent.log', 'temp/review.json',
            'temp/runs/old/result.json', 'temp/runs/new/result.json', 'temp/backup.cpp',
            'temp/mixed/disposable.bin', 'temp/generated-build', 'temp/emulator', 'android/tools/deep/tmp',
            'android/temp/custom_name', 'misc/copy [2].tmp')) {
        Assert-CleanupExists $path $false
    }
    foreach ($path in @('temp/tracked.log', 'temp/user-notes.txt', 'temp/mixed/keep.txt',
            'temp/locked/result.log', 'temp/nested-repo/.git/config',
            'build-old/output.obj', 'build-protected/keep.cpp', 'downloads/tool.zip',
            'game_data/precious.log', 'android/regression_demos/precious.dximdemo', 'outside/precious.log')) {
        Assert-CleanupExists $path
    }
    if ($state.Prompts) { throw 'Temporary cleanup prompted' }
    # Normal cleanup also removes old disposable artifacts without asking
    & $helper -RepositoryRoot $fixture
    foreach ($path in @('build-old', 'downloads/tool.zip', 'server/target')) {
        Assert-CleanupExists $path $false
    }
    if ($state.Prompts) { throw 'Default cleanup prompted' }

    # A file created after discovery invalidates the whole deletion candidate
    New-CleanupFixtureFile 'temp/race/original.bin'
    $state.IdleChecks = 0
    $state.OnIdle = {
        if ($state.IdleChecks -eq 2) { New-CleanupFixtureFile 'temp/race/late.bin' }
    }
    & $helper -RepositoryRoot $fixture -TemporaryOnly
    Assert-CleanupExists 'temp/race/late.bin'
    $state.OnIdle = $null
    & $helper -RepositoryRoot $fixture -TemporaryOnly
    Assert-CleanupExists 'temp/race' $false

    # Staging an existing candidate after discovery must also protect it
    New-CleanupFixtureFile 'temp/staged/keep.bin'
    $state.IdleChecks = 0
    $state.OnIdle = {
        if ($state.IdleChecks -eq 2) {
            & git -C $fixture add -f temp/staged/keep.bin
            if ($LASTEXITCODE -ne 0) { throw 'Could not stage candidate' }
        }
    }
    & $helper -RepositoryRoot $fixture -TemporaryOnly
    Assert-CleanupExists 'temp/staged/keep.bin'
    $state.OnIdle = $null
    # Hashed native builds are separate families for each module and configuration
    Add-Content -LiteralPath (Join-Path $fixture '.gitignore') -Value ".cxx/`nandroid/build-outputs/"
    $generationFiles = @(
        'android/app/.cxx/Debug/11111111/arm64-v8a/output.o',
        'android/app/.cxx/Debug/22222222/arm64-v8a/output.o',
        'android/app/.cxx/Debug/33333333/arm64-v8a/output.o',
        'android/app/.cxx/Debug/44444444/arm64-v8a/output.o',
        'android/app/.cxx/Release/55555555/arm64-v8a/output.o',
        'android/app/.cxx/Release/66666666/arm64-v8a/output.o',
        'android/mission-metadata-core/.cxx/Debug/77777777/x86_64/output.o',
        'android/mission-metadata-core/.cxx/Debug/88888888/x86_64/output.o',
        'android/build-outputs/deploy-internal-v100-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.aab',
        'android/build-outputs/deploy-internal-v110-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.aab',
        'android/build-outputs/deploy-release-v100-cccccccccccccccccccccccccccccccc.aab',
        'android/build-outputs/dxx-redux-internal-20260101-120000-v100.aab',
        'android/build-outputs/dxx-redux-internal-20260102-120000-v110.aab'
    )
    foreach ($path in $generationFiles) { New-CleanupFixtureFile $path }
    New-CleanupFixtureFile 'android/app/.cxx/tools/internal/tool-info.txt'
    New-CleanupFixtureFile 'android/app/.cxx/Debug/11111111/arm64-v8a/CMakeCache.txt'
    New-CleanupFixtureFile 'android/app/.cxx/Debug/11111111/arm64-v8a/_deps/vendor-src/.git/config'
    New-CleanupFixtureFile 'android/app/.cxx/Debug/11111111/arm64-v8a/_deps/vendor-src/source.c'
    New-CleanupFixtureFile 'android/app/.cxx/Debug/11111111/arm64-v8a/_deps/vendor-src/tests/submodule/.git'
    New-CleanupFixtureFile 'android/app/.cxx/Debug/11111111/arm64-v8a.stale-20260101-1/_deps/vendor-src/.git/config'
    foreach ($buildName in @('build-native-old', 'temp/experiment/build-native-new', 'build-other-arch')) {
        New-CleanupFixtureFile "$buildName/output.o"
        $triplet = if ($buildName -eq 'build-other-arch') { 'x64-windows' } else { 'x86-windows' }
        [IO.File]::WriteAllText((Join-Path $fixture "$buildName/CMakeCache.txt"),
            "CMAKE_HOME_DIRECTORY:INTERNAL=C:/source/d1`nCMAKE_GENERATOR:INTERNAL=Ninja`nCMAKE_BUILD_TYPE:STRING=Debug`nVCPKG_TARGET_TRIPLET:STRING=$triplet`n")
    }
    foreach ($tree in @('android/app/.cxx', 'android/mission-metadata-core/.cxx', 'android/build-outputs',
            'build-native-old', 'temp/experiment', 'build-other-arch')) {
        foreach ($item in Get-ChildItem -LiteralPath (Join-Path $fixture $tree) -Recurse -Force) {
            $item.LastWriteTimeUtc = $old
        }
        (Get-Item -LiteralPath (Join-Path $fixture $tree)).LastWriteTimeUtc = $old
    }
    foreach ($path in @($generationFiles[1], $generationFiles[5], $generationFiles[7], $generationFiles[9], $generationFiles[12],
            'temp/experiment/build-native-new/output.o')) {
        (Get-Item -LiteralPath (Join-Path $fixture $path)).LastWriteTimeUtc = $old.AddDays(20)
    }
    (Get-Item -LiteralPath (Join-Path $fixture $generationFiles[2])).LastWriteTimeUtc = [DateTime]::UtcNow.AddHours(-2)
    (Get-Item -LiteralPath (Join-Path $fixture $generationFiles[3])).LastWriteTimeUtc = [DateTime]::UtcNow.AddHours(-1)
    $beforeBuilds = $state.Prompts
    & $helper -RepositoryRoot $fixture -Preview
    Assert-CleanupExists $generationFiles[0]
    & $helper -RepositoryRoot $fixture -BuildsOnly -AutoOnly -KeepBuildGenerations 2 -BuildGraceHours 24
    Assert-CleanupExists $generationFiles[0] $false
    Assert-CleanupExists $generationFiles[1] $false
    Assert-CleanupExists $generationFiles[4]
    New-CleanupFixtureFile 'temp/builds-only.log'
    (Get-Item -LiteralPath (Join-Path $fixture 'temp/builds-only.log')).LastWriteTimeUtc = $old
    & $helper -RepositoryRoot $fixture -BuildsOnly -BuildGraceHours 24
    Assert-CleanupExists 'temp/builds-only.log'
    Assert-CleanupExists 'android/app/.cxx/tools/internal/tool-info.txt'
    foreach ($path in @($generationFiles[4], $generationFiles[6], $generationFiles[8], $generationFiles[11], 'build-native-old')) {
        Assert-CleanupExists $path $false
    }
    foreach ($path in @($generationFiles[2], $generationFiles[3], $generationFiles[5], $generationFiles[7],
            $generationFiles[9], $generationFiles[10], $generationFiles[12], 'build-other-arch', 'temp/experiment/build-native-new')) {
        Assert-CleanupExists $path
    }
    if ($state.Prompts -ne $beforeBuilds) { throw 'Superseded builds prompted for confirmation' }
    Add-Content -LiteralPath (Join-Path $fixture '.gitignore') -Value '/android/temp/'
    & $helper -RepositoryRoot $fixture -BuildsOnly
    Assert-CleanupExists $generationFiles[2] $false
    Assert-CleanupExists $generationFiles[3]
    # Producer startup keeps exactly three prior hashes, including equal fresh timestamps
    $producerFiles = 5..9 | ForEach-Object { "android/app/.cxx/Debug/test000$_/arm64-v8a/output.o" }
    foreach ($path in $producerFiles) { New-CleanupFixtureFile $path }
    $sameTime = [DateTime]::UtcNow
    foreach ($item in Get-ChildItem -LiteralPath (Join-Path $fixture 'android/app/.cxx/Debug') -Recurse -Force) {
        $item.LastWriteTimeUtc = $sameTime
    }
    & $helper -RepositoryRoot $fixture -BuildsOnly -Producer -BuildRoots (Join-Path $fixture 'android/app/.cxx') -KeepBuildGenerations 3
    if (@(Get-ChildItem -LiteralPath (Join-Path $fixture 'android/app/.cxx/Debug') -Directory).Count -ne 3) {
        throw 'Native producer startup must retain exactly three prior hashes, even with timestamp ties'
    }
    New-CleanupFixtureFile 'android/app/.cxx/Debug/testnew0/arm64-v8a/output.o'
    if (@(Get-ChildItem -LiteralPath (Join-Path $fixture 'android/app/.cxx/Debug') -Directory).Count -ne 4) {
        throw 'Native production should leave four hashes'
    }
    $run = 'android/temp/guidebot_simulation_regression/20260904_120000'
    $worker = 'android/temp/mission_zip_host_metadata/20260904_120000/workers/0001'
    foreach ($path in @("$run/raw/mission/assets.hog", "$run/stages/mission/assets.hog",
            "$run/raw/mission.metadata.json", "$run/results/level.json", "$run/summary.json",
            "$run/raw/recent/assets.hog", "$run/raw/tracked/notes.txt",
            "$worker/raw/partial/assets.hog", "$worker/stages/partial/assets.hog",
            "$worker/logs/worker.log", 'android/temp/unrecognized/20260904_120000/raw/mission/assets.hog')) {
        New-CleanupFixtureFile $path
    }
    & git -C $fixture add -f "$run/raw/tracked/notes.txt"
    foreach ($collection in @('android/temp/guidebot_simulation_regression', 'android/temp/mission_zip_host_metadata')) {
        foreach ($item in Get-ChildItem -LiteralPath (Join-Path $fixture $collection) -Recurse -Force) {
            $item.CreationTimeUtc = $old
            $item.LastWriteTimeUtc = $old
        }
    }
    # Copied assets can have old modification dates but new creation dates
    (Get-Item -LiteralPath (Join-Path $fixture "$run/raw/recent/assets.hog")).CreationTimeUtc = [DateTime]::UtcNow
    & $helper -RepositoryRoot $fixture -PayloadsOnly -Preview
    Assert-CleanupExists "$run/raw/mission/assets.hog"
    $state.Busy = $true
    $blocked = $false
    try { & $helper -RepositoryRoot $fixture -PayloadsOnly -AutoOnly -BusyWaitSeconds 0 } catch { $blocked = $true }
    $state.Busy = $false
    if (-not $blocked) { throw 'Active regression payload cleanup was not blocked' }
    & $helper -RepositoryRoot $fixture -PayloadsOnly -AutoOnly
    foreach ($path in @("$run/raw/mission", "$run/stages/mission", "$worker/raw/partial", "$worker/stages/partial")) {
        Assert-CleanupExists $path $false
    }
    foreach ($path in @("$run/raw/mission.metadata.json", "$run/results/level.json", "$run/summary.json",
            "$run/raw/recent/assets.hog", "$run/raw/tracked/notes.txt", "$worker/logs/worker.log",
            'android/temp/unrecognized/20260904_120000/raw/mission/assets.hog')) {
        Assert-CleanupExists $path
    }
    if ($state.Prompts -ne $beforeBuilds) { throw 'Regression payload cleanup prompted' }
    Write-Host 'PASS: unattended recursive temp cleanup, safety, build generations, and payload retention'
} finally {
    if ($heldLock) { $heldLock.Dispose() }
    # Validate the fixture boundary before removing only this test's own files
    $expectedPrefix = [IO.Path]::GetFullPath((Join-Path $repoRoot 'temp/cleanup-fixture-'))
    $resolvedFixture = [IO.Path]::GetFullPath($fixture)
    if (-not $resolvedFixture.StartsWith($expectedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Unexpected cleanup fixture path'
    }
    $fixtureLink = Join-Path $fixture 'android/temp/link'
    if (Test-Path -LiteralPath $fixtureLink) { Remove-Item -LiteralPath $fixtureLink -Force }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}

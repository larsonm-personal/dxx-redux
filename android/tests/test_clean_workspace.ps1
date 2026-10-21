#!/usr/bin/env pwsh
# Exercise cleanup against a synthetic Git repository, never real build outputs
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$helper = Join-Path $repoRoot 'android/clean-workspace.ps1'
$fixture = Join-Path $repoRoot "temp/cleanup-fixture-$([guid]::NewGuid().ToString('N'))"
$old = [DateTime]::UtcNow.AddDays(-90)
$state = [pscustomobject]@{ Answers = [Collections.Generic.Queue[string]]::new(); Prompts = 0; Busy = $false; BusyName = 'ninja.exe'; BusyCommandLine = 'synthetic active build'; OnAnswer = $null; OnIdle = $null; IdleChecks = 0 }
$heldLock = $null

${function:Read-Host} = {
    param([string]$Prompt)
    $state.Prompts++
    if (-not $state.Answers.Count) { throw "Unexpected prompt: $Prompt" }
    if ($state.OnAnswer) { & $state.OnAnswer }
    return $state.Answers.Dequeue()
}.GetNewClosure()

${function:Get-DxxHostProcessInventory} = {
    $state.IdleChecks++
    if ($state.OnIdle) { & $state.OnIdle }
    if ($state.Busy) {
        [pscustomobject]@{ ProcessId = -10; ParentProcessId = 0; Name = $state.BusyName; CommandLine = $state.BusyCommandLine }
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
    [IO.File]::WriteAllText((Join-Path $fixture '.gitignore'), "temp/`nandroid/temp/`nandroid/store-assets/`nbuild*/`ndownloads/`nserver/target/`nandroid/tools/code-quality/python/`nandroid/tools/code-quality/node-path.txt`n")
    foreach ($path in @('temp/old.log', 'temp/recent.log', 'temp/tracked.log',
            'temp/copy [1].tmp', 'temp/review.json', 'temp/runs/old/result.json',
            'temp/runs/new/result.json', 'temp/nested-repo/.git/config',
            'temp/locked/result.log', 'temp/locked/owner.lock',
            'build-old/CMakeCache.txt', 'build-old/output.obj', 'build-old/released.lock',
            'build-protected/keep.cpp', 'downloads/tool.zip',
            'server/target/old/output.o', 'game_data/precious.log',
            'android/tools/code-quality/python/ruff/__main__.py', 'android/tools/code-quality/node-path.txt',
            'android/store-assets/capture_20260101_010101/selected-stills/01.png',
            'android/store-assets/capture_20260101_010101/build.log',
            'android/store-assets/capture_20260101_010101/temp/source.bin',
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
    $linkType = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { 'Junction' } else { 'SymbolicLink' }
    New-Item -ItemType $linkType -Path $link -Target (Join-Path $fixture 'outside') | Out-Null
    foreach ($item in Get-ChildItem -LiteralPath $fixture -Recurse -Force | Where-Object {
            -not ($_.Attributes -band ([IO.FileAttributes]::ReparsePoint -bor [IO.FileAttributes]::ReadOnly))
        }) { $item.LastWriteTimeUtc = $old }
    (Get-Item -Force -LiteralPath (Join-Path $fixture 'temp/recent.log')).LastWriteTimeUtc = [DateTime]::UtcNow
    (Get-Item -Force -LiteralPath (Join-Path $fixture 'temp/runs/new/result.json')).LastWriteTimeUtc = [DateTime]::UtcNow
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
            'game_data/precious.log', 'android/store-assets/capture_20260101_010101/selected-stills/01.png',
            'android/store-assets/capture_20260101_010101/build.log',
            'android/store-assets/capture_20260101_010101/temp/source.bin',
            'android/regression_demos/precious.dximdemo', 'outside/precious.log')) {
        Assert-CleanupExists $path
    }
    if ($state.Prompts) { throw 'Temporary cleanup prompted' }
    # A protected owner must not pin its entire ignored scratch collection
    foreach ($path in @('android/temp/lock-collection/locked/result.json',
            'android/temp/lock-collection/locked/producer.lock',
            'android/temp/lock-collection/disposable/result.json',
            'android/temp/marker-collection/locked/result.json',
            'android/temp/marker-collection/disposable/result.json',
            'android/temp/custom-emulator/avd/config.ini',
            'android/temp/custom-emulator/avd/userdata.img')) {
        New-CleanupFixtureFile $path
    }
    $collectionLock = [IO.File]::Open((Join-Path $fixture 'android/temp/lock-collection/locked/producer.lock'), 'Open', 'Read', 'None')
    try {
        Set-Content -LiteralPath (Join-Path $fixture 'android/temp/custom-emulator/avd/config.ini') -Value 'image.sysdir.1=system-images/android-34/google_apis/x86_64/'
        New-Item -ItemType Directory -Path (Join-Path $fixture 'android/temp/marker-collection/locked/unknown.lock') -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $fixture 'android/temp/custom-emulator/avd/hardware-qemu.ini.lock') -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $fixture 'android/temp/custom-emulator/avd/snapshot.lock.lock') -Force | Out-Null
        & $helper -RepositoryRoot $fixture -TemporaryOnly -Preview
        Assert-CleanupExists 'android/temp/lock-collection/disposable/result.json'
        Assert-CleanupExists 'android/temp/custom-emulator/avd/userdata.img'
        & $helper -RepositoryRoot $fixture -TemporaryOnly
        Assert-CleanupExists 'android/temp/lock-collection/locked/result.json'
        Assert-CleanupExists 'android/temp/marker-collection/locked/result.json'
        Assert-CleanupExists 'android/temp/lock-collection/disposable' $false
        Assert-CleanupExists 'android/temp/marker-collection/disposable' $false
        Assert-CleanupExists 'android/temp/custom-emulator/avd/config.ini'
        Assert-CleanupExists 'android/temp/custom-emulator/avd/userdata.img' $false
        # Git protection within a locked owner must not cause descent past its lock
        & git -C $fixture add -f android/temp/lock-collection/locked/result.json
        New-CleanupFixtureFile 'android/temp/lock-collection/locked/ignored.bin'
        & $helper -RepositoryRoot $fixture -TemporaryOnly
        Assert-CleanupExists 'android/temp/lock-collection/locked/ignored.bin'
    } finally { $collectionLock.Dispose() }
    # Normal cleanup also removes old disposable artifacts without asking
    & $helper -RepositoryRoot $fixture
    foreach ($path in @('build-old', 'downloads/tool.zip', 'server/target')) {
        Assert-CleanupExists $path $false
    }
    if ($state.Prompts) { throw 'Default cleanup prompted' }
    Assert-CleanupExists 'android/tools/code-quality/python/ruff/__main__.py'
    Assert-CleanupExists 'android/tools/code-quality/node-path.txt'

    # Recent Cargo incremental state should not accumulate beside warm executables
    foreach ($path in @('server/target/debug/incremental/session/work.o',
            'server/target/release/incremental/session/work.o',
            'server/target/debug/server.exe', 'server/target/debug/deps/library.rlib')) {
        New-CleanupFixtureFile $path
    }
    & $helper -RepositoryRoot $fixture -BuildsOnly -Preview
    Assert-CleanupExists 'server/target/debug/incremental/session/work.o'
    & $helper -RepositoryRoot $fixture -BuildsOnly
    Assert-CleanupExists 'server/target/debug/incremental' $false
    Assert-CleanupExists 'server/target/release/incremental' $false
    Assert-CleanupExists 'server/target/debug/server.exe'
    Assert-CleanupExists 'server/target/debug/deps/library.rlib'
    New-CleanupFixtureFile 'server/target/debug/incremental/session/work.o'
    New-CleanupFixtureFile 'server/target/debug/incremental/session/producer.lock'
    $cacheLock = [IO.File]::Open((Join-Path $fixture 'server/target/debug/incremental/session/producer.lock'), 'Open', 'Read', 'None')
    try {
        & $helper -RepositoryRoot $fixture -BuildsOnly
        Assert-CleanupExists 'server/target/debug/incremental/session/work.o'
    } finally { $cacheLock.Dispose() }

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
    if ([Environment]::OSVersion.Platform -eq [PlatformID]::Unix) {
        $sourcePath = Join-Path $fixture 'android/app/.cxx/Debug/11111111/arm64-v8a/_deps/vendor-src/source.c'
        New-Item -ItemType SymbolicLink -Path ($sourcePath + '.link') -Target $sourcePath | Out-Null
    }
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
        (Get-Item -Force -LiteralPath (Join-Path $fixture $tree)).LastWriteTimeUtc = $old
    }
    foreach ($path in @($generationFiles[1], $generationFiles[5], $generationFiles[7], $generationFiles[9], $generationFiles[12],
            'temp/experiment/build-native-new/output.o')) {
        (Get-Item -Force -LiteralPath (Join-Path $fixture $path)).LastWriteTimeUtc = $old.AddDays(20)
    }
    (Get-Item -Force -LiteralPath (Join-Path $fixture $generationFiles[2])).LastWriteTimeUtc = [DateTime]::UtcNow.AddHours(-2)
    (Get-Item -Force -LiteralPath (Join-Path $fixture $generationFiles[3])).LastWriteTimeUtc = [DateTime]::UtcNow.AddHours(-1)
    $beforeBuilds = $state.Prompts
    & $helper -RepositoryRoot $fixture -Preview
    Assert-CleanupExists $generationFiles[0]
    & $helper -RepositoryRoot $fixture -BuildsOnly -AutoOnly -KeepBuildGenerations 2 -BuildGraceHours 24
    Assert-CleanupExists $generationFiles[0] $false
    Assert-CleanupExists $generationFiles[1] $false
    Assert-CleanupExists $generationFiles[4]
    New-CleanupFixtureFile 'temp/builds-only.log'
    (Get-Item -Force -LiteralPath (Join-Path $fixture 'temp/builds-only.log')).LastWriteTimeUtc = $old
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
    $state.Busy = $true
    $state.BusyName = 'qemu-system-x86_64.exe'
    $blocked = $false
    try { & $helper -RepositoryRoot $fixture -BuildsOnly -BusyWaitSeconds 0 } catch {
        $blocked = $_.Exception.Message -match 'processes are active'
    }
    if (-not $blocked) { throw 'General cleanup must still protect running emulators' }
    & $helper -RepositoryRoot $fixture -BuildsOnly -Producer -BuildRoots (Join-Path $fixture 'android/app/.cxx') -KeepBuildGenerations 3 -BusyWaitSeconds 0
    $state.Busy = $false
    $state.BusyName = 'ninja.exe'
    if (@(Get-ChildItem -LiteralPath (Join-Path $fixture 'android/app/.cxx/Debug') -Directory).Count -ne 3) {
        throw 'Native producer startup must retain exactly three prior hashes, even with timestamp ties'
    }
    # An already-trimmed producer scope is a safe no-op even during another job
    $state.Busy = $true
    & $helper -RepositoryRoot $fixture -BuildsOnly -Producer -BuildRoots (Join-Path $fixture 'android/app/.cxx') -KeepBuildGenerations 3 -BusyWaitSeconds 0
    New-CleanupFixtureFile 'android/app/.cxx/Debug/testnew0/arm64-v8a/output.o'
    $blocked = $false
    try { & $helper -RepositoryRoot $fixture -BuildsOnly -Producer -BuildRoots (Join-Path $fixture 'android/app/.cxx') -KeepBuildGenerations 3 -BusyWaitSeconds 0 } catch {
        $blocked = $_.Exception.Message -match 'processes are active'
        if (-not $blocked) { throw }
    }
    if (-not $blocked) { throw 'Producer cleanup must protect deletable generations during active jobs' }
    $state.Busy = $false
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
    (Get-Item -Force -LiteralPath (Join-Path $fixture "$run/raw/recent/assets.hog")).CreationTimeUtc = [DateTime]::UtcNow
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
    # Explicit exclusions preserve entire evidence trees without pinning siblings
    New-CleanupFixtureFile 'temp/excluded-evidence/deep/report.json'
    New-CleanupFixtureFile 'temp/exclusion-sibling/delete.bin'
    & $helper -RepositoryRoot $fixture -TemporaryOnly -ExcludePaths 'temp/excluded-evidence'
    Assert-CleanupExists 'temp/excluded-evidence/deep/report.json'
    Assert-CleanupExists 'temp/exclusion-sibling/delete.bin' $false
    $badExclude = $false
    try { & $helper -RepositoryRoot $fixture -Preview -ExcludePaths (Split-Path $fixture) } catch {
        $badExclude = $_.Exception.Message -match 'Excluded path must be below'
    }
    if (-not $badExclude) { throw 'Outside-repository exclusion was accepted' }

    # A satisfied free-space target is a true no-op, including while builds run
    New-CleanupFixtureFile 'temp/free-target/keep.bin'
    $state.Busy = $true
    & $helper -RepositoryRoot $fixture -TemporaryOnly -TargetFreeSpaceGB 0.001
    $state.Busy = $false
    Assert-CleanupExists 'temp/free-target/keep.bin'

    # Explicit emulator-aware temp cleanup preserves whole AVDs and other evidence
    New-CleanupFixtureFile 'temp/active-emulator/avd/userdata-qemu.img.qcow2'
    New-CleanupFixtureFile 'temp/active-emulator/avd/keep.log'
    New-CleanupFixtureFile 'temp/emulator-unrelated/delete.bin'
    $state.Busy = $true
    $state.BusyName = 'qemu-system-x86_64.exe'
    $state.BusyCommandLine = 'emulator -avd ActiveTest'
    & $helper -RepositoryRoot $fixture -TemporaryOnly -PreserveEmulatorWorkspaces -ExcludePaths 'temp/excluded-evidence' -BusyWaitSeconds 0
    Assert-CleanupExists 'temp/active-emulator/avd/userdata-qemu.img.qcow2'
    Assert-CleanupExists 'temp/active-emulator/avd/keep.log'
    Assert-CleanupExists 'temp/excluded-evidence/deep/report.json'
    Assert-CleanupExists 'temp/emulator-unrelated/delete.bin' $false
    $state.BusyCommandLine = ''
    $blocked = $false
    try { & $helper -RepositoryRoot $fixture -TemporaryOnly -PreserveEmulatorWorkspaces -BusyWaitSeconds 0 } catch {
        $blocked = $_.Exception.Message -match 'processes are active'
        if (-not $blocked) { throw }
    }
    if (-not $blocked) { throw 'Unreadable emulator command was not blocked' }
    $state.BusyCommandLine = 'emulator -avd ActiveTest -data custom.bin'
    $blocked = $false
    try { & $helper -RepositoryRoot $fixture -TemporaryOnly -PreserveEmulatorWorkspaces -BusyWaitSeconds 0 } catch {
        $blocked = $_.Exception.Message -match 'processes are active'
        if (-not $blocked) { throw }
    }
    if (-not $blocked) { throw 'Custom emulator image override was not blocked' }
    $state.BusyCommandLine = 'emulator -avd ActiveTest "-data" "custom path.bin"'
    $blocked = $false
    try { & $helper -RepositoryRoot $fixture -TemporaryOnly -PreserveEmulatorWorkspaces -BusyWaitSeconds 0 } catch {
        $blocked = $_.Exception.Message -match 'processes are active'
        if (-not $blocked) { throw }
    }
    if (-not $blocked) { throw 'Quoted custom emulator image override was not blocked' }
    $state.Busy = $false
    $state.BusyName = 'ninja.exe'
    $state.BusyCommandLine = 'synthetic active build'

    # Explicit zero retention can retire a configuration without a replacement
    $state.Busy = $true
    $state.BusyName = 'qemu-system-x86_64.exe'
    $state.BusyCommandLine = 'emulator -avd ActiveTest'
    New-CleanupFixtureFile 'android/app/.cxx/Recovery/old00000/arm64-v8a/output.o'
    New-CleanupFixtureFile 'android/app/.cxx/Recovery/old00000/arm64-v8a/_deps/lib/value.a'
    foreach ($item in Get-ChildItem -LiteralPath (Join-Path $fixture 'android/app/.cxx/Recovery/old00000') -Recurse -Force) {
        $item.LastWriteTimeUtc = $old
    }
    (Get-Item -LiteralPath (Join-Path $fixture 'android/app/.cxx/Recovery/old00000')).LastWriteTimeUtc = $old
    New-CleanupFixtureFile 'android/app/.cxx/Recovery/new00000/arm64-v8a/output.o'
    New-CleanupFixtureFile 'android/app/.cxx/Recovery/keep0000/arm64-v8a/output.o'
    New-CleanupFixtureFile 'android/app/build.gradle.kts'
    New-CleanupFixtureFile 'android/app/build/intermediates/old.bin'
    New-CleanupFixtureFile 'server/Cargo.toml'
    New-CleanupFixtureFile 'server/target/debug/deps/old.bin'
    New-CleanupFixtureFile 'server/target/debug/incremental/session/value.o'
    foreach ($path in @('android/app/build', 'server/target')) {
        foreach ($item in Get-ChildItem -LiteralPath (Join-Path $fixture $path) -Recurse -Force) {
            $item.LastWriteTimeUtc = $old
        }
        (Get-Item -LiteralPath (Join-Path $fixture $path)).LastWriteTimeUtc = $old
    }
    & $helper -RepositoryRoot $fixture -BuildsOnly -KeepBuildGenerations 0 -BuildGraceHours 24 -PreserveEmulatorWorkspaces -BusyWaitSeconds 0 -ExcludePaths 'android/app/.cxx/Recovery/keep0000'
    Assert-CleanupExists 'android/app/.cxx/Recovery/old00000' $false
    Assert-CleanupExists 'android/app/build' $false
    Assert-CleanupExists 'server/target' $false
    Assert-CleanupExists 'android/app/build.gradle.kts'
    Assert-CleanupExists 'server/Cargo.toml'
    Assert-CleanupExists 'android/app/.cxx/Recovery/new00000/arm64-v8a/output.o'
    Assert-CleanupExists 'android/app/.cxx/Recovery/keep0000/arm64-v8a/output.o'
    Assert-CleanupExists 'temp/active-emulator/avd/userdata-qemu.img.qcow2'
    $state.BusyName = 'ninja.exe'
    $blocked = $false
    try { & $helper -RepositoryRoot $fixture -BuildsOnly -KeepBuildGenerations 0 -PreserveEmulatorWorkspaces -BusyWaitSeconds 0 } catch {
        $blocked = $_.Exception.Message -match 'processes are active'
        if (-not $blocked) { throw }
    }
    if (-not $blocked) { throw 'Zero build retention ignored an active native build' }
    $state.Busy = $false

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
    if (Test-Path -LiteralPath $fixtureLink) { [IO.Directory]::Delete($fixtureLink) }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}

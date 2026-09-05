#!/usr/bin/env pwsh
# Exercise cleanup against a synthetic Git repository, never real build outputs
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$helper = Join-Path $repoRoot 'android/clean-workspace.ps1'
$fixture = Join-Path $repoRoot "temp/cleanup-fixture-$([guid]::NewGuid().ToString('N'))"
$old = [DateTime]::UtcNow.AddDays(-90)
$state = [pscustomobject]@{ Answers = [Collections.Generic.Queue[string]]::new(); Prompts = 0; Busy = $false; OnAnswer = $null }
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
    try { & $helper -RepositoryRoot $fixture -AutoOnly } catch {
        $blocked = $_.Exception.Message -match 'processes are active'
        if (-not $blocked) { throw }
    }
    if (-not $blocked) { throw 'Active build was not blocked' }
    $state.Busy = $false

    & $helper -RepositoryRoot $fixture -AutoOnly
    Assert-CleanupExists 'temp/old.log' $false
    Assert-CleanupExists 'temp/copy [1].tmp' $false
    foreach ($path in @('temp/recent.log', 'temp/tracked.log', 'temp/user-notes.txt',
            'temp/review.json', 'temp/runs/old/result.json', 'temp/runs/new/result.json',
            'temp/locked/result.log', 'temp/nested-repo/.git/config',
            'build-old/output.obj', 'build-protected/keep.cpp', 'downloads/tool.zip',
            'game_data/precious.log', 'android/regression_demos/precious.dximdemo', 'outside/precious.log')) {
        Assert-CleanupExists $path
    }
    if ($state.Prompts) { throw 'AutoOnly prompted' }

    $state.Answers.Enqueue('noToAll')
    & $helper -RepositoryRoot $fixture
    if ($state.Prompts -ne 1) { throw 'NoToAll did not suppress later prompts' }
    Assert-CleanupExists 'build-old/output.obj'

    # Explicit approval is required for each remaining ambiguous artifact
    1..20 | ForEach-Object { $state.Answers.Enqueue('yes') }
    & $helper -RepositoryRoot $fixture
    foreach ($path in @('build-old', 'downloads/tool.zip', 'temp/review.json', 'temp/runs/old', 'server/target')) {
        Assert-CleanupExists $path $false
    }
    foreach ($path in @('temp/recent.log', 'temp/tracked.log', 'temp/user-notes.txt',
            'build-protected/keep.cpp', 'outside/precious.log', 'temp/locked/result.log')) {
        Assert-CleanupExists $path
    }
    # Recheck files created and Git state changed while a prompt is open
    New-CleanupFixtureFile 'build-race/output.obj'
    (Get-Item -LiteralPath (Join-Path $fixture 'build-race/output.obj')).LastWriteTimeUtc = $old
    (Get-Item -LiteralPath (Join-Path $fixture 'build-race')).LastWriteTimeUtc = $old
    $state.Answers.Clear()
    $state.Answers.Enqueue('yes')
    $raceFile = Join-Path $fixture 'build-race/new-result.log'
    $state.OnAnswer = { [IO.File]::WriteAllText($raceFile, 'new result during prompt') }.GetNewClosure()
    & $helper -RepositoryRoot $fixture
    Assert-CleanupExists 'build-race/new-result.log'

    New-CleanupFixtureFile 'build-stage/keep.obj'
    (Get-Item -LiteralPath (Join-Path $fixture 'build-stage/keep.obj')).LastWriteTimeUtc = $old
    (Get-Item -LiteralPath (Join-Path $fixture 'build-stage')).LastWriteTimeUtc = $old
    $state.Answers.Enqueue('yes')
    $state.OnAnswer = {
        & git -C $fixture add -f build-stage/keep.obj
        if ($LASTEXITCODE -ne 0) { throw 'Could not stage fixture during prompt' }
    }.GetNewClosure()
    & $helper -RepositoryRoot $fixture
    Assert-CleanupExists 'build-stage/keep.obj'

    New-CleanupFixtureFile 'temp/decline.json'
    (Get-Item -LiteralPath (Join-Path $fixture 'temp/decline.json')).LastWriteTimeUtc = $old
    $state.OnAnswer = $null
    $state.Answers.Enqueue('')
    & $helper -RepositoryRoot $fixture
    Assert-CleanupExists 'temp/decline.json'
    $state.Answers.Enqueue('quit')
    & $helper -RepositoryRoot $fixture
    Assert-CleanupExists 'temp/decline.json'
    New-CleanupFixtureFile 'temp/group.json'
    (Get-Item -LiteralPath (Join-Path $fixture 'temp/group.json')).LastWriteTimeUtc = $old
    $state.Answers.Enqueue('folder')
    $state.Answers.Enqueue('yes')
    $beforeGroup = $state.Prompts
    & $helper -RepositoryRoot $fixture
    if ($state.Prompts -ne $beforeGroup + 2) { throw 'Folder approval did not replace individual prompts' }
    Assert-CleanupExists 'temp/decline.json' $false
    Assert-CleanupExists 'temp/group.json' $false
    Assert-CleanupExists 'temp/recent.log'
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
    & $helper -RepositoryRoot $fixture -AutoOnly -KeepBuildGenerations 2
    Assert-CleanupExists $generationFiles[0] $false
    Assert-CleanupExists $generationFiles[1] $false
    Assert-CleanupExists $generationFiles[4]
    New-CleanupFixtureFile 'temp/builds-only.log'
    (Get-Item -LiteralPath (Join-Path $fixture 'temp/builds-only.log')).LastWriteTimeUtc = $old
    & $helper -RepositoryRoot $fixture -BuildsOnly
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
    Write-Host 'PASS: cleanup safety, prompts, build generations, generated dependency clones, and retained configurations'
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

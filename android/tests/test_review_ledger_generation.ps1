#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$fixture = Join-Path $repoRoot ('android/temp/review_ledger_generation_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $fixture
$utf8 = [Text.UTF8Encoding]::new($false)

function Write-Fixture {
    param([string]$Path, [string]$Text)
    $destination = Join-Path $fixture $Path
    [IO.Directory]::CreateDirectory((Split-Path $destination)) | Out-Null
    [IO.File]::WriteAllText($destination, $Text, $utf8)
}

function Invoke-FixtureGit {
    param([string[]]$GitArguments)
    $output = @(& git -C $fixture -c user.name=Fixture -c user.email=fixture@example.invalid -c commit.gpgsign=false -c core.autocrlf=false -c "core.hooksPath=$fixture/no-hooks" @GitArguments)
    if ($LASTEXITCODE -ne 0) { throw "Fixture git failed: $GitArguments" }
    return $output
}

try {
    $helper = 'android/helpers/new_adversarial_review_ledger.ps1'
    Write-Fixture $helper ([IO.File]::ReadAllText((Join-Path $repoRoot $helper)))
    $source = 'android/app/src/main/cpp/shared'
    foreach ($name in @('original', 'edited-old', 'modified')) {
        Write-Fixture "$source/$name.c" ((1..30 | ForEach-Object { "int $($name.Replace('-', '_'))_$_ = $_;" }) -join "`n")
    }
    Write-Fixture "$source/deleted.c" 'void deleted(void) {}'
    Write-Fixture 'android/tests/old space.cpp' 'int test_space = 42;'
    $unicodePath = "$source/caf$([char]0xe9).c"
    Write-Fixture $unicodePath 'int unicode_value = 1;'
    Write-Fixture 'android/helpers/mode.sh' "#!/bin/sh`necho fixture`n"
    Invoke-FixtureGit @('init', '--quiet') | Out-Null
    Invoke-FixtureGit @('add', '.') | Out-Null
    Invoke-FixtureGit @('commit', '--quiet', '-m', 'before') | Out-Null
    $base = (Invoke-FixtureGit @('rev-parse', 'HEAD')) -join ''

    Move-Item -LiteralPath (Join-Path $fixture "$source/original.c") -Destination (Join-Path $fixture "$source/renamed.c")
    Move-Item -LiteralPath (Join-Path $fixture "$source/edited-old.c") -Destination (Join-Path $fixture "$source/edited-new.c")
    $edited = [IO.File]::ReadAllText((Join-Path $fixture "$source/edited-new.c"))
    Write-Fixture "$source/edited-new.c" ($edited.Replace('= 15;', '= 99;'))
    Write-Fixture "$source/modified.c" 'int modified = 2;'
    Remove-Item -LiteralPath (Join-Path $fixture "$source/deleted.c")
    Move-Item -LiteralPath (Join-Path $fixture 'android/tests/old space.cpp') -Destination (Join-Path $fixture 'android/tests/new space.cpp')
    Write-Fixture $unicodePath 'int unicode_value = 2;'
    Write-Fixture "$source/added.cpp" 'int newly_added = 3;'
    Invoke-FixtureGit @('add', '.') | Out-Null
    Invoke-FixtureGit @('update-index', '--chmod=+x', 'android/helpers/mode.sh') | Out-Null
    Invoke-FixtureGit @('commit', '--quiet', '-m', 'after') | Out-Null

    $output = Join-Path $fixture 'review.md'
    $shell = (Get-Process -Id $PID).Path
    & $shell -NoProfile -File (Join-Path $fixture $helper) -BaseRef $base -HeadRef HEAD -CampaignId FIXTURE -OutputPath $output
    if ($LASTEXITCODE -ne 0) { throw 'Review generator failed on rename/mode fixture' }
    $ledger = [IO.File]::ReadAllText($output)
    foreach ($path in @("$source/renamed.c", "$source/edited-new.c", "$source/modified.c", "$source/deleted.c", $unicodePath, "$source/added.cpp", 'android/tests/new space.cpp', 'android/helpers/mode.sh')) {
        if (-not $ledger.Contains($path)) { throw "Review scope omitted $path" }
    }
    if ($ledger -notmatch 'Changed paths: 8\b') { throw 'Review inventory count differs from fixture' }
    if ($ledger.Contains(' => ')) { throw 'Review scope contains abbreviated rename notation' }
    if ($ledger -notmatch 'whole diff') { throw 'No-hunk changes lost their review scope' }
    Write-Host 'Review ledger rename, edited rename, mode, Unicode, add/delete and modified-file generation: PASS'
    & python (Join-Path $PSScriptRoot 'review_ledger_git_paths_fixture.py') $fixture (Join-Path $repoRoot $helper)
    if ($LASTEXITCODE -ne 0) { throw 'Canonical Git path fixture failed' }
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    $expectedParent = [IO.Path]::GetFullPath((Join-Path $repoRoot 'android/temp')) + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($expectedParent, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($resolved) -notlike 'review_ledger_generation_*') {
        throw "Refusing fixture cleanup outside owned scratch root: $resolved"
    }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}

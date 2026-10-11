#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'input_demo_game_data.ps1')

$runnerPath = Join-Path $PSScriptRoot 'test_secret_area_baseline.ps1'
$runner = [Management.Automation.Language.Parser]::ParseFile($runnerPath, [ref]$null, [ref]$null)
$editionParameter = $runner.ParamBlock.Parameters | Where-Object { $_.Name.VariablePath.UserPath -eq 'DataEdition' }
if ($editionParameter.DefaultValue.SafeGetValue() -cne 'retail') {
    throw 'The full retail campaign baseline must remain the default'
}
foreach ($name in @('Get-SecretAreaGameConfig', 'Resolve-SecretAreaDataDir', 'Select-SecretAreaBaselineGame', 'Write-CanonicalJsonFile')) {
    $definition = $runner.Find({ param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
        }, $true)
    if (-not $definition) { throw "Baseline runner function missing: $name" }
    . ([scriptblock]::Create($definition.Extent.Text))
}

foreach ($game in @('d1', 'd2')) {
    $pwsh = Get-RegressionCurrentPwshPath
    $output = & $pwsh -NoProfile -File $runnerPath -Game $game -UpdateBaseline 2>&1 | Out-String
    if ($LASTEXITCODE -eq 0 -or $output -notmatch 'Updating a baseline requires -Game both') {
        throw "A single-game update must fail before reading data or replacing a two-game fixture: $game"
    }
}

# Check profile dispatch without native executables or game assets
$calls = [Collections.Generic.List[object]]::new()
function Resolve-InputDemoDataDir {
    param($RepoRoot, $Config, $RequestedDataDir, $Purpose)
    $calls.Add(@{ Edition = 'retail'; Game = $Config.Name; DataDir = $RequestedDataDir })
    return "retail-$($Config.Name)"
}
function Resolve-SharewareTestData {
    param($RepoRoot, $Game, $DataDir)
    $calls.Add(@{ Edition = 'shareware'; Game = $Game; DataDir = $DataDir })
    if ($DataDir -eq 'wrong-edition') { throw 'Fixture data does not match pinned shareware hashes' }
    return "shareware-$Game"
}

$RequireAssets = $true
foreach ($DataEdition in @('retail', 'shareware')) {
    foreach ($game in @('d1', 'd2')) {
        $calls.Clear()
        $resolved = Resolve-SecretAreaDataDir -Name $game -Requested 'explicit-data'
        if ($resolved -cne "$DataEdition-$game" -or $calls.Count -ne 1 -or
            $calls[0].Edition -cne $DataEdition -or $calls[0].Game -cne $game -or
            $calls[0].DataDir -cne 'explicit-data') {
            throw "Baseline profile dispatched to the wrong data resolver: $DataEdition/$game"
        }
    }
}
$DataEdition = 'shareware'
$calls.Clear()
$rejected = $false
try { Resolve-SecretAreaDataDir -Name d1 -Requested 'wrong-edition' | Out-Null }
catch { $rejected = $_ -match 'pinned shareware hashes' }
if (-not $rejected -or $calls.Count -ne 1 -or $calls[0].Edition -cne 'shareware') {
    throw 'Required shareware data must fail without falling back to retail discovery'
}

foreach ($profile in @(
        @{ File = 'secret_area_base_game_baseline.json'; Counts = @{ d1 = 30; d2 = 30 } },
        @{ File = 'secret_area_shareware_baseline.json'; Counts = @{ d1 = 7; d2 = 3 } }
    )) {
    $fixturePath = Join-Path $repoRoot "android/test_fixtures/$($profile.File)"
    $fixture = Get-Content -LiteralPath $fixturePath -Raw | ConvertFrom-Json
    if ($fixture.schema -cne 'dxx-secret-area-regression-v1' -or
        @($fixture.games).Count -ne 2 -or ($fixture.games.game -join ',') -cne 'd1,d2') {
        throw "Incomplete two-engine secret-area fixture: $fixturePath"
    }
    foreach ($game in $fixture.games) {
        if ($game.schema -cne 'dxx-secret-area-baseline-v1' -or $game.algorithm_version -ne 2 -or
            @($game.levels).Count -ne $profile.Counts[$game.game] -or @($game.music_tracks).Count -eq 0) {
            throw "Wrong edition or incomplete native metadata in $($profile.File): $($game.game)"
        }
        $secretCount = 0
        foreach ($level in $game.levels) {
            if (-not $level.scanner_enabled -or -not $level.secret_areas_complete -or
                @($level.secrets).Count -ne $level.secret_count -or @($level.route_steps).Count -eq 0) {
                throw "Incomplete native level analysis in $($profile.File): $($game.game)/$($level.level_num)"
            }
            $secretCount += $level.secret_count
        }
        if ($secretCount -le 0 -or $secretCount -ne $game.total_secret_count) {
            throw "Secret totals do not match level metadata in $($profile.File): $($game.game)"
        }
        $selected = Select-SecretAreaBaselineGame -Baseline (Get-Content -LiteralPath $fixturePath -Raw | ConvertFrom-Json) -SelectedGame $game.game
        if (@($selected.games).Count -ne 1 -or $selected.schema -cne $fixture.schema -or
            ($selected.games[0] | ConvertTo-Json -Depth 100 -Compress) -cne ($game | ConvertTo-Json -Depth 100 -Compress)) {
            throw "Single-game selection changed native metadata in $($profile.File): $($game.game)"
        }
    }
}

foreach ($invalidGames in @('[]', '[{"game":"d1"},{"game":"d1"}]')) {
    $rejected = $false
    try { Select-SecretAreaBaselineGame -Baseline ("{`"games`":$invalidGames}" | ConvertFrom-Json) -SelectedGame d1 | Out-Null }
    catch { $rejected = $_ -match 'exactly one entry' }
    if (-not $rejected) { throw 'Single-game comparison accepted missing or duplicate baseline entries' }
}

$tempRoot = Join-Path $repoRoot "android/temp/secret_area_profile_$([Guid]::NewGuid().ToString('N'))"
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $tempRoot -DirectoryPrefix secret_area_profile_
New-Item -ItemType Directory -Path $tempRoot | Out-Null
try {
    $path = Join-Path $tempRoot 'normalization.json'
    $original = '{ "field": 1, "nested": [true, false] }' + "`n"
    [IO.File]::WriteAllText($path, $original, [Text.UTF8Encoding]::new($false))
    [IO.File]::SetLastWriteTimeUtc($path, [datetime]'2000-01-01T00:00:00Z')
    $timestamp = [IO.File]::GetLastWriteTimeUtc($path)
    $value = $original | ConvertFrom-Json
    Write-CanonicalJsonFile -Value $value -Path $path
    if ([IO.File]::ReadAllText($path) -cne $original -or [IO.File]::GetLastWriteTimeUtc($path) -ne $timestamp) {
        throw 'Unchanged JSON regeneration rewrote existing bytes or modification time'
    }
    $value.field = 2
    Write-CanonicalJsonFile -Value $value -Path $path
    if ((Get-Content -LiteralPath $path -Raw | ConvertFrom-Json).field -ne 2) {
        throw 'Changed JSON metadata was not regenerated'
    }
    [IO.File]::WriteAllText($path, 'damaged JSON')
    Write-CanonicalJsonFile -Value $value -Path $path
    if ((Get-Content -LiteralPath $path -Raw | ConvertFrom-Json).field -ne 2) {
        throw 'Damaged previous JSON prevented regeneration'
    }
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}

Write-Host 'PASS: retail default, isolated shareware resolution and complete two-engine baseline fixtures'

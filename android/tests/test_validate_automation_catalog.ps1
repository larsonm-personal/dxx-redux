#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"

$androidDir = Split-Path -Parent $PSScriptRoot
$gameScriptsDir = Join-Path $androidDir "game_scripts"
. (Join-Path (Join-Path $androidDir "helpers") "test_helpers.ps1")

$jsonFiles = @(Get-ChildItem -LiteralPath $gameScriptsDir -Filter "test_*.jsonc" -File | Sort-Object Name)
$ps1Files = @(Get-ChildItem -LiteralPath $PSScriptRoot -Filter "test_*.ps1" -File | Sort-Object Name)
$allPowerShellFiles = @(
    Get-ChildItem -LiteralPath $androidDir -Filter "*.ps1" -File
    Get-ChildItem -LiteralPath $PSScriptRoot -Filter "*.ps1" -File -Recurse
    Get-ChildItem -LiteralPath (Join-Path $androidDir "helpers") -Filter "*.ps1" -File -Recurse
)
$powerShellTextByPath = @{}
foreach ($file in $allPowerShellFiles) {
    $powerShellTextByPath[$file.FullName] = Get-Content -LiteralPath $file.FullName -Raw
}
$standaloneJson = @{}
$ps1ByName = @{}
$ps1SupportOwners = @{}
$infoByPath = @{}
$ownerClosureByName = @{}
$failures = [System.Collections.Generic.List[string]]::new()

# Discovery alone does not execute the native fault checker/corpus tests in CI
function Assert-CommonFaultToolingSelection {
    param([Parameter(Mandatory)][string]$SmokeText)
    $tokens = $null
    $parseErrors = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseInput($SmokeText, [ref]$tokens, [ref]$parseErrors)
    if ($parseErrors.Count) { throw 'Tooling smoke script has parse errors' }
    $assignments = @($ast.EndBlock.Statements | Where-Object {
            $_ -is [System.Management.Automation.Language.AssignmentStatementAst] -and
            $_.Left -is [System.Management.Automation.Language.VariableExpressionAst] -and
            $_.Left.VariablePath.UserPath -ceq 'tests' -and $_.Operator -eq 'Equals'
        })
    if ($assignments.Count -ne 1 -or
        $assignments[0].Right.Expression -isnot [System.Management.Automation.Language.ArrayExpressionAst]) {
        throw 'Tooling smoke must have one literal common test array'
    }
    $statements = @($assignments[0].Right.Expression.SubExpression.Statements)
    if ($statements.Count -ne 1 -or $statements[0].PipelineElements.Count -ne 1 -or
        $statements[0].PipelineElements[0].Expression -isnot [System.Management.Automation.Language.ArrayLiteralAst]) {
        throw 'Tooling smoke common tests must be a literal string list'
    }
    $commonTests = @($statements[0].PipelineElements[0].Expression.SafeGetValue())
    if (@($commonTests | Where-Object { $_ -ceq 'test_public_d1_fault_tools' }).Count -ne 1) {
        throw 'Common platform tooling list must include the fault tools exactly once'
    }
}

try {
    $toolingSmokeText = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'run_tooling_smoke.ps1') -Raw
    Assert-CommonFaultToolingSelection -SmokeText $toolingSmokeText
    $omittedFaultTools = $toolingSmokeText.Replace("'test_public_d1_fault_tools',", '')
    foreach ($negative in @(
            ($omittedFaultTools + "`n# test_public_d1_fault_tools is mentioned but not selected`n"),
            ($omittedFaultTools + "`nif (Test-RegressionWindowsHost) { `$tests += 'test_public_d1_fault_tools' }`n")
        )) {
        $rejected = $false
        try { Assert-CommonFaultToolingSelection -SmokeText $negative } catch {
            if ($_.Exception.Message -cne 'Common platform tooling list must include the fault tools exactly once') { throw }
            $rejected = $true
        }
        if (-not $rejected) { throw 'Tooling smoke selection accepted an omitted or platform-only fault suite' }
    }
} catch {
    $failures.Add("hosted fault tooling selection regression: $($_.Exception.Message)")
}

$jsoncProbe = Join-Path ([IO.Path]::GetTempPath()) ("dxx-jsonc-catalog-" + [guid]::NewGuid().ToString('N') + '.jsonc')
try {
    $jsoncProbeText = @'
[
  { "_info": { "games": ["d1"], "marker": "marker,}", "vars": { "d1": { "URL": "https://example.invalid/a" } } } },
  { "action": "write_config", "value": "https://example.invalid/a", }, // retained URL
]
'@
    [IO.File]::WriteAllText($jsoncProbe, $jsoncProbeText + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
    $probeInfo = Get-TestScriptInfo -ScriptPath $jsoncProbe
    $probeResolved = Resolve-TestScript -ScriptPath $jsoncProbe -GameId 'd1'
    $probeParsed = Read-JsoncFile -Path $probeResolved
    if ($probeInfo.marker -cne 'marker,}' -or
        $probeParsed[0].value -cne 'https://example.invalid/a') {
        $failures.Add('shared JSONC catalog parsing rewrote quoted comment or trailing-comma text')
    }
    $plainProbe = @'
[
  { "_info": { "games": ["d1"], }, },
  { "action": "write_config", "value": "https://example.invalid/a//b", "literal": "text,} /* literal */", },
]
'@
    [IO.File]::WriteAllText($jsoncProbe, $plainProbe, [Text.UTF8Encoding]::new($false))
    $probeResolved = & {
        Set-StrictMode -Version Latest
        Resolve-TestScript -ScriptPath $jsoncProbe -GameId 'd1'
    }
    $probeParsed = Read-StrictJsonFile -Path $probeResolved
    if ($probeResolved -eq $jsoncProbe -or @($probeParsed).Count -ne 1 -or
        $probeParsed[0].value -cne 'https://example.invalid/a//b' -or
        $probeParsed[0].literal -cne 'text,} /* literal */') {
        $failures.Add('scripts without variables must emit strict native JSON without rewriting strings')
    }
} catch {
    $failures.Add("shared JSONC catalog parsing probe failed: $($_.Exception.Message)")
} finally {
    Remove-Item -LiteralPath $jsoncProbe -Force -ErrorAction SilentlyContinue
    if ($probeResolved -and $probeResolved -ne $jsoncProbe) {
        Remove-Item -LiteralPath $probeResolved -Force -ErrorAction SilentlyContinue
    }
}

function Get-PowerShellDependencyClosureText {
    param([Parameter(Mandatory = $true)][System.IO.FileInfo]$RootFile)

    $pending = [System.Collections.Generic.Queue[System.IO.FileInfo]]::new()
    $visited = [System.Collections.Generic.HashSet[string]]::new()
    $text = [System.Collections.Generic.List[string]]::new()
    $pending.Enqueue($RootFile)

    while ($pending.Count -gt 0) {
        $file = $pending.Dequeue()
        if (-not $visited.Add($file.FullName)) { continue }
        $fileText = $powerShellTextByPath[$file.FullName]
        $text.Add($fileText)
        foreach ($candidate in $allPowerShellFiles) {
            if (-not $visited.Contains($candidate.FullName) -and
                $fileText -match [regex]::Escape($candidate.Name)) {
                $pending.Enqueue($candidate)
            }
        }
    }

    return $text -join "`n"
}

function Test-FirstPlayLauncherButtonException {
    param([Parameter(Mandatory)][string]$ScriptPath, [AllowNull()][object]$Info)

    # Other automation avoids stale rendered buttons after reset_state (143a6381f)
    # Only these cold-content owner phases test the actual buttons without reset_state
    if ($null -eq $Info -or $Info._standalone -isnot [bool] -or $Info._standalone -ne $false -or
        $Info._owner -cne 'test_demo_import') { return $false }
    $reviewedNames = @(
        'test_public_first_play_empty.jsonc', 'test_public_first_play_d1.jsonc',
        'test_public_first_play_d1_in_d2.jsonc', 'test_public_first_play_d2.jsonc', 'test_public_first_play_mission.jsonc',
        'test_public_first_play_interruption_old.jsonc', 'test_public_first_play_interruption_new.jsonc'
    )
    $fullPath = [IO.Path]::GetFullPath($ScriptPath)
    foreach ($name in $reviewedNames) {
        if ($fullPath -ceq [IO.Path]::GetFullPath((Join-Path $gameScriptsDir $name))) { return $true }
    }
    return $false
}

foreach ($file in $ps1Files) {
    $ps1ByName[$file.BaseName] = $file
    $supportOwner = Get-PowerShellTestSupportOwner -ScriptPath $file.FullName
    if ($supportOwner) {
        $ps1SupportOwners[$file.BaseName] = $supportOwner
    }
}
$nativeParser = Get-Content -LiteralPath (Join-Path $androidDir 'app/src/main/cpp/shared/game_automate.cpp') -Raw
$nativeActions = @([regex]::Matches($nativeParser, 'action == "([a-z_]+)"') | ForEach-Object { $_.Groups[1].Value })
foreach ($file in $jsonFiles) {
    $nativeExecutor = $true
    foreach ($step in (Read-JsoncFile -Path $file.FullName)) {
        if ($step.action -eq 'enter_launcher') { $nativeExecutor = $false }
        if ($step.action -eq 'enter_game') { $nativeExecutor = $true }
        if ($step.action -and $step.action -notmatch '^\$\{' -and $step.action -notin $nativeActions) {
            $failures.Add("$($file.Name): native parser must preserve indices for action '$($step.action)'")
        }
        if ($nativeExecutor -and $step.action -eq 'wait_for' -and $step.field -match '[.\[]') {
            $failures.Add("$($file.Name): native wait_for requires expect for nested field '$($step.field)'")
        }
    }
    $info = Get-TestScriptInfo -ScriptPath $file.FullName
    if ($null -eq $info) {
        $failures.Add("$($file.Name): missing or unparseable first _info object")
        continue
    }
    $infoByPath[$file.FullName] = $info
    if (-not ($info._standalone -eq $false)) {
        $standaloneJson[$file.BaseName] = $file
    }

    if ($file.BaseName -notmatch '_manual(?:_|$)' -and
        -not (Test-FirstPlayLauncherButtonException -ScriptPath $file.FullName -Info $info)) {
        $scriptText = Get-Content -LiteralPath $file.FullName -Raw
        if ($scriptText -match '(?s)\{[^{}]*"action"\s*:\s*"tap_button"[^{}]*"text"\s*:\s*"(?:Launch )?Descent\b[^"]*"[^{}]*\}') {
            $failures.Add("$($file.Name): automated game launch must use enter_game instead of launcher button taps")
        }
    }
}

foreach ($name in $standaloneJson.Keys) {
    if ($ps1ByName.ContainsKey($name)) {
        $failures.Add("duplicate top-level test name '$name'")
    }
}

$supportCount = $ps1SupportOwners.Count
foreach ($supportName in $ps1SupportOwners.Keys) {
    $supportOwner = $ps1SupportOwners[$supportName]
    if ($supportName -eq $supportOwner) {
        $failures.Add("$supportName.ps1: support script cannot own itself")
        continue
    }
    if (-not $ps1ByName.ContainsKey($supportOwner)) {
        $failures.Add("$supportName.ps1: owner '$supportOwner' is not a PowerShell test")
        continue
    }
    if ($ps1SupportOwners.ContainsKey($supportOwner)) {
        $failures.Add("$supportName.ps1: owner '$supportOwner' is itself a support script")
        continue
    }
    $ownerFile = $ps1ByName[$supportOwner]
    if (-not $ownerClosureByName.ContainsKey($supportOwner)) {
        $ownerClosureByName[$supportOwner] = Get-PowerShellDependencyClosureText -RootFile $ownerFile
    }
    if ($ownerClosureByName[$supportOwner] -notmatch [regex]::Escape($ps1ByName[$supportName].Name)) {
        $failures.Add("$supportName.ps1: owner '$supportOwner' does not reference the support script")
    }
}
foreach ($file in $jsonFiles) {
    if (-not $infoByPath.ContainsKey($file.FullName)) { continue }
    $info = $infoByPath[$file.FullName]
    $standalone = -not ($info._standalone -eq $false)
    $owner = if ($info._owner) { [string]$info._owner } else { $null }
    if ($standalone) {
        if ($owner) {
            $failures.Add("$($file.Name): standalone scripts cannot declare _owner")
        }
        continue
    }

    $supportCount++
    if (-not $owner) {
        $failures.Add("$($file.Name): _standalone=false requires _owner")
        continue
    }

    $ownerFile = if ($ps1ByName.ContainsKey($owner) -and -not $ps1SupportOwners.ContainsKey($owner)) { $ps1ByName[$owner] } else { $null }
    if (-not $ownerFile) {
        $failures.Add("$($file.Name): owner '$owner' is not a top-level PowerShell test")
        continue
    }

    if (-not $ownerClosureByName.ContainsKey($owner)) {
        $ownerClosureByName[$owner] = Get-PowerShellDependencyClosureText -RootFile $ownerFile
    }
    $ownerText = $ownerClosureByName[$owner]
    if ($ownerText -notmatch [regex]::Escape($file.Name)) {
        $failures.Add("$($file.Name): owner '$owner' does not reference the support script")
    }
}

. (Join-Path $PSScriptRoot 'guidebot_route_regression_cases.ps1')
$routeCases = @(Get-GuidebotRouteRegressionCases)
$firstSample = @(Select-GuidebotRouteRegressionCases -Cases $routeCases -Seed 1)
$repeatedSample = @(Select-GuidebotRouteRegressionCases -Cases $routeCases -Seed 1)
if (($firstSample.File -join '|') -cne ($repeatedSample.File -join '|') -or
    @($firstSample.File | Select-Object -Unique).Count -ne $firstSample.Count) {
    $failures.Add('Route sampling must be reproducible and contain no duplicates')
}
if (@($routeCases | Where-Object { $_.Always -and $_.File -notin $firstSample.File }).Count -or
    @($routeCases.Family | Where-Object { $_ -notin $firstSample.Family }).Count) {
    $failures.Add('Route sampling omitted a mandatory canary or mission family')
}
$visitedCases = [Collections.Generic.HashSet[string]]::new()
foreach ($seed in 1..$routeCases.Count) {
    foreach ($case in @(Select-GuidebotRouteRegressionCases -Cases $routeCases -Seed $seed)) {
        [void]$visitedCases.Add($case.File)
    }
}
if ($visitedCases.Count -ne $routeCases.Count -or
    @(Select-GuidebotRouteRegressionCases -Cases $routeCases -Seed 1 -AllCases).Count -ne $routeCases.Count) {
    $failures.Add('Route rotation or exhaustive mode cannot reach every retained case')
}

. (Join-Path (Join-Path $androidDir 'helpers') 'test_suite_coverage.ps1')
$coverageTests = @(
    $standaloneJson.Keys | ForEach-Object { @{ Name = $_; BaseName = $_ } }
    $ps1ByName.Keys | Where-Object { -not $ps1SupportOwners.ContainsKey($_) -and $_ -ne 'test_input_demo_regressions_graphics' } |
        ForEach-Object { @{ Name = $_; BaseName = $_ } }
)
try {
    $policy = Get-TestSuiteCoveragePolicy
    foreach ($anchor in @(
            'test_input_demo_regressions', 'test_gradle_unit_tests', 'test_native_host_unit_tests',
            'test_guidebot_route_regressions', 'test_launch_to_automap',
            'test_android_sdk_lifecycle', 'test_android_saveload_dispatch_unified', 'test_mp'
        )) {
        if ($anchor -notin $policy.core) { $failures.Add("Fixed integration coverage missing: $anchor") }
    }
    $extendedSamples = @(1..40 | ForEach-Object { Get-TestSuiteExtendedSample -Seed $_ })
    if (@($extendedSamples | Where-Object Graphics).Count -ne 2 -or
        @($extendedSamples | Where-Object Multiplayer).Count -ne 2 -or
        @($extendedSamples | Where-Object { $_.Graphics -and $_.Multiplayer }).Count) {
        $failures.Add('Extended variants must each run on 5% of seeds without coinciding')
    }
    if (-not (Get-TestSuiteExtendedSample -Seed 20).Graphics -or
        -not (Get-TestSuiteExtendedSample -Seed 30).Multiplayer) {
        $failures.Add('Extended sampling must be reproducible from the suite seed')
    }
    $allCoverage = @(Select-TestSuiteCoverage -Tests $coverageTests -Seed 1 -AllScenarios)
    $policyNames = @($policy.Values | ForEach-Object { $_ })
    foreach ($name in $policyNames) {
        if ($name -notin $coverageTests.Name) { $failures.Add("Stale suite coverage entry: $name") }
    }
    $coverageSample = @(Select-TestSuiteCoverage -Tests $coverageTests -Seed 1)
    if (($coverageSample.Name -join '|') -cne ((Select-TestSuiteCoverage -Tests $coverageTests -Seed 1).Name -join '|')) {
        $failures.Add('Suite scenario selection is not reproducible')
    }
    $visited = [Collections.Generic.HashSet[string]]::new()
    $rotationLength = ($policy.Keys | Where-Object { $_ -notin @('core', 'explicit', 'extended_graphics', 'three_player_network') } |
            ForEach-Object { $policy[$_].Count } | Measure-Object -Maximum).Maximum
    foreach ($seed in 1..$rotationLength) {
        foreach ($test in @(Select-TestSuiteCoverage -Tests $coverageTests -Seed $seed)) { [void]$visited.Add($test.Name) }
    }
    $exactOnlyNames = @($policy.explicit) + @($policy.three_player_network)
    foreach ($name in $policyNames | Where-Object { $_ -notin $exactOnlyNames }) {
        if (-not $visited.Contains($name)) { $failures.Add("Suite rotation cannot reach $name") }
    }
    foreach ($name in $policy.core) {
        if ($name -notin $coverageSample.Name) { $failures.Add("Suite sampling omitted mandatory owner $name") }
    }
    if ('test_lan_three_player_startup' -notin $policy.three_player_network -or
        'test_lan_three_player_startup' -in $coverageSample.Name -or
        'test_lan_three_player_startup' -in $allCoverage.Name) {
        $failures.Add('Three-player coverage must retain a distinct category selected only explicitly or by the scheduled plan')
    }
    $expectedUnattendedNames = @($coverageTests | Where-Object Name -notin $exactOnlyNames | ForEach-Object Name | Sort-Object)
    $allCoverageNames = @($allCoverage | ForEach-Object Name | Sort-Object)
    if (Compare-Object $expectedUnattendedNames $allCoverageNames) {
        $failures.Add('Exhaustive unattended suite must retain ordinary scenarios and exclude explicit or scheduled-only probes')
    }
} catch { $failures.Add("Suite coverage policy: $_") }

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Host "FAIL: $failure"
    }
    exit 1
}

$standalonePowerShellCount = $ps1ByName.Count - $ps1SupportOwners.Count
Write-Host "Automation catalog valid: $($standaloneJson.Count) standalone JSON tests, $supportCount support scripts, $standalonePowerShellCount standalone PowerShell tests"
exit 0

# TEST-SUPPORT: owner=test_guidebot_route_regressions
# Native escort loop and physics, without rendering or player auto-follow
param([switch]$NoBuild, [ValidateRange(0, 899)][int]$ReturnTargetSegment = 35)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputRoot = Join-Path $repoRoot 'android/temp/guidebot_live_navigation'
$hogDir = Join-Path $repoRoot 'game_data/CD images/Descent II (USA) (v1.1)/data_tracks/d2data'
$exe = Join-Path $repoRoot 'buildd2/main/test_guidebot_live_navigation.exe'
if (-not $NoBuild) {
    & (Join-Path $repoRoot 'run-windows-build.ps1') -Target d2
    if ($LASTEXITCODE -ne 0) { throw 'D2 Windows build failed' }
}
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$extraDir = Join-Path $outputRoot 'maximum'
Expand-Archive -LiteralPath (Join-Path $repoRoot 'game_data/mission_files/descent_maximum_fixed.zip') -DestinationPath (Join-Path $extraDir 'missions') -Force
$cases = @(
    @{ Name = 'counterstrike_grate'; Mission = 'd2'; Level = 11 },
    @{ Name = 'maximum_hostages'; Mission = 'max_f'; Level = 16 }
)
foreach ($case in $cases) {
    $reference = $null
    for ($repeat = 1; $repeat -le 2; ++$repeat) {
        $output = Join-Path $outputRoot "$($case.Name)_$repeat.json"
        $log = Join-Path $outputRoot "$($case.Name)_$repeat.log"
        $userDir = Join-Path $outputRoot "$($case.Name)_user_$repeat"
        New-Item -ItemType Directory -Force -Path $userDir | Out-Null
        & $exe -hogdir $hogDir -extra-dir $extraDir -mission $case.Mission -level $case.Level `
            -route-confirm-user-dir $userDir -route-confirm-json-out $output -escort-return-target $ReturnTargetSegment 2>&1 |
            Out-File -LiteralPath $log -Encoding utf8
        if ($LASTEXITCODE -ne 0) { throw "Escort test failed: $($case.Name), see $log" }
        $result = Get-Content -LiteralPath $output -Raw
        if (-not ($result | ConvertFrom-Json).passed) { throw "Escort assertions failed: $output" }
        if ($repeat -eq 1) { $reference = $result }
        elseif ($result -cne $reference) { throw "Escort results differ across repeats: $($case.Name)" }
    }
}
Write-Host 'Live escort navigation passed: grate detour, return isolation, and Maximum Hostages, two identical runs each'

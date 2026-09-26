# TEST-SUPPORT: owner=test_guidebot_route_regressions
# Compare live Original decisions with frozen Redux routing on real D2 levels
param([switch]$NoBuild, [string]$BuildDir = 'buildd2')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputRoot = Join-Path $repoRoot 'android/temp/guidebot_original_navigation'
$hogDir = Join-Path $repoRoot 'game_data/CD images/Descent II (USA) (v1.1)/data_tracks/d2data'
$exe = Join-Path $repoRoot "$BuildDir/main/test_guidebot_original_navigation.exe"
if (-not $NoBuild) {
    & (Join-Path $repoRoot 'run-windows-build.ps1') -Target d2
    if ($LASTEXITCODE -ne 0) { throw 'D2 Windows build failed' }
}
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
foreach ($level in @(1, 11)) {
    $reference = $null
    for ($repeat = 1; $repeat -le 2; ++$repeat) {
        $output = Join-Path $outputRoot "level_${level}_$repeat.json"
        $log = Join-Path $outputRoot "level_${level}_$repeat.log"
        $userDir = Join-Path $outputRoot "level_${level}_user_$repeat"
        New-Item -ItemType Directory -Force -Path $userDir | Out-Null
        & $exe -hogdir $hogDir -mission d2 -level $level -route-confirm-user-dir $userDir -route-confirm-json-out $output 2>&1 |
            Out-File -LiteralPath $log -Encoding utf8
        if ($LASTEXITCODE -ne 0) { throw "Original routing failed: level $level, see $log" }
        $result = Get-Content -LiteralPath $output -Raw
        if (-not ($result | ConvertFrom-Json).passed) { throw "Original assertions failed: $output" }
        if ($repeat -eq 1) { $reference = $result }
        elseif ($result -cne $reference) { throw "Original results differ across repeats: level $level" }
    }
}
Write-Host 'Original Redux routing comparisons and native save modes passed twice on levels 1 and 11'

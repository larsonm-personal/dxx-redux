# TEST-SUPPORT: owner=test_guidebot_route_regressions
# Compare live Original decisions with frozen Redux routing on real D2 levels
param([switch]$NoBuild, [string]$BuildDir = 'buildd2')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
$outputRoot = Join-Path $repoRoot ('android/temp/guidebot_original_navigation/run_' + [guid]::NewGuid().ToString('N'))
$hogDir = Join-Path $repoRoot 'game_data/CD images/Descent II (USA) (v1.1)/data_tracks/d2data'
$exe = Join-RegressionPath $repoRoot $BuildDir 'main' (Get-RegressionHostExecutableNames -BaseName 'test_guidebot_original_navigation')[0]
if (-not $NoBuild) {
    Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target d2
}
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $outputRoot -DirectoryPrefix 'run_'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$fixtureLock = $null
try {
    $fixtureLock = [IO.File]::Open((Join-Path $outputRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
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
} finally {
    # Remove copied mission and player data, retaining bounded JSON/log diagnostics
    try {
        Get-ChildItem -LiteralPath $outputRoot -Directory | Remove-Item -Recurse -Force
    } finally {
        if ($fixtureLock) { $fixtureLock.Dispose() }
    }
}

# TEST-SUPPORT: owner=test_guidebot_route_regressions
# Compare live Original decisions with frozen Redux routing on real D2 levels
param([switch]$NoBuild, [string]$BuildDir = 'buildd2', [string]$HogDir, [ValidateRange(1, 300)][int]$ProcessTimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
. (Join-Path $PSScriptRoot 'guidebot_navigation_host.ps1')
if (-not [IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $repoRoot $BuildDir }
$outputRoot = Join-Path $repoRoot ('android/temp/guidebot_original_navigation/run_' + [guid]::NewGuid().ToString('N'))
$exe = Join-RegressionPath $BuildDir 'main' (Get-RegressionHostExecutableNames -BaseName 'test_guidebot_original_navigation')[0]
if (-not $NoBuild) {
    Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target d2
}
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $outputRoot -DirectoryPrefix 'run_'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$fixtureLock = $null
try {
    $fixtureLock = [IO.File]::Open((Join-Path $outputRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    Write-Host "Navigation diagnostics: $outputRoot"
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Navigation executable not found: $exe" }
    $dataDir = New-GuidebotNavigationData -RepoRoot $repoRoot -OutputRoot $outputRoot -HogDir $HogDir
    foreach ($level in @(1, 11)) {
        $reference = $null
        for ($repeat = 1; $repeat -le 2; ++$repeat) {
            $output = Join-Path $outputRoot "level_${level}_$repeat.json"
            $log = Join-Path $outputRoot "level_${level}_$repeat.log"
            $userDir = Join-Path $outputRoot "level_${level}_user_$repeat"
            New-Item -ItemType Directory -Force -Path $userDir | Out-Null
            Invoke-GuidebotNavigationProcess -Exe $exe -RepoRoot $repoRoot -LogPath $log -TimeoutSeconds $ProcessTimeoutSeconds -Arguments @(
                '-hogdir', $dataDir, '-mission', 'd2', '-level', [string]$level,
                '-route-confirm-user-dir', $userDir, '-route-confirm-json-out', $output)
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

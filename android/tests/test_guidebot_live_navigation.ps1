# TEST-SUPPORT: owner=test_guidebot_route_regressions
# Native escort loop and physics, without rendering or player auto-follow
param([switch]$NoBuild, [ValidateRange(0, 899)][int]$ReturnTargetSegment = 35, [string]$BuildDir = 'buildd2', [string]$HogDir, [ValidateRange(1, 300)][int]$ProcessTimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
. (Join-Path $PSScriptRoot 'guidebot_navigation_host.ps1')
if (-not [IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $repoRoot $BuildDir }
$outputRoot = Join-Path $repoRoot ('android/temp/guidebot_live_navigation/run_' + [guid]::NewGuid().ToString('N'))
$exe = Join-RegressionPath $BuildDir 'main' (Get-RegressionHostExecutableNames -BaseName 'test_guidebot_live_navigation')[0]
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
    $extraDir = Join-Path $outputRoot 'maximum'
    New-Item -ItemType Directory -Path $extraDir | Out-Null
    $maximumArchive = Join-Path $repoRoot 'game_data/mission_files/descent_maximum_fixed.zip'
    $hasMaximum = Test-Path -LiteralPath $maximumArchive -PathType Leaf
    if ($hasMaximum) {
        Expand-Archive -LiteralPath $maximumArchive -DestinationPath (Join-Path $extraDir 'missions') -Force
    }
    $cases = @(
        @{ Name = 'counterstrike_grate'; Mission = 'd2'; Level = 11 },
        @{ Name = 'counterstrike_reactor'; Mission = 'd2'; Level = 1 },
        @{ Name = 'maximum_hostages'; Mission = 'max_f'; Level = 16 }
    )
    foreach ($case in $cases) {
        if ($case.Mission -eq 'max_f' -and -not $hasMaximum) { continue }
        $reference = $null
        for ($repeat = 1; $repeat -le 2; ++$repeat) {
            $output = Join-Path $outputRoot "$($case.Name)_$repeat.json"
            $log = Join-Path $outputRoot "$($case.Name)_$repeat.log"
            $userDir = Join-Path $outputRoot "$($case.Name)_user_$repeat"
            New-Item -ItemType Directory -Force -Path $userDir | Out-Null
            Invoke-GuidebotNavigationProcess -Exe $exe -RepoRoot $repoRoot -LogPath $log -TimeoutSeconds $ProcessTimeoutSeconds -Arguments @(
                '-hogdir', $dataDir, '-extra-dir', $extraDir, '-mission', $case.Mission, '-level', [string]$case.Level,
                '-route-confirm-user-dir', $userDir, '-route-confirm-json-out', $output, '-escort-return-target', [string]$ReturnTargetSegment)
            $result = Get-Content -LiteralPath $output -Raw
            if (-not ($result | ConvertFrom-Json).passed) { throw "Escort assertions failed: $output" }
            if ($repeat -eq 1) { $reference = $result }
            elseif ($result -cne $reference) { throw "Escort results differ across repeats: $($case.Name)" }
        }
    }
    if (-not $hasMaximum) {
        Write-Host 'Counterstrike live escort cases passed twice'
        Write-Host "RESULT: SKIP (Maximum Hostages requires missing archive: $maximumArchive)"
        exit 2
    }
    Write-Host 'Live escort navigation passed: grate detour, endpoint patrol, reactor arrival, and Maximum Hostages, two identical runs each'
} finally {
    # Remove copied mission and player data, retaining bounded JSON/log diagnostics
    try {
        Get-ChildItem -LiteralPath $outputRoot -Directory | Remove-Item -Recurse -Force
    } finally {
        if ($fixtureLock) { $fixtureLock.Dispose() }
    }
}

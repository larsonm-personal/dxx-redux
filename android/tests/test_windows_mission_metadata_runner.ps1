#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Assert-Contains([string]$Text, [string]$Needle, [string]$Message) {
    if (-not $Text.Contains($Needle, [StringComparison]::Ordinal)) { throw $Message }
}

function Invoke-NoisyBuildFixture {
    $powershell = (Get-Process -Id $PID).Path
    & $powershell -NoProfile -NonInteractive -Command "Write-Output 'build stdout'; [Console]::Error.WriteLine('build stderr')" 2>&1 |
        ForEach-Object { Write-Host ([string]$_) }
    if ($LASTEXITCODE -ne 0) { throw "Noisy build fixture failed with exit code $LASTEXITCODE" }
    return 'metadata-cli.bat'
}

$publicRunner = [IO.File]::ReadAllText((Join-Path $repoRoot 'android\helpers\regenerate_all_mission_metadata.ps1'))
$hostRunner = [IO.File]::ReadAllText((Join-Path $repoRoot 'android\helpers\regenerate_all_mission_metadata_host.ps1'))
$nativeAnalyzer = [IO.File]::ReadAllText((Join-Path $repoRoot 'android\app\src\main\cpp\jni_level_metadata.cpp'))
$projection = [IO.File]::ReadAllText((Join-Path $repoRoot 'android\mission-metadata-core\src\main\kotlin\com\dxxredux\app\MissionMetadataProjection.kt'))
$d1Cmake = [IO.File]::ReadAllText((Join-Path $repoRoot 'd1\main\CMakeLists.txt'))
$d2Cmake = [IO.File]::ReadAllText((Join-Path $repoRoot 'd2\main\CMakeLists.txt'))

Assert-Contains $publicRunner "[string]`$Engine = 'Host'" 'The native host must be the default metadata engine'
Assert-Contains $publicRunner "`$Engine -in @('Host', 'Windows')" 'The public runner must dispatch Host and the legacy Windows alias to the native host path'
Assert-Contains $publicRunner 'IncludeBuiltInCounterstrike' 'The public runner must forward focused built-in Counterstrike requests'
Assert-Contains $publicRunner 'IncludeBuiltInFirstStrike' 'The public runner must forward focused built-in First Strike requests'
Assert-Contains $publicRunner 'if ($RoutingDevelopmentSet)' 'The public runner must resolve the shared routing development set'
Assert-Contains $hostRunner 'dxx-redux-$Game-metadata-worker' 'The host runner must use the shared native worker'
Assert-Contains $hostRunner '-or $IncludeBuiltInCounterstrike' 'Focused host runs must be able to include built-in Counterstrike'
Assert-Contains $hostRunner '-or $IncludeBuiltInFirstStrike' 'Focused host runs must be able to include built-in First Strike'
Assert-Contains $hostRunner 'Invoke-BuiltinHeadlessScan -Game d1' 'First Strike metadata must come from the native D1 analyzer'
Assert-Contains $hostRunner '--directory-precedence' 'The host runner must load variant precedence from shared Kotlin'
Assert-Contains $hostRunner "op = 'project'" 'The host runner must use the shared Kotlin checked-in projection'
Assert-Contains $hostRunner "op = 'descriptor'" 'The host runner must use the shared Kotlin descriptor parser'
Assert-Contains $nativeAnalyzer 'row["route_required_key_mask"]' 'The native analyzer must serialize required key masks'
Assert-Contains $nativeAnalyzer 'row["route_completing_key_mask_set"]' 'The native analyzer must serialize completing key-mask sets'
Assert-Contains $projection 'level.requiredInt("route_required_key_mask")' 'The projection must reject a missing required key mask'
Assert-Contains $projection 'level.requiredInt("route_completing_key_mask_set")' 'The projection must reject a missing completing key-mask set'
Assert-Contains $d1Cmake 'jni_level_metadata.cpp' 'D1 worker must compile the Android analyzer source'
Assert-Contains $d2Cmake 'jni_level_metadata.cpp' 'D2 worker must compile the Android analyzer source'

$containedBuildOutputCount = [regex]::Matches(
    $hostRunner,
    '2>&1\s*\|\s*ForEach-Object \{ Write-Host \(\[string\]\$_\) \}'
).Count
if ($containedBuildOutputCount -ne 4) {
    throw "All four Windows/Linux build and fallback invocations must contain console output; found $containedBuildOutputCount"
}

$fixtureResult = @(Invoke-NoisyBuildFixture)
if ($fixtureResult.Count -ne 1 -or $fixtureResult[0] -ne 'metadata-cli.bat') {
    throw "Noisy build output leaked into the function result: $($fixtureResult -join ', ')"
}

Write-Host 'Host mission metadata runner wiring passed' -ForegroundColor Green

# Exercise the actual persistent Kotlin protocol without requiring retail game data
$androidRoot = Join-Path $repoRoot 'android'
. (Join-Path $androidRoot 'helpers/mission_archive_variants.ps1')
. (Join-Path $androidRoot 'helpers/host_metadata_worker.ps1')
$NoBuild = $false
$cli = Initialize-MetadataKotlinCli
$ast = [Management.Automation.Language.Parser]::ParseInput($hostRunner, [ref]$null, [ref]$null)
foreach ($name in @('New-MetadataKotlinWorker', 'Invoke-MetadataKotlinWorker')) {
    $function = $ast.Find({
            param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
        }, $true)
    if (-not $function) { throw "Missing worker function: $name" }
    . ([scriptblock]::Create($function.Extent.Text))
}
$fixtureRoot = Join-Path $androidRoot ('temp/kotlin-worker-' + [guid]::NewGuid().ToString('N'))
& (Join-Path $androidRoot 'helpers/retain-recent-artifacts.ps1') -Artifacts $fixtureRoot -DirectoryPrefix 'kotlin-worker-' -MinimumFreeSpaceGB 0.01
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
$worker = $null
$fixtureLock = $null
try {
    $fixtureLock = [IO.File]::Open((Join-Path $fixtureRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $descriptor = Join-Path $fixtureRoot 'worker fixture.mn2'
    [IO.File]::WriteAllText($descriptor, "name = Worker Fixture`ntype = normal`nnum_levels = 1`nlevel01.rl2`n")
    $worker = New-MetadataKotlinWorker -CliPath $cli
    $workerPid = $worker.Process.Id
    foreach ($iteration in 1..2) {
        $result = Invoke-MetadataKotlinWorker -Worker $worker -Request ([ordered]@{ op = 'descriptor'; path = $descriptor })
        if (-not $result.valid -or $result.display_name -ne 'Worker Fixture' -or
            @($result.normal_level_files).Count -ne 1 -or $result.normal_level_files[0] -ne 'level01.rl2') {
            throw "Persistent Kotlin descriptor request $iteration returned invalid metadata"
        }
        if ($worker.Process.Id -ne $workerPid -or $worker.Process.HasExited) { throw 'Kotlin worker was not persistent' }
    }
} finally {
    Stop-MetadataWorkerProcess -Worker $worker
    if ($fixtureLock) { $fixtureLock.Dispose() }
    Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
}
if (Get-Process -Id $workerPid -ErrorAction SilentlyContinue) { throw 'Kotlin worker survived shutdown' }
Write-Host 'Persistent Kotlin metadata requests and shutdown passed'

param(
    [Parameter(Mandatory = $true)]
    [int[]] $Level,
    [string] $Mission = "d2",
    [string] $HogDir = "game_data/CD images/Descent II (USA) (v1.1)/data_tracks/d2data",
    [ValidateRange(1, 20)]
    [int] $Repeat = 2,
    [switch] $NoBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputDir = Join-Path $repoRoot "android/temp/route_confirmation_engine"
$exe = Join-Path $repoRoot "buildd2/main/dxx-redux-d2-headless-route.exe"
$hogPath = if ([System.IO.Path]::IsPathRooted($HogDir)) {
    $HogDir
} else {
    Join-Path $repoRoot $HogDir
}
$resolvedHogDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($hogPath)

if (-not $NoBuild) {
    & (Join-Path $repoRoot "run-windows-build.ps1") -Target d2
    if ($LASTEXITCODE -ne 0) {
        throw "D2 Windows build failed with exit code $LASTEXITCODE"
    }
}
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Route confirmation executable not found at $exe"
}
if (-not (Test-Path -LiteralPath $resolvedHogDir -PathType Container)) {
    throw "HOG directory not found at $resolvedHogDir"
}
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

$rows = foreach ($levelNumber in $Level) {
    $referenceHash = $null
    $referenceResult = $null
    for ($run = 1; $run -le $Repeat; $run++) {
        $filename = "${Mission}_level_${levelNumber}_run_${run}.json"
        $output = Join-Path $outputDir $filename
        $log = Join-Path $outputDir "${Mission}_level_${levelNumber}_run_${run}.log"
        $arguments = @(
            "-hogdir", $resolvedHogDir,
            "-mission", $Mission,
            "-level", $levelNumber,
            "-route-confirm-json-out", $output
        )
        & $exe @arguments 2>&1 | Out-File -LiteralPath $log -Encoding utf8
        if ($LASTEXITCODE -ne 0) {
            throw "Route confirmation failed for $Mission level $levelNumber run $run"
        }
        $hash = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
        $result = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
        if ($run -eq 1) {
            $referenceHash = $hash
            $referenceResult = $result
        } elseif ($hash -ne $referenceHash) {
            throw "Determinism mismatch for $Mission level $levelNumber run $run"
        }
    }
    [pscustomobject]@{
        Mission = $Mission
        Level = $levelNumber
        Status = $referenceResult.status
        Seed = $referenceResult.seed
        Frames = $referenceResult.frames
        Seconds = $referenceResult.simulation_seconds
        Objectives = @($referenceResult.objectives).Count
        Repeats = $Repeat
        Sha256 = $referenceHash
    }
}

$rows | Format-Table -AutoSize

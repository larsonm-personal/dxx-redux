param(
    [ValidateSet("both", "d1", "d2")]
    [string]$Target = "both",
    [string]$Preset = "x86-release",
    [string]$BuildType = "RelWithDebInfo",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSCommandPath
Set-Location $repoRoot

function Require-Tool($name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "$name not found in PATH. Run this from a Visual Studio Developer PowerShell with CMake and Ninja available"
    }
}

Require-Tool cmake
Require-Tool ninja
Require-Tool cl.exe

Get-Process cl -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

$targets =
    switch ($Target) {
        "d1" { @("d1") }
        "d2" { @("d2") }
        default { @("d1", "d2") }
    }

foreach ($game in $targets) {
    $buildDir = Join-Path $repoRoot (if ($game -eq "d1") { "buildd1" } else { "buildd2" })
    if ($Clean -and (Test-Path $buildDir)) {
        Remove-Item -Recurse -Force $buildDir
    }

    Write-Host "Configuring $game with preset $Preset ($BuildType)"
    & cmake --preset=$Preset -D CMAKE_BUILD_TYPE=$BuildType -S $game -B $buildDir
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed for $game"
    }

    Write-Host "Building $game"
    & cmake --build $buildDir -- /m /errorlimit:10
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed for $game"
    }
}

Write-Host "Windows build complete for $($targets -join ', ')"
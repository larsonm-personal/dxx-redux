#!/usr/bin/env pwsh
param(
    [string]$DemoRoot,
    [ValidateSet('auto', 'd1', 'd2')]
    [string]$Game = 'auto',
    [ValidateSet('prompt', 'realtime', 'accelerated')]
    [string]$Mode = 'accelerated',
    [int]$TimeoutSeconds = 300,
    [Alias('HogDir')]
    [string]$DataDir,
    [string]$Pilot,
    [switch]$NoRender,
    [switch]$ReuseSandbox,
    [switch]$KeepSandbox,
    [switch]$ListOnly,
    [switch]$StopOnFirstFailure
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$wrapper = Join-Path $PSScriptRoot 'run_input_demo_replay.ps1'
if (-not $DemoRoot) {
    $DemoRoot = Join-Path $repoRoot 'android\regression_demos'
}
if (-not (Test-Path -LiteralPath $DemoRoot)) {
    throw "Regression demo directory not found: $DemoRoot"
}
$resolvedDemoRoot = (Resolve-Path -LiteralPath $DemoRoot).Path
$demos = @(Get-ChildItem -LiteralPath $resolvedDemoRoot -Recurse -Filter '*.dximdemo' -File | Sort-Object FullName)

if ($ListOnly) {
    if ($demos.Count -eq 0) {
        Write-Host 'No regression demos found'
        exit 0
    }
    foreach ($demo in $demos) {
        Write-Host ([System.IO.Path]::GetRelativePath($repoRoot, $demo.FullName))
    }
    exit 0
}

if ($demos.Count -eq 0) {
    throw "No .dximdemo files found under $resolvedDemoRoot"
}

$pwsh = (Get-Process -Id $PID).Path
$failures = New-Object System.Collections.Generic.List[string]

for ($index = 0; $index -lt $demos.Count; $index++) {
    $demo = $demos[$index]
    $relativeDemo = [System.IO.Path]::GetRelativePath($repoRoot, $demo.FullName)
    Write-Host ''
    Write-Host ("[{0}/{1}] {2}" -f ($index + 1), $demos.Count, $relativeDemo)

    $args = @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $wrapper,
        '-DemoPath', $demo.FullName,
        '-Game', $Game,
        '-Mode', $Mode,
        '-TimeoutSeconds', [string]$TimeoutSeconds
    )
    if ($DataDir) {
        $args += @('-DataDir', $DataDir)
    }
    if ($Pilot) {
        $args += @('-Pilot', $Pilot)
    }
    if ($NoRender) {
        $args += '-NoRender'
    }
    if ($ReuseSandbox) {
        $args += '-ReuseSandbox'
    }
    if ($KeepSandbox) {
        $args += '-KeepSandbox'
    }

    & $pwsh @args
    if ($LASTEXITCODE -ne 0) {
        $failures.Add($relativeDemo)
        if ($StopOnFirstFailure) {
            break
        }
    }
}

Write-Host ''
if ($failures.Count -gt 0) {
    Write-Host 'RESULT: FAIL' -ForegroundColor Red
    foreach ($failure in $failures) {
        Write-Host "Failed: $failure"
    }
    exit 1
}

Write-Host ("RESULT: PASS ({0} regression demo(s))" -f $demos.Count) -ForegroundColor Green
exit 0
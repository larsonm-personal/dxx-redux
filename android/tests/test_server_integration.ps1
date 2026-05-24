#!/usr/bin/env pwsh
# test_server_integration.ps1 -- Runs the Rust matchmaking server's
# integration test suite (cargo test --test integration).
#
# Usage: .\android\tests\test_server_integration.ps1
# Requires: cargo (Rust toolchain), PowerShell 5.1+

param(
    [int]$Port = 0,  # 0 = auto-pick
    [int]$TimeoutSec = 30
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\..\test_env.ps1"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path "$scriptDir\..\..").Path
$serverDir = Join-Path $repoRoot "server"

Write-Host "=== Server Integration Test ==="
Write-Host "Building server..."

Push-Location $serverDir
try {
    $buildOut = cargo build 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Server build failed:"
        $buildOut | ForEach-Object { Write-Host $_ }
        exit 1
    }
    Write-Host "Server build OK"
} finally {
    Pop-Location
}

# Run the server integration tests that cover the C2/C3 flows
Write-Host ""
Write-Host "Running server integration tests (C2/C3 flows)..."
Push-Location $serverDir
try {
    # Run just the new C2/C3 tests plus existing ones
    $testOut = cargo test --test integration 2>&1
    $testExitCode = $LASTEXITCODE
    $testOut | ForEach-Object { Write-Host $_ }
    if ($testExitCode -ne 0) {
        Write-Host ""
        Write-Host "FAIL: Server integration tests failed"
        exit 1
    }
    Write-Host ""
    Write-Host "PASS: All server integration tests passed"
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "=== Server Integration Test PASSED ==="

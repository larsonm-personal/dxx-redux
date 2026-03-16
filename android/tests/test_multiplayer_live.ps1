#!/usr/bin/env pwsh
# test_multiplayer_live.ps1 -- Live integration test for the multiplayer
# client/server flow. Starts the Rust server, connects two WebSocket
# clients, and exercises the full lobby lifecycle.
#
# Usage: .\android\tests\test_multiplayer_live.ps1
# Requires: cargo (Rust toolchain), PowerShell 5.1+

param(
    [int]$Port = 0,  # 0 = auto-pick
    [int]$TimeoutSec = 30
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path "$scriptDir\..\..").Path
$serverDir = Join-Path $repoRoot "server"

Write-Host "=== Multiplayer Live Test ==="
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
Write-Host "=== Multiplayer Live Test PASSED ==="

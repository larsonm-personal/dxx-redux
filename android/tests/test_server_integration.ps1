#!/usr/bin/env pwsh
# test_server_integration.ps1 -- Runs the Rust matchmaking server test suite.
#
# Usage: .\android\tests\test_server_integration.ps1
# Requires: cargo (Rust toolchain), PowerShell 5.1+

param(
    [int]$Port = 0,  # 0 = auto-pick
    [int]$TimeoutSec = 30
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\..\helpers\test_env.ps1"
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

# Run the full server test domain, including integration, NAT simulator, and
# any module unit tests added later.
Write-Host ""
Write-Host "Running server tests..."
Push-Location $serverDir
try {
    $testOut = cargo test 2>&1
    $testExitCode = $LASTEXITCODE
    $testOut | ForEach-Object { Write-Host $_ }
    if ($testExitCode -ne 0) {
        Write-Host ""
        Write-Host "FAIL: Server tests failed"
        exit 1
    }
    Write-Host ""
    Write-Host "PASS: All server tests passed"
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "=== Server Test Suite PASSED ==="

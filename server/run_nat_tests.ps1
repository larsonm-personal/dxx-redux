# Run NAT simulator integration tests
# Usage: .\run_nat_tests.ps1

$ErrorActionPreference = "Stop"
Push-Location $PSScriptRoot\server

Write-Host "Running NAT simulator tests..." -ForegroundColor Cyan
cargo test --test nat_sim_tests -- --nocapture 2>&1
$exitCode = $LASTEXITCODE

Pop-Location

if ($exitCode -ne 0) {
    Write-Host "NAT tests FAILED (exit code $exitCode)" -ForegroundColor Red
} else {
    Write-Host "NAT tests PASSED" -ForegroundColor Green
}
exit $exitCode

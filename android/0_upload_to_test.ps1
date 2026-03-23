# upload_to_test.ps1 -- Automated AAB build, sign, and Play Store upload
#
# Usage:
#   .\0_upload_to_test.ps1                     # Build (Release) and upload to production track
#   .\0_upload_to_test.ps1 -BuildType "1"     # Build Debug instead
#   .\0_upload_to_test.ps1 -TrackIndex "2"    # Upload to alpha track instead (1=internal, 2=alpha, 3=beta, 4=production)
#   .\0_upload_to_test.ps1 -BuildType "2" -TrackIndex "3"  # Both options

param(
    # these might not have a fixed order but for now that's what these are
    # maybe need to match based on strings in the future
    [string]$BuildType = "2",        # Default: Release
    [int]$TrackIndex = 4             # Default: internal (1=production, 2=beta, 3=alpha, 4=internal)
)

$ErrorActionPreference = "Stop"

Push-Location $PSScriptRoot
try {
    Write-Host ""
    Write-Host "================================"
    Write-Host "DXX-Redux Automated Build & Upload"
    Write-Host "================================"
    Write-Host ""

    # Build the AAB
    Write-Host "Step 1: Building AAB..."
    Write-Host ""
    & .\1_build-aab.ps1 -BuildType $BuildType
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }

    # Deploy to Play Store
    Write-Host ""
    Write-Host "Step 2: Uploading to Play Store..."
    Write-Host ""
    & .\2_deploy-playstore.ps1 -TrackIndex $TrackIndex
    if ($LASTEXITCODE -ne 0) {
        throw "Deploy failed with exit code $LASTEXITCODE"
    }

    Write-Host ""
    Write-Host "================================"
    Write-Host "Build and upload completed successfully!"
    Write-Host "================================"
    Write-Host ""

} catch {
    Write-Host ""
    Write-Host "ERROR: $_"
    Write-Host ""
    exit 1
} finally {
    Pop-Location
}

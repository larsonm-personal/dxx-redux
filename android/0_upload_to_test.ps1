#!/usr/bin/env pwsh
# upload_to_test.ps1 -- Automated AAB build, sign, and Play Store upload
#
# Computes versionCode = commitCount*10 + rev, where rev auto-increments
# if the target track already has a version from the same commit count.
#
# Usage:
#   .\0_upload_to_test.ps1                     # Build (Internal) and upload to internal track
#   .\0_upload_to_test.ps1 -BuildType "1"     # Build Debug instead
#   .\0_upload_to_test.ps1 -BuildType "2"     # Build Release instead
#   .\0_upload_to_test.ps1 -TrackName "alpha"  # Upload to alpha track instead
#   .\0_upload_to_test.ps1 -BuildOnly          # Build through this wrapper without uploading

param(
    [string]$BuildType = "3",        # Default: Internal (debug + release signing)
    [string]$TrackName = "internal",  # Default: internal track
    [switch]$BuildOnly                # Skip Play Store auth/query/upload and only run the build step
)

$ErrorActionPreference = "Stop"

Push-Location $PSScriptRoot
try {
    Write-Host ""
    Write-Host "================================"
    Write-Host "DXX-Redux Automated Build & Upload"
    Write-Host "================================"
    Write-Host ""

    # Load shared auth helpers
    . "$PSScriptRoot\playstore-auth.ps1"

    # ---------------------------------------------------------------
    #  Query the target track to determine version code with rev
    # ---------------------------------------------------------------

    $commitCount = [int](git rev-list --count HEAD).Trim()
    Write-Host "Git commit count: $commitCount"

    $rev = 0
    if ($BuildOnly) {
        Write-Host "Build-only mode: skipping Play Store version query and upload"
    } else {
        $credPath = Join-Path $PSScriptRoot "play-store-credentials.json"
        if (-not (Test-Path $credPath)) {
            Write-Error "Credentials file not found: $credPath"
        }
        $creds = Get-Content $credPath -Raw | ConvertFrom-Json

        Write-Host "Authenticating to check deployed version..."
        $token = Get-PlayStoreAccessToken $creds
        if (-not $token) { Write-Error "Authentication failed" }

        $PACKAGE = "com.dxxredux.app"
        $headers = @{ Authorization = "Bearer $token" }
        $baseUrl = "https://androidpublisher.googleapis.com/androidpublisher/v3/applications/$PACKAGE"

        # Create a temporary edit just for querying
        $edit = Invoke-RestMethod -Uri "$baseUrl/edits" -Method POST -Headers $headers `
            -ContentType "application/json" -Body "{}" -TimeoutSec 30
        $editId = $edit.id

        $deployedCode = 0
        try {
            $deployedCode = Get-TrackVersionCode -BaseUrl $baseUrl -EditId $editId `
                -Headers $headers -TrackName $TrackName
        } catch {
            Write-Host "Could not query track '$TrackName' (new track?) -- using rev 0"
        }

        # Delete the temporary edit
        try {
            Invoke-RestMethod -Uri "$baseUrl/edits/$editId" -Method DELETE `
                -Headers $headers -TimeoutSec 10 | Out-Null
        } catch {}

        # Compute rev
        if ($deployedCode -gt 0) {
            $deployedCommit = [math]::Floor($deployedCode / 10)
            $deployedRev = $deployedCode % 10
            Write-Host "Deployed version: $deployedCode (commit $deployedCommit, rev $deployedRev)"

            if ($deployedCommit -eq $commitCount) {
                $rev = $deployedRev + 1
                if ($rev -gt 9) {
                    Write-Error "Deployed version is already at rev 9 for commit $commitCount -- commit new changes first"
                }
                Write-Host "Same commit count -- bumping rev to $rev"
            } elseif ($deployedCommit -gt $commitCount) {
                Write-Host "WARNING: deployed commit count ($deployedCommit) > local ($commitCount)"
            }
        } else {
            Write-Host "No deployed version on track '$TrackName'"
        }
    }

    $versionCode = $commitCount * 10 + $rev
    Write-Host "Version code: $versionCode (commit $commitCount, rev $rev)"
    Write-Host ""

    # Build the AAB
    Write-Host "Step 1: Building AAB..."
    Write-Host ""
    & .\1_build-aab.ps1 -BuildType $BuildType -VersionCode $versionCode
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }

    if ($BuildOnly) {
        Write-Host ""
        Write-Host "================================"
        Write-Host "Build-only run completed successfully"
        Write-Host "================================"
        Write-Host ""
        return
    }

    # Deploy to Play Store
    Write-Host ""
    Write-Host "Step 2: Uploading to Play Store..."
    Write-Host ""
    & .\2_deploy-playstore.ps1 -TrackName $TrackName
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

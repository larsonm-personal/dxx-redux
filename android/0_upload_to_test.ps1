#!/usr/bin/env pwsh
# Menu for Play Store uploads, GitHub releases, or both
#
# Computes versionCode = commitCount*10 + rev, where rev auto-increments
# if the target track already has a version from the same commit count.
#
# Usage:
#   .\0_upload_to_test.ps1                     # Menu; Enter selects Play Store
#   .\0_upload_to_test.ps1 -BuildType "1"     # Build Debug instead
#   .\0_upload_to_test.ps1 -BuildType "2"     # Build Release instead
#   .\0_upload_to_test.ps1 -TrackName "alpha"  # Upload to alpha track instead
#   .\0_upload_to_test.ps1 -BuildOnly          # Build through this wrapper without uploading
#   .\0_upload_to_test.ps1 -Action 2 -ReleaseVersion 1.2.0  # GitHub release only
#   .\0_upload_to_test.ps1 -Action 3 -ReleaseVersion 1.2.0  # Both, without prompts
# Existing BuildType/TrackName/BuildOnly invocations keep the Play Store path unless Action is supplied

param(
    [string]$BuildType = "3",        # Default: Internal (debug + release signing)
    [string]$TrackName = "internal",  # Default: internal track
    [switch]$BuildOnly,               # Build selected destinations locally without publishing
    [ValidateSet('1', '2', '3')][string]$Action,
    [ValidatePattern('^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z]+([.-][0-9A-Za-z]+)*)?$')][string]$ReleaseVersion
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot

function Get-UploadedBuildStamp {
    $buildInfoPath = Join-Path $PSScriptRoot "app\src\main\java\com\dxxredux\app\BuildInfo.kt"
    if (-not (Test-Path $buildInfoPath)) {
        return "<missing BuildInfo.kt>"
    }

    $content = Get-Content $buildInfoPath -Raw
    $dateMatch = [regex]::Match($content, 'const val BUILD_DATE = "([^"]+)"')
    $timeMatch = [regex]::Match($content, 'const val BUILD_TIME = "([^"]+)"')

    if (-not $dateMatch.Success) {
        return "<missing BUILD_DATE>"
    }
    if (-not $timeMatch.Success) {
        return $dateMatch.Groups[1].Value
    }
    return "$($dateMatch.Groups[1].Value) $($timeMatch.Groups[1].Value)"
}

function Invoke-PlayBuildAndUpload {
    # Load shared auth helpers
    . "$PSScriptRoot\helpers\playstore-auth.ps1"

    # ---------------------------------------------------------------
    #  Query the target track to determine version code with rev
    # ---------------------------------------------------------------

    $commitCountText = git -C $repoRoot rev-list --count HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read the Git commit count' }
    $commitCount = [int]$commitCountText.Trim()
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

        $deployedCode = Get-TrackVersionCode -BaseUrl $baseUrl -EditId $editId `
            -Headers $headers -TrackName $TrackName

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

    # Build both local artifacts; only the AAB is uploaded
    Write-Host "Step 1: Building AAB and universal APK..."
    Write-Host ""
    $variantLabel = switch ($BuildType) { "1" { "debug" } "3" { "internal" } default { "release" } }
    $artifactPath = Join-Path $PSScriptRoot "build-outputs\deploy-$variantLabel-v$versionCode-$([guid]::NewGuid().ToString('N')).aab"
    & (Join-Path $PSScriptRoot "1_build_aab_apk.ps1") -BuildType $BuildType -VersionCode $versionCode -OutputPath $artifactPath
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }

    if ($BuildOnly) {
        $buildStamp = Get-UploadedBuildStamp
        Write-Host ""
        Write-Host "================================"
        Write-Host "Build-only run completed successfully"
        Write-Host "Build stamp: $buildStamp"
        Write-Host "================================"
        Write-Host ""
        return
    }

    # Deploy to Play Store
    Write-Host ""
    Write-Host "Step 2: Uploading to Play Store..."
    Write-Host ""
    & (Join-Path $PSScriptRoot "2_deploy-playstore.ps1") -TrackName $TrackName -AabPath $artifactPath
    if ($LASTEXITCODE -ne 0) {
        throw "Deploy failed with exit code $LASTEXITCODE"
    }

    Write-Host ""
    $buildStamp = Get-UploadedBuildStamp
    Write-Host "================================"
    Write-Host "Build and upload completed successfully!"
    Write-Host "Uploaded build stamp: $buildStamp"
    Write-Host "================================"
    Write-Host ""

}

try {
    Write-Host ""
    Write-Host "================================"
    Write-Host "DXX-Revival Build & Publish"
    Write-Host "================================"
    Write-Host ""

    if (-not $Action) {
        if ($BuildOnly -or $PSBoundParameters.ContainsKey('BuildType') -or $PSBoundParameters.ContainsKey('TrackName')) {
            $Action = '1'
        } else {
            Write-Host "  1) Build and upload to Play Store (default)"
            Write-Host "  2) Build and release on GitHub"
            Write-Host "  3) Do both"
            Write-Host ""
            while (-not $Action) {
                $choice = (Read-Host 'Select action (1-3, default 1)').Trim()
                if (-not $choice) { $choice = '1' }
                if ($choice -in @('1', '2', '3')) {
                    $Action = $choice
                } else {
                    Write-Host 'Enter a number between 1 and 3' -ForegroundColor Yellow
                }
            }
        }
    }

    if ($Action -eq '1' -and $ReleaseVersion) { throw '-ReleaseVersion requires -Action 2 or 3' }
    if ($Action -in @('2', '3')) {
        # Collect and validate the version before authentication, building or publishing
        while (-not $ReleaseVersion) {
            $candidate = (Read-Host 'GitHub release version (for example 1.2.0)').Trim()
            $versionValidation = (Get-Variable ReleaseVersion).Attributes | Where-Object { $_ -is [System.Management.Automation.ValidatePatternAttribute] }
            if ($candidate -cmatch $versionValidation.RegexPattern) {
                $ReleaseVersion = $candidate
            } else {
                Write-Host 'Enter a version like 1.2.0 or 1.2.0-rc.1' -ForegroundColor Yellow
            }
        }
    }

    if ($Action -in @('1', '3')) { Invoke-PlayBuildAndUpload }
    if ($Action -in @('2', '3')) {
        Write-Host "Building GitHub release $ReleaseVersion"
        $releaseParameters = @{ Version = $ReleaseVersion }
        if ($BuildOnly) { $releaseParameters.BuildOnly = $true }
        & (Join-Path $PSScriptRoot 'release-github.ps1') @releaseParameters
        if ($LASTEXITCODE -ne 0) { throw "GitHub release failed with exit code $LASTEXITCODE" }
        Write-Host "GitHub release $ReleaseVersion completed successfully"
    }
} catch {
    Write-Host ""
    Write-Host "ERROR: $_"
    Write-Host ""
    exit 1
}

# deploy-playstore.ps1 -- Upload AAB to Google Play via the Developer API
#
# Usage:
#   .\deploy-playstore.ps1                              # auto-finds latest AAB in build-outputs/
#   .\deploy-playstore.ps1 -AabPath path\to\app.aab     # explicit AAB
#   .\deploy-playstore.ps1 -CredentialsPath creds.json  # non-default credentials
#   .\deploy-playstore.ps1 -TrackName internal                   # select track by name
#   .\deploy-playstore.ps1 -TrackIndex 4                # select track by index (order varies -- prefer -TrackName)
#
# Prerequisites:
#   - play-store-credentials.json in the android/ directory
#     (see play-store-credentials.sample.json for setup instructions)

param(
    [string]$AabPath,
    [string]$CredentialsPath,
    [int]$TrackIndex,
    [string]$TrackName
)

$ErrorActionPreference = "Stop"
$PACKAGE = "com.dxxredux.app"

function Read-NumberedChoice {
    param(
        [string]$Prompt,
        [int]$OptionCount,
        [int]$DefaultChoice = 1
    )

    while ($true) {
        $choice = Read-Host $Prompt
        if ([string]::IsNullOrWhiteSpace($choice)) {
            return [string]$DefaultChoice
        }

        $selected = 0
        if ([int]::TryParse($choice, [ref]$selected) -and $selected -ge 1 -and $selected -le $OptionCount) {
            return [string]$selected
        }

        Write-Host "Enter a number between 1 and $OptionCount" -ForegroundColor Yellow
    }
}

# -- Locate credentials ----------------------------------------------
if (-not $CredentialsPath) {
    $CredentialsPath = Join-Path $PSScriptRoot "play-store-credentials.json"
}
if (-not (Test-Path $CredentialsPath)) {
    Write-Error @"
Credentials file not found: $CredentialsPath

Create one by following the instructions in play-store-credentials.sample.json
"@
}
$creds = Get-Content $CredentialsPath -Raw | ConvertFrom-Json
Write-Host "Service account: $($creds.client_email)"

# -- Locate AAB ------------------------------------------------------
if (-not $AabPath) {
    # Find the newest AAB in build-outputs/
    $outDir = Join-Path $PSScriptRoot "build-outputs"
    if (Test-Path $outDir) {
        $aab = Get-ChildItem "$outDir\*.aab" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    }
    if (-not $aab) {
        # Fall back to Gradle output
        $releaseAab = Join-Path $PSScriptRoot "app\build\outputs\bundle\release\app-release.aab"
        $internalAab = Join-Path $PSScriptRoot "app\build\outputs\bundle\internal\app-internal.aab"
        $debugAab = Join-Path $PSScriptRoot "app\build\outputs\bundle\debug\app-debug.aab"
        if (Test-Path $releaseAab) { $aab = Get-Item $releaseAab }
        elseif (Test-Path $internalAab) { $aab = Get-Item $internalAab }
        elseif (Test-Path $debugAab) { $aab = Get-Item $debugAab }
    }
    if (-not $aab) {
        Write-Error "No AAB found. Build one first with .\build-aab.ps1 or specify -AabPath"
    }
    $AabPath = $aab.FullName
}
if (-not (Test-Path $AabPath)) {
    Write-Error "AAB file not found: $AabPath"
}
$aabSize = [math]::Round((Get-Item $AabPath).Length / 1MB, 1)
Write-Host "AAB file: $AabPath ($aabSize MB)"
Write-Host ""

# ===================================================================
#  JWT / OAuth2 authentication (shared helper)
# ===================================================================

. "$PSScriptRoot\playstore-auth.ps1"

Write-Host "Authenticating..."
$token = Get-PlayStoreAccessToken $creds
if (-not $token) { Write-Error 'Authentication failed - no access token received.' }
Write-Host "Authenticated successfully"
Write-Host ""

$headers = @{ Authorization = "Bearer $token" }
$baseUrl = "https://androidpublisher.googleapis.com/androidpublisher/v3/applications/$PACKAGE"

# ===================================================================
#  Create an edit
# ===================================================================

Write-Host "Creating edit..."
$edit = Invoke-RestMethod -Uri "$baseUrl/edits" -Method POST -Headers $headers `
    -ContentType "application/json" -Body "{}" -TimeoutSec 30
$editId = $edit.id
Write-Host "Edit created: $editId"
Write-Host ""

# ===================================================================
#  List tracks and present menu
# ===================================================================

Write-Host "Fetching available tracks..."
$tracksResp = Invoke-RestMethod -Uri "$baseUrl/edits/$editId/tracks" -Method GET `
    -Headers $headers -TimeoutSec 30
$tracks = $tracksResp.tracks

if (-not $tracks -or $tracks.Count -eq 0) {
    # Provide defaults if the API returns empty (new app with no releases)
    $trackNames = @("internal", "alpha", "beta", "production")
} else {
    $trackNames = $tracks | ForEach-Object { $_.track }
}

Write-Host ""
Write-Host "Available tracks:"
for ($i = 0; $i -lt $trackNames.Count; $i++) {
    $name = $trackNames[$i]
    # Show current release info if available
    $info = ""
    $existing = $tracks | Where-Object { $_.track -eq $name }
    if ($existing -and $existing.releases) {
        $latest = $existing.releases | Select-Object -First 1
        $vers = ($latest.versionCodes -join ", ")
        $info = "  (status: $($latest.status), versionCodes: $vers)"
    }
    $num = $i + 1
    Write-Host "  ${num}) ${name}${info}"
}
# Use TrackName if provided, then TrackIndex, otherwise prompt
if ($TrackName) {
    $found = $false
    for ($k = 0; $k -lt $trackNames.Count; $k++) {
        if ($trackNames[$k] -eq $TrackName) {
            $choice = "$($k + 1)"
            $found = $true
            break
        }
    }
    if (-not $found) {
        Write-Error "Track '$TrackName' not found. Available: $($trackNames -join ', ')"
    }
} elseif ($TrackIndex -gt 0) {
    $choice = "$TrackIndex"
} else {
    Write-Host ""
    $choice = Read-NumberedChoice -Prompt "Select track (1-$($trackNames.Count))" -OptionCount $trackNames.Count
}
$idx = [int]$choice - 1
if ($idx -lt 0 -or $idx -ge $trackNames.Count) {
    Write-Error "Invalid selection: $choice"
}
$selectedTrack = $trackNames[$idx]
Write-Host "Selected track: $selectedTrack"
Write-Host ""

# ===================================================================
#  Helper: commit an edit, handling changesNotSentForReview quirk
#  Returns $null on success, or the error message string on failure.
# ===================================================================
function TryCommitEdit {
    param($baseUrl, $editId, $headers)
    $commitUrl = "$baseUrl/edits/$editId" + ":commit"
    try {
        Invoke-RestMethod -Uri $commitUrl -Method POST -Headers $headers `
            -ContentType "application/json" -Body "{}" -TimeoutSec 30 | Out-Null
        return $null
    } catch {
        $err = $_.ErrorDetails.Message
        if (-not $err) { try { $err = $_.Exception.Response.GetResponseStream() | ForEach-Object { (New-Object System.IO.StreamReader($_)).ReadToEnd() } } catch {} }
        if ($err -match "changesNotSentForReview") {
            Write-Host "Retrying commit with changesNotSentForReview..."
            $url2 = "$baseUrl/edits/$editId" + ":commit?changesNotSentForReview=true"
            try {
                Invoke-RestMethod -Uri $url2 -Method POST -Headers $headers `
                    -ContentType "application/json" -Body "{}" -TimeoutSec 30 | Out-Null
                return $null
            } catch {
                $err2 = $_.ErrorDetails.Message
                if (-not $err2) { try { $err2 = $_.Exception.Response.GetResponseStream() | ForEach-Object { (New-Object System.IO.StreamReader($_)).ReadToEnd() } } catch {} }
                return $err2
            }
        }
        return $err
    }
}

# ===================================================================
#  Upload AAB
# ===================================================================

Write-Host "Uploading AAB (${aabSize} MB) - this may take a while..."
$uploadUrl = "https://androidpublisher.googleapis.com/upload/androidpublisher/v3/applications/$PACKAGE/edits/$editId/bundles?uploadType=media"

# Read the entire file into memory and upload via Invoke-WebRequest
# (PS 5.1 doesn't have System.Net.Http.HttpClient loaded by default)
$aabBytes = [System.IO.File]::ReadAllBytes($AabPath)
$alreadyUploaded = $false
try {
    $uploadResp = Invoke-WebRequest -Uri $uploadUrl -Method POST -Headers $headers `
        -ContentType "application/octet-stream" -Body $aabBytes -TimeoutSec 600 `
        -UseBasicParsing
    $uploadResult = $uploadResp.Content | ConvertFrom-Json
    $versionCode = $uploadResult.versionCode
    Write-Host "Upload complete. versionCode: $versionCode"
} catch {
    $errBody = $_.ErrorDetails.Message
    if (-not $errBody) { try { $errBody = $_.Exception.Response.GetResponseStream() | ForEach-Object { (New-Object System.IO.StreamReader($_)).ReadToEnd() } } catch {} }
    if ($errBody -match "already been used") {
        # Extract the versionCode from the AAB filename or error message
        if ($errBody -match "Version code (\d+)") { $versionCode = [int]$Matches[1] }
        Write-Host "Version code $versionCode already uploaded -- skipping to promotion"
        $alreadyUploaded = $true
    } else {
        Write-Error "Upload failed: $errBody`n$_"
    }
}
Write-Host ""

if (-not $alreadyUploaded) {
    # ===================================================================
    #  Update track with the new release
    # ===================================================================

    Write-Host "Assigning versionCode $versionCode to track '$selectedTrack'..."
    $releaseStatus = "completed"
    $trackBody = @{
        track    = $selectedTrack
        releases = @(
            @{
                name         = "v$versionCode"
                versionCodes = @( $versionCode )
                status       = $releaseStatus
            }
        )
    } | ConvertTo-Json -Depth 5

    try {
        $trackResult = Invoke-RestMethod -Uri "$baseUrl/edits/$editId/tracks/$selectedTrack" `
            -Method PUT -Headers $headers `
            -ContentType "application/json" -Body $trackBody -TimeoutSec 30
        Write-Host "Track updated: $($trackResult.track)  status=$($trackResult.releases[0].status)"
    } catch {
        $errBody = $_.ErrorDetails.Message
        if (-not $errBody) { try { $errBody = $_.Exception.Response.GetResponseStream() | ForEach-Object { (New-Object System.IO.StreamReader($_)).ReadToEnd() } } catch {} }
        Write-Error "Track update failed: $errBody`n$_"
    }
    Write-Host ""

    # ===================================================================
    #  Commit the edit
    #  Handles: changesNotSentForReview quirk, draft-app status constraint
    # ===================================================================

    Write-Host "Committing edit..."
    $commitError = TryCommitEdit $baseUrl $editId $headers

    # Draft apps reject status=completed; fall back to status=draft
    if ($commitError -and $commitError -match "draft app" -and $releaseStatus -ne "draft") {
        Write-Host "App is in draft state. Re-assigning track with status=draft..."
        $releaseStatus = "draft"
        $trackBody = @{
            track    = $selectedTrack
            releases = @(
                @{
                    name         = "v$versionCode"
                    versionCodes = @( $versionCode )
                    status       = "draft"
                }
            )
        } | ConvertTo-Json -Depth 5
        try {
            Invoke-RestMethod -Uri "$baseUrl/edits/$editId/tracks/$selectedTrack" `
                -Method PUT -Headers $headers `
                -ContentType "application/json" -Body $trackBody -TimeoutSec 30 | Out-Null
            Write-Host "Track re-assigned with status=draft"
        } catch {
            $errBody = $_.ErrorDetails.Message
            if (-not $errBody) { try { $errBody = $_.Exception.Response.GetResponseStream() | ForEach-Object { (New-Object System.IO.StreamReader($_)).ReadToEnd() } } catch {} }
            Write-Error "Track re-assignment failed: $errBody`n$_"
        }
        $commitError = TryCommitEdit $baseUrl $editId $headers
    }

    if ($commitError) {
        Write-Error "Commit failed: $commitError"
    } else {
        Write-Host "Edit committed"
    }
    Write-Host ""
} else {
    # Already uploaded -- promote existing version via new edit
    Write-Host "Promoting existing versionCode $versionCode..."
    try {
        Invoke-RestMethod -Uri "$baseUrl/edits/$editId" -Method DELETE -Headers $headers -TimeoutSec 10 | Out-Null
    } catch {}

    $edit2 = Invoke-RestMethod -Uri "$baseUrl/edits" -Method POST -Headers $headers `
        -ContentType "application/json" -Body "{}" -TimeoutSec 30
    $editId = $edit2.id

    $releaseStatus = "completed"
    $trackBody = @{
        track    = $selectedTrack
        releases = @(
            @{
                name         = "v$versionCode"
                versionCodes = @( $versionCode )
                status       = $releaseStatus
            }
        )
    } | ConvertTo-Json -Depth 5

    try {
        $trackResult = Invoke-RestMethod -Uri "$baseUrl/edits/$editId/tracks/$selectedTrack" `
            -Method PUT -Headers $headers `
            -ContentType "application/json" -Body $trackBody -TimeoutSec 30
        Write-Host "Track updated: status=$($trackResult.releases[0].status)"
    } catch {
        $errBody = $_.ErrorDetails.Message
        if (-not $errBody) { try { $errBody = $_.Exception.Response.GetResponseStream() | ForEach-Object { (New-Object System.IO.StreamReader($_)).ReadToEnd() } } catch {} }
        Write-Error "Track update failed: $errBody`n$_"
    }

    Write-Host "Committing edit..."
    $commitError = TryCommitEdit $baseUrl $editId $headers

    # Draft apps reject status=completed; fall back to status=draft
    if ($commitError -and $commitError -match "draft app" -and $releaseStatus -ne "draft") {
        Write-Host "App is in draft state. Re-assigning track with status=draft..."
        $releaseStatus = "draft"
        $trackBody = @{
            track    = $selectedTrack
            releases = @(
                @{
                    name         = "v$versionCode"
                    versionCodes = @( $versionCode )
                    status       = "draft"
                }
            )
        } | ConvertTo-Json -Depth 5
        try {
            Invoke-RestMethod -Uri "$baseUrl/edits/$editId/tracks/$selectedTrack" `
                -Method PUT -Headers $headers `
                -ContentType "application/json" -Body $trackBody -TimeoutSec 30 | Out-Null
            Write-Host "Track re-assigned with status=draft"
        } catch {
            $errBody = $_.ErrorDetails.Message
            if (-not $errBody) { try { $errBody = $_.Exception.Response.GetResponseStream() | ForEach-Object { (New-Object System.IO.StreamReader($_)).ReadToEnd() } } catch {} }
            Write-Error "Track re-assignment failed: $errBody`n$_"
        }
        $commitError = TryCommitEdit $baseUrl $editId $headers
    }

    if ($commitError) {
        Write-Error "Commit failed: $commitError"
    } else {
        Write-Host "Edit committed"
    }
    Write-Host ""
}

# ===================================================================
#  If the release ended up as "draft", promote it to "completed"
# ===================================================================

# note that this will fail until a closed alpha has been created (with screenshots, description, etc.)
# https://stackoverflow.com/questions/76541480/how-does-fully-create-an-internal-release-on-google-play-console-via-api

if ($releaseStatus -eq "draft") {
    Write-Host "Promoting release from draft to completed..."
    $promoteEdit = Invoke-RestMethod -Uri "$baseUrl/edits" -Method POST -Headers $headers `
        -ContentType "application/json" -Body "{}" -TimeoutSec 30
    $promoteEditId = $promoteEdit.id

    $promoteBody = @{
        track    = $selectedTrack
        releases = @(
            @{
                name         = "v$versionCode"
                versionCodes = @( $versionCode )
                status       = "completed"
            }
        )
    } | ConvertTo-Json -Depth 5

    try {
        Invoke-RestMethod -Uri "$baseUrl/edits/$promoteEditId/tracks/$selectedTrack" `
            -Method PUT -Headers $headers `
            -ContentType "application/json" -Body $promoteBody -TimeoutSec 30 | Out-Null

        $promoteCommitError = TryCommitEdit $baseUrl $promoteEditId $headers
        if ($promoteCommitError) {
            Write-Host "WARNING: Could not promote to completed: $promoteCommitError"
            Write-Host "Release remains in draft status"
        } else {
            $releaseStatus = "completed"
            Write-Host "Release promoted to completed"
        }
    } catch {
        $errBody = $_.ErrorDetails.Message
        if (-not $errBody) { try { $errBody = $_.Exception.Response.GetResponseStream() | ForEach-Object { (New-Object System.IO.StreamReader($_)).ReadToEnd() } } catch {} }
        Write-Host "WARNING: Could not promote to completed: $errBody"
        Write-Host "Release remains in draft status"
    }
    Write-Host ""
}

Write-Host ""
Write-Host "=== Deployment successful ==="
Write-Host "  Package:     $PACKAGE"
Write-Host "  Track:       $selectedTrack"
Write-Host "  VersionCode: $versionCode"
Write-Host "  Status:      $releaseStatus"
Write-Host ""

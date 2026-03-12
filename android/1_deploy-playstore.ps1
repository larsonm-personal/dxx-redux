# deploy-playstore.ps1 -- Upload AAB to Google Play via the Developer API
#
# Usage:
#   .\deploy-playstore.ps1                              # auto-finds latest AAB in build-outputs/
#   .\deploy-playstore.ps1 -AabPath path\to\app.aab     # explicit AAB
#   .\deploy-playstore.ps1 -CredentialsPath creds.json  # non-default credentials
#
# Prerequisites:
#   - play-store-credentials.json in the android/ directory
#     (see play-store-credentials.sample.json for setup instructions)

param(
    [string]$AabPath,
    [string]$CredentialsPath
)

$ErrorActionPreference = "Stop"
$PACKAGE = "com.dxxredux.app"

# ── Locate credentials ──────────────────────────────────────────────
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

# ── Locate AAB ──────────────────────────────────────────────────────
if (-not $AabPath) {
    # Find the newest AAB in build-outputs/
    $outDir = Join-Path $PSScriptRoot "build-outputs"
    if (Test-Path $outDir) {
        $aab = Get-ChildItem "$outDir\*.aab" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    }
    if (-not $aab) {
        # Fall back to Gradle output
        $releaseAab = Join-Path $PSScriptRoot "app\build\outputs\bundle\release\app-release.aab"
        $debugAab   = Join-Path $PSScriptRoot "app\build\outputs\bundle\debug\app-debug.aab"
        if     (Test-Path $releaseAab) { $aab = Get-Item $releaseAab }
        elseif (Test-Path $debugAab)   { $aab = Get-Item $debugAab }
    }
    if (-not $aab) {
        Write-Error "No AAB found. Build one first with .\build-aab.ps1 or specify -AabPath."
    }
    $AabPath = $aab.FullName
}
if (-not (Test-Path $AabPath)) {
    Write-Error "AAB file not found: $AabPath"
}
$aabSize = [math]::Round((Get-Item $AabPath).Length / 1MB, 1)
Write-Host "AAB file: $AabPath ($aabSize MB)"
Write-Host ""

# ═══════════════════════════════════════════════════════════════════
#  JWT / OAuth2 authentication
# ═══════════════════════════════════════════════════════════════════

function ConvertTo-Base64Url([byte[]]$bytes) {
    [Convert]::ToBase64String($bytes).TrimEnd('=').Replace('+', '-').Replace('/', '_')
}

# Read an ASN.1 tag + length from a BinaryReader; returns @{Tag; Length}
function Read-Asn1TL([System.IO.BinaryReader]$reader) {
    $tag = $reader.ReadByte()
    [int]$len = $reader.ReadByte()
    if ($len -band 0x80) {
        $numBytes = $len -band 0x7F
        $len = 0
        for ($j = 0; $j -lt $numBytes; $j++) {
            $len = ($len -shl 8) + $reader.ReadByte()
        }
    }
    return @{ Tag = $tag; Length = $len }
}

# Read an ASN.1 INTEGER and return the magnitude bytes (leading-zero stripped)
function Read-Asn1Integer([System.IO.BinaryReader]$reader) {
    $tl = Read-Asn1TL $reader
    [byte[]]$raw = $reader.ReadBytes($tl.Length)
    if ($raw.Length -gt 1 -and $raw[0] -eq 0) {
        $raw = $raw[1..($raw.Length - 1)]
    }
    return ,$raw   # comma prevents PowerShell from unrolling the array
}

# Parse a PKCS#8-encoded RSA private key (DER bytes) into an RSACryptoServiceProvider.
# Used on .NET Framework (PowerShell 5.1) where ImportPkcs8PrivateKey is unavailable.
function Import-Pkcs8RsaKey([byte[]]$pkcs8Bytes) {
    $ms = New-Object System.IO.MemoryStream(,$pkcs8Bytes)
    $br = New-Object System.IO.BinaryReader($ms)

    # PKCS#8 PrivateKeyInfo: SEQUENCE { version INTEGER, SEQUENCE { OID, NULL }, OCTET STRING }
    $null = Read-Asn1TL $br              # outer SEQUENCE
    $ver  = Read-Asn1TL $br              # version INTEGER (value 0)
    $null = $br.ReadBytes($ver.Length)    # skip version value
    $alg  = Read-Asn1TL $br              # algorithm identifier SEQUENCE
    $null = $br.ReadBytes($alg.Length)    # skip OID + params
    $oct  = Read-Asn1TL $br              # OCTET STRING wrapping RSAPrivateKey
    [byte[]]$rsaKeyBytes = $br.ReadBytes($oct.Length)
    $br.Close(); $ms.Close()

    # RSAPrivateKey: SEQUENCE { version, n, e, d, p, q, dp, dq, iq }
    $ms2 = New-Object System.IO.MemoryStream(,$rsaKeyBytes)
    $br2 = New-Object System.IO.BinaryReader($ms2)

    $null = Read-Asn1TL $br2             # outer SEQUENCE
    $null = Read-Asn1Integer $br2        # version (0)

    $rsaParams = New-Object System.Security.Cryptography.RSAParameters
    $rsaParams.Modulus  = Read-Asn1Integer $br2
    $rsaParams.Exponent = Read-Asn1Integer $br2
    $rsaParams.D        = Read-Asn1Integer $br2
    $rsaParams.P        = Read-Asn1Integer $br2
    $rsaParams.Q        = Read-Asn1Integer $br2
    $rsaParams.DP       = Read-Asn1Integer $br2
    $rsaParams.DQ       = Read-Asn1Integer $br2
    $rsaParams.InverseQ = Read-Asn1Integer $br2
    $br2.Close(); $ms2.Close()

    $rsa = New-Object System.Security.Cryptography.RSACryptoServiceProvider
    $rsa.ImportParameters($rsaParams)
    return $rsa
}

function Get-AccessToken {
    param($creds)

    $now = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()

    # JWT header
    $header = @{ alg = "RS256"; typ = "JWT" } | ConvertTo-Json -Compress
    $headerB64 = ConvertTo-Base64Url ([System.Text.Encoding]::UTF8.GetBytes($header))

    # JWT claims
    $claims = @{
        iss   = $creds.client_email
        scope = "https://www.googleapis.com/auth/androidpublisher"
        aud   = $creds.token_uri
        iat   = $now
        exp   = $now + 3600
    } | ConvertTo-Json -Compress
    $claimsB64 = ConvertTo-Base64Url ([System.Text.Encoding]::UTF8.GetBytes($claims))

    $unsigned = "$headerB64.$claimsB64"

    # Sign with RSA private key from the service-account JSON
    $pemKey = $creds.private_key -replace "-----BEGIN PRIVATE KEY-----", "" `
                                 -replace "-----END PRIVATE KEY-----", "" `
                                 -replace "-----BEGIN RSA PRIVATE KEY-----", "" `
                                 -replace "-----END RSA PRIVATE KEY-----", "" `
                                 -replace "\s+", ""
    $keyBytes = [Convert]::FromBase64String($pemKey)

    # Import the private key -- works on both .NET Framework (PS 5.1) and .NET Core (PS 7+)
    $rsa = $null
    # Try modern API first (available on .NET Core / PS 7+)
    try {
        $rsa = [System.Security.Cryptography.RSA]::Create()
        $rsa.ImportPkcs8PrivateKey($keyBytes, [ref]$null)
    } catch {
        # Fall back for .NET Framework (PS 5.1): manually parse PKCS#8 → RSAPrivateKey ASN.1
        $rsa = Import-Pkcs8RsaKey $keyBytes
    }

    $sigBytes = $rsa.SignData(
        [System.Text.Encoding]::UTF8.GetBytes($unsigned),
        [System.Security.Cryptography.HashAlgorithmName]::SHA256,
        [System.Security.Cryptography.RSASignaturePadding]::Pkcs1
    )
    $sig = ConvertTo-Base64Url $sigBytes
    $jwt = "$unsigned.$sig"

    # Exchange JWT for access token
    $tokenResp = Invoke-RestMethod -Uri $creds.token_uri -Method POST -Body @{
        grant_type = "urn:ietf:params:oauth:grant-type:jwt-bearer"
        assertion  = $jwt
    } -TimeoutSec 30
    return $tokenResp.access_token
}

Write-Host "Authenticating..."
$token = Get-AccessToken $creds
if (-not $token) { Write-Error 'Authentication failed - no access token received.' }
Write-Host "Authenticated successfully."
Write-Host ""

$headers = @{ Authorization = "Bearer $token" }
$baseUrl = "https://androidpublisher.googleapis.com/androidpublisher/v3/applications/$PACKAGE"

# ═══════════════════════════════════════════════════════════════════
#  Create an edit
# ═══════════════════════════════════════════════════════════════════

Write-Host "Creating edit..."
$edit = Invoke-RestMethod -Uri "$baseUrl/edits" -Method POST -Headers $headers `
        -ContentType "application/json" -Body "{}" -TimeoutSec 30
$editId = $edit.id
Write-Host "Edit created: $editId"
Write-Host ""

# ═══════════════════════════════════════════════════════════════════
#  List tracks and present menu
# ═══════════════════════════════════════════════════════════════════

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
# Default to "internal" if available, otherwise first track
$defaultIdx = 0
for ($j = 0; $j -lt $trackNames.Count; $j++) {
    if ($trackNames[$j] -eq "internal") { $defaultIdx = $j; break }
}
$defaultNum = $defaultIdx + 1
Write-Host ""
$choice = Read-Host "Select track [$defaultNum]"
if (-not $choice) { $choice = "$defaultNum" }
$idx = [int]$choice - 1
if ($idx -lt 0 -or $idx -ge $trackNames.Count) {
    Write-Error "Invalid selection: $choice"
}
$selectedTrack = $trackNames[$idx]
Write-Host "Selected track: $selectedTrack"
Write-Host ""

# ═══════════════════════════════════════════════════════════════════
#  Helper: commit an edit, handling changesNotSentForReview quirk
#  Returns $null on success, or the error message string on failure.
# ═══════════════════════════════════════════════════════════════════
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

# ═══════════════════════════════════════════════════════════════════
#  Upload AAB
# ═══════════════════════════════════════════════════════════════════

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
        Write-Host "Version code $versionCode already uploaded -- skipping to promotion."
        $alreadyUploaded = $true
    } else {
        Write-Error "Upload failed: $errBody`n$_"
    }
}
Write-Host ""

if (-not $alreadyUploaded) {
    # ═══════════════════════════════════════════════════════════════════
    #  Update track with the new release
    # ═══════════════════════════════════════════════════════════════════

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

    # ═══════════════════════════════════════════════════════════════════
    #  Commit the edit
    #  Handles: changesNotSentForReview quirk, draft-app status constraint
    # ═══════════════════════════════════════════════════════════════════

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
        Write-Host "Edit committed."
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
        Write-Host "Edit committed."
    }
    Write-Host ""
}

# ═══════════════════════════════════════════════════════════════════
#  If the release ended up as "draft", promote it to "completed"
# ═══════════════════════════════════════════════════════════════════

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
            Write-Host "Release remains in draft status."
        } else {
            $releaseStatus = "completed"
            Write-Host "Release promoted to completed."
        }
    } catch {
        $errBody = $_.ErrorDetails.Message
        if (-not $errBody) { try { $errBody = $_.Exception.Response.GetResponseStream() | ForEach-Object { (New-Object System.IO.StreamReader($_)).ReadToEnd() } } catch {} }
        Write-Host "WARNING: Could not promote to completed: $errBody"
        Write-Host "Release remains in draft status."
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

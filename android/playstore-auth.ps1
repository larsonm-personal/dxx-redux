# playstore-auth.ps1 -- Shared Play Store API authentication helpers
# Dot-source this from scripts that need Play Store API access:
#   . "$PSScriptRoot\playstore-auth.ps1"

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
    return , $raw   # comma prevents PowerShell from unrolling the array
}

# Parse a PKCS#8-encoded RSA private key (DER bytes) into an RSACryptoServiceProvider.
# Used on .NET Framework (PowerShell 5.1) where ImportPkcs8PrivateKey is unavailable.
function Import-Pkcs8RsaKey([byte[]]$pkcs8Bytes) {
    $ms = New-Object System.IO.MemoryStream(, $pkcs8Bytes)
    $br = New-Object System.IO.BinaryReader($ms)

    # PKCS#8 PrivateKeyInfo: SEQUENCE { version INTEGER, SEQUENCE { OID, NULL }, OCTET STRING }
    $null = Read-Asn1TL $br              # outer SEQUENCE
    $ver = Read-Asn1TL $br              # version INTEGER (value 0)
    $null = $br.ReadBytes($ver.Length)    # skip version value
    $alg = Read-Asn1TL $br              # algorithm identifier SEQUENCE
    $null = $br.ReadBytes($alg.Length)    # skip OID + params
    $oct = Read-Asn1TL $br              # OCTET STRING wrapping RSAPrivateKey
    [byte[]]$rsaKeyBytes = $br.ReadBytes($oct.Length)
    $br.Close(); $ms.Close()

    # RSAPrivateKey: SEQUENCE { version, n, e, d, p, q, dp, dq, iq }
    $ms2 = New-Object System.IO.MemoryStream(, $rsaKeyBytes)
    $br2 = New-Object System.IO.BinaryReader($ms2)

    $null = Read-Asn1TL $br2             # outer SEQUENCE
    $null = Read-Asn1Integer $br2        # version (0)

    $rsaParams = New-Object System.Security.Cryptography.RSAParameters
    $rsaParams.Modulus = Read-Asn1Integer $br2
    $rsaParams.Exponent = Read-Asn1Integer $br2
    $rsaParams.D = Read-Asn1Integer $br2
    $rsaParams.P = Read-Asn1Integer $br2
    $rsaParams.Q = Read-Asn1Integer $br2
    $rsaParams.DP = Read-Asn1Integer $br2
    $rsaParams.DQ = Read-Asn1Integer $br2
    $rsaParams.InverseQ = Read-Asn1Integer $br2
    $br2.Close(); $ms2.Close()

    $rsa = New-Object System.Security.Cryptography.RSACryptoServiceProvider
    $rsa.ImportParameters($rsaParams)
    return $rsa
}

function Get-PlayStoreAccessToken {
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
        # Fall back for .NET Framework (PS 5.1): manually parse PKCS#8 -> RSAPrivateKey ASN.1
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

# Get the highest version code currently on a given track.
# Returns 0 if no releases exist on the track.
function Get-TrackVersionCode {
    param(
        [string]$BaseUrl,
        [string]$EditId,
        [hashtable]$Headers,
        [string]$TrackName
    )

    $trackResp = Invoke-RestMethod -Uri "$BaseUrl/edits/$EditId/tracks/$TrackName" `
        -Method GET -Headers $Headers -TimeoutSec 30
    if ($trackResp.releases) {
        $allCodes = @()
        foreach ($rel in $trackResp.releases) {
            if ($rel.versionCodes) { $allCodes += $rel.versionCodes }
        }
        if ($allCodes.Count -gt 0) {
            return ($allCodes | Measure-Object -Maximum).Maximum
        }
    }
    return 0
}

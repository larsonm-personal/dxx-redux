Set-StrictMode -Version Latest

function Read-DxxDependencyConfig {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)

    $path = Join-Path $RepoRoot 'android\get_deps\tool_versions.conf'
    $config = @{}
    foreach ($line in Get-Content -LiteralPath $path) {
        if ($line -match '^([A-Z0-9_]+)=(.+)$') {
            $config[$Matches[1]] = $Matches[2].Trim("'")
        }
    }
    return $config
}

function Assert-DxxFileSha256 {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$ExpectedSha256,
        [string]$Label = $Path
    )

    if ($ExpectedSha256 -notmatch '^[0-9a-fA-F]{64}$') {
        throw "Invalid configured SHA-256 for $Label"
    }
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label not found at $Path"
    }
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $ExpectedSha256.ToLowerInvariant()) {
        throw "$Label SHA-256 mismatch expected=$($ExpectedSha256.ToLowerInvariant()) actual=$actual"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Get-DxxTreeSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    $root = (Resolve-Path -LiteralPath $Path).Path
    $records = @(
        Get-ChildItem -LiteralPath $root -Recurse -File |
            ForEach-Object {
                $relative = $_.FullName.Substring($root.Length + 1).Replace('\', '/')
                $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                "$relative`0$hash`n"
            } |
            Sort-Object
    )
    $bytes = [Text.Encoding]::UTF8.GetBytes(($records -join ''))
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($bytes)) -replace '-', '').ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

function Assert-DxxTreeSha256 {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$ExpectedSha256,
        [string]$Label = $Path
    )

    if ($ExpectedSha256 -notmatch '^[0-9a-fA-F]{64}$') {
        throw "Invalid configured tree SHA-256 for $Label"
    }
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Label not found at $Path"
    }
    $actual = Get-DxxTreeSha256 -Path $Path
    if ($actual -ne $ExpectedSha256.ToLowerInvariant()) {
        throw "$Label tree SHA-256 mismatch expected=$($ExpectedSha256.ToLowerInvariant()) actual=$actual"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Resolve-DxxVerifiedDependencyExecutable {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$DirectoryKey,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Sha256Key,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $config = Read-DxxDependencyConfig -RepoRoot $RepoRoot
    foreach ($key in @($DirectoryKey, $Sha256Key)) {
        if (-not $config.ContainsKey($key) -or [string]::IsNullOrWhiteSpace($config[$key])) {
            throw "$key not found in tool_versions.conf"
        }
    }
    $depBaseFile = Join-Path $RepoRoot 'dependency_base.txt'
    if (-not (Test-Path -LiteralPath $depBaseFile -PathType Leaf)) {
        throw "dependency_base.txt not found at $depBaseFile"
    }
    $depBase = (Get-Content -LiteralPath $depBaseFile -First 1).Trim()
    $path = Join-Path (Join-Path $depBase $config[$DirectoryKey]) $RelativePath
    return Assert-DxxFileSha256 -Path $path -ExpectedSha256 $config[$Sha256Key] -Label $Label
}

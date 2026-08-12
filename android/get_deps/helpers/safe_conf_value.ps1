function Test-SafeToolConfValue {
    param(
        [Parameter(Mandatory)] [string] $Key,
        [Parameter(Mandatory)] [string] $Value
    )

    if ($Key -notmatch '^[A-Z][A-Z0-9_]*$' -or $Value.Length -lt 1 -or $Value.Length -gt 4096) {
        return $false
    }
    # This grammar deliberately excludes every shell expansion, quoting,
    # control, whitespace, separator, glob, and redirection character.
    if ($Value -notmatch '^[A-Za-z0-9][A-Za-z0-9._~:/?&=%+@,-]*$') {
        return $false
    }
    if ($Key -match '_(?:COMMIT|SHA|SHA256)$' -and $Value -notmatch '^(?:[0-9A-Fa-f]{40}|[0-9A-Fa-f]{64})$') {
        return $false
    }
    if ($Key -match '_URL$') {
        $uri = $null
        if (-not [Uri]::TryCreate($Value, [UriKind]::Absolute, [ref] $uri) -or
            $uri.Scheme -ne 'https' -or -not $uri.Host -or $uri.UserInfo) {
            return $false
        }
    }
    return $true
}

function Format-SafeToolConfAssignment {
    param(
        [Parameter(Mandatory)] [string] $Key,
        [Parameter(Mandatory)] [string] $Value
    )

    if (-not (Test-SafeToolConfValue -Key $Key -Value $Value)) {
        throw "Unsafe value rejected for $Key"
    }
    # The allowlist excludes a single quote; explicit shell quoting keeps
    # accepted URL punctuation inert when Bash sources the legacy manifest.
    return "$Key='$Value'"
}

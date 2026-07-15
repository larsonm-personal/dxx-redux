function Read-InputDemoGraphicsCanaryManifest {
    param([Parameter(Mandatory)][string]$ManifestPath)

    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        throw "Input-demo graphics canary manifest not found: $ManifestPath"
    }

    $entries = @{}
    foreach ($line in [System.IO.File]::ReadLines($ManifestPath)) {
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith('#')) { continue }
        $parts = $trimmed -split '\|', 3
        if ($parts.Count -ne 3 -or $parts[0] -notin @('d1', 'd2') -or
            $parts[2] -notmatch '^[0-9a-fA-F]{64}$') {
            throw "Invalid input-demo graphics canary manifest row: $trimmed"
        }
        $game = $parts[0]
        if ($entries.ContainsKey($game)) {
            throw "Duplicate input-demo graphics canary for $game"
        }
        $entries[$game] = [pscustomobject]@{
            Game = $game
            FileName = $parts[1]
            Sha256 = $parts[2].ToLowerInvariant()
        }
    }

    foreach ($game in @('d1', 'd2')) {
        if (-not $entries.ContainsKey($game)) {
            throw "Input-demo graphics canary manifest is missing $game"
        }
    }
    return $entries
}

function Get-InputDemoGraphicsCanaryIssue {
    param(
        [Parameter(Mandatory)][string]$DemoRoot,
        [Parameter(Mandatory)]$Entry
    )

    if (-not (Test-Path -LiteralPath $DemoRoot -PathType Container)) {
        return "demo root not found: $DemoRoot"
    }
    $matches = @(Get-ChildItem -LiteralPath $DemoRoot -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ceq $Entry.FileName })
    if ($matches.Count -ne 1) {
        return "expected exactly one $($Entry.FileName), found $($matches.Count)"
    }
    $actualHash = (Get-FileHash -LiteralPath $matches[0].FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -cne $Entry.Sha256) {
        return "hash mismatch for $($Entry.FileName): expected $($Entry.Sha256), actual $actualHash"
    }
    return $null
}

function Resolve-InputDemoGraphicsCanaryPath {
    param(
        [Parameter(Mandatory)][string]$DemoRoot,
        [Parameter(Mandatory)]$Entry
    )

    $issue = Get-InputDemoGraphicsCanaryIssue -DemoRoot $DemoRoot -Entry $Entry
    if ($issue) { throw "Input-demo graphics canary unavailable: $issue" }
    return @(Get-ChildItem -LiteralPath $DemoRoot -Recurse -File |
            Where-Object { $_.Name -ceq $Entry.FileName })[0].FullName
}

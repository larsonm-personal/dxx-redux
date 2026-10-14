#!/usr/bin/env pwsh
# Playback metadata only; recordings and independent comparison oracles stay unchanged

function Get-InputDemoFixtureRoots {
    param([string]$RepoRoot)

    foreach ($name in @('public_level_replays', 'public_d1_fault_probes', 'public_d1_fan_probes', 'public_all_weapons')) {
        Join-Path $RepoRoot "android/tests/fixtures/$name"
    }
}

function Get-InputDemoFixtureInfo {
    param([string]$RepoRoot, [string]$Path)

    $directory = [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Path))
    $collection = $null
    $comparison = if ([IO.Path]::DirectorySeparatorChar -eq '\') { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
    foreach ($root in (Get-InputDemoFixtureRoots -RepoRoot $RepoRoot)) {
        if ($directory.Equals([IO.Path]::GetFullPath($root), $comparison)) {
            $collection = Split-Path $root -Leaf
            break
        }
    }
    if (-not $collection) { return $null }
    $manifest = ConvertFrom-CompatibleJsonHashtable -Json ([IO.File]::ReadAllText((Join-Path $directory 'manifest.json')))
    $fileName = [IO.Path]::GetFileName($Path)
    $case = @($manifest.cases | Where-Object {
            $inputName = if ($_.ContainsKey('demo')) { [string]$_.demo } elseif ($_.ContainsKey('input')) { [string]$_.input } else { '' }
            $inputName.Equals($fileName, $comparison)
        })
    if ($case.Count -ne 1) { return $null }
    $name = [IO.Path]::GetFileNameWithoutExtension($Path)
    $note = switch ($name) {
        'd1-monitor-phase' { 'KNOWN IMPORTED FAULT: monitor animation is one frame early; terminal summary does not check phase' }
        'd1-mine-blast' { 'KNOWN IMPORTED FAULT: concussion moves the live mine differently; terminal summary does not check mine force' }
        'd1-fan-transparency' { 'KNOWN IMPORTED FAULT: fan collision diverges, then strict carried-RNG abort before input 301; nonzero exit remains a failure' }
        'd1-mine-no-concussion' { 'CAUSAL CONTROL: no concussion; full imported parity remains unqualified' }
        'd1-self-splash-no-fire' { 'CAUSAL CONTROL: no missile; full imported parity remains unqualified' }
        'd1-fan-no-fire' { 'CAUSAL CONTROL: no flare; full imported parity remains unqualified' }
        'd1-fan-delay' { 'CAUSAL CONTROL: flare delayed one frame; full imported parity remains unqualified' }
        'd1-public-door-lifecycle' { 'UNQUALIFIED IMPORTED DIAGNOSTIC: excluded from imported coverage (controlling_trigger difference)' }
        'd1-public-secondary-mouse-first' { 'UNQUALIFIED IMPORTED DIAGNOSTIC: excluded from imported coverage; native ordered-dispatch fixture' }
        'd1-public-secondary-release-first' { 'UNQUALIFIED IMPORTED DIAGNOSTIC: excluded from imported coverage; native ordered-dispatch fixture' }
        default { 'Imported D1 is diagnostic; a terminal summary match does not establish full parity' }
    }
    return [pscustomobject]@{
        Collection = $collection
        DataEdition = 'shareware'
        NativeOnly = ($collection -eq 'public_all_weapons')
        ImportedNote = $note
    }
}

function Copy-InputDemoComparisonPilot {
    param([string]$RepoRoot, [string]$Destination)

    $root = Join-Path $RepoRoot 'android/tests/fixtures/public_d1_fan_probes'
    $manifest = ConvertFrom-CompatibleJsonHashtable -Json ([IO.File]::ReadAllText((Join-Path $root 'manifest.json')))
    # These files were produced and read back by the native D2 writer
    foreach ($name in @('d1match.plr', 'd1match.plx')) {
        $relative = "profile/$name"
        $entry = $manifest.files[$relative]
        $source = Join-Path $root $relative
        if (-not $entry -or -not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "Missing verified comparison pilot: $source"
        }
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ine $entry.sha256) {
            throw "Comparison pilot SHA-256 mismatch: $source"
        }
    }
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root 'profile/d1match.plr') -Destination (Join-Path $Destination 'd1match.plr') -Force
    Copy-Item -LiteralPath (Join-Path $root 'profile/d1match.plx') -Destination (Join-Path $Destination 'd1match.plx') -Force
}

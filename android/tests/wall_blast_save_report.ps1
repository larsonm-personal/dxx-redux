# Validate the real native/imported save evidence; also used by focused rejection tests
function Assert-WallBlastSaveReport {
    param(
        [Parameter(Mandatory)][string]$Directory,
        [Parameter(Mandatory)][ValidateSet('shareware', 'retail')][string]$DataEdition
    )

    Set-StrictMode -Version Latest
    $failures = @()
    $reports = @{}
    $rejectedMutations = 0
    foreach ($name in @('native/wall-blast.json', 'imported/wall-blast.json', 'imported/wall-blast-d2.json')) {
        $rows = @(Get-Content -LiteralPath (Join-Path $Directory $name) -Raw | ConvertFrom-Json)
        if (($rows.elapsed -join ',') -cne '0,8,24,25,31') { throw "Incomplete wall-blast cases: $name" }
        $reports[$name] = $rows
        $ordinaryD2 = $name -eq 'imported/wall-blast-d2.json'
        $expectedGame = if ($ordinaryD2) { 'd2' } else { 'd1' }
        $expectedMission = if ($ordinaryD2) {
            if ($DataEdition -eq 'shareware') { 'd2demo' } else { 'd2' }
        } elseif ($name -eq 'imported/wall-blast.json') { 'descent' } else { '' }
        foreach ($row in $rows) {
            $selection = $row.selection
            if ($selection.edition -cne $DataEdition -or $selection.content_game -cne $expectedGame -or
                $selection.mission -cne $expectedMission -or $selection.level_num -ne 1 -or
                $selection.wall_type -ne 1 -or ($selection.clip_flags -band 1) -ne 1 -or
                $selection.segment -lt 0 -or $selection.side -lt 0 -or $selection.side -ge 6) {
                throw "Wrong wall-blast profile or non-exploding wall: $name"
            }
            if ($DataEdition -eq 'shareware') {
                $expectedLevel = if ($ordinaryD2) { 'd2leva-1.sl2' } else { 'level01.sdl' }
                $expectedWall = if ($ordinaryD2) { 37 } else { 10 }
                if ($selection.level -cne $expectedLevel -or $selection.wall -ne $expectedWall -or $selection.clip -ne 6) {
                    throw "Wrong pinned public wall selection: $name"
                }
            }
            $frames = @($row.elapsed..32)
            if (($row.uninterrupted.frame -join ',') -cne ($frames -join ',') -or
                ($row.restored.frame -join ',') -cne ($frames -join ',')) {
                throw "Incomplete wall-blast continuation: $name elapsed=$($row.elapsed)"
            }
            if ($row.differences.Count -or $row.endian_differences.Count -or
                ($row.uninterrupted | ConvertTo-Json -Depth 10 -Compress) -cne ($row.restored | ConvertTo-Json -Depth 10 -Compress)) {
                $failures += "$name elapsed=$($row.elapsed) save/endian divergence"
            }
            if ($name -eq 'imported/wall-blast.json') {
                $sources = @("wall-blast-$($row.elapsed).sav", "opposite-wall-blast-$($row.elapsed).sav")
                if ((@($row.import_differences.PSObject.Properties.Name | Sort-Object) -join ',') -cne
                    (@($sources | Sort-Object) -join ',') -or
                    (@($row.rejected_mutations.PSObject.Properties.Name | Sort-Object) -join ',') -cne
                    (@($sources | Sort-Object) -join ',')) {
                    throw 'Missing both native-checkpoint byte orders or corruption evidence'
                }
                foreach ($source in $sources) {
                    if ($row.import_differences.$source.Count) { $failures += "$name elapsed=$($row.elapsed) $source import divergence" }
                    # Native v21: 120 pool bytes + eight phase/Fusion tail bytes + six invalid field/late-slot cases
                    if ($row.rejected_mutations.$source -ne 134) { throw "Incomplete atomic corruption checks: $name $source" }
                    $rejectedMutations += $row.rejected_mutations.$source
                }
                if ($row.legacy_cleared -ne $true) { throw "Missing legacy wall-state clearing: $name" }
            }
        }
    }
    $native = $reports['native/wall-blast.json']
    $imported = $reports['imported/wall-blast.json']
    for ($i = 0; $i -lt $native.Count; ++$i) {
        if (($native[$i].uninterrupted | ConvertTo-Json -Depth 10 -Compress) -cne
            ($imported[$i].uninterrupted | ConvertTo-Json -Depth 10 -Compress)) {
            $failures += "Native/imported uninterrupted blast differs at elapsed=$($native[$i].elapsed)"
        }
    }
    $report = [ordered]@{
        dataEdition = $DataEdition; phases = $native.elapsed; failures = $failures
        nativeImportByteOrders = 2; ordinaryD2Control = $true; rejectedMutations = $rejectedMutations; legacyClearedPhases = 5
    }
    $report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $Directory 'report.json')
    if ($failures.Count) { throw "Wall-blast persistence failed; see $Directory/report.json" }
    return $report
}

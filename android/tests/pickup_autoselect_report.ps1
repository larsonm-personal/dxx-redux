# Admission checks for the existing controlled pickup matrix, not traversal evidence
function Assert-PickupAutoselectRows {
    param([object[]]$Rows)
    if ($Rows.Count -ne 2112) { throw "Expected all 2112 pickup cases, got $($Rows.Count)" }
    $index = 0
    foreach ($order in 0..3) {
        foreach ($scenario in 0..10) {
            foreach ($firing in 0..3) {
                foreach ($once in 0..1) {
                    foreach ($consumed in 0..1) {
                        foreach ($pending in @(-1, 2, 3)) {
                            $row = $Rows[$index++]
                            if (($row.case -join ',') -cne (@($order, $scenario, $firing, $once, $consumed, $pending) -join ',')) {
                                throw "Missing, duplicated or reordered pickup case at $($index - 1)"
                            }
                            if ($row.used -notin @(0, 1)) { throw 'Pickup consumption result must be boolean' }
                            foreach ($phase in @('acquired', 'released', 'followup')) {
                                foreach ($field in @('selected', 'pending', 'picked_up', 'inventory', 'flags', 'laser_level', 'energy', 'ammo', 'rearm', 'firing_count', 'fusion_charge')) {
                                    $value = $row.$phase.$field
                                    if ($null -eq $value -or $value -is [string] -or $value -is [bool] -or $value -ne [long]$value) {
                                        throw "Invalid pickup $phase.$field evidence"
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

function Assert-PickupAssetProfile {
    param([object]$Profile, [string]$DataEdition, [string]$Role)
    $d2 = $Role -eq 'ordinary-d2'
    $mission = if ($d2) { if ($DataEdition -eq 'shareware') { 'd2demo' } else { 'd2' } } elseif ($Role -eq 'imported') { 'descent' } else { '' }
    if ($Profile.edition -cne $DataEdition -or $Profile.content_game -cne $(if ($d2) { 'd2' } else { 'd1' }) -or
        $Profile.mission -cne $mission -or $Profile.geometry -cne 'controlled-corridor' -or $Profile.powerups.Count -ne 5) {
        throw "Wrong loaded pickup asset profile for $Role"
    }
    if (($Profile.powerups.id -join ',') -cne '3,12,14,16,22') { throw 'Wrong pickup asset ID sequence' }
    if ($DataEdition -eq 'shareware') {
        $sounds = if ($Role -eq 'native') { 77 } else { 70 }
        $level = if ($d2) { 'd2leva-1.sl2' } else { 'level01.sdl' }
        if ($Profile.first_level -cne $level -or (-not $d2 -and ($Profile.models -ne 56 -or $Profile.sounds -ne $sounds))) {
            throw "Wrong public pickup asset bank for $Role"
        }
    }
}

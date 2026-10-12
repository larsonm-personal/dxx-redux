# Independent expected case keys for the shared C++ fixture matrices
function Get-PublicCombatCaseKeys {
    param([bool]$Native)
    $robots = if ($Native) { @(0, 1, 2, 3, 4, 14, 16, 17, 18, 19, 20) } else { @(25, 30, 31, 33, 37, 38, 39, 40, 42, 50) }
    $eggs = if ($Native) { @(0, 14, 20) } else { @(25, 37, 50) }
    $weapons = if ($Native) { @(13) } else { @(13, 28) }
    return @{
        pickups = @(foreach ($difficulty in 0..4) { foreach ($id in @(1, 2)) { foreach ($start in @(6553600, 13074432, 13107200)) { "$difficulty,$id,$start" } } })
        drops = @(foreach ($id in @(23, 13, 14, 15, 16, 12, 22, 1, 0)) { foreach ($flags in 0..7) { foreach ($nearby in -1..3) { foreach ($seed in @(1, 7, 1000)) { foreach ($gated in 0..1) { "$id,$flags,$nearby,$seed,$gated" } } } } })
        motion = @(foreach ($type in @(4, 2, 5, 8)) { foreach ($bounce in 0..1) { foreach ($direction in @(-1, 1)) { foreach ($speed in @(32, 128)) { "$type,$bounce,$direction,$speed" } } } })
        contacts = @(foreach ($exploding in 0..1) { foreach ($invulnerable in 0..1) { foreach ($speed in @(4, 32)) { "$exploding,$invulnerable,$speed" } } })
        robot_pairs = @(foreach ($first in 0..1) { foreach ($second in 0..1) { foreach ($direction in @(-1, 1)) { foreach ($offset in @(0, 3)) { "$first,$second,$direction,$offset" } } } })
        resource_drops = @(foreach ($source in @(4, 2)) { foreach ($id in @(1, 2)) { foreach ($inventory in @(50, 100, 150)) { foreach ($seed in @(1, 7, 1000)) { "$source,$id,$inventory,$seed" } } } })
        weapon_drops = @(foreach ($source in @(4, 2)) { foreach ($id in $weapons) { foreach ($count in @(1, 2)) { "$source,$id,$count" } } })
        robot_drops = @(foreach ($id in $eggs) { foreach ($count in @(0, 1, 3)) { foreach ($seed in @(1, 123, 456)) { "$id,$count,$seed" } } })
        robot_blasts = @(foreach ($id in $robots) { foreach ($distance in @(2, 8)) { foreach ($flash in 0..1) { "$id,$distance,$flash" } } })
        secondary_explosions = @($robots | Where-Object { $Native -or $_ -notin @(33, 42) } | ForEach-Object { "$_" })
        volatile_impacts = @(foreach ($id in @(0, 8, 15)) { foreach ($difficulty in 0..4) { foreach ($exploding in 0..1) { "$id,$difficulty,$exploding" } } })
    }
}

function Assert-PublicCombatCaseKeys {
    param([object]$Trace, [bool]$Native)
    $expected = Get-PublicCombatCaseKeys -Native $Native
    foreach ($name in $expected.Keys) {
        $rows = if ($name -in @('motion', 'contacts')) { @($Trace.contact_motion.$name) } else { @($Trace.$name) }
        if ($rows.Count -ne $expected[$name].Count) { throw "Incomplete public combat case keys: $name" }
        for ($index = 0; $index -lt $rows.Count; ++$index) {
            $row = $rows[$index]
            $key = if ($name -eq 'motion') { @($row.type, $row.bounce, $row.direction, $row.speed) -join ',' }
            elseif ($name -eq 'resource_drops') { @($row.source, $row.id, $row.inventory, $row.seed) -join ',' }
            else { $length = $expected[$name][$index].Split(',').Count; $row[0..($length - 1)] -join ',' }
            if ($key -cne $expected[$name][$index]) { throw "Duplicated, substituted or reordered public combat case: $name/$index" }
        }
    }
}

# Public combat evidence admission; engine assertions remain the behavioral oracle
function Assert-PublicCombatRows {
    param([object]$Trace, [bool]$Native)
    $counts = @{ pickups = 30; damage = 30; drops = 2160; robot_pairs = 16; resource_drops = 36; robot_drops = 27; volatile_impacts = 30 }
    $counts.weapon_drops = if ($Native) { 4 } else { 8 }
    $counts.robot_blasts = if ($Native) { 44 } else { 40 }
    $counts.secondary_explosions = if ($Native) { 11 } else { 8 }
    foreach ($name in $counts.Keys) {
        if (@($Trace.$name).Count -ne $counts[$name]) { throw "Incomplete public combat evidence: $name" }
    }
    if (@($Trace.contact_motion.motion).Count -ne 32 -or @($Trace.contact_motion.contacts).Count -ne 8) {
        throw 'Incomplete public contact/motion evidence'
    }
    Assert-PublicCombatCaseKeys -Trace $Trace -Native $Native
    $row = 0
    foreach ($difficulty in 0..4) {
        foreach ($invulnerable in 0..1) {
            foreach ($kind in 0..2) {
                $entry = $Trace.damage[$row++]
                if ($entry.Count -ne 7 -or ($entry[0..2] -join ',') -cne (@($difficulty, $invulnerable, $kind) -join ',') -or
                    ($invulnerable -eq 1 -and $entry[3] -ne 0) -or ($invulnerable -eq 0 -and $entry[3] -le 0)) {
                    throw 'Invalid difficulty/invulnerability damage evidence'
                }
            }
        }
    }
}

function Assert-PublicCombatProfile {
    param([object]$Profile, [string]$Role)
    $native = $Role -ne 'ordinary-d2'
    $robots = if ($native) { '0,1,2,3,4,14,16,17,18,19,20' } else { '25,30,31,33,37,38,39,40,42,50' }
    $mission = if (-not $native) { 'd2demo' } elseif ($Role -eq 'imported') { 'descent' } else { '' }
    $level = if ($native) { 'level01.sdl' } else { 'd2leva-1.sl2' }
    if ($Profile.edition -cne 'shareware' -or $Profile.geometry -cne 'controlled-corridor' -or
        $Profile.content_game -cne $(if ($native) { 'd1' } else { 'd2' }) -or
        $Profile.mission -cne $mission -or $Profile.first_level -cne $level -or
        ($Profile.robots -join ',') -cne $robots -or ($Profile.impact_projectiles -join ',') -cne '0,8,15') {
        throw "Wrong public combat asset profile for $Role"
    }
    if (($Profile.pair_actors -join ',') -cne $(if ($native) { '0,1' } else { '25,40' }) -or
        ($Profile.egg_actors -join ',') -cne $(if ($native) { '0,14,20' } else { '25,37,50' })) {
        throw "Wrong public combat actor mapping for $Role"
    }
}

function Assert-PublicCombatReport {
    param([string]$Directory)
    $nativeText = Get-Content "$Directory/native/rules.json" -Raw
    $importedText = Get-Content "$Directory/imported/rules.json" -Raw
    $native = $nativeText | ConvertFrom-Json
    $imported = $importedText | ConvertFrom-Json
    $control = Get-Content "$Directory/imported/rules-d2.json" -Raw | ConvertFrom-Json
    Assert-PublicCombatRows -Trace $native -Native $true
    Assert-PublicCombatRows -Trace $imported -Native $true
    Assert-PublicCombatRows -Trace $control -Native $false
    foreach ($role in @('native', 'imported', 'ordinary-d2')) {
        $file = if ($role -eq 'ordinary-d2') { 'imported/combat-assets-d2.json' } else { "$role/combat-assets.json" }
        Assert-PublicCombatProfile -Profile (Get-Content (Join-Path $Directory $file) -Raw | ConvertFrom-Json) -Role $role
    }
    if ($nativeText -cne $importedText) { throw "Native/imported public combat differs; see $Directory" }
    for ($index = 0; $index -lt 30; ++$index) {
        $entry = $native.damage[$index]
        $divisor = if ($entry[0] -eq 0) { if ($entry[2] -eq 2) { 4 } else { 2 } } else { 1 }
        if ($control.damage[$index][3] -ne [math]::Truncate($entry[3] / $divisor)) { throw 'Ordinary D2 trainee damage control differs' }
    }
    $report = [ordered]@{
        edition = 'shareware'; fixture = 'controlled loaded-asset corridor'; nativeImportedEqual = $true
        damage = 30; motion = 32; contacts = 8; robotPairs = 16; resourceDrops = 36; weaponDrops = 4; robotEggs = 27
        robotBlasts = 44; secondaryExplosions = 11; volatileImpacts = 30; difficultyPickups = 30; replacementPolicy = 2160
        ordinaryD2 = @{ damage = 30; robotBlasts = 40; secondaryExplosions = 8; weaponDrops = 8 }
        retailOnly = @('resistant-boss-quarter-damage', 'mega-projectile', 'omega-physical-drop')
    }
    $report | ConvertTo-Json -Depth 5 | Set-Content "$Directory/report.json"
    return $report
}

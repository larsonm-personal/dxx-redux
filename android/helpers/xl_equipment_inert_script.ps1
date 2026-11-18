# Compose a separate owner from the shared equipment startup and disk-transition controls
function New-InertEquipmentScript {
    param([Parameter(Mandatory)][int]$ItemId)
    $steps = @(Get-Content "$PSScriptRoot/../game_scripts/test_xl_equipment_runtime.jsonc" -Raw | ConvertFrom-Json)
    $end = 0
    while ($end -lt $steps.Count -and !$steps[$end].expect.'xl_pilot.equipment_generators.0.items') { $end++ }
    if ($end -eq $steps.Count) { throw 'Missing shared equipment production witness' }
    $steps[5].text = "XL: Derived XL item $ItemId"
    $result = [Collections.Generic.List[object]]::new()
    $result.AddRange([object[]]$steps[0..($end - 1)])
    $spawned = [ordered]@{
        'xl_pilot.active' = $true
        'xl_pilot.equipment_generators.0.pending' = $false
        'xl_pilot.equipment_generators.0.item_count' = 1
        'xl_pilot.equipment_generators.0.items' = @{ contains_object = @{ id = $ItemId; lifeleft = 1073741823 } }
    }
    $result.Add(@{ action = 'assert'; expect = ($spawned | ConvertTo-Json -Depth 10 | ConvertFrom-Json) })
    if ($ItemId -ge 46) {
        # Native fixture center is (-2446340, -655360, 2785809); look along +Z from five units behind
        $result.Add(@{ action = 'pose_view'; segment = 20; position_fixed = @(-2446340, -655360, 2458129); pitch = 0; bank = 0; heading = 0 })
    } else {
        $result.Add(@{ action = 'face_view'; segment = 20; side = 0; distance = 5 })
    }
    $result.Add(@{ action = 'wait_ms'; ms = 1200 })
    $result.Add(@{ action = 'set_debug'; field = 'xl_equipment_pickup_probe'; value = "$ItemId" })
    # Freeze the real producer through its authored toggle, then save the spawned object
    $result.Add(@{ action = 'wait_for'; expect = @{ 'xl_pilot.wall_triggers.0.cooldown_age_ms' = 750 }; timeout_ms = 5000 })
    $result.Add(@{ action = 'set_debug'; field = 'fire_trigger'; value = '0' })
    $spawned['xl_pilot.equipment_generators.0.enabled'] = $false
    $result.Add(@{ action = 'assert'; expect = $spawned })
    $begin = 0
    while ($begin -lt $steps.Count -and $steps[$begin].value -ne 'quick_save') { $begin++ }
    $finish = $begin
    while ($finish -lt $steps.Count -and $steps[$finish].expect.PSObject.Properties.Name -notcontains 'time_paused') { $finish++ }
    if ($begin -eq $steps.Count -or $finish -eq $steps.Count) { throw 'Missing shared equipment cold transition' }
    $result.AddRange([object[]]$steps[$begin..($finish - 1)])
    $result.Add(@{ action = 'wait_for'; expect = $spawned; timeout_ms = 20000 })
    $result.Add(@{ action = 'set_debug'; field = 'xl_equipment_pickup_probe'; value = "$ItemId" })
    $result.Add(@{ action = 'wait_for'; expect = @{ 'xl_pilot.wall_triggers.0.cooldown_age_ms' = 750 }; timeout_ms = 5000 })
    $result.Add(@{ action = 'set_debug'; field = 'fire_trigger'; value = '0' })
    $result.Add(@{ action = 'wait_ms'; ms = 10000 })
    $occupied = @{
        'xl_pilot.equipment_generators.0.enabled' = $true
        'xl_pilot.equipment_generators.0.pending' = $false
        'xl_pilot.equipment_generators.0.item_count' = 1
    }
    $result.Add(@{ action = 'assert'; expect = $occupied })
    $result.Add(@{ action = 'set_debug'; field = 'xl_equipment_pickup_probe'; value = "$ItemId" })
    return , $result.ToArray()
}

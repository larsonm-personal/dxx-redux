# Continue a running game through correlated automation and retain the actual engine state
function Invoke-UudGameplayProbe {
    param(
        [Parameter(Mandatory)][object[]]$Steps,
        [Parameter(Mandatory)][string]$OutputDirectory,
        [Parameter(Mandatory)][string]$Name
    )
    $runId = [guid]::NewGuid().ToString('N')
    $filename = "uud-probe-$runId.jsonc"
    $path = Join-Path $OutputDirectory $filename
    $Steps | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $path -Encoding utf8
    Adb -AdbArgs @('push', $path, "/data/local/tmp/$filename") | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', "/data/local/tmp/$filename", "files/$filename") | Out-Null
    Adb -AdbArgs @('shell', 'rm', '-f', "/data/local/tmp/$filename") | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/automation_result.json') | Out-Null
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', $filename, '--es', 'run_id', $runId) | Out-Null
    $passed = Watch-AutomationResult -TimeoutSeconds 300 -ExpectedRunId $runId
    $json = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/introspect.json')
    $json | Set-Content -LiteralPath (Join-Path $OutputDirectory "$Name.json") -Encoding utf8
    Adb -AdbArgs @('logcat', '-d') -Seconds 30 | Set-Content -LiteralPath (Join-Path $OutputDirectory "$Name.log") -Encoding utf8
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/automation_result.json') | Set-Content -LiteralPath (Join-Path $OutputDirectory "$Name-result.json") -Encoding utf8
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', "files/$filename") | Out-Null
    if (!$passed) { throw "UUD gameplay probe failed: $Name; $OutputDirectory" }
    return ($json | ConvertFrom-Json)
}

# Exercise the added HAM semantics with ordinary game input after rendering the fixture
function Test-UudExtraGameplay {
    param(
        [Parameter(Mandatory)][object]$Fixture,
        [Parameter(Mandatory)][string]$OutputDirectory,
        [Parameter(Mandatory)][int]$Level
    )
    foreach ($texture in @(923, 924)) {
        $surface = $Fixture.surfaces | Where-Object texture -EQ $texture
        $prefix = "level_scene.segment.sides[$($surface.side)].base_asset"
        $state = Invoke-UudGameplayProbe -OutputDirectory $OutputDirectory -Name "level-$Level-effect-$texture-start" -Steps @(
            @{ action = 'face_view'; segment = $surface.segment; side = $surface.side; distance = 7; post_delay_ms = 300 }
            @{ action = 'introspect' }
        )
        $asset = $state.level_scene.segment.sides[$surface.side].base_asset
        $range = if ($texture -eq 923) { @(2605, 2608) } else { @(2609, 2616) }
        $damage = if ($texture -eq 923) { 10 } else { 0 }
        $effect = if ($texture -eq 923) { 105 } else { 106 }
        Invoke-UudGameplayProbe -OutputDirectory $OutputDirectory -Name "level-$Level-effect-$texture-moving" -Steps @(
            @{ action = 'wait_for'; timeout_ms = 4000; expect = @{ "$prefix.bitmap" = @{ ne = $asset.bitmap } } }
            @{ action = 'assert'; expect = @{
                    "$prefix.bitmap" = @{ range = $range }
                    "$prefix.damage" = @{ range = @($damage, $damage) }
                    "$prefix.effect" = $effect
                    "$prefix.rendered" = $true
                    "$prefix.indexed_mod_owned" = $true
                    "$prefix.render_source" = @{ contains = 'uud2tp-textures.dxa' }
                } 
            }
            @{ action = 'introspect' }
        ) | Out-Null
    }
    $door = $Fixture.door
    $prefix = "level_scene.segment.sides[$($door.side)]"
    $state = Invoke-UudGameplayProbe -OutputDirectory $OutputDirectory -Name "level-$Level-door-start" -Steps @(
        @{ action = 'face_view'; segment = $door.segment; side = $door.side; distance = 7; post_delay_ms = 300 }
        @{ action = 'assert'; expect = @{
                "$prefix.wall_clip" = 51
                "$prefix.wall_frame_count" = 7
                "$prefix.wall_play_time" = @{ range = @(1, 1) }
                "$prefix.base" = 912
            } 
        }
        @{ action = 'introspect' }
    )
    $child = $state.level_scene.segment.sides[$door.side].child
    if ($child -lt 0) { throw 'Green door does not connect rooms' }
    Invoke-UudGameplayProbe -OutputDirectory $OutputDirectory -Name "level-$Level-door-crossed" -Steps @(
        @{ action = 'send_button'; button = 100; held = 1 }
        @{ action = 'wait_for'; timeout_ms = 5000; expect = @{ "$prefix.wall_state" = @{ gt = 0 } } }
        @{ action = 'send_button'; button = 100; pressed = 0 }
        @{ action = 'wait_for'; timeout_ms = 5000; expect = @{ "$prefix.base" = 918 } }
        @{ action = 'assert'; expect = @{
                "$prefix.base_asset.bitmap" = 2600
                "$prefix.base_asset.rendered" = $true
                "$prefix.base_asset.render_source" = @{ contains = 'uud2tp-textures.dxa' }
            } 
        }
        @{ action = 'send_button'; button = 102; held = 1 }
        @{ action = 'wait_for'; timeout_ms = 5000; expect = @{ 'position.segment' = $child } }
        @{ action = 'send_button'; button = 102; pressed = 0 }
        @{ action = 'introspect' }
    ) | Out-Null
    $lava = $Fixture.surfaces | Where-Object texture -EQ 923
    $state = Invoke-UudGameplayProbe -OutputDirectory $OutputDirectory -Name "level-$Level-lava-start" -Steps @(
        @{ action = 'face_view'; segment = $lava.segment; side = $lava.side; distance = 7; post_delay_ms = 300 }
        @{ action = 'introspect' }
    )
    Invoke-UudGameplayProbe -OutputDirectory $OutputDirectory -Name "level-$Level-lava-damage" -Steps @(
        @{ action = 'send_button'; button = 102; held = 1 }
        @{ action = 'wait_for'; timeout_ms = 5000; expect = @{ 'player.shields' = @{ lt = $state.player.shields } } }
        @{ action = 'send_button'; button = 102; pressed = 0 }
        @{ action = 'assert'; expect = @{ 'position.segment' = $lava.segment; 'player.shields' = @{ gt = 0 } } }
        @{ action = 'introspect' }
    ) | Out-Null
}

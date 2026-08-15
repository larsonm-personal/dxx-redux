#!/usr/bin/env pwsh

param(
    [string]$MissionZip = "",
    [ValidateSet("d1", "d2")]
    [string]$Game = "d2",
    [switch]$BaseGame,
    [int]$RobotNumber = 0,
    [string]$ExpectedAttackRole = "",
    [int]$ExpectedWeapon = -1,
    [int]$Seed = 6042,
    [int]$TimeoutSeconds = 180,
    [string]$Serial = "emulator-5554"
)

$ErrorActionPreference = "Stop"
$testsDir = Split-Path -Parent $PSCommandPath
$androidRoot = Split-Path -Parent $testsDir
$repoRoot = Split-Path -Parent $androidRoot
. (Join-Path $androidRoot "helpers\test_helpers.ps1")

if (-not $BaseGame -and -not $MissionZip) {
    $MissionZip = Join-Path $repoRoot "game_data\mission_files\Obsidian.zip"
}
$MissionZip = if ($BaseGame) { "" } else { (Resolve-Path -LiteralPath $MissionZip).Path }
$deviceZip = "robot_preview_smoke_$Seed.zip"
$deviceScript = "robot_preview_smoke_$Seed.json5"
$localScript = Join-Path $androidRoot "temp\robot_preview_smoke\$deviceScript"
$selectionFile = "files/robot_preview_smoke_selection.json"
$introspectionFile = "files/robot_preview_introspect.json"
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial

function Read-AppJson {
    param([Parameter(Mandatory = $true)][string]$Path)
    $text = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", $Path) -Seconds 8
    if (-not $text -or $text -match "No such file") { return $null }
    try { return ($text | ConvertFrom-Json) } catch { return $null }
}

function Assert-Near {
    param(
        [Parameter(Mandatory = $true)][double]$Actual,
        [Parameter(Mandatory = $true)][double]$Expected,
        [Parameter(Mandatory = $true)][string]$Description,
        [double]$Tolerance = 0.001
    )
    if ([Math]::Abs($Actual - $Expected) -gt $Tolerance) {
        throw "$Description expected $Expected, got $Actual"
    }
}

try {
    if (-not (Test-DeviceOnline -Serial $Serial)) { throw "Android device $Serial is not online" }
    Reset-DeviceGameState -Serial $Serial
    if (-not $BaseGame) {
        $temporaryZip = "/data/local/tmp/$deviceZip"
        Adb -AdbArgs @("push", $MissionZip, $temporaryZip) | Out-Null
        Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "mkdir", "-p", "files/mods") | Out-Null
        Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", $temporaryZip, "files/mods/$deviceZip") | Out-Null
        Adb -AdbArgs @("shell", "rm", "-f", $temporaryZip) | Out-Null
    }

    $templateName = if ($BaseGame) { "test_base_robot_preview.json5" } else { "test_robot_preview.json5" }
    $template = Get-Content -LiteralPath (Join-Path $androidRoot "game_scripts\$templateName") -Raw
    $body = $template.Replace('${MISSION_ZIP}', $deviceZip)
    $body = $body.Replace('${GAME}', $Game)
    $body = $body.Replace('${ROBOT_NUMBER}', "$RobotNumber")
    $body = $body.Replace('${SEED}', "$Seed")
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $localScript) | Out-Null
    [IO.File]::WriteAllText($localScript, $body + "`n", [Text.UTF8Encoding]::new($false))
    $temporaryScript = "/data/local/tmp/$deviceScript"
    Adb -AdbArgs @("push", $localScript, $temporaryScript) | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", $temporaryScript, "files/$deviceScript") | Out-Null
    Adb -AdbArgs @("shell", "rm", "-f", $temporaryScript) | Out-Null
    Adb -AdbArgs @(
        "shell", "run-as", $script:PACKAGE, "rm", "-f", $selectionFile, $introspectionFile,
        "$introspectionFile.tmp", "files/automation_result.json"
    ) | Out-Null

    if (-not (Start-SetupActivity -Serial $Serial)) { throw "SetupActivity did not become ready" }
    Adb -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_AUTOMATE", "--es", "script", $deviceScript
    ) | Out-Null

    $selection = $null
    $initial = $null
    $ready = Wait-ForCondition -Description "robot preview introspection" -TimeoutSec $TimeoutSeconds -PollMs 1000 -Condition {
        $result = Read-AppJson -Path "files/automation_result.json"
        if ($result -and $result.result -eq "FAIL") { throw "Launcher automation failed: $($result.reason)" }
        $script:selection = Read-AppJson -Path $selectionFile
        Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_INTROSPECT", "-p", $script:PACKAGE) | Out-Null
        Start-Sleep -Milliseconds 200
        $script:initial = Read-AppJson -Path $introspectionFile
        return $script:selection -and $script:initial -and $script:initial.level_preview.active -and
        $script:initial.level_preview.schema -eq "dxx-robot-preview-introspection-v1" -and
        [long]$script:initial.level_preview.frame_count -gt 0 -and
        [int]$script:initial.framebuffer_probe.gl_error -eq 0 -and
        [long]$script:initial.framebuffer_probe.visible_pixels -gt 0
    }
    if (-not $ready) { throw "Robot preview did not produce a live introspection snapshot" }
    if ([int]$initial.level_preview.robot_number -ne [int]$selection.robot_number) {
        throw "Previewed robot does not match the selected replacement"
    }
    if ([int]$initial.level_preview.model_number -lt 0 -or -not $initial.level_preview.palette_ready) {
        throw "Robot preview did not resolve a model and palette"
    }
    if ($ExpectedAttackRole -and $initial.level_preview.attack_role -ne $ExpectedAttackRole) {
        throw "Expected attack role '$ExpectedAttackRole', got '$($initial.level_preview.attack_role)'"
    }
    if ($null -eq $initial.level_preview.animated_joint_count -or
        $null -eq $initial.level_preview.motion_updates) {
        throw "Robot preview did not report HAM/HXM joint animation"
    }
    if ([int]$initial.level_preview.animated_joint_count -gt 0 -and
        [long]$initial.level_preview.motion_updates -le 0 -and
        $initial.level_preview.attack_role -ne "non-firing") {
        throw "Robot preview has animated joints but did not move them"
    }
    if ([int]$initial.framebuffer_probe.gl_error -ne 0 -or [long]$initial.framebuffer_probe.visible_pixels -le 0) {
        throw "Robot preview did not produce a visible framebuffer"
    }
    if (-not $initial.level_preview.preserve_aspect -or
        [int]$initial.level_preview.source_width -le 0 -or
        [int]$initial.level_preview.source_height -le 0 -or
        [int]$initial.level_preview.draw_width -le 0 -or
        [int]$initial.level_preview.draw_height -le 0) {
        throw "Robot preview did not report aspect-preserving output geometry"
    }
    $aspectError = [Math]::Abs(
        [long]$initial.level_preview.draw_width * [long]$initial.level_preview.source_height -
        [long]$initial.level_preview.draw_height * [long]$initial.level_preview.source_width
    )
    if ($aspectError -gt [Math]::Max(
            [int]$initial.level_preview.source_width,
            [int]$initial.level_preview.source_height
        )) {
        throw "Robot preview output geometry changed the framebuffer aspect ratio"
    }

    $navigationNumbers = @($initial.level_preview.navigation_numbers)
    if ($navigationNumbers.Count -ne [int]$initial.level_preview.robot_count -or
        [int]$initial.level_preview.robot_number -notin $navigationNumbers) {
        throw "Robot preview did not report a valid navigation list"
    }
    if (-not $BaseGame) {
        $requestState =
        Read-AppJson -Path "cache/robot_preview/$([string]$selection.request_id)/request.json"
        $requestedNumbers = @($requestState.robot_numbers)
        if (-not $requestState -or $requestedNumbers.Count -le 0 -or
            (@($requestedNumbers | ForEach-Object { [int]$_ }) -join ",") -ne
            (@($navigationNumbers | ForEach-Object { [int]$_ }) -join ",")) {
            throw "Mod robot navigation did not match the modified-robot list"
        }
    }
    if ([double]$initial.level_preview.model_radius -le 0 -or
        [double]$initial.level_preview.camera_view_radius -lt [double]$initial.level_preview.model_radius -or
        [double]$initial.level_preview.normal_camera_view_radius -le 0 -or
        [double]$initial.level_preview.large_camera_view_radius -lt
        [double]$initial.level_preview.normal_camera_view_radius -or
        $initial.level_preview.camera_tier -notin @("normal", "large")) {
        throw "Robot preview did not report a valid fixed camera tier"
    }

    $robotCount = [int]$initial.level_preview.robot_count
    $currentNavigationIndex = 0
    for ($index = 0; $index -lt $navigationNumbers.Count; $index++) {
        if ([int]$navigationNumbers[$index] -eq [int]$initial.level_preview.robot_number) {
            $currentNavigationIndex = $index
            break
        }
    }
    $nextRobot = [int]$navigationNumbers[($currentNavigationIndex + 1) % $robotCount]
    Adb -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_COMMAND", "-p", $script:PACKAGE,
        "--es", "command", "next"
    ) | Out-Null
    Start-Sleep -Milliseconds 500
    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_INTROSPECT", "-p", $script:PACKAGE) | Out-Null
    Start-Sleep -Milliseconds 300
    $navigated = Read-AppJson -Path $introspectionFile
    if (-not $navigated -or [int]$navigated.level_preview.robot_number -ne $nextRobot) {
        throw "Robot preview did not advance to the next robot"
    }
    if ($navigated.level_preview.camera_tier -eq $initial.level_preview.camera_tier -and
        [Math]::Abs(
            [double]$navigated.level_preview.camera_view_radius -
            [double]$initial.level_preview.camera_view_radius
        ) -gt 0.001) {
        throw "Robots in the same camera tier used different zoom levels"
    }
    Adb -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_COMMAND", "-p", $script:PACKAGE,
        "--es", "command", "previous"
    ) | Out-Null
    Start-Sleep -Milliseconds 500

    Adb -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_COMMAND", "-p", $script:PACKAGE,
        "--es", "command", "attack", "--ez", "enabled", "true"
    ) | Out-Null
    $attackMode = Wait-ForCondition -Description "robot preview attack simulation" -TimeoutSec 12 -PollMs 500 -Condition {
        Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_INTROSPECT", "-p", $script:PACKAGE) | Out-Null
        Start-Sleep -Milliseconds 200
        $script:attackState = Read-AppJson -Path $introspectionFile
        if (-not $script:attackState -or -not $script:attackState.level_preview.attack_enabled) { return $false }
        $preview = $script:attackState.level_preview
        $hasBehavior = $preview.behavior -and $preview.behavior_source -and [long]$preview.attack_frames -gt 2
        $hasMovementStats = $null -ne $preview.max_speed -and $null -ne $preview.circle_distance -and
        $null -ne $preview.evade_speed -and $null -ne $preview.firing_wait
        $hasWeaponFlight = switch ($preview.attack_role) {
            "non-firing" { [long]$preview.shots_fired -eq 0 -and [long]$preview.active_projectiles -eq 0 }
            "melee" { [int]$preview.attack_type -ne 0 -and [long]$preview.shots_fired -eq 0 }
            "mine dropper" {
                $preview.weapon -and [long]$preview.mines_dropped -gt 0 -and
                [long]$preview.projectile_updates -gt 0 -and
                ([long]$preview.actual_weapon_renders + [long]$preview.fallback_weapon_renders) -gt 0
            }
            default {
                $preview.weapon -and [long]$preview.shots_fired -gt 0 -and
                [long]$preview.projectile_updates -gt 0 -and
                ([long]$preview.actual_weapon_renders + [long]$preview.fallback_weapon_renders) -gt 0
            }
        }
        return $hasBehavior -and $hasMovementStats -and $hasWeaponFlight
    }
    if (-not $attackMode) { throw "Robot preview did not model AI movement and weapon flight" }

    $preview = $script:attackState.level_preview
    if ($null -eq $preview.dps -or $null -eq $preview.dps.total -or
        $preview.dps.assumption -ne "all direct damaging attacks connect") {
        throw "Robot preview did not report sustained direct-hit DPS"
    }
    $channels = @($preview.dps.channels)
    $channelTotal = 0.0
    foreach ($channel in $channels) {
        if ([double]$channel.cycle_attacks -le 0 -or [double]$channel.cycle_seconds -le 0) {
            throw "DPS channel '$($channel.kind)' has an invalid attack cycle"
        }
        $expectedRate = [double]$channel.cycle_attacks / [double]$channel.cycle_seconds
        Assert-Near -Actual ([double]$channel.attacks_per_second) -Expected $expectedRate `
            -Description "DPS channel '$($channel.kind)' attack rate"
        $expectedChannelDps = [double]$channel.damage_per_hit * $expectedRate
        Assert-Near -Actual ([double]$channel.dps) -Expected $expectedChannelDps `
            -Description "DPS channel '$($channel.kind)' damage"
        $channelTotal += [double]$channel.dps
    }
    Assert-Near -Actual ([double]$preview.dps.total) -Expected $channelTotal `
        -Description "Total sustained DPS"

    switch ($preview.attack_role) {
        "non-firing" {
            if ($channels.Count -ne 0) { throw "Non-firing robot reported damaging DPS channels" }
            Assert-Near -Actual ([double]$preview.dps.total) -Expected 0 `
                -Description "Non-firing robot DPS"
        }
        "melee" {
            if ($channels.Count -ne 1 -or $channels[0].kind -ne "melee") {
                throw "Melee robot did not report one melee DPS channel"
            }
            Assert-Near -Actual ([double]$channels[0].damage_per_hit) `
                -Expected ([int]$preview.difficulty + 1) -Description "Melee damage per hit"
        }
        "mine dropper" {
            if ($channels.Count -ne 1 -or $channels[0].kind -ne "mine") {
                throw "Mine dropper did not report one mine DPS channel"
            }
            $expectedMineWait = if ($Game -eq "d1") { 5.0 } else { (10 - [int]$preview.difficulty) / 2.0 }
            Assert-Near -Actual ([double]$channels[0].cycle_seconds) -Expected $expectedMineWait `
                -Description "Mine drop interval"
        }
        default {
            $primary = @($channels | Where-Object { $_.kind -eq "primary" })
            if ($primary.Count -ne 1) { throw "Ranged robot did not report one primary DPS channel" }
            $burstAttacks = [Math]::Max(1, [int]$preview.rapidfire_count)
            if ($preview.dps.randomized_timing_average -and [int]$preview.rapidfire_count -gt 0) {
                $burstAttacks *= 2
            }
            $shortWait = [Math]::Min(0.125, [double]$preview.firing_wait / 2.0)
            $expectedCycle = [double]$preview.firing_wait + ($burstAttacks - 1) * $shortWait
            Assert-Near -Actual ([double]$primary[0].cycle_attacks) -Expected $burstAttacks `
                -Description "Primary rapid-fire cycle attacks"
            Assert-Near -Actual ([double]$primary[0].cycle_seconds) -Expected $expectedCycle `
                -Description "Primary rapid-fire cycle duration"
            $secondary = @($channels | Where-Object { $_.kind -eq "secondary" })
            if ($secondary.Count -gt 0) {
                Assert-Near -Actual ([double]$secondary[0].cycle_seconds) `
                    -Expected ([double]$preview.firing_wait2) -Description "Secondary firing interval"
            }
        }
    }

    if ($preview.attack_role -eq "ranged") {
        $retiredOffscreen = Wait-ForCondition -Description "off-screen projectile retirement" -TimeoutSec 8 `
            -PollMs 300 -Condition {
            Adb -AdbArgs @(
                "shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_INTROSPECT", "-p", $script:PACKAGE
            ) | Out-Null
            Start-Sleep -Milliseconds 150
            $script:attackState = Read-AppJson -Path $introspectionFile
            return $script:attackState -and
            [long]$script:attackState.level_preview.projectiles_retired_offscreen -gt 0
        }
        if (-not $retiredOffscreen) { throw "Robot projectiles did not retire after leaving the preview" }
        $preview = $script:attackState.level_preview
    } elseif ($preview.attack_role -eq "mine dropper") {
        $mineCap = Wait-ForCondition -Description "two newest preview mines" -TimeoutSec 14 -PollMs 400 -Condition {
            Adb -AdbArgs @(
                "shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_INTROSPECT", "-p", $script:PACKAGE
            ) | Out-Null
            Start-Sleep -Milliseconds 150
            $script:attackState = Read-AppJson -Path $introspectionFile
            return $script:attackState -and [long]$script:attackState.level_preview.mines_dropped -ge 3
        }
        if (-not $mineCap -or [int]$script:attackState.level_preview.active_mines -ne 2 -or
            [int]$script:attackState.level_preview.max_active_mines -ne 2) {
            throw "Mine-dropping robot did not retain exactly its two newest mines"
        }
        $preview = $script:attackState.level_preview
    }
    if ($ExpectedWeapon -ge 0 -and [int]$script:attackState.level_preview.weapon.number -ne $ExpectedWeapon) {
        throw "Expected preview weapon $ExpectedWeapon, got $($script:attackState.level_preview.weapon.number)"
    }
    if ($script:attackState.level_preview.attack_role -eq "ranged" -and
        [int]$script:attackState.level_preview.weapon.speed_variance -eq 128 -and
        [double]$script:attackState.level_preview.weapon.thrust -eq 0 -and
        [Math]::Abs(
            [double]$script:attackState.level_preview.last_projectile_initial_speed -
            [double]$script:attackState.level_preview.weapon.speed
        ) -gt 0.01) {
        throw "Preview projectile speed does not match the loaded constant-speed weapon"
    }

    $firstShotCount = [long]$script:attackState.level_preview.shots_fired
    $firstDirection = @($script:attackState.level_preview.last_projectile_direction) -join ","

    Adb -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_COMMAND", "-p", $script:PACKAGE,
        "--es", "command", "attack", "--ez", "enabled", "false"
    ) | Out-Null
    Start-Sleep -Milliseconds 300

    if ($script:attackState.level_preview.attack_role -in @("ranged", "mine dropper")) {
        Adb -AdbArgs @(
            "shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_COMMAND", "-p", $script:PACKAGE,
            "--es", "command", "rotate", "--ei", "heading", "8192", "--ei", "pitch", "4096"
        ) | Out-Null
        Adb -AdbArgs @(
            "shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_COMMAND", "-p", $script:PACKAGE,
            "--es", "command", "attack", "--ez", "enabled", "true"
        ) | Out-Null
        $orientedShot = Wait-ForCondition -Description "orientation-dependent robot shot" -TimeoutSec 8 -PollMs 400 -Condition {
            Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_INTROSPECT", "-p", $script:PACKAGE) | Out-Null
            Start-Sleep -Milliseconds 150
            $script:orientedState = Read-AppJson -Path $introspectionFile
            $direction = @($script:orientedState.level_preview.last_projectile_direction) -join ","
            return $script:orientedState -and [long]$script:orientedState.level_preview.shots_fired -gt $firstShotCount -and
            $direction -ne $firstDirection
        }
        if (-not $orientedShot) { throw "Projectile direction did not follow robot rotation" }
        Adb -AdbArgs @(
            "shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_COMMAND", "-p", $script:PACKAGE,
            "--es", "command", "attack", "--ez", "enabled", "false"
        ) | Out-Null
        Start-Sleep -Milliseconds 300
    }

    Adb -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_COMMAND", "-p", $script:PACKAGE,
        "--es", "command", "sounds", "--ez", "enabled", "true"
    ) | Out-Null
    $playedSound = Wait-ForCondition -Description "robot preview sound" -TimeoutSec 8 -PollMs 500 -Condition {
        Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_INTROSPECT", "-p", $script:PACKAGE) | Out-Null
        Start-Sleep -Milliseconds 200
        $script:soundState = Read-AppJson -Path $introspectionFile
        return $script:soundState -and $script:soundState.level_preview.sounds_enabled -and
        [long]$script:soundState.level_preview.sounds_played -gt 0 -and
        [int]$script:soundState.level_preview.last_sound -ge 0 -and
        [int]$script:soundState.level_preview.last_sound_sample -eq [int]$script:soundState.audio.sfx_last_soundnum -and
        $script:soundState.level_preview.last_sound_kind -in @("see", "attack", "claw") -and
        [long]$script:soundState.audio.sfx_probe_count -gt 0 -and
        [int]$script:soundState.audio.sfx_last_soundnum -ge 0
    }
    if (-not $playedSound) { throw "Robot preview did not schedule a robot sound" }

    $oldPitch = [int]$initial.level_preview.pitch
    Adb -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_COMMAND", "-p", $script:PACKAGE,
        "--es", "command", "rotate", "--ei", "heading", "1200", "--ei", "pitch", "800"
    ) | Out-Null
    Start-Sleep -Milliseconds 500
    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_INTROSPECT", "-p", $script:PACKAGE) | Out-Null
    Start-Sleep -Milliseconds 300
    $rotated = Read-AppJson -Path $introspectionFile
    if (-not $rotated -or [int]$rotated.level_preview.pitch -eq $oldPitch) {
        throw "Robot preview did not respond to rotation input"
    }

    Adb -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.ROBOT_PREVIEW_COMMAND", "-p", $script:PACKAGE,
        "--es", "command", "close"
    ) | Out-Null
    $closed = Wait-ForCondition -Description "robot preview closes" -TimeoutSec 30 -PollMs 500 -Condition {
        $activities = Adb-Timeout -AdbArgs @("shell", "dumpsys", "activity", "activities") -Seconds 8
        $requestState = Adb-Timeout -AdbArgs @(
            "shell", "run-as", $script:PACKAGE, "find", "cache/robot_preview", "-maxdepth", "1",
            "-name", ([string]$selection.request_id), "-print"
        ) -Seconds 8
        $setupResumed = $activities -match "(?m)^\s*(?:topResumedActivity|mResumedActivity|ResumedActivity)[=:].*SetupActivity"
        return $setupResumed -and $requestState -notmatch [regex]::Escape([string]$selection.request_id)
    }
    if (-not $closed) { throw "Robot preview did not close cleanly" }
    Write-Status "PASS: robot $($selection.robot_number) preserved aspect, navigated, played sound, animated, and rotated" "Green"
} finally {
    try {
        if (Test-DeviceOnline -Serial $Serial) {
            Adb -AdbArgs @("shell", "am", "force-stop", $script:PACKAGE) | Out-Null
            $cleanupFiles = @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/$deviceScript")
            if (-not $BaseGame) { $cleanupFiles += "files/mods/$deviceZip" }
            Adb -AdbArgs $cleanupFiles | Out-Null
        }
    } catch {
        Write-Warning "Robot preview cleanup failed: $($_.Exception.Message)"
    }
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}

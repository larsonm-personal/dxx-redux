#!/usr/bin/env pwsh

param(
    [string]$MissionZip = "",
    [ValidateSet("d1", "d2")]
    [string]$Game = "d2",
    [switch]$BaseGame,
    [int]$RobotNumber = 0,
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
    if ($null -eq $initial.level_preview.animated_joint_count -or
        $null -eq $initial.level_preview.motion_updates) {
        throw "Robot preview did not report HAM/HXM joint animation"
    }
    if ([int]$initial.level_preview.animated_joint_count -gt 0 -and
        [long]$initial.level_preview.motion_updates -le 0) {
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

    $robotCount = [int]$initial.level_preview.robot_count
    if ($robotCount -le 1) { throw "Robot preview did not report an adjacent robot" }
    $nextRobot = ([int]$initial.level_preview.robot_number + 1) % $robotCount
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
        $hasWeaponFlight = [int]$preview.attack_type -ne 0 -or
        ($preview.weapon -and [long]$preview.shots_fired -gt 0 -and [long]$preview.projectile_updates -gt 0)
        return $hasBehavior -and $hasMovementStats -and $hasWeaponFlight
    }
    if (-not $attackMode) { throw "Robot preview did not model AI movement and weapon flight" }

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

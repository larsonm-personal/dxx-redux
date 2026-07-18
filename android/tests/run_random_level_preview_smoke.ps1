#!/usr/bin/env pwsh

param(
    [string]$MissionDir = "",
    [ValidateSet("", "d1", "d2")]
    [string]$Game = "",
    [int]$Seed = 0,
    [long]$MaxZipBytes = 268435456,
    [int]$TimeoutSeconds = 180,
    [string]$Serial = "emulator-5554"
)

$ErrorActionPreference = "Stop"
$testsDir = Split-Path -Parent $PSCommandPath
$androidRoot = Split-Path -Parent $testsDir
$repoRoot = Split-Path -Parent $androidRoot
. (Join-Path $androidRoot "helpers\test_helpers.ps1")

if (-not $MissionDir) {
    $MissionDir = Join-Path $repoRoot "game_data\mission_files"
}
if ($Seed -eq 0) {
    $Seed = Get-Random -Minimum 1 -Maximum ([int]::MaxValue)
}
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial
$deviceZip = "level_preview_smoke_$Seed.zip"
$deviceScript = "level_preview_smoke_$Seed.json5"
$localScript = Join-Path $androidRoot "temp\level_preview_smoke\$deviceScript"
$selectionFile = "files/level_preview_smoke_selection.json"
$introspectionFile = "files/level_preview_introspect.json"
$requestId = ""

function Read-AppJson {
    param([Parameter(Mandatory = $true)][string]$Path)
    $text = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", $Path) -Seconds 8
    if (-not $text -or $text -match "No such file") { return $null }
    try { return ($text | ConvertFrom-Json) } catch { return $null }
}

try {
    $candidates =
    @(Get-ChildItem -LiteralPath $MissionDir -File -Filter *.zip | Where-Object {
            $_.Length -le $MaxZipBytes -and (Test-Path -LiteralPath (Join-Path $_.DirectoryName "$($_.BaseName).json"))
        } | ForEach-Object {
            $zip = $_
            $metadataPath = Join-Path $zip.DirectoryName "$($zip.BaseName).json"
            try {
                $metadata = @(Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json)
                $games = @($metadata | Where-Object {
                        $_.status -eq "ok" -and $_.game -in @("d1", "d2") -and @($_.levels).Count -gt 0
                    } | ForEach-Object { $_.game } | Sort-Object -Unique)
                if ($games.Count -gt 0 -and (-not $Game -or $Game -in $games)) {
                    [pscustomobject]@{ Zip = $zip; Games = $games }
                }
            } catch {
                Write-Warning "Skipping malformed metadata $metadataPath"
            }
        } | Sort-Object { $_.Zip.Name })
    if ($candidates.Count -eq 0) {
        throw "No eligible mission ZIPs found in $MissionDir"
    }

    $random = [System.Random]::new($Seed)
    $selectedPack = $candidates[$random.Next($candidates.Count)]
    $selectedGame = if ($Game) { $Game } else { $selectedPack.Games[$random.Next($selectedPack.Games.Count)] }
    Write-Status "Random preview seed: $Seed"
    Write-Status "Selected pack $($selectedPack.Zip.Name) from $($candidates.Count) eligible packs; game=$selectedGame"

    if (-not (Test-DeviceOnline -Serial $Serial)) {
        throw "Android device $Serial is not online"
    }
    $deviceTemporaryZip = "/data/local/tmp/$deviceZip"
    $push = Adb -AdbArgs @("push", $selectedPack.Zip.FullName, $deviceTemporaryZip)
    if ($push -match "failed") { throw "adb push failed: $push" }
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "mkdir", "-p", "files/mods") | Out-Null
    Adb -AdbArgs @(
        "shell", "run-as", $script:PACKAGE, "cp", $deviceTemporaryZip, "files/mods/$deviceZip"
    ) | Out-Null
    Adb -AdbArgs @("shell", "rm", "-f", $deviceTemporaryZip) | Out-Null

    $scriptBody = @(
        [ordered]@{ _info = [ordered]@{ description = "Seeded random level automap preview smoke"; games = @($selectedGame) } },
        [ordered]@{ action = "enter_launcher" },
        [ordered]@{
            action = "launch_random_level_preview"
            label = $selectedPack.Zip.Name
            mod_file = $deviceZip
            game = $selectedGame
            seed = $Seed
        }
    ) | ConvertTo-Json -Depth 8
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $localScript) | Out-Null
    [IO.File]::WriteAllText($localScript, $scriptBody + "`n", [Text.UTF8Encoding]::new($false))
    $deviceTemporaryScript = "/data/local/tmp/$deviceScript"
    Adb -AdbArgs @("push", $localScript, $deviceTemporaryScript) | Out-Null
    Adb -AdbArgs @(
        "shell", "run-as", $script:PACKAGE, "cp", $deviceTemporaryScript, "files/$deviceScript"
    ) | Out-Null
    Adb -AdbArgs @("shell", "rm", "-f", $deviceTemporaryScript) | Out-Null
    Adb -AdbArgs @(
        "shell", "run-as", $script:PACKAGE, "rm", "-f", $selectionFile, "$selectionFile.tmp",
        $introspectionFile, "$introspectionFile.tmp", "files/automation_result.json"
    ) | Out-Null

    if (-not (Start-SetupActivity -Serial $Serial)) { throw "SetupActivity did not become ready" }
    Adb -AdbArgs @("logcat", "-c") | Out-Null
    Adb -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_AUTOMATE", "--es", "script", $deviceScript
    ) | Out-Null

    $selection = $null
    $initial = $null
    $ready = Wait-ForCondition -Description "random level preview introspection" -TimeoutSec $TimeoutSeconds -PollMs 1000 -Condition {
        $result = Read-AppJson -Path "files/automation_result.json"
        if ($result -and $result.result -eq "FAIL") { throw "Launcher automation failed: $($result.reason)" }
        $script:selection = Read-AppJson -Path $selectionFile
        Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.LEVEL_PREVIEW_INTROSPECT") | Out-Null
        Start-Sleep -Milliseconds 250
        $script:initial = Read-AppJson -Path $introspectionFile
        return $script:selection -and $script:initial -and $script:initial.level_preview.active -and
        $script:initial.level_preview.palette_ready -and
        -not [string]::IsNullOrWhiteSpace([string]$script:initial.level_preview.palette_name) -and
        $script:initial.automap_active -and $script:initial.automap
    }
    if (-not $ready) { throw "Preview did not produce a live automap introspection snapshot" }
    $requestId = [string]$selection.request_id
    Write-Status "Selected level $($selection.level_num): $($selection.level_name) [$($selection.level_file)]"

    $initialDistance = [double]$initial.automap.view_dist
    $initialViewX = [double]$initial.automap.view_x
    $initialViewY = [double]$initial.automap.view_y
    $initialViewZ = [double]$initial.automap.view_z
    $initialForwardX = [double]$initial.automap.view_forward_x
    $initialForwardY = [double]$initial.automap.view_forward_y
    $initialForwardZ = [double]$initial.automap.view_forward_z
    $initialPitch = [int]$initial.automap.tangles_p
    $initialHeading = [int]$initial.automap.tangles_h
    $initialIterations = [long]$initial.level_preview.event_iterations
    $zoomAxis = [int]$initial.axis_bind_throttle
    $turnAxis = [int]$initial.axis_bind_turn
    $pitchAxis = [int]$initial.axis_bind_pitch
    if ($zoomAxis -lt 0 -or $zoomAxis -ge 8) { $zoomAxis = 1 }
    if ($turnAxis -lt 0 -or $turnAxis -ge 8) { $turnAxis = 2 }
    if ($pitchAxis -lt 0 -or $pitchAxis -ge 8) { $pitchAxis = 3 }

    foreach ($input in @(
            @{ Axis = $zoomAxis; Value = -0.9 },
            @{ Axis = $turnAxis; Value = 0.8 },
            @{ Axis = $pitchAxis; Value = -0.7 }
        )) {
        foreach ($sample in 1..8) {
            Adb -AdbArgs @(
                "shell", "am", "broadcast", "-a", "com.dxxredux.LEVEL_PREVIEW_COMMAND",
                "--es", "command", "axis", "--ei", "axis", "$($input.Axis)",
                "--ef", "value", "$($input.Value)", "--ez", "active", "true"
            ) | Out-Null
            Start-Sleep -Milliseconds 80
        }
        Adb -AdbArgs @(
            "shell", "am", "broadcast", "-a", "com.dxxredux.LEVEL_PREVIEW_COMMAND",
            "--es", "command", "axis", "--ei", "axis", "$($input.Axis)",
            "--ef", "value", "0.0", "--ez", "active", "false"
        ) | Out-Null
    }

    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.LEVEL_PREVIEW_INTROSPECT") | Out-Null
    $after = $null
    $refreshed = Wait-ForCondition -Description "post-input preview introspection" -TimeoutSec 20 -PollMs 500 -Condition {
        $script:after = Read-AppJson -Path $introspectionFile
        $script:after -and
        $script:after.level_preview.active -and
        [long]$script:after.level_preview.event_iterations -gt $initialIterations
    }
    if (-not $refreshed) { throw "Preview heartbeat did not advance after input" }
    if (-not $after -or -not $after.level_preview.active -or -not $after.automap_active) {
        throw "Preview was no longer active after zoom/rotation input"
    }
    $cameraChanged =
    [math]::Abs([double]$after.automap.view_dist - $initialDistance) -gt 0.01 -or
    [math]::Abs([double]$after.automap.view_x - $initialViewX) -gt 0.01 -or
    [math]::Abs([double]$after.automap.view_y - $initialViewY) -gt 0.01 -or
    [math]::Abs([double]$after.automap.view_z - $initialViewZ) -gt 0.01 -or
    [math]::Abs([double]$after.automap.view_forward_x - $initialForwardX) -gt 0.0001 -or
    [math]::Abs([double]$after.automap.view_forward_y - $initialForwardY) -gt 0.0001 -or
    [math]::Abs([double]$after.automap.view_forward_z - $initialForwardZ) -gt 0.0001 -or
    [int]$after.automap.tangles_p -ne $initialPitch -or
    [int]$after.automap.tangles_h -ne $initialHeading
    if (-not $cameraChanged) {
        $diagnostics = [ordered]@{
            control_type = $after.control_type
            axis_bind_pitch = $after.axis_bind_pitch
            axis_bind_turn = $after.axis_bind_turn
            axis_bind_throttle = $after.axis_bind_throttle
            axis_mailbox = $after.axis_mailbox
            axis_probe = $after.axis_probe
            automap = $after.automap
        } | ConvertTo-Json -Depth 8 -Compress
        throw "Automap camera did not respond to zoom/rotation input; diagnostics=$diagnostics"
    }

    Adb -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.LEVEL_PREVIEW_COMMAND", "--es", "command", "close"
    ) | Out-Null
    $closed = Wait-ForCondition -Description "preview closes and request cache is removed" -TimeoutSec 30 -PollMs 500 -Condition {
        $activities = Adb-Timeout -AdbArgs @("shell", "dumpsys", "activity", "activities") -Seconds 8
        $requestState = if ($requestId -match '^[A-Za-z0-9._-]+$') {
            Adb-Timeout -AdbArgs @(
                "shell", "run-as", $script:PACKAGE, "find", "cache/level_preview", "-maxdepth", "1",
                "-name", $requestId, "-print"
            ) -Seconds 8
        } else { "present" }
        $setupResumed = $activities -match "(?m)^\s*(?:topResumedActivity|mResumedActivity|ResumedActivity)[=:].*SetupActivity"
        return $setupResumed -and $requestState -notmatch [regex]::Escape($requestId)
    }
    if (-not $closed) { throw "Preview did not close cleanly or its request cache remained" }
    $returnLog = Adb-Timeout -AdbArgs @("logcat", "-d", "-s", "DXX-Setup:I", "*:S") -Seconds 8
    if ($returnLog -notmatch "Preserving launcher metadata state after read-only level preview") {
        throw "SetupActivity resumed without preserving metadata state after the preview"
    }
    Write-Status "PASS: seeded random preview loaded, changed camera state, stayed alive, and returned without metadata refresh" "Green"
} finally {
    try {
        if (Test-DeviceOnline -Serial $Serial) {
            Adb -AdbArgs @(
                "shell", "am", "broadcast", "-a", "com.dxxredux.LEVEL_PREVIEW_COMMAND", "--es", "command", "close"
            ) | Out-Null
            if (Start-SetupActivity -Serial $Serial) {
                $cleanupBody = @(
                    [ordered]@{ action = "enter_launcher" },
                    [ordered]@{ action = "delete_mod"; mod_file = $deviceZip }
                ) | ConvertTo-Json -Depth 5
                [IO.File]::WriteAllText($localScript, $cleanupBody + "`n", [Text.UTF8Encoding]::new($false))
                $deviceTemporaryScript = "/data/local/tmp/$deviceScript"
                Adb -AdbArgs @("push", $localScript, $deviceTemporaryScript) | Out-Null
                Adb -AdbArgs @(
                    "shell", "run-as", $script:PACKAGE, "cp", $deviceTemporaryScript, "files/$deviceScript"
                ) | Out-Null
                Adb -AdbArgs @("shell", "rm", "-f", $deviceTemporaryScript) | Out-Null
                Adb -AdbArgs @(
                    "shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json"
                ) | Out-Null
                Adb -AdbArgs @(
                    "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_AUTOMATE", "--es", "script", $deviceScript
                ) | Out-Null
                $cleanupPassed = Wait-ForCondition -Description "smoke mission cleanup" -TimeoutSec 30 -PollMs 500 -Condition {
                    $cleanupResult = Read-AppJson -Path "files/automation_result.json"
                    return $cleanupResult -and $cleanupResult.result -eq "PASS"
                }
                if (-not $cleanupPassed) { Write-Warning "App-managed mission cleanup did not complete" }
            }
            if ($requestId -match '^[A-Za-z0-9._-]+$') {
                Adb -AdbArgs @(
                    "shell", "run-as", $script:PACKAGE, "rm", "-rf", "cache/level_preview/$requestId"
                ) | Out-Null
            }
            Adb -AdbArgs @(
                "shell", "run-as", $script:PACKAGE, "rm", "-f", "files/$deviceScript"
            ) | Out-Null
        }
    } catch {
        Write-Warning "Smoke cleanup failed: $($_.Exception.Message)"
    }
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}

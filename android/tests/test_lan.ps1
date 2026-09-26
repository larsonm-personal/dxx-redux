#!/usr/bin/env pwsh
# test_lan.ps1 -- Two-player LAN multiplayer integration test.
#
# Uses the lan_launch MP_COMMAND to bypass the launcher lobby UI entirely,
# launching the game engine directly in host/join mode on two emulators.
#
# Modes:
#   -DirectLan  (default) Uses emulator 36.5+ shared Wi-Fi. The joiner
#               connects to the host's wlan0 IP directly. No relay needed.
#   -UseRelay   Legacy mode: a UDP relay bridges two emulators' separate
#               SLIRP NATs (pre-36.5 emulators).
#
# Prerequisites:
#   - Two emulators running (emulator-5554 and emulator-5556)
#   - APK installed on both
#   - Game data present on both
#
# Usage:
#   .\test_lan.ps1
#   .\test_lan.ps1 -Game d1
#   .\test_lan.ps1 -Game d2 -D1LevelTransition  # First Strike in the D2 engine
#   .\test_lan.ps1 -Game d1 -D1LevelTransition  # Native control
#   .\test_lan.ps1 -Game d2 -EndgameBoss
#   .\test_lan.ps1 -Game d1 -Endgame -EndgameClientFirst
#   .\test_lan.ps1 -Game d2 -EndgameObserverHost
#   .\test_lan.ps1 -Game d2 -EndgameContent custom
#   .\test_lan.ps1 -Game d2 -EndgameContent missing -EndgameClientFirst
#   .\test_lan.ps1 -GuidebotOwnership
#   .\test_lan.ps1 -GuidebotClientRelease cage
#   .\test_lan.ps1 -GuidebotClientRelease deploy
#   .\test_lan.ps1 -GuidebotSpawn -InitialLevel 8 -AllowSecretWarps
#   .\test_lan.ps1 -GuidebotTravel -InitialLevel 8 -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -GuidebotHostObserver
#   .\test_lan.ps1 -GuidebotSlotRemapRestore
#   .\test_lan.ps1 -SavedLateJoin -Game d2
#   .\test_lan.ps1 -SavedLateJoin -RestoreStatus -Game d2
#   .\test_lan.ps1 -HostMigration
#   .\test_lan.ps1 -GuidebotRoutingMode Original -HostMigration
#   .\test_lan.ps1 -HostDevice emulator-5556 -HostAvd Nexus5X_Light_2 -JoinDevice emulator-5558 -JoinAvd DxxSdk36
#   .\test_lan.ps1 -SpewRecovery
#   .\test_lan.ps1 -Game d1 -CoopDeath
#   .\test_lan.ps1 -Game d2 -BriefingCase paused_force  # Requires other-h.mvl in game data
#   .\test_lan.ps1 -Game d2 -BriefingCase paused_deadline
#   .\test_lan.ps1 -Game d2 -BriefingCase paused_overall
#   .\test_lan.ps1 -Game d1 -BriefingFailure host
#   .\test_lan.ps1 -Game d2 -BriefingFailure client
#   .\test_lan.ps1 -Game d2 -BriefingFailure host -BriefingFailurePaused  # Requires other-h.mvl
#   .\test_lan.ps1 -Game d1 -BriefingFailure host -BriefingFailureRelease
#   .\test_lan.ps1 -Game d1 -Briefings -BriefingCase reading
#   .\test_lan.ps1 -Game d1 -Briefings -BriefingCase partial_skip
#   .\test_lan.ps1 -Game d2 -MissionFile max_f -InitialLevel 13 -BriefingPalette  # Requires Descent Maximum (fixed)
#   .\test_lan.ps1 -Game d2 -MissionFile max_f -InitialLevel 14 -EmptyBriefing
#   .\test_lan.ps1 -Game d2 -BriefingCase missing_movie  # No pla.mve/other-h.mvl installed
#   .\test_lan.ps1 -Game d1 -BriefingCase observer_host
#   .\test_lan.ps1 -Game d1 -BriefingCase rejoin
#   .\test_lan.ps1 -Game d1 -BriefingCase first_join
#   .\test_lan.ps1 -Game d2 -InitialLevel 8 -SecretDeath -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -Game d2 -InitialLevel 8 -SecretDying -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -Game d2 -InitialLevel 8 -SecretReactorDeath -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -Game d2 -InitialLevel 8 -SecretCountdown -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -Game d2 -InitialLevel 8 -SecretEndgame -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -Game d2 -InitialLevel 8 -SecretEndgameModal -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -Game d2 -InitialLevel 8 -SecretEndgameHostLeaves -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -Game d2 -InitialLevel 8 -NormalReactorDeath -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -Game d2 -InitialLevel 8 -NormalCountdown -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -Game d2 -InitialLevel 8 -SecretWorld -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -Game d2 -InitialLevel 8 -SecretCrossRestore -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -Game d2 -InitialLevel 8 -TravelGate -AllowSecretWarps -NoCoopQol
#   .\test_lan.ps1 -UseRelay
#   .\test_lan.ps1 -SkipBuild
#   .\test_lan.ps1 -Game d2 -MissionFile max_f -InitialLevel 18 -AllowSecretWarps -MaximumExitProbe

param(
    [string]$Game = "d2",
    [string]$HostDevice = 'emulator-5554',
    [string]$JoinDevice = 'emulator-5556',
    [string]$HostAvd = 'Nexus5X_Light_1',
    [string]$JoinAvd = 'Nexus5X_Light_2',
    [string]$MissionFile,
    [int]$InitialLevel = 1,
    [string]$HostCallsign = "LanHost",
    [string]$JoinCallsign = "LanJoin",
    [string]$RestoreSavePath,
    [switch]$SkipBuild,
    [switch]$UseRelay,
    [switch]$GuidebotOwnership,
    [ValidateSet('Original', 'Enhanced')]
    [string]$GuidebotRoutingMode,
    [ValidateSet('cage', 'deploy')]
    [string]$GuidebotClientRelease,
    [switch]$GuidebotSpawn,
    [switch]$GuidebotTravel,
    [switch]$GuidebotHostObserver,
    [switch]$GuidebotSlotRemapRestore,
    [switch]$SavedLateJoin,
    [switch]$RestoreStatus,
    [switch]$RestoreResilience,
    [ValidateSet("unexpected_exit", "interrupted_restore", "normal_quit", "fail_after_hide")]
    [string]$RestoreReportCase,
    [switch]$HostMigration,
    [switch]$SpewRecovery,
    [switch]$SpewPickup,
    [switch]$SpewPartialPickup,
    [switch]$Briefings,
    [switch]$D1LevelTransition,
    [switch]$Flyouts,
    [ValidateSet("natural", "deadline", "force")]
    [string]$FlyoutCase = "natural",
    [string]$FlyoutMovieLibrary,
    [switch]$BriefingPalette,
    [switch]$EmptyBriefing,
    [ValidateSet("force", "host_deadline", "overall_deadline", "release_delay", "overlay_touch", "paused_force", "paused_deadline", "paused_overall", "reading", "partial_skip", "missing_movie", "observer_host", "rejoin", "first_join")]
    [string]$BriefingCase = "force",
    [switch]$BriefingRestore,
    [ValidateSet("host", "client")]
    [string]$BriefingFailure,
    [switch]$BriefingFailurePaused,
    [switch]$BriefingFailureRelease,
    [switch]$CountdownSave,
    [Alias("RestoreParticipantLoss")]
    [ValidateSet("client", "host", "stalled", "sync_stalled", "load_client", "load_host")]
    [string]$RestoreFailure,
    [switch]$RestoreLossResume,
    [switch]$LevelRestart,
    [switch]$CoopRewind,
    [switch]$ClientRewind,
    [switch]$SecretRewind,
    [switch]$SecretRestart,
    [switch]$AllowSecretWarps,
    [switch]$NoCoopQol,
    [switch]$WorldRestore,
    [switch]$SecretWorld,
    [ValidateSet("freezing", "capturing", "loading", "committed", "release")]
    [string]$SecretDisconnectPhase,
    [switch]$SecretRevisit,
    [switch]$SecretRollback,
    [switch]$SecretPhysical,
    [switch]$SecretDeath,
    [switch]$SecretDying,
    [switch]$SecretReactorDeath,
    [switch]$DestroyedGearRestore,
    [switch]$SecretCountdown,
    [switch]$CoopDeath,
    [switch]$SecretSaveRestore,
    [switch]$SecretCrossRestore,
    [switch]$SecretColdResume,
    [switch]$NormalPhysical,
    [switch]$MaximumExitProbe,
    [switch]$NormalExitRace,
    [switch]$NormalReactorDeath,
    [switch]$NormalCountdown,
    [switch]$SecretExitRace,
    [switch]$SecretAdvance,
    [switch]$SecretEndgame,
    [switch]$Endgame,
    [switch]$EndgameClientFirst,
    [switch]$EndgameBoss,
    [switch]$EndgameObserverHost,
    [ValidateSet("builtin", "custom", "missing")]
    [string]$EndgameContent = "builtin",
    [string]$EndgameMovieLibrary,
    [switch]$SecretEndgameModal,
    [switch]$SecretEndgameHostLeaves,
    [switch]$VerifyAutomationFailure,
    [switch]$TravelGate,
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if ($HostDevice -eq $JoinDevice) { throw 'Host and joiner require separate devices' }
if ($GuidebotRoutingMode -and $Game -ne 'd2') { throw 'Guidebot routing requires D2' }
if ($GuidebotTravel) {
    if ($Game -ne 'd2' -or $InitialLevel -ne 8 -or $MissionFile -or -not $AllowSecretWarps) {
        throw 'GuidebotTravel requires Counterstrike level 8 and AllowSecretWarps'
    }
    $SecretWorld = $SecretRevisit = $true
}
if ($D1LevelTransition) {
    if ($Game -notin @('d1', 'd2') -or $InitialLevel -ne 1 -or $RestoreSavePath -or
        ($MissionFile -and $MissionFile -ne 'descent')) {
        throw 'D1LevelTransition requires a fresh First Strike level 1 session in d1 or d2'
    }
    if ($Game -eq 'd2') { $MissionFile = 'descent' }
    $Briefings = $true
}
if ($BriefingPalette -or $EmptyBriefing) { $Briefings = $true }
if ($EndgameClientFirst -or $EndgameBoss -or $EndgameObserverHost -or $EndgameContent -ne "builtin") { $Endgame = $true }
if ($EndgameBoss -and ($Game -ne "d2" -or $EndgameClientFirst -or $EndgameObserverHost -or $EndgameContent -ne "builtin")) {
    throw "EndgameBoss uses the D2 built-in final boss with the playing host finishing first"
}
if ($EndgameObserverHost -and ($EndgameClientFirst -or $EndgameContent -ne "builtin")) {
    throw "EndgameObserverHost uses the built-in ending with the observing host finishing first"
}
if ($Endgame -and $EndgameContent -ne "builtin") {
    if ($Game -ne "d2") { throw "Custom ending fixtures use D2" }
    $MissionFile = "coopend"
    $InitialLevel = 1
}
if ($Endgame -and -not $MissionFile -and $InitialLevel -eq 1) {
    $InitialLevel = if ($Game -eq "d1") { 27 } else { 24 }
}
if ($ClientRewind) { $CoopRewind = $true }
if ($RestoreLossResume -and -not $RestoreFailure) { throw "RestoreLossResume requires RestoreFailure" }
if ($RestoreFailure -and ($MissionFile -or $InitialLevel -ne 1 -or $CountdownSave -or $CoopRewind -or
        $LevelRestart -or $SecretWorld -or $SecretPhysical -or $SecretSaveRestore -or $SecretRewind -or $SecretRestart)) {
    throw "Restore participant loss requires the base mission at level 1; run other save/travel scenarios separately"
}
if ($SecretRewind -or $SecretRestart) { $SecretPhysical = $SecretWorld = $true }
if ($BriefingRestore) { $Briefings = $true }
if ($BriefingFailurePaused -and (-not $BriefingFailure -or $Game -ne 'd2')) {
    throw "BriefingFailurePaused requires D2 with BriefingFailure host or client and other-h.mvl installed"
}
if ($BriefingFailureRelease -and (-not $BriefingFailure -or $BriefingFailurePaused)) {
    throw "BriefingFailureRelease requires BriefingFailure host or client; run paused-video loss separately"
}
if ($BriefingFailure) {
    if ($InitialLevel -ne 1 -or $MissionFile -or $BriefingCase -ne 'force' -or $BriefingRestore -or $RestoreFailure) {
        throw "BriefingFailure requires a fresh base-mission level 1 briefing; run other scenarios separately"
    }
    $Briefings = $true
}
if ($Flyouts) {
    if ($Game -ne 'd2' -or $InitialLevel -ne 1 -or $MissionFile) { throw 'Flyouts requires D2 Counterstrike level 1' }
    $Briefings = $true
}
if ($BriefingCase -like "paused_*") {
    if ($Game -ne "d2" -or $InitialLevel -ne 1 -or $MissionFile) { throw "Paused-video cases require D2 Counterstrike level 1 with other-h.mvl installed" }
    $Briefings = $true
}
if ($BriefingCase -eq "missing_movie") {
    if ($Game -ne "d2" -or $InitialLevel -ne 1 -or $MissionFile) { throw "Missing-movie coverage requires D2 Counterstrike level 1 without pla.mve installed" }
    $Briefings = $true
}
if ($BriefingCase -eq "observer_host") {
    if ($InitialLevel -ne 1 -or $MissionFile -or $BriefingRestore) { throw "Observer-host coverage requires a fresh base-mission level 1 briefing" }
    $Briefings = $true
}
if ($BriefingCase -in @("rejoin", "first_join")) {
    if ($InitialLevel -ne 1 -or $MissionFile -or $BriefingRestore) { throw "Briefing rejoin requires a fresh base-mission level 1" }
    $Briefings = $true
}
if ($SecretRevisit) { $SecretWorld = $true }
if ($SecretRollback) { $SecretWorld = $true }
if ($DestroyedGearRestore) { $SecretReactorDeath = $true }
if ($SecretDeath -or $SecretDying -or $SecretReactorDeath -or $SecretCountdown) { $SecretPhysical = $true }
if ($SecretPhysical) { $SecretWorld = $true }
if ($SecretColdResume) { $SecretSaveRestore = $Briefings = $true }
if ($SecretCrossRestore) { $SecretSaveRestore = $true }
if ($SecretSaveRestore) { $SecretPhysical = $SecretWorld = $true }
if ($NormalExitRace -or $NormalReactorDeath -or $NormalCountdown) { $NormalPhysical = $true }
if ($SecretAdvance) { $SecretExitRace = $Briefings = $true }
if ($SecretEndgameHostLeaves) { $SecretEndgameModal = $true }
if ($SecretEndgameModal) { $SecretEndgame = $true }
if ($SecretEndgame) { $SecretExitRace = $Briefings = $true }
if ($SecretEndgame -and $SecretAdvance) { throw "Run secret campaign advancement and ending separately" }
if ($Briefings -and $Game -eq "d2" -and $InitialLevel -eq 8 -and -not $MissionFile -and $BriefingCase -ne "force") {
    throw "Counterstrike level 8 has no authored briefing; use level 1 to exercise briefing timing and input cases"
}
if ($SecretPhysical -and $SecretRollback) { throw "Run physical-trigger and rollback fixtures separately" }
if (($SecretRewind -or $SecretRestart) -and ($Game -ne 'd2' -or $InitialLevel -ne 8 -or -not $AllowSecretWarps -or
        $CoopRewind -or $LevelRestart -or $CountdownSave -or $SecretSaveRestore -or $SecretDeath -or
        $SecretDying -or $SecretReactorDeath -or $SecretCountdown)) {
    throw "Secret rewind/restart requires D2 level 8 with secret warps enabled; run other save, restart and death scenarios separately"
}
if ($SecretRewind -and $SecretRestart) { throw "Run secret rewind and secret restart separately" }
if (($SecretReactorDeath -or $SecretCountdown) -and ($SecretRevisit -or $SecretDying -or $SecretSaveRestore -or $SecretDeath)) {
    throw "Run destroyed-secret scenarios separately from revisit, save and other death fixtures"
}
if ($SecretReactorDeath -and $SecretCountdown) { throw "Run reactor teammate death and whole-team countdown expiry separately" }
if (@(@($NormalExitRace, $NormalReactorDeath, $NormalCountdown) | Where-Object { $_ }).Count -gt 1) {
    throw "Run normal exit races, reactor teammate death and whole-team countdown expiry separately"
}
if ($CoopDeath -and $NoCoopQol -and $Game -eq "d1") { throw "D1 death recovery coverage requires co-op QoL" }
if ($SpewRecovery -and $NoCoopQol -and ($Game -ne "d2" -or -not $AllowSecretWarps)) {
    throw "SpewRecovery requires co-op QoL or D2 secret warps to enable recovery"
}
if (@(@($WorldRestore, $SecretWorld, $TravelGate, $NormalPhysical, $SecretExitRace, $CoopDeath) | Where-Object { $_ }).Count -gt 1) {
    throw "Run WorldRestore, SecretWorld, TravelGate, NormalPhysical, SecretExitRace and CoopDeath separately; each owns the mine state"
}
if (($NormalPhysical -or $SecretExitRace) -and ($Game -ne "d2" -or $InitialLevel -ne 8 -or -not $AllowSecretWarps)) {
    throw "Physical exit race fixtures require D2 level 8 with secret warps enabled"
}

. "$PSScriptRoot\..\helpers\test_helpers.ps1"

# -- Constants --
$REPO_ROOT = Split-Path (Split-Path $PSScriptRoot)
$DEP_BASE = (Get-Content (Join-Path $REPO_ROOT "dependency_base.txt") -First 1).Trim()
$ADB = Resolve-RegressionAndroidSdkTool -DepBase $DEP_BASE -Subdir "platform-tools" -ToolName "adb" -EnvironmentVariable "ADB"
$PACKAGE = "com.dxxredux.app"
$ACTIVITY = "com.dxxredux.app.SetupActivity"

$EMULATOR = Resolve-RegressionAndroidSdkTool -DepBase $DEP_BASE -Subdir "emulator" -ToolName "emulator"
$EMU1 = $HostDevice
$EMU2 = $JoinDevice
$AVD_MAP = @{ $EMU1 = $HostAvd; $EMU2 = $JoinAvd }
$CALLSIGN1 = $HostCallsign
$CALLSIGN2 = $JoinCallsign
$MIGRATED_HOST_PORT = 42425

# Mission filenames as used by the engine (not display names)
$MISSION = if ($MissionFile) { $MissionFile } elseif ($Game -eq "d1") { "" } else { "d2" }
$MODE = "coop"

$relayProc = $null
$testPassed = $false
$script:EndgameFiles = @()

$script:LogFile = Join-Path $REPO_ROOT "temp\lan_test_log.txt"
try { if (Test-Path $script:LogFile) { Remove-Item $script:LogFile -Force -ErrorAction SilentlyContinue } } catch { }
try { [IO.File]::WriteAllText($script:LogFile, [Environment]::NewLine, [Text.UTF8Encoding]::new($false)) } catch {}

function Cleanup {
    Write-Status "Cleaning up..."
    try { if ($logcatProc1 -and -not $logcatProc1.HasExited) { Stop-Process -Id $logcatProc1.Id -Force } } catch {}
    try { if ($logcatProc2 -and -not $logcatProc2.HasExited) { Stop-Process -Id $logcatProc2.Id -Force } } catch {}
    foreach ($emu in @($EMU1, $EMU2)) {
        try {
            Adb-Dev-Timeout -Serial $emu -AdbArgs @(
                "shell", "am", "force-stop", $PACKAGE
            ) -Seconds 5 | Out-Null
        } catch {}
    }
    foreach ($file in $script:EndgameFiles) {
        Adb-Dev-Timeout -Serial $file.Serial -AdbArgs @("shell", "run-as", $PACKAGE, "rm", "-f", $file.Path) -Seconds 5 | Out-Null
    }
    $script:EndgameFiles = @()
    if ($Endgame -and $EndgameContent -ne 'builtin') {
        foreach ($serial in @($EMU1, $EMU2)) {
            Remove-EndgameMissionContent -Serial $serial
            Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) -Seconds 5 | Out-Null
        }
    }
    $relayLog = Join-Path $REPO_ROOT "temp\udp_relay.log"
    if ($UseRelay -and (Test-Path $relayLog)) {
        $lines = Get-Content $relayLog -ErrorAction SilentlyContinue
        if ($lines) {
            Write-Status "  Relay log ($($lines.Count) lines):" "Gray"
            foreach ($l in ($lines | Select-Object -Last 20)) {
                Write-Status "    $l" "Gray"
            }
        }
    }
    if ($script:relayProc -and -not $script:relayProc.HasExited) {
        Write-Status "Stopping UDP relay (PID $($script:relayProc.Id))..."
        try { $script:relayProc.Kill() } catch {}
    }
}

function Stage-EndgameFile {
    param([string]$Serial, [string]$LocalPath, [string]$DevicePath)
    $existing = Adb-Dev-Timeout -Serial $Serial -AdbArgs @("shell", "run-as", $PACKAGE, "ls", $DevicePath) -Seconds 5
    if ($existing -and $existing.Trim() -eq $DevicePath) { return }
    $temporary = "/data/local/tmp/coop_endgame_" + [IO.Path]::GetFileName($LocalPath)
    Adb-Dev -Serial $Serial -AdbArgs @("push", $LocalPath, $temporary) | Out-Null
    $directory = $DevicePath.Substring(0, $DevicePath.LastIndexOf('/'))
    Adb-Dev -Serial $Serial -AdbArgs @("shell", "run-as", $PACKAGE, "mkdir", "-p", $directory) | Out-Null
    Adb-Dev -Serial $Serial -AdbArgs @("shell", "run-as", $PACKAGE, "cp", $temporary, $DevicePath) | Out-Null
    Adb-Dev -Serial $Serial -AdbArgs @("shell", "rm", "-f", $temporary) | Out-Null
    $script:EndgameFiles += @{ Serial = $Serial; Path = $DevicePath }
}

function Remove-EndgameMissionContent {
    param([string]$Serial)
    # The launcher adopts loose missions into managed content; remove the generated
    # fixture through its API so a previous run cannot supply supposedly missing assets
    $paths = Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'run-as', $PACKAGE, 'find',
        'files/imported/sets/default/.content/entries', '-path', '*/payload/missions/coopend.mn2') -Seconds 5
    foreach ($rawPath in @($paths -split "`n")) {
        $path = $rawPath.Trim()
        if ($path -notmatch '/entries/([a-f0-9]+)/payload/missions/coopend.mn2$') { continue }
        $id = $Matches[1]
        $contents = Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'run-as', $PACKAGE, 'cat', $path.Trim()) -Seconds 5
        if ($contents -notmatch 'name = Co-op ending fixture') { throw 'Existing coopend mission is not the generated fixture' }
        if (-not (Start-SetupActivity -Serial $Serial -TimeoutSec 60)) { throw 'Could not open launcher to remove ending fixture' }
        Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
            '--es', 'command', 'delete_content', '--es', 'id', $id) -Seconds 5 | Out-Null
        if (-not (Wait-ForCondition -Description 'Remove managed ending fixture' -TimeoutSec 15 -PollMs 500 -Condition {
                    $remaining = Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'run-as', $PACKAGE, 'ls', $path.Trim()) -Seconds 5 -IncludeStandardError
                    return $remaining -match 'No such file'
                })) { throw 'Managed ending fixture was not removed' }
    }
}

function Initialize-EndgameContent {
    if ((-not $Endgame -and -not $SecretEndgame) -or $Game -ne "d2") { return }
    $output = Join-Path $REPO_ROOT "temp/coop-endgame-fixture"
    & "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output | Out-Null
    $arguments = @("$PSScriptRoot/prepare_coop_endgame_fixture.py", "--output", $output)
    if ($EndgameContent -eq "builtin" -and -not $MissionFile) {
        $library = $EndgameMovieLibrary
        if (-not $library) { $library = Join-Path $REPO_ROOT "game_data/CD images/Descent II (USA)/data_tracks/d2data/intro-h.mvl" }
        if (-not (Test-Path -LiteralPath $library)) { throw "Pass -EndgameMovieLibrary with an owned intro-h.mvl or intro-l.mvl containing end.mve" }
        $arguments += @("--movie-library", $library)
    }
    & python @arguments
    if ($LASTEXITCODE) { throw "Could not prepare ending fixtures" }
    foreach ($serial in @($EMU1, $EMU2)) {
        if ($EndgameContent -eq "builtin") {
            Stage-EndgameFile -Serial $serial -LocalPath "$output/end.mve" -DevicePath "files/d2x-redux/end.mve"
        } else {
            Remove-EndgameMissionContent -Serial $serial
            Stage-EndgameFile -Serial $serial -LocalPath "$output/coopend.mn2" -DevicePath "files/imported/sets/default/missions/coopend.mn2"
            $fastSerial = if ($EndgameClientFirst) { $EMU2 } else { $EMU1 }
            if ($EndgameContent -ne "missing" -or $serial -ne $fastSerial) {
                Stage-EndgameFile -Serial $serial -LocalPath "$output/coopend.hog" -DevicePath "files/imported/sets/default/missions/coopend.hog"
            }
        }
    }
}

function Get-IntroMultiplayer {
    param($Intro)

    if (-not $Intro) {
        return $null
    }

    $prop = $Intro.PSObject.Properties['multiplayer']
    if ($null -eq $prop) {
        return $null
    }

    return $prop.Value
}

function Get-IntroNumConnected {
    param($Intro)

    $mp = Get-IntroMultiplayer -Intro $Intro
    if (-not $mp) {
        return $null
    }

    $prop = $mp.PSObject.Properties['num_connected']
    if ($null -eq $prop) {
        return $null
    }

    return [int]$prop.Value
}

function Get-IntroGuidebot {
    param($Intro)

    if (-not $Intro) {
        return $null
    }
    $prop = $Intro.PSObject.Properties['guidebot']
    if ($null -eq $prop) {
        return $null
    }
    return $prop.Value
}

function Get-IntroPdataSequence {
    param($Intro, [int]$PlayerSlot)

    $mp = Get-IntroMultiplayer -Intro $Intro
    if (-not $mp -or $PlayerSlot -lt 0) {
        return $null
    }
    $prop = $mp.PSObject.Properties['last_pdata_received']
    if ($null -eq $prop -or $PlayerSlot -ge @($prop.Value).Count) {
        return $null
    }
    return [int]$prop.Value[$PlayerSlot]
}

function Start-DeviceGameAutomation {
    param([string]$Serial, [string]$ScriptName)

    $scriptPath = Join-Path $REPO_ROOT "android\game_scripts\$ScriptName"
    if (-not (Test-Path $scriptPath)) {
        Write-Status "FAIL: automation script not found: $scriptPath" "Red"
        return $false
    }

    # The shared resolver reads optional JSON properties under normal PowerShell semantics
    $scriptPath = & { Set-StrictMode -Off; Resolve-TestScript -ScriptPath $scriptPath -GameId $Game }

    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "push", $scriptPath, "/data/local/tmp/$ScriptName"
    ) -Seconds 30 | Out-Null
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cp", "/data/local/tmp/$ScriptName", "files/$ScriptName"
    ) -Seconds 10 | Out-Null
    $copied = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "ls", "files/$ScriptName"
    ) -Seconds 5
    if (-not $copied) {
        # A slow directory read must not restage or relaunch an already copied script
        $copied = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
            "shell", "run-as", $PACKAGE, "ls", "files/$ScriptName"
        ) -Seconds 15
    }
    if (-not $copied -or $copied -notmatch [regex]::Escape($ScriptName)) {
        Write-Status "FAIL: could not stage $ScriptName on $Serial" "Red"
        return $false
    }

    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "rm", "-f",
        "files/automation_result.json", "files/automation_log.jsonl"
    ) -Seconds 5 | Out-Null
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE",
        "--es", "script", $ScriptName
    ) -Seconds 10 | Out-Null
    return $true
}

function Get-DeviceAutomationResult {
    param([string]$Serial)

    $json = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cat", "files/automation_result.json"
    ) -Seconds 5
    if (-not $json -or $json -notmatch '^\s*\{') {
        return $null
    }
    try {
        return $json | ConvertFrom-Json
    } catch {
        return $null
    }
}

function Write-DeviceAutomationDiagnostics {
    param([string]$Serial)

    # Refresh before Cleanup stops the engine; the last polled snapshot may
    # predate the failed gameplay action
    $snapshot = Get-GameIntrospection -Serial $Serial
    if ($snapshot) {
        $snapshotPath = Join-Path $REPO_ROOT "temp/lan-failure-$Serial.json"
        $snapshot | ConvertTo-Json -Depth 40 | Set-Content -Encoding utf8 -LiteralPath $snapshotPath
        Write-Status "  $Serial failure state: $snapshotPath" 'Yellow'
    }
    $result = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cat", "files/automation_result.json"
    ) -Seconds 5
    $log = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cat", "files/automation_log.jsonl"
    ) -Seconds 5
    Write-Status "  $Serial automation result: $result" "Yellow"
    if ($log) {
        Write-Status "  $Serial automation log tail:" "Yellow"
        ($log -split "`n") | Select-Object -Last 20 | ForEach-Object {
            Write-Status "    $_" "Gray"
        }
    }
}

function Invoke-CoopRewindScenario {
    param([switch]$FromClient)

    $rewindPhases = if ($FromClient) { @('seed', 'client_record', 'reactor', 'client_requests') } else { @('seed', 'record', 'reactor', 'mutate', 'rewind', 'verify') }
    $phaseScripts = @{
        seed = 'test_coop_rewind_seed.jsonc'
        client_record = 'test_coop_rewind_client_record.jsonc'
        record = 'test_coop_rewind_record.jsonc'
        reactor = 'test_coop_countdown_save_seed.jsonc'
        mutate = 'test_coop_rewind_mutate.jsonc'
        rewind = 'test_coop_rewind_rewind.jsonc'
        verify = 'test_coop_rewind_verify.jsonc'
        client_requests = 'test_coop_rewind_client_host.jsonc'
    }
    foreach ($phase in $rewindPhases) {
        $hostScript = $phaseScripts[$phase]
        $clientScript = if ($phase -eq 'rewind') { 'test_coop_countdown_save_wait.jsonc' } else { $hostScript }
        if ($phase -eq 'client_requests') {
            $hostScript = 'test_coop_rewind_client_host.jsonc'
            $clientScript = 'test_coop_rewind_client_peer.jsonc'
        }
        # Two sequential 180-second restore waits, request waits and verification
        $phaseTimeout = if ($phase -eq 'client_requests') { 420 } else { 180 }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $hostScript `
                    -SecondarySerial $EMU2 -SecondaryScript $clientScript `
                    -Description "Natural co-op rewind: $phase" -TimeoutSec $phaseTimeout)) { throw "Co-op rewind $phase failed" }
    }
    return $true
}

function Invoke-BriefingRejoinScenario {
    param([switch]$FirstJoin)

    if ($FirstJoin) {
        if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName 'test_coop_briefing_solo_start.jsonc')) {
            throw 'Could not start the host briefing before the first join'
        }
        if (-not (Wait-ForCondition -Description 'Host starts its briefing before admitting any peers' -TimeoutSec 60 -PollMs 500 -Condition {
                    $result = Get-DeviceAutomationResult -Serial $EMU1
                    if ($result -and $result.result -eq 'FAIL') { throw 'Solo host briefing startup failed' }
                    return $result -and $result.result -eq 'PASS'
                })) { throw 'Host did not begin its solo briefing' }
    }
    if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName 'test_coop_briefing_rejoin_prepare.jsonc')) {
        throw 'Could not enable briefing reconnect diagnostics'
    }
    if (-not (Wait-ForCondition -Description 'Briefing reconnect diagnostics enabled' -TimeoutSec 10 -PollMs 500 -Condition {
                $result = Get-DeviceAutomationResult -Serial $EMU1
                return $result -and $result.result -eq 'PASS'
            })) { throw 'Briefing reconnect diagnostics did not initialize' }
    $initial = (Get-GameIntrospection -Serial $EMU1).coop_briefing
    $generation = $initial.generation
    $seconds = $initial.seconds_remaining
    if (-not $FirstJoin) {
        Adb-Dev-Timeout -Serial $EMU2 -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) -Seconds 10 | Out-Null
        if (-not (Wait-ForCondition -Description 'Host removes the disconnected reader without ending its briefing' -TimeoutSec 40 -PollMs 500 -Condition {
                    $state = Get-GameIntrospection -Serial $EMU1
                    return $state -and $state.coop_briefing.phase -eq 6 -and
                    $state.coop_briefing.participants -eq 1 -and $state.coop_briefing.presenting -and $state.time_paused
                })) { throw 'Host did not remain in its briefing after reader loss' }
    }
    $before = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('logcat', '-d', '-s', 'DXX-DLOG:D') -Seconds 10
    $deferredBefore = @($before -split '\r?\n' | Where-Object { $_ -match 'network join deferred for coop transition' }).Count
    if (-not $FirstJoin -and -not (Start-SetupActivity -Serial $EMU2)) { throw 'Could not restart the returning player' }
    Send-MpCommand -Serial $EMU2 -Command 'lan_launch' -Extras $joinExtras
    if (-not (Wait-ForCondition -Description 'Returning player is deferred while the original briefing timer continues' -TimeoutSec 50 -PollMs 1000 -Condition {
                $state = Get-GameIntrospection -Serial $EMU1
                if (-not $state) { return $false }
                if ($state.coop_briefing.generation -ne $generation -or $state.coop_briefing.phase -ne 6 -or
                    $state.coop_briefing.participants -ne 1 -or -not $state.time_paused) {
                    throw 'A reconnect changed the active briefing generation, roster or phase'
                }
                $output = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('logcat', '-d', '-s', 'DXX-DLOG:D') -Seconds 10
                $count = @($output -split '\r?\n' | Where-Object { $_ -match 'network join deferred for coop transition' }).Count
                return $count -gt $deferredBefore -and $state.coop_briefing.seconds_remaining -lt $seconds
            })) { throw 'The returning player did not reach the briefing join guard' }
    if (-not (Start-DeviceGameAutomation -Serial $EMU2 -ScriptName 'test_coop_briefing_rejoin_client.jsonc') -or
        -not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName 'test_coop_briefing_rejoin_host.jsonc')) {
        throw 'Could not arm briefing rejoin verification'
    }
    if (-not (Wait-ForCondition -Description 'Host finishes the original briefing with the returning player still deferred' -TimeoutSec 15 -PollMs 500 -Condition {
                $result = Get-DeviceAutomationResult -Serial $EMU1
                if ($result -and $result.result -eq 'FAIL') { throw 'Host briefing completion failed' }
                return $result -and $result.result -eq 'PASS'
            })) { throw 'Host did not finish its original briefing' }
    if (-not (Wait-ForCondition -Description 'Returning player joins the settled mine without replaying briefings' -TimeoutSec 80 -PollMs 1000 -Condition {
                $result = Get-DeviceAutomationResult -Serial $EMU2
                if ($result -and $result.result -eq 'FAIL') {
                    Write-DeviceAutomationDiagnostics -Serial $EMU2
                    throw 'Returning player replayed or stalled in briefings'
                }
                if ($result -and $result.result -eq 'PASS') { return $true }
                Start-DeviceGameAutomation -Serial $EMU1 -ScriptName 'test_coop_late_join_accept.jsonc' | Out-Null
                return $false
            })) { throw 'Returning player did not enter the committed mine' }
    $hostState = Get-GameIntrospection -Serial $EMU1
    if (-not $hostState -or $hostState.coop_briefing.generation -ne $generation -or
        $hostState.coop_briefing.active -or $hostState.time_paused -or (Get-IntroNumConnected -Intro $hostState) -ne 2) {
        throw 'Rejoin changed the settled host briefing or failed to restore the team'
    }
    return $true
}

function Invoke-BriefingFailureScenario {
    $lost = if ($BriefingFailure -eq 'host') { $EMU1 } else { $EMU2 }
    $survivor = if ($BriefingFailure -eq 'host') { $EMU2 } else { $EMU1 }
    $scriptName = if ($BriefingFailure -eq 'host') { 'test_coop_briefing_host_lost.jsonc' } else { 'test_coop_briefing_client_lost.jsonc' }
    if ($BriefingFailurePaused) {
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_briefing_pause_host.jsonc' `
                    -SecondarySerial $EMU2 -SecondaryScript 'test_coop_briefing_pause_host.jsonc' `
                    -Description 'Both peers pause their authored briefing videos before participant loss' -TimeoutSec 25)) {
            throw 'Both peers must pause a real briefing movie before interruption'
        }
        if ($BriefingFailure -eq 'client') { $scriptName = 'test_coop_briefing_paused_client_lost.jsonc' }
    }
    if ($BriefingFailureRelease) {
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_briefing_release_loss_prepare_host.jsonc' `
                    -SecondarySerial $EMU2 -SecondaryScript 'test_coop_briefing_release_loss_prepare_client.jsonc' `
                    -Description 'Both presentations close while the client waits for final release' -TimeoutSec 25)) {
            throw 'Both peers must reach the final release handshake before interruption'
        }
        if ($BriefingFailure -eq 'client') { $scriptName = 'test_coop_briefing_release_client_lost.jsonc' }
    }
    $gamePid = (Adb-Dev-Timeout -Serial $survivor -AdbArgs @('shell', 'pidof', "${PACKAGE}:game") -Seconds 5).Trim()
    if (-not $gamePid -or -not (Start-DeviceGameAutomation -Serial $survivor -ScriptName $scriptName)) {
        throw 'Could not arm briefing participant-loss verification'
    }
    if ($BriefingFailureRelease) {
        $hostState = Get-GameIntrospection -Serial $EMU1
        $clientState = Get-GameIntrospection -Serial $EMU2
        if (-not $hostState -or -not $clientState -or -not $hostState.time_paused -or -not $clientState.time_paused -or
            -not $hostState.coop_briefing.active -or -not $clientState.coop_briefing.active -or
            $hostState.coop_briefing.phase -ne 0 -or $hostState.coop_briefing.release_acknowledged -ne 1 -or
            $clientState.coop_briefing.phase -ne 9 -or $clientState.coop_briefing.release_packets_dropped -lt 1) {
            throw 'The final release hold expired before participant loss could be injected'
        }
    }
    $lossPhase = if ($BriefingFailureRelease) { 'waiting for final release' } else { 'reading briefings' }
    Write-Status "Force-stopping briefing $BriefingFailure while peers are $lossPhase" 'Yellow'
    Adb-Dev-Timeout -Serial $lost -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) -Seconds 10 | Out-Null
    if (-not (Wait-ForCondition -Description "Briefing $BriefingFailure loss has a finite, usable outcome" -TimeoutSec 90 -PollMs 500 -Condition {
                $result = Get-DeviceAutomationResult -Serial $survivor
                if ($result -and $result.result -eq 'FAIL') {
                    Write-DeviceAutomationDiagnostics -Serial $survivor
                    throw 'Briefing participant-loss automation failed'
                }
                return $result -and $result.result -eq 'PASS'
            })) { throw 'Briefing participant loss did not settle' }
    if ($BriefingFailure -eq 'host') {
        if (-not (Start-DeviceGameAutomation -Serial $survivor -ScriptName 'test_coop_restore_load_error_new_game.jsonc')) {
            throw 'Could not start same-process new game after briefing loss'
        }
        if (-not (Wait-ForCondition -Description 'survivor starts a new game without restarting the app' -TimeoutSec 180 -PollMs 1000 -Condition {
                    $result = Get-DeviceAutomationResult -Serial $survivor
                    if ($result -and $result.result -eq 'FAIL') {
                        Write-DeviceAutomationDiagnostics -Serial $survivor
                        throw 'New game after briefing loss failed'
                    }
                    return $result -and $result.result -eq 'PASS'
                })) { throw 'Survivor could not start a new game' }
    }
    $retryPid = (Adb-Dev-Timeout -Serial $survivor -AdbArgs @('shell', 'pidof', "${PACKAGE}:game") -Seconds 5).Trim()
    if ($retryPid -ne $gamePid) { throw 'Briefing loss unexpectedly replaced the surviving game process' }
    return $true
}

function Invoke-RestoreFailureScenario {
    $loadError = $RestoreFailure -like 'load_*'
    $syncStalled = $RestoreFailure -eq 'sync_stalled'
    $loaderFailure = $loadError -or $syncStalled
    $failedLoader = if ($RestoreFailure -eq 'load_host') { $EMU1 } else { $EMU2 }
    foreach ($phase in @('seed', 'save', 'mutate', 'arm')) {
        $hostScript = if ($phase -eq 'arm') { 'test_coop_restore_loss_arm.jsonc' } else { "test_coop_countdown_save_$phase.jsonc" }
        $clientScript = if ($phase -eq 'save') { 'test_coop_countdown_save_idle.jsonc' } else { $hostScript }
        if ($phase -eq 'arm' -and $loadError) {
            if ($failedLoader -eq $EMU1) { $hostScript = 'test_coop_restore_load_error_arm.jsonc' } else { $clientScript = 'test_coop_restore_load_error_arm.jsonc' }
        }
        if ($phase -eq 'arm' -and $syncStalled) {
            $hostScript = $clientScript = 'test_coop_restore_sync_stall_arm.jsonc'
        }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $hostScript `
                    -SecondarySerial $EMU2 -SecondaryScript $clientScript `
                    -Description "Restore failure: $phase" -TimeoutSec 60)) { throw "Restore failure $phase failed" }
    }
    $gameDir = if ($Game -eq 'd1') { 'd1x-redux' } else { 'd2x-redux' }
    $missionKey = if ($Game -eq 'd1') { 'default' } else { 'd2' }
    $saveName = $CALLSIGN1.ToLowerInvariant()
    $save = "files/$gameDir/Players/save_sets/coop/$missionKey/$saveName.mg0"
    $saveHash = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('shell', 'run-as', $PACKAGE, 'sha256sum', $save) -Seconds 10
    if (-not $saveHash -or $saveHash -notmatch '^[a-f0-9]{64} ') { throw 'Missing manual save before loss test' }
    if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName 'test_coop_restore_loss_start.jsonc')) { throw 'Could not start restore' }
    if (-not (Wait-ForCondition -Description 'both peers frozen before failure' -TimeoutSec 180 -PollMs 500 -Condition {
                $script:lossHost = Get-GameIntrospection -Serial $EMU1
                $script:lossClient = Get-GameIntrospection -Serial $EMU2
                if (-not $script:lossHost -or -not $script:lossClient) { return $false }
                return ($loaderFailure -or ($script:lossHost.coop_restore.local_loaded -and $script:lossClient.coop_restore.local_loaded)) -and
                $script:lossHost.coop_restore.barrier_phase -eq 'loading' -and $script:lossClient.coop_restore.barrier_phase -eq 'loading' -and
                $script:lossHost.coop_restore.barrier_visit -eq 2 -and $script:lossClient.coop_restore.barrier_visit -eq 2 -and
                $script:lossHost.time_paused -and $script:lossClient.time_paused
            })) { throw 'Restore did not reach the loaded injection point' }
    $stalled = $RestoreFailure -eq 'stalled' -or $syncStalled
    $lost = if ($RestoreFailure -eq 'client') { $EMU2 } else { $EMU1 }
    $survivors = if ($stalled -or $loadError) { @($EMU1, $EMU2) } elseif ($RestoreFailure -eq 'client') { @($EMU1) } else { @($EMU2) }
    foreach ($entry in @(@('host', $script:lossHost), @('client', $script:lossClient))) {
        $stage = if ($loaderFailure) { 'before-error' } else { 'loaded' }
        $entry[1] | ConvertTo-Json -Depth 30 | Set-Content -Encoding utf8 (Join-Path $REPO_ROOT "temp/coop-restore-loss-$Game-$RestoreFailure-$($entry[0])-$stage.json")
    }
    if ($loadError) {
        Write-Status "Waiting for the native load failure on $failedLoader" 'Yellow'
    } elseif ($stalled) {
        Write-Status 'Leaving both peers connected until the restore deadline' 'Yellow'
    } else {
        Write-Status "Force-stopping $RestoreFailure after both peers loaded visit 2" 'Yellow'
        Adb-Dev-Timeout -Serial $lost -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) -Seconds 10 | Out-Null
    }
    $script:lossFinals = @{}
    if (-not (Wait-ForCondition -Description 'all survivors exit failed restore without resuming the mine' -TimeoutSec 90 -PollMs 1000 -Condition {
                $ready = $true
                foreach ($survivor in $survivors) {
                    $intro = Get-GameIntrospection -Serial $survivor
                    if (-not $intro) { $ready = $false; continue }
                    if ($intro.in_game -and -not $intro.time_paused) { throw "Survivor resumed gameplay during failed restore: $survivor" }
                    if ($intro.in_game -or $intro.coop_restore.transfer_busy -or $intro.coop_restore.barrier_phase -ne 'idle' -or
                        -not $intro.PSObject.Properties['menu'] -or $intro.menu.title -ne 'Co-op restore interrupted') { $ready = $false; continue }
                    $script:lossFinals[$survivor] = $intro
                }
                return $ready
            })) { throw 'A survivor remained stuck after restore failure' }
    $deadlineSeen = $false
    foreach ($survivor in $survivors) {
        $final = $script:lossFinals[$survivor]
        $reason = if ($RestoreFailure -eq 'host') { 'The host disconnected during restore' } else { 'A player disconnected during restore' }
        if ($loaderFailure) {
            $reason = if ($survivor -eq $failedLoader) { 'This player could not load the saved mine' } else { 'Another player could not finish restoring' }
            if ($syncStalled -and $survivor -eq $failedLoader) {
                $reason = 'A player did not finish restoring in time'
                $deadlineSeen = $true
            }
            if ($final.menu.subtitle -notlike "$reason*") { throw "Missing coordinated load-error reason on $survivor" }
        } elseif ($stalled) {
            $deadlineSeen = $deadlineSeen -or $final.menu.subtitle.StartsWith('A player did not finish restoring in time')
            if ($final.menu.subtitle -notmatch '^(A player did not finish restoring in time|Another player could not finish restoring|The host disconnected during restore|A player disconnected during restore)') {
                throw 'Unexpected connected-stall failure reason'
            }
        } elseif ($final.menu.subtitle -notlike "$reason*") { throw 'Missing restore failure reason' }
        if ($final.menu.subtitle -notlike '*Host or join a saved co-op game to continue*') { throw 'Missing recovery instructions' }
        $artifactCase = if ($stalled -or $loadError) { "$RestoreFailure-$survivor" } else { $RestoreFailure }
        $final | ConvertTo-Json -Depth 30 | Set-Content -Encoding utf8 (Join-Path $REPO_ROOT "temp/coop-restore-loss-$Game-$artifactCase-final.json")
        $nativeLog = Adb-Dev -Serial $survivor -AdbArgs @('logcat', '-d', '-s', 'DXX-DLOG:D', 'DXX:I', 'DXX-MP:I')
        $nativeLog | Set-Content -Encoding utf8 (Join-Path $REPO_ROOT "temp/coop-restore-failure-$Game-$artifactCase-native.log")
        $loadedPattern = if ($syncStalled) { '0' } elseif ($loadError) { if ($survivor -eq $failedLoader) { '0' } else { '[01]' } } else { '1' }
        if ($nativeLog -notmatch "restore barrier failed: visit=2 phase=1 local_loaded=$loadedPattern paused=1 clock_drift=0") { throw 'Missing frozen-clock barrier failure evidence' }
        if ($syncStalled) {
            if ($survivor -eq $failedLoader) {
                if ($nativeLog -notmatch 'restore test entered synchronous loader wait: visit=2 player=1' -or
                    $nativeLog -notmatch 'restore synchronous loader deadline expired: visit=2 local_loaded=0 paused=1') { throw 'Missing real synchronous-loader deadline evidence' }
            } elseif ($nativeLog -notmatch 'restore test holding host apply after peer acknowledgement: visit=2') { throw 'Missing delayed host application evidence' }
        }
        if ($nativeLog -match 'host migration: player|Host migration: notifying|Host migration: proxy') { throw 'Unfinished restore promoted a replacement host' }
        if ($stalled -and $nativeLog -match 'MPDIAG: timeout_check: player') { throw 'Connected stall was masked by a network timeout' }
    }
    if ($stalled -and -not $deadlineSeen) { throw 'Connected stall did not exercise the restore deadline' }
    if ($loaderFailure) {
        $gamePid = (Adb-Dev-Timeout -Serial $failedLoader -AdbArgs @('shell', 'pidof', "${PACKAGE}:game") -Seconds 5).Trim()
        if (-not $gamePid) { throw 'Failed loader process did not survive' }
        $files = Adb-Dev-Timeout -Serial $failedLoader -AdbArgs @('shell', 'run-as', $PACKAGE, 'ls', 'files/tombstones') -Seconds 5
        $reports = @($files -split '\r?\n' | ForEach-Object { $_.Trim() } | Where-Object { $_ -match "^crash_error_restore_.*_${gamePid}_\d+\.txt$" })
        if ($reports.Count -ne 1) { throw 'Expected one native load-error report' }
        $report = Adb-Dev-Timeout -Serial $failedLoader -AdbArgs @('shell', 'run-as', $PACKAGE, 'cat', "files/tombstones/$($reports[0])") -Seconds 5
        $report | Set-Content -Encoding utf8 (Join-Path $REPO_ROOT "temp/coop-restore-$Game-$RestoreFailure-report.txt")
        $failurePhase = if ($syncStalled) { 'co-op level synchronization interrupted' } else { 'injected core restore failure after hiding window' }
        if ($report -notmatch 'Save restore failure' -or $report -notmatch 'Visible window: 1' -or
            $report -notlike "*Failure phase: $failurePhase*") { throw 'Missing native load-error phase or visible recovery evidence' }
    }
    $afterHash = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('shell', 'run-as', $PACKAGE, 'sha256sum', $save) -Seconds 10
    if ($afterHash -ne $saveHash) { throw 'Failed restore changed the selected manual save' }
    foreach ($survivor in $survivors) {
        if (-not (Start-DeviceGameAutomation -Serial $survivor -ScriptName 'test_coop_restore_loss_dismiss.jsonc')) { throw 'Could not start failure dismissal' }
        if (-not (Wait-ForCondition -Description 'failure message persists until acknowledged and returns to menus' -TimeoutSec 30 -PollMs 500 -Condition {
                    $result = Get-DeviceAutomationResult -Serial $survivor
                    if ($result -and $result.result -eq 'FAIL') { throw 'Failure dismissal automation failed' }
                    return $result -and $result.result -eq 'PASS'
                })) { throw 'Could not dismiss restore failure' }
    }
    Write-Status "Restore failure verified: $RestoreFailure, all survivors left frozen visit 2, manual save unchanged" 'Green'
    if ($loaderFailure) {
        if (-not (Start-DeviceGameAutomation -Serial $failedLoader -ScriptName 'test_coop_restore_load_error_new_game.jsonc')) { throw 'Could not start same-process recovery check' }
        if (-not (Wait-ForCondition -Description 'failed loader can start a game without restarting the app' -TimeoutSec 180 -PollMs 1000 -Condition {
                    $result = Get-DeviceAutomationResult -Serial $failedLoader
                    if ($result -and $result.result -eq 'FAIL') { Write-DeviceAutomationDiagnostics -Serial $failedLoader; throw 'Same-process recovery failed' }
                    return $result -and $result.result -eq 'PASS'
                })) { throw 'Failed loader could not start a new game' }
        $retryPid = (Adb-Dev-Timeout -Serial $failedLoader -AdbArgs @('shell', 'pidof', "${PACKAGE}:game") -Seconds 5).Trim()
        if ($retryPid -ne $gamePid) { throw 'New game unexpectedly replaced the failed loader process' }
        $retryIntro = Get-GameIntrospection -Serial $failedLoader
        $retryIntro | ConvertTo-Json -Depth 30 | Set-Content -Encoding utf8 (Join-Path $REPO_ROOT "temp/coop-restore-$Game-$RestoreFailure-same-process.json")
    }
    if ($RestoreLossResume) {
        foreach ($serial in @($EMU1, $EMU2)) {
            if (-not (Start-SetupActivity -Serial $serial)) { throw 'Could not restart launcher after restore failure' }
            Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $PACKAGE, 'rm', '-f', 'files/introspect.json') -Seconds 5 | Out-Null
        }
        if (-not (Set-DeviceCoopRestoreSlot -Serial $EMU1 -Slot 0)) { throw 'Could not select preserved manual save' }
        Send-MpCommand -Serial $EMU1 -Command 'lan_launch' -Extras $hostExtras
        if (-not (Wait-ForCondition -Description 'recovery host lobby' -TimeoutSec 60 -PollMs 500 -Condition {
                    $intro = Get-GameIntrospection -Serial $EMU1
                    return $intro -and $intro.is_network -and (Get-IntroNumConnected -Intro $intro) -eq 1
                })) { throw 'Recovery host did not reach lobby' }
        Send-MpCommand -Serial $EMU2 -Command 'lan_launch' -Extras $joinExtras
        if (-not (Wait-ForCondition -Description 'cold recovery restores both players without briefing replay' -TimeoutSec 240 -PollMs 1000 -Condition {
                    $ready = $true
                    foreach ($serial in @($EMU1, $EMU2)) {
                        $intro = Get-GameIntrospection -Serial $serial
                        if (-not $intro -or -not $intro.in_game -or -not $intro.is_network -or (Get-IntroNumConnected -Intro $intro) -ne 2) { $ready = $false; continue }
                        if ($intro.coop_briefing.presentations_started -ne 0) { throw 'Cold recovery replayed a briefing' }
                        if ($intro.coop_restore.status -eq 'error') { throw 'Cold recovery restore failed' }
                        if ($intro.coop_restore.transfer_busy -or $intro.coop_restore.status -ne 'idle' -or $intro.time_paused -or
                            $intro.coop_restore.barrier_phase -ne 'done') { $ready = $false; continue }
                        $slot = if ($serial -eq $EMU1) { 0 } else { 1 }
                        if ($intro.current_level_num -ne 1 -or $intro.player.secondary_ammo[1] -ne 6 + $slot -or
                            $intro.coop_briefing.enabled -ne [bool]$Briefings -or -not $intro.coop_briefing.suppressed_for_restore) {
                            throw "Cold recovery lost inventory, world or presentation state on $serial"
                        }
                    }
                    return $ready
                })) { throw 'Cold recovery did not settle on both peers' }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_countdown_save_verify.jsonc' `
                    -SecondarySerial $EMU2 -SecondaryScript 'test_coop_countdown_save_verify.jsonc' `
                    -Description 'Recovered reactor, inventory and simulation clocks' -TimeoutSec 30)) { throw 'Recovered countdown failed verification' }
        if (-not (Wait-BidirectionalPdata -FirstSerial $EMU1 -FirstRemoteSlot 1 -SecondSerial $EMU2 -SecondRemoteSlot 0 `
                    -Description 'network updates after cold recovery')) { throw 'Cold recovery did not resume networking' }
        foreach ($serial in @($EMU1, $EMU2)) {
            $intro = Get-GameIntrospection -Serial $serial
            $intro | ConvertTo-Json -Depth 30 | Set-Content -Encoding utf8 (Join-Path $REPO_ROOT "temp/coop-restore-recovered-$Game-$RestoreFailure-$serial.json")
        }
        Write-Status 'Cold restore recovery verified on both peers' 'Green'
    }
    return $true
}

function Invoke-CoopLevelRestartScenario {
    foreach ($phase in @('remember', 'seed', 'mutate', 'restart', 'verify')) {
        $hostScript = @{
            seed = 'test_coop_countdown_save_seed.jsonc'
            remember = 'test_coop_level_restart_remember.jsonc'
            mutate = 'test_coop_level_restart_mutate.jsonc'
            restart = 'test_coop_level_restart_restart.jsonc'
            verify = 'test_coop_level_restart_verify.jsonc'
        }[$phase]
        $clientScript = if ($phase -eq 'restart') { 'test_coop_countdown_save_wait.jsonc' } elseif ($phase -eq 'remember') { 'test_coop_level_restart_remember_client.jsonc' } else { $hostScript }
        # Cover the 180-second restore wait plus request and verification steps
        $phaseTimeout = if ($phase -eq 'restart') { 210 } else { 180 }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $hostScript `
                    -SecondarySerial $EMU2 -SecondaryScript $clientScript `
                    -Description "Natural level checkpoint restart: $phase" -TimeoutSec $phaseTimeout)) { throw "Level restart $phase failed" }
    }
    return $true
}

function Invoke-GuidebotOwnershipScenario {
    $hostScript = "test_coop_guidebot_owner_host.jsonc"
    $joinScript = "test_coop_guidebot_owner_joiner.jsonc"

    Write-Status ""
    Write-Status "--- Guide-Bot ownership and route-intent scenario ---" "White"
    if (-not (Start-DeviceGameAutomation -Serial $EMU2 -ScriptName $joinScript)) {
        return $false
    }
    Start-Sleep -Milliseconds 250
    if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName $hostScript)) {
        return $false
    }

    $script:hostAutomationResult = $null
    $script:joinAutomationResult = $null
    $finished = Wait-ForCondition -Description "paired Guide-Bot automation" -TimeoutSec 120 -PollMs 1000 -Condition {
        $script:hostAutomationResult = Get-DeviceAutomationResult -Serial $EMU1
        $script:joinAutomationResult = Get-DeviceAutomationResult -Serial $EMU2
        $hostDone = $script:hostAutomationResult -and
        $script:hostAutomationResult.result -in @("PASS", "FAIL")
        $joinDone = $script:joinAutomationResult -and
        $script:joinAutomationResult.result -in @("PASS", "FAIL")
        return $hostDone -and $joinDone
    }
    if (-not $finished -or
        $script:hostAutomationResult.result -ne "PASS" -or
        $script:joinAutomationResult.result -ne "PASS") {
        Write-Status "FAIL: paired Guide-Bot automation did not pass" "Red"
        Write-DeviceAutomationDiagnostics -Serial $EMU1
        Write-DeviceAutomationDiagnostics -Serial $EMU2
        return $false
    }

    $hostIntro = Get-GameIntrospection -Serial $EMU1
    $joinIntro = Get-GameIntrospection -Serial $EMU2
    $hostGuidebot = Get-IntroGuidebot -Intro $hostIntro
    $joinGuidebot = Get-IntroGuidebot -Intro $joinIntro
    $stateMatches = $hostGuidebot -and $joinGuidebot -and
    [int]$hostGuidebot.owner_player -eq 1 -and
    [int]$joinGuidebot.owner_player -eq 1 -and
    [int]$hostGuidebot.owner_generation -eq [int]$joinGuidebot.owner_generation -and
    [int]$hostGuidebot.owner_generation -ge 3 -and
    -not [bool]$hostGuidebot.owner_is_local -and
    [bool]$joinGuidebot.owner_is_local -and
    -not [bool]$hostGuidebot.local_control_slot_matches -and
    [bool]$joinGuidebot.local_control_slot_matches -and
    $hostGuidebot.route_target_mode_name -eq "unexplored" -and
    $joinGuidebot.route_target_mode_name -eq "unexplored" -and
    [int]$joinGuidebot.unexplored_component_size -eq 1 -and
    [int]$joinGuidebot.unexplored_target_seg -eq 56
    if (-not $stateMatches) {
        Write-Status "FAIL: final Guide-Bot ownership state differs between peers" "Red"
        Write-DeviceAutomationDiagnostics -Serial $EMU1
        Write-DeviceAutomationDiagnostics -Serial $EMU2
        return $false
    }

    Write-Status "Guide-Bot owner synchronized to joiner at generation $($hostGuidebot.owner_generation)" "Green"
    Write-Status "Unexplored route intent synchronized back to host" "Green"

    Write-Status "Stopping the Guide-Bot owner to exercise disconnect adoption"
    Adb-Dev-Timeout -Serial $EMU2 -AdbArgs @(
        "shell", "am", "force-stop", $PACKAGE
    ) -Seconds 10 | Out-Null
    $script:adoptionIntro = $null
    $adopted = Wait-ForCondition -Description "host adopts Guide-Bot after owner disconnect" -TimeoutSec 35 -PollMs 1000 -Condition {
        $script:adoptionIntro = Get-GameIntrospection -Serial $EMU1
        $mp = Get-IntroMultiplayer -Intro $script:adoptionIntro
        $guidebot = Get-IntroGuidebot -Intro $script:adoptionIntro
        return $mp -and $guidebot -and
        [int]$mp.num_connected -eq 1 -and
        [int]$guidebot.owner_player -eq 0 -and
        [int]$guidebot.owner_generation -ge 4 -and
        [bool]$guidebot.owner_is_local -and
        [int]$guidebot.remote_owner -eq 0 -and
        [bool]$guidebot.local_control_slot_matches -and
        $guidebot.route_target_mode_name -eq "unexplored"
    }
    if (-not $adopted) {
        Write-Status "FAIL: host did not adopt Guide-Bot after owner disconnect" "Red"
        Write-DeviceAutomationDiagnostics -Serial $EMU1
        return $false
    }

    $adoptedGuidebot = Get-IntroGuidebot -Intro $script:adoptionIntro
    Write-Status "Host adopted Guide-Bot at generation $($adoptedGuidebot.owner_generation) with Unexplored intent preserved" "Green"

    $script:replannedIntro = $null
    $replanned = Wait-ForCondition -Description "new owner recomputes Unexplored route from local automap" -TimeoutSec 20 -PollMs 1000 -Condition {
        $script:replannedIntro = Get-GameIntrospection -Serial $EMU1
        $guidebot = Get-IntroGuidebot -Intro $script:replannedIntro
        return $guidebot -and
        [int]$guidebot.owner_player -eq 0 -and
        $guidebot.route_target_mode_name -eq "unexplored" -and
        [int]$guidebot.unexplored_component_size -gt 1 -and
        [int]$guidebot.unexplored_target_seg -ge 0 -and
        [bool]$guidebot.unexplored_direct_reachable -and
        [bool]$guidebot.route_goal_active -and
        $guidebot.route_goal_label -eq "Unexplored" -and
        [int]$guidebot.route_goal_objective_kind -eq 1000 -and
        [int]$guidebot.route_metadata_rescan_count -ge 1 -and
        [int]$guidebot.route_ignored_nonowner_key_change_count -ge 1
    }
    if (-not $replanned) {
        Write-Status "FAIL: adopted Guide-Bot did not recompute Unexplored from the host automap" "Red"
        return $false
    }
    $replannedGuidebot = Get-IntroGuidebot -Intro $script:replannedIntro
    Write-Status "Host recomputed a $($replannedGuidebot.unexplored_component_size)-segment unexplored component" "Green"
    return $true
}

function Invoke-GuidebotHostObserverScenario {
    $hostScript = "test_coop_guidebot_observer_host.jsonc"
    $joinScript = "test_coop_guidebot_observer_joiner.jsonc"

    Write-Status ""
    Write-Status "--- Guide-Bot observer-host exclusion scenario ---" "White"
    if (-not (Start-DeviceGameAutomation -Serial $EMU2 -ScriptName $joinScript)) {
        return $false
    }
    Start-Sleep -Milliseconds 250
    if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName $hostScript)) {
        return $false
    }

    $script:hostAutomationResult = $null
    $script:joinAutomationResult = $null
    $finished = Wait-ForCondition -Description "paired observer-host Guide-Bot automation" -TimeoutSec 90 -PollMs 1000 -Condition {
        $script:hostAutomationResult = Get-DeviceAutomationResult -Serial $EMU1
        $script:joinAutomationResult = Get-DeviceAutomationResult -Serial $EMU2
        $hostDone = $script:hostAutomationResult -and
        $script:hostAutomationResult.result -in @("PASS", "FAIL")
        $joinDone = $script:joinAutomationResult -and
        $script:joinAutomationResult.result -in @("PASS", "FAIL")
        return $hostDone -and $joinDone
    }
    if (-not $finished -or
        $script:hostAutomationResult.result -ne "PASS" -or
        $script:joinAutomationResult.result -ne "PASS") {
        Write-Status "FAIL: observer-host Guide-Bot automation did not pass" "Red"
        Write-DeviceAutomationDiagnostics -Serial $EMU1
        Write-DeviceAutomationDiagnostics -Serial $EMU2
        return $false
    }

    $hostIntro = Get-GameIntrospection -Serial $EMU1
    $joinIntro = Get-GameIntrospection -Serial $EMU2
    $hostMp = Get-IntroMultiplayer -Intro $hostIntro
    $joinMp = Get-IntroMultiplayer -Intro $joinIntro
    $hostGuidebot = Get-IntroGuidebot -Intro $hostIntro
    $joinGuidebot = Get-IntroGuidebot -Intro $joinIntro
    $stateMatches = $hostMp -and $joinMp -and $hostGuidebot -and $joinGuidebot -and
    [bool]$hostMp.host_is_observer -and [bool]$joinMp.host_is_observer -and
    [int]$hostGuidebot.owner_player -eq 1 -and
    [int]$joinGuidebot.owner_player -eq 1 -and
    [int]$hostGuidebot.owner_generation -eq [int]$joinGuidebot.owner_generation -and
    [int]$hostGuidebot.owner_generation -ge 1 -and
    -not [bool]$hostGuidebot.owner_is_local -and
    [bool]$joinGuidebot.owner_is_local -and
    -not [bool]$hostGuidebot.local_control_slot_matches -and
    [bool]$joinGuidebot.local_control_slot_matches
    if (-not $stateMatches) {
        Write-Status "FAIL: observer host was not excluded from Guide-Bot ownership" "Red"
        Write-DeviceAutomationDiagnostics -Serial $EMU1
        Write-DeviceAutomationDiagnostics -Serial $EMU2
        return $false
    }

    Write-Status "Observer host excluded; playing joiner owns Guide-Bot at generation $($joinGuidebot.owner_generation)" "Green"
    return $true
}

function Test-IntroHostRole {
    param(
        $Intro,
        [int]$LocalSlot,
        [int]$MasterSlot,
        [int]$ConnectedPlayers,
        [bool]$IsMaster
    )

    $mp = Get-IntroMultiplayer -Intro $Intro
    return $Intro -and $mp -and
    [bool]$Intro.in_game -and [bool]$Intro.is_network -and
    [int]$mp.network_status -eq 1 -and
    [int]$mp.my_player_num -eq $LocalSlot -and
    [int]$mp.master_player_num -eq $MasterSlot -and
    ([bool]$mp.i_am_master -eq $IsMaster) -and
    [int]$mp.num_connected -eq $ConnectedPlayers
}

function Get-IntroObjectOwnerSignature {
    param($Intro)

    $mp = Get-IntroMultiplayer -Intro $Intro
    if (-not $mp) {
        return $null
    }
    $ownerCounts = @($mp.synchronized_object_owner_counts) -join ','
    return "$($mp.synchronized_object_count)/$($mp.synchronized_unowned_object_count)/$ownerCounts"
}

function Wait-SoloMigratedHost {
    param(
        [string]$Serial,
        [int]$HostSlot,
        [int]$PreviousGuidebotGeneration,
        [bool]$CheckGuidebot,
        [string]$Description
    )

    $script:migrationSoloIntro = $null
    return Wait-ForCondition -Description $Description -TimeoutSec 40 -PollMs 1000 -Condition {
        $script:migrationSoloIntro = Get-GameIntrospection -Serial $Serial
        $mp = Get-IntroMultiplayer -Intro $script:migrationSoloIntro
        $guidebot = Get-IntroGuidebot -Intro $script:migrationSoloIntro
        if (-not (Test-IntroHostRole -Intro $script:migrationSoloIntro `
                    -LocalSlot $HostSlot -MasterSlot $HostSlot `
                    -ConnectedPlayers 1 -IsMaster $true)) {
            return $false
        }
        $ownedCount = 0
        foreach ($count in @($mp.synchronized_object_owner_counts)) {
            $ownedCount += [int]$count
        }
        $guidebotMatches = -not $CheckGuidebot
        if ($CheckGuidebot) {
            $guidebotMatches = $guidebot -and
            [int]$guidebot.owner_player -eq $HostSlot -and
            [int]$guidebot.owner_generation -gt $PreviousGuidebotGeneration -and
            [bool]$guidebot.owner_is_local -and
            [int]$guidebot.remote_owner -eq $HostSlot -and
            [bool]$guidebot.local_control_slot_matches
        }
        return $guidebotMatches -and
        [int]$mp.synchronized_object_count -gt 0 -and
        $ownedCount -eq 0 -and
        [int]$mp.synchronized_unowned_object_count -eq [int]$mp.synchronized_object_count
    }
}

function Start-MigratedPeerRejoin {
    param(
        [string]$JoiningSerial,
        [string]$HostSerial,
        [string]$Callsign
    )

    if (-not (Start-SetupActivity -Serial $JoiningSerial)) {
        Write-Status "FAIL: SetupActivity didn't restart on $JoiningSerial" "Red"
        return $false
    }
    $hostIp = Get-DeviceWlanIp -Serial $HostSerial
    if (-not $hostIp) {
        Write-Status "FAIL: could not get migrated host wlan0 IP from $HostSerial" "Red"
        return $false
    }
    Adb-Dev-Timeout -Serial $JoiningSerial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "rm", "-f", "files/introspect.json"
    ) -Seconds 10 | Out-Null
    Write-Status "Rejoining $JoiningSerial to migrated host ${hostIp}:$MIGRATED_HOST_PORT"
    $rejoinExtras = @(
        "--es", "game", $Game,
        "--es", "mp_mode", "join",
        "--es", "mode", $MODE,
        "--ei", "max_players", "2",
        "--ei", "level_num", "1",
        "--ei", "difficulty", "1",
        "--es", "callsign", $Callsign,
        "--es", "host_addr", $hostIp,
        "--ei", "host_port", $MIGRATED_HOST_PORT.ToString()
    )
    if ($MISSION) {
        $rejoinExtras += @("--es", "mission", $MISSION)
    }
    Send-MpCommand -Serial $JoiningSerial -Command "lan_launch" -Extras $rejoinExtras
    if (-not (Wait-ForCondition -Description "Rejoining game process on $JoiningSerial" `
                -TimeoutSec 30 -PollMs 500 -Condition {
                $gPid = Adb-Dev-Timeout -Serial $JoiningSerial -AdbArgs @(
                    "shell", "pidof", "${PACKAGE}:game"
                ) -Seconds 5
                return ($gPid -and $gPid -match '^\d+')
            })) {
        Write-Status "FAIL: Rejoining game process did not start on $JoiningSerial" "Red"
        return $false
    }
    return $true
}

function Wait-MigratedPairSync {
    param(
        [string]$HostSerial,
        [int]$HostSlot,
        [string]$HostCallsign,
        [string]$JoinSerial,
        [int]$JoinSlot,
        [string]$JoinCallsign,
        [bool]$CheckGuidebot,
        [string]$Description
    )

    $script:migratedHostIntro = $null
    $script:migratedJoinIntro = $null
    return Wait-ForCondition -Description $Description -TimeoutSec $TimeoutSeconds -PollMs 1500 -Condition {
        $script:migratedHostIntro = Get-GameIntrospection -Serial $HostSerial
        $script:migratedJoinIntro = Get-GameIntrospection -Serial $JoinSerial
        if (-not (Test-IntroHostRole -Intro $script:migratedHostIntro `
                    -LocalSlot $HostSlot -MasterSlot $HostSlot `
                    -ConnectedPlayers 2 -IsMaster $true) -or
            -not (Test-IntroHostRole -Intro $script:migratedJoinIntro `
                    -LocalSlot $JoinSlot -MasterSlot $HostSlot `
                    -ConnectedPlayers 2 -IsMaster $false)) {
            return $false
        }
        $hostMp = Get-IntroMultiplayer -Intro $script:migratedHostIntro
        $joinMp = Get-IntroMultiplayer -Intro $script:migratedJoinIntro
        $hostGuidebot = Get-IntroGuidebot -Intro $script:migratedHostIntro
        $joinGuidebot = Get-IntroGuidebot -Intro $script:migratedJoinIntro
        $hostObjects = Get-IntroObjectOwnerSignature -Intro $script:migratedHostIntro
        $joinObjects = Get-IntroObjectOwnerSignature -Intro $script:migratedJoinIntro
        $guidebotMatches = -not $CheckGuidebot
        if ($CheckGuidebot) {
            $guidebotMatches = $hostGuidebot -and $joinGuidebot -and
            [int]$hostGuidebot.owner_player -eq $HostSlot -and
            [int]$joinGuidebot.owner_player -eq $HostSlot -and
            [int]$hostGuidebot.owner_generation -eq [int]$joinGuidebot.owner_generation -and
            [bool]$hostGuidebot.owner_is_local -and
            -not [bool]$joinGuidebot.owner_is_local -and
            [int]$hostGuidebot.remote_owner -eq $HostSlot -and
            [int]$joinGuidebot.remote_owner -eq $HostSlot -and
            [int]$hostGuidebot.segment -eq [int]$joinGuidebot.segment
        }
        return $guidebotMatches -and
        [int]$hostMp.players[$HostSlot].slot -eq $HostSlot -and
        $hostMp.players[$HostSlot].callsign -eq $HostCallsign -and
        $hostMp.players[$JoinSlot].callsign -eq $JoinCallsign -and
        [int]$joinMp.players[$HostSlot].slot -eq $HostSlot -and
        $joinMp.players[$HostSlot].callsign -eq $HostCallsign -and
        $joinMp.players[$JoinSlot].callsign -eq $JoinCallsign -and
        [int]$hostMp.synchronized_object_count -gt 0 -and
        $hostObjects -eq $joinObjects
    }
}

function Wait-BidirectionalPdata {
    param(
        [string]$FirstSerial,
        [int]$FirstRemoteSlot,
        [string]$SecondSerial,
        [int]$SecondRemoteSlot,
        [string]$Description
    )

    $firstIntro = Get-GameIntrospection -Serial $FirstSerial
    $secondIntro = Get-GameIntrospection -Serial $SecondSerial
    $firstStart = Get-IntroPdataSequence -Intro $firstIntro -PlayerSlot $FirstRemoteSlot
    $secondStart = Get-IntroPdataSequence -Intro $secondIntro -PlayerSlot $SecondRemoteSlot
    if ($null -eq $firstStart -or $null -eq $secondStart) {
        Write-Status "FAIL: PDATA sequence counters unavailable" "Red"
        return $false
    }
    return Wait-ForCondition -Description $Description -TimeoutSec 15 -PollMs 1000 -Condition {
        $firstNow = Get-GameIntrospection -Serial $FirstSerial
        $secondNow = Get-GameIntrospection -Serial $SecondSerial
        $firstMp = Get-IntroMultiplayer -Intro $firstNow
        $secondMp = Get-IntroMultiplayer -Intro $secondNow
        $firstSequence = Get-IntroPdataSequence -Intro $firstNow -PlayerSlot $FirstRemoteSlot
        $secondSequence = Get-IntroPdataSequence -Intro $secondNow -PlayerSlot $SecondRemoteSlot
        return $firstMp -and $secondMp -and
        [int]$firstMp.num_connected -eq 2 -and [int]$secondMp.num_connected -eq 2 -and
        $null -ne $firstSequence -and $null -ne $secondSequence -and
        $firstSequence -ne $firstStart -and $secondSequence -ne $secondStart
    }
}

function Assert-GuidebotRoutingSelection {
    param([string[]]$Serials = @($EMU1, $EMU2))
    if (-not $GuidebotRoutingMode) { return }
    foreach ($serial in $Serials) {
        $intro = Get-GameIntrospection -Serial $serial
        $guidebot = Get-IntroGuidebot -Intro $intro
        if (-not $guidebot -or $guidebot.routing_mode_name -ne $GuidebotRoutingMode) {
            throw "Guidebot routing on $serial did not retain host selection $GuidebotRoutingMode"
        }
    }
    Write-Status "$($Serials -join ', ') retain $GuidebotRoutingMode routing" 'Green'
}

function Invoke-HostMigrationScenario {
    $checkGuidebot = $Game -eq "d2"
    $initialGuidebotGeneration = -1

    Write-Status ""
    Write-Status "--- Host migration, rejoin, and second-swap scenario ---" "White"
    if ($checkGuidebot) {
        if (-not (Invoke-PairedGameAutomation `
                    -PrimarySerial $EMU1 `
                    -PrimaryScript "test_coop_host_migration_prepare_host.jsonc" `
                    -SecondarySerial $EMU2 `
                    -SecondaryScript "test_coop_host_migration_prepare_joiner.jsonc" `
                    -Description "paired host-migration preparation" `
                    -TimeoutSec $TimeoutSeconds)) {
            return $false
        }
    }

    $initialHost = Get-GameIntrospection -Serial $EMU1
    $initialJoin = Get-GameIntrospection -Serial $EMU2
    $initialGuidebot = Get-IntroGuidebot -Intro $initialHost
    if (-not (Test-IntroHostRole -Intro $initialHost -LocalSlot 0 -MasterSlot 0 `
                -ConnectedPlayers 2 -IsMaster $true) -or
        -not (Test-IntroHostRole -Intro $initialJoin -LocalSlot 1 -MasterSlot 0 `
                -ConnectedPlayers 2 -IsMaster $false) -or
        ($checkGuidebot -and -not $initialGuidebot) -or
        (Get-IntroObjectOwnerSignature -Intro $initialHost) -ne
        (Get-IntroObjectOwnerSignature -Intro $initialJoin)) {
        Write-Status "FAIL: initial peers do not agree on host/object state" "Red"
        return $false
    }
    if ($checkGuidebot) {
        $initialGuidebotGeneration = [int]$initialGuidebot.owner_generation
    }
    if (-not (Wait-BidirectionalPdata -FirstSerial $EMU1 -FirstRemoteSlot 1 `
                -SecondSerial $EMU2 -SecondRemoteSlot 0 `
                -Description "initial bidirectional PDATA")) {
        return $false
    }

    Write-Status "Stopping original host $EMU1"
    Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @(
        "shell", "am", "force-stop", $PACKAGE
    ) -Seconds 10 | Out-Null
    if (-not (Wait-SoloMigratedHost -Serial $EMU2 -HostSlot 1 `
                -PreviousGuidebotGeneration $initialGuidebotGeneration `
                -CheckGuidebot $checkGuidebot `
                -Description "slot 1 becomes host and resets object ownership")) {
        return $false
    }
    Write-Status "Slot 1 became host and reset object ownership$(if ($checkGuidebot) { ', with Guide-Bot authority' })" "Green"
    Assert-GuidebotRoutingSelection -Serials @($EMU2)

    if (-not (Start-MigratedPeerRejoin -JoiningSerial $EMU1 -HostSerial $EMU2 -Callsign $CALLSIGN1)) {
        return $false
    }
    if (-not (Wait-MigratedPairSync -HostSerial $EMU2 -HostSlot 1 -HostCallsign $CALLSIGN2 `
                -JoinSerial $EMU1 -JoinSlot 0 -JoinCallsign $CALLSIGN1 `
                -CheckGuidebot $checkGuidebot `
                -Description "former host rejoins slot-1 host with matching objects")) {
        return $false
    }
    if (-not (Wait-BidirectionalPdata -FirstSerial $EMU2 -FirstRemoteSlot 0 `
                -SecondSerial $EMU1 -SecondRemoteSlot 1 `
                -Description "PDATA after first host migration")) {
        return $false
    }
    Write-Status "Former host rejoined with object parity and sustained PDATA" "Green"
    Assert-GuidebotRoutingSelection

    $firstMigrationGuidebotGeneration = if ($checkGuidebot) {
        [int](Get-IntroGuidebot -Intro $script:migratedHostIntro).owner_generation
    } else {
        -1
    }
    Write-Status "Stopping migrated host $EMU2 for the second swap"
    Adb-Dev-Timeout -Serial $EMU2 -AdbArgs @(
        "shell", "am", "force-stop", $PACKAGE
    ) -Seconds 10 | Out-Null
    if (-not (Wait-SoloMigratedHost -Serial $EMU1 -HostSlot 0 `
                -PreviousGuidebotGeneration $firstMigrationGuidebotGeneration `
                -CheckGuidebot $checkGuidebot `
                -Description "slot 0 regains host authority")) {
        return $false
    }
    Write-Status "Slot 0 regained host authority$(if ($checkGuidebot) { ' and the Guide-Bot' })" "Green"
    Assert-GuidebotRoutingSelection -Serials @($EMU1)

    if (-not (Start-MigratedPeerRejoin -JoiningSerial $EMU2 -HostSerial $EMU1 -Callsign $CALLSIGN2)) {
        return $false
    }
    if (-not (Wait-MigratedPairSync -HostSerial $EMU1 -HostSlot 0 -HostCallsign $CALLSIGN1 `
                -JoinSerial $EMU2 -JoinSlot 1 -JoinCallsign $CALLSIGN2 `
                -CheckGuidebot $checkGuidebot `
                -Description "second host rejoins with matching objects")) {
        return $false
    }
    if (-not (Wait-BidirectionalPdata -FirstSerial $EMU1 -FirstRemoteSlot 1 `
                -SecondSerial $EMU2 -SecondRemoteSlot 0 `
                -Description "PDATA after second host migration")) {
        return $false
    }
    Write-Status "Second host swap, rejoin, object parity, and sustained PDATA passed" "Green"
    return $true
}

function Assert-CoopWorldVisit {
    param([uint64]$Expected)
    if (-not (Wait-ForCondition -Description "Both peers use world visit $Expected" -TimeoutSec 30 -PollMs 500 -Condition {
                foreach ($serial in @($EMU1, $EMU2)) {
                    $intro = Get-GameIntrospection -Serial $serial
                    if (-not $intro -or -not $intro.PSObject.Properties['coop_world_visit'] -or
                        $intro.coop_world_visit.active -ne $Expected -or
                        $intro.coop_world_visit.reserved -lt $Expected) { return $false }
                }
                return $true
            })) { throw "World visit did not advance consistently to $Expected" }
}

function Assert-CoopGameplayFences {
    if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_mdata_fence_arm.jsonc" `
                -SecondarySerial $EMU2 -SecondaryScript "test_coop_mdata_fence_arm.jsonc" `
                -Description "Arm released-world gameplay probes on both peers" -TimeoutSec 30)) {
        throw "Could not arm gameplay probes on both peers"
    }
    if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_mdata_fence.jsonc" `
                -SecondarySerial $EMU2 -SecondaryScript "test_coop_mdata_fence.jsonc" `
                -Description "Both peers receive and reject old gameplay and end-level packets" -TimeoutSec 30)) {
        throw "Released-world gameplay packet fences failed"
    }
    # Keep sending until both receivers have passed, even if one starts later
    if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_packet_fences_verify.jsonc" `
                -SecondarySerial $EMU2 -SecondaryScript "test_coop_packet_fences_verify.jsonc" `
                -Description "Stop packet probes after both peers are ready" -TimeoutSec 30)) {
        throw "Released-world packet probe verification failed"
    }
    # Preserve native evidence before a cold-restore phase clears logcat
    foreach ($serial in @($EMU1, $EMU2)) {
        $wireOutput = Adb-Dev -Serial $serial -AdbArgs @("logcat", "-d", "-s", "DXX")
        $wireLines = if ($wireOutput) { $wireOutput -split '\r?\n' } else { @() }
        foreach ($kind in @("MDATA", "PDATA", "ENDLEVEL")) {
            $evidence = @($wireLines | Where-Object { $_ -match "Android $kind fence:" } | Select-Object -Last 1)
            if ($evidence.Count -ne 1) { throw "Missing $kind fence evidence on $serial" }
            Write-Status "  ${serial}: $($evidence[0])"
        }
        if ($serial -eq $EMU2) {
            $metadataEvidence = @($wireLines | Where-Object {
                    $_ -match 'Android game info fence: visit=\d+ rejected=10 preserved=1'
                } | Select-Object -Last 1)
            if ($metadataEvidence.Count -ne 1) { throw "Missing game info fence evidence on $serial" }
            Write-Status "  ${serial}: $($metadataEvidence[0])"
        }
    }
}

function Get-GameUiIntrospection {
    param([string]$Serial)
    $requestId = [guid]::NewGuid().ToString('N')
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        'shell', 'am', 'broadcast', '-a', 'com.dxxredux.INTROSPECT', '--es', 'request_id', $requestId
    ) -Seconds 10 | Out-Null
    $raw = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        'shell', 'run-as', $PACKAGE, 'cat', 'files/introspect_ui.json'
    ) -Seconds 5
    try {
        $snapshot = $raw | ConvertFrom-Json
        if ($snapshot.request_id -eq $requestId) { return $snapshot }
    } catch { }
    return $null
}

function Assert-PostTransitionAndroidControls {
    foreach ($serial in @($EMU1, $EMU2)) {
        if (-not (Wait-ForCondition -Description "Android touch overlay active on $serial" -TimeoutSec 15 -PollMs 500 -Condition {
                    $ui = Get-GameUiIntrospection -Serial $serial
                    return $ui -and $ui.touch_overlay_active -and $ui.touch_overlay_shown -and
                    $ui.touch_overlay_attached -and -not $ui.controller_menu_open -and -not $ui.admin_tray_open
                })) { throw "Post-transition touch overlay is unavailable on $serial" }
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'input', 'keyevent', '4') -Seconds 10 | Out-Null
        if (-not (Wait-ForCondition -Description "Android Back opens the game menu on $serial" -TimeoutSec 15 -PollMs 500 -Condition {
                    $intro = Get-GameIntrospection -Serial $serial
                    return $intro -and $intro.current_level_num -eq 2 -and -not $intro.game_window_is_front -and
                    $intro.menu -and $intro.menu.type -eq 'newmenu'
                })) { throw "Android Back did not open the post-transition game menu on $serial" }
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'input', 'keyevent', '4') -Seconds 10 | Out-Null
        if (-not (Wait-ForCondition -Description "Android Back returns to the playable mine on $serial" -TimeoutSec 15 -PollMs 500 -Condition {
                    $intro = Get-GameIntrospection -Serial $serial
                    $ui = Get-GameUiIntrospection -Serial $serial
                    return $intro -and $intro.current_level_num -eq 2 -and $intro.game_window_is_front -and
                    -not $intro.time_paused -and (Get-IntroNumConnected -Intro $intro) -eq 2 -and
                    $ui -and $ui.touch_overlay_active -and $ui.touch_overlay_shown -and $ui.touch_overlay_attached
                })) { throw "Android Back did not restore post-transition gameplay on $serial" }
    }
}

function Invoke-PairedGameAutomation {
    param(
        [string]$PrimarySerial,
        [string]$PrimaryScript,
        [string]$SecondarySerial,
        [string]$SecondaryScript,
        [string]$Description,
        [int]$TimeoutSec = 60,
        [switch]$IndependentEndgame
    )

    if (-not (Start-DeviceGameAutomation -Serial $SecondarySerial -ScriptName $SecondaryScript)) {
        return $false
    }
    Start-Sleep -Milliseconds 250
    if (-not (Start-DeviceGameAutomation -Serial $PrimarySerial -ScriptName $PrimaryScript)) {
        return $false
    }

    $script:independentEndgameSeen = $false
    $script:primaryAutomationResult = $null
    $script:secondaryAutomationResult = $null
    $finished = Wait-ForCondition -Description $Description -TimeoutSec $TimeoutSec -PollMs 1000 -Condition {
        $script:primaryAutomationResult = Get-DeviceAutomationResult -Serial $PrimarySerial
        $script:secondaryAutomationResult = Get-DeviceAutomationResult -Serial $SecondarySerial
        $primaryDone = $script:primaryAutomationResult -and
        $script:primaryAutomationResult.result -in @("PASS", "FAIL")
        $secondaryDone = $script:secondaryAutomationResult -and
        $script:secondaryAutomationResult.result -in @("PASS", "FAIL")
        if ($IndependentEndgame -and $primaryDone -and -not $secondaryDone -and -not $script:independentEndgameSeen) {
            $viewer = Get-GameIntrospection -Serial $SecondarySerial
            if ($viewer -and $viewer.coop_endgame.active -and $viewer.coop_endgame.released -and
                ($viewer.screen_advance_kind -in @('movie', 'briefing') -or $viewer.menu.type -eq 'credits') -and
                -not $viewer.coop_briefing.active -and
                @($viewer.multiplayer.players | Where-Object { -not $_.is_me -and $_.connected -eq 0 }).Count -gt 0) {
                $script:independentEndgameSeen = $true
                $evidence = @{ viewer = $SecondarySerial; departed = $PrimarySerial; ending = $viewer.coop_endgame;
                    screen = $viewer.screen_advance_kind; players = $viewer.multiplayer.players; paused = $viewer.time_paused
                }
                $evidence | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 -LiteralPath (Join-Path $REPO_ROOT "temp/coop-endgame-independent-$Game-$PrimarySerial.json")
                Write-Status "Verified $SecondarySerial still viewing after $PrimarySerial left" "Green"
            }
        }
        return $primaryDone -and $secondaryDone
    }
    if (-not $finished -or
        $script:primaryAutomationResult.result -ne "PASS" -or
        $script:secondaryAutomationResult.result -ne "PASS") {
        Write-Status "FAIL: $Description did not pass" "Red"
        Write-DeviceAutomationDiagnostics -Serial $PrimarySerial
        Write-DeviceAutomationDiagnostics -Serial $SecondarySerial
        return $false
    }
    if ($IndependentEndgame -and -not $script:independentEndgameSeen) {
        Write-Status "FAIL: did not observe local ending content after the other participant left" "Red"
        return $false
    }
    return $true
}

function Invoke-CoopDeathScenario {
    foreach ($dyingSerial in @($EMU2, $EMU1)) {
        $survivorSerial = if ($dyingSerial -eq $EMU1) { $EMU2 } else { $EMU1 }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_death_prepare.jsonc" `
                    -SecondarySerial $EMU2 -SecondaryScript "test_coop_death_prepare.jsonc" `
                    -Description "Record both ships before $dyingSerial dies")) { return $false }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $dyingSerial -PrimaryScript "test_coop_death.jsonc" `
                    -SecondarySerial $survivorSerial -SecondaryScript "test_coop_death_survivor.jsonc" `
                    -Description "Respawn $dyingSerial while the other player keeps playing")) { return $false }
    }
    return $true
}

function Test-DeviceCoopSaveSlot {
    param([string]$Serial, [int]$Slot, [string]$Callsign)

    $saveName = if ($Callsign) { $Callsign.ToLowerInvariant() } else { "coopsave" }
    $path = "files/d2x-redux/Players/save_sets/coop/d2/$saveName.mg$Slot"
    $found = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "ls", $path
    ) -Seconds 5
    return $found -and $found -match [regex]::Escape("$saveName.mg$Slot")
}

function Get-DeviceLatestCoopAutosaveSlot {
    param([string]$Serial)

    $gameDir = if ($Game -eq "d1") { "d1x-redux" } else { "d2x-redux" }
    $historyJson = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cat",
        "files/$gameDir/coop_autosave_history.json"
    ) -Seconds 5
    if (-not $historyJson -or $historyJson -notmatch '^\s*\[') {
        return -1
    }
    try {
        $history = @(ConvertFrom-CompatibleJsonItems -Json $historyJson)
        if ($history.Count -gt 0) {
            return [int]$history[0].slot
        }
    } catch {
        return -1
    }
    return -1
}

function Set-DeviceCoopRestoreSlot {
    param([string]$Serial, [int]$Slot)

    $gameDir = if ($Game -eq "d1") { "d1x-redux" } else { "d2x-redux" }
    $localPath = Join-Path $REPO_ROOT "temp\coop_restore_slot.txt"
    [System.IO.File]::WriteAllText(
        $localPath,
        $Slot.ToString(),
        [System.Text.UTF8Encoding]::new($false)
    )
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "push", $localPath, "/data/local/tmp/coop_restore_slot.txt"
    ) -Seconds 30 | Out-Null
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "mkdir", "-p", "files/$gameDir"
    ) -Seconds 5 | Out-Null
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cp", "/data/local/tmp/coop_restore_slot.txt",
        "files/$gameDir/coop_restore_slot.txt"
    ) -Seconds 5 | Out-Null
    $actual = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cat", "files/$gameDir/coop_restore_slot.txt"
    ) -Seconds 5
    return $actual -and $actual.Trim() -eq $Slot.ToString()
}

function Invoke-SavedLateJoinScenario {
    Write-Status "--- Saved client joins after host restores alone ---" "White"
    if (-not (Start-DeviceGameAutomation -Serial $EMU2 -ScriptName "test_coop_late_join_seed.jsonc")) { return $false }
    $geared = Wait-ForCondition -Description "host receives client inventory before save" -TimeoutSec 20 -PollMs 500 -Condition {
        $intro = Get-GameIntrospection -Serial $EMU1
        if (-not $intro) { return $false }
        $peer = @($intro.multiplayer.players | Where-Object { $_.callsign -eq $CALLSIGN2 })
        return $peer.Count -eq 1 -and $peer[0].primary_flags -eq 9 -and $peer[0].homing_ammo -eq 6
    }
    if (-not $geared) { return $false }
    if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName "test_coop_late_join_save.jsonc")) { return $false }
    $saved = Wait-ForCondition -Description "coop save completes" -TimeoutSec 20 -PollMs 500 -Condition {
        $result = Get-DeviceAutomationResult -Serial $EMU1
        return $result -and $result.result -eq "PASS"
    }
    if (-not $saved) { return $false }
    $slot = Get-DeviceLatestCoopAutosaveSlot -Serial $EMU1
    if ($slot -lt 0) { return $false }
    foreach ($serial in @($EMU1, $EMU2)) {
        Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "am", "force-stop", $PACKAGE) -Seconds 10 | Out-Null
        Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "run-as", $PACKAGE, "rm", "-f", "files/introspect.json") -Seconds 5 | Out-Null
    }
    if (-not (Start-SetupActivity -Serial $EMU1)) { return $false }
    if (-not (Set-DeviceCoopRestoreSlot -Serial $EMU1 -Slot $slot)) { return $false }
    Send-MpCommand -Serial $EMU1 -Command "lan_launch" -Extras $hostExtras
    $lobby = Wait-ForCondition -Description "host enters empty lobby" -TimeoutSec 30 -PollMs 500 -Condition {
        $intro = Get-GameIntrospection -Serial $EMU1
        return $intro -and $intro.is_network -and (Get-IntroNumConnected -Intro $intro) -eq 1
    }
    if (-not $lobby -or -not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName "test_coop_late_join_start.jsonc")) { return $false }
    $restored = Wait-ForCondition -Description "host restores save alone" -TimeoutSec 60 -PollMs 1000 -Condition {
        $intro = Get-GameIntrospection -Serial $EMU1
        return $intro -and $intro.in_game -and $intro.coop_restore.status -eq "idle" -and
        (Get-IntroNumConnected -Intro $intro) -eq 1
    }
    if (-not $restored -or -not (Start-SetupActivity -Serial $EMU2)) { return $false }
    Send-MpCommand -Serial $EMU2 -Command "lan_launch" -Extras $joinExtras
    $recovered = Wait-ForCondition -Description "late join restores saved plasma and six homing missiles" -TimeoutSec 60 -PollMs 1000 -Condition {
        $intro = Get-GameIntrospection -Serial $EMU2
        if ($intro -and $intro.in_game -and (Get-IntroNumConnected -Intro $intro) -eq 2) {
            $localPlayer = @($intro.multiplayer.players | Where-Object { $_.is_me })[0]
            return $localPlayer.primary_flags -eq 9 -and $localPlayer.homing_ammo -eq 6 -and $intro.player.laser_level -eq 2
        }
        Start-DeviceGameAutomation -Serial $EMU1 -ScriptName "test_coop_late_join_accept.jsonc" | Out-Null
        return $false
    }
    if (-not $recovered) {
        Write-DeviceAutomationDiagnostics -Serial $EMU1
        Write-DeviceAutomationDiagnostics -Serial $EMU2
    }
    return $recovered
}

function Invoke-RestoreResilienceScenario {
    Write-Status "--- Missing gear restore, report publication, and clean round trip ---" "White"
    $gameDir = if ($Game -eq "d1") { "d1x-redux" } else { "d2x-redux" }
    $missionKey = if ($Game -eq "d1") { "default" } else { "d2" }
    $fixtureTool = Join-Path $REPO_ROOT "build$Game/tests/coop_restore_fixture.exe"
    if (-not (Test-Path -LiteralPath $fixtureTool)) { throw "Build coop_restore_fixture for $Game first" }
    if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_restore_resilience_seed.jsonc" `
                -SecondarySerial $EMU2 -SecondaryScript "test_coop_late_join_seed.jsonc" -Description "known inventory before restore")) { return $false }
    for ($cycle = 1; $cycle -le 3; $cycle++) {
        if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName "test_coop_late_join_save.jsonc")) { return $false }
        if (-not (Wait-ForCondition -Description "resilience save completes" -TimeoutSec 30 -PollMs 500 -Condition {
                    $result = Get-DeviceAutomationResult -Serial $EMU1
                    return $result -and $result.result -eq "PASS"
                })) { return $false }
        $slot = Get-DeviceLatestCoopAutosaveSlot -Serial $EMU1
        if ($slot -lt 0) { return $false }
        $save = "files/$gameDir/Players/save_sets/coop/$missionKey/coopsave.mg$slot"
        foreach ($serial in @($EMU1, $EMU2)) {
            Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "am", "force-stop", $PACKAGE) -Seconds 10 | Out-Null
        }
        if ($cycle -le 2) {
            $original = Join-Path $REPO_ROOT "temp/coop-resilience-original.mg$slot"
            $modified = Join-Path $REPO_ROOT "temp/coop-resilience-missing.mg$slot"
            # PowerShell 7.4+ preserves bytes when redirecting native stdout
            & $ADB -s $EMU1 exec-out run-as $PACKAGE cat $save > $original
            if ($LASTEXITCODE -ne 0) { throw "Could not pull fixture source" }
            $fixtureArgs = @($original, $modified)
            if ($cycle -eq 2) { $fixtureArgs += "bad_counts" }
            & $fixtureTool @fixtureArgs | ForEach-Object { Write-Status $_ }
            if ($LASTEXITCODE -ne 0) { throw "Could not create missing-gear fixture" }
            Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("push", $modified, "/data/local/tmp/coop-resilience.mg") -Seconds 15 | Out-Null
            Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "run-as", $PACKAGE, "cp", "/data/local/tmp/coop-resilience.mg", $save) -Seconds 10 | Out-Null
        }
        foreach ($serial in @($EMU1, $EMU2)) {
            if (-not (Start-SetupActivity -Serial $serial)) { return $false }
            Send-MpCommand -Serial $serial -Command "set_callsign" -Extras @("--es", "callsign", $(if ($serial -eq $EMU1) { $CALLSIGN1 } else { $CALLSIGN2 }))
            Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "run-as", $PACKAGE, "rm", "-f", "files/introspect.json") -Seconds 5 | Out-Null
        }
        if (-not (Set-DeviceCoopRestoreSlot -Serial $EMU1 -Slot $slot)) { return $false }
        Send-MpCommand -Serial $EMU1 -Command "lan_launch" -Extras $hostExtras
        if (-not (Wait-ForCondition -Description "fresh restore host lobby" -TimeoutSec 30 -PollMs 500 -Condition {
                    $intro = Get-GameIntrospection -Serial $EMU1
                    return $intro -and $intro.is_network -and (Get-IntroNumConnected -Intro $intro) -eq 1
                })) { return $false }
        Send-MpCommand -Serial $EMU2 -Command "lan_launch" -Extras $joinExtras
        if (-not (Wait-ForCondition -Description "both fresh engines accept automation" -TimeoutSec 60 -PollMs 1000 -Condition {
                    $hostIntro = Get-GameIntrospection -Serial $EMU1
                    $clientIntro = Get-GameIntrospection -Serial $EMU2
                    return $hostIntro -and $clientIntro -and $hostIntro.in_game -and $clientIntro.in_game -and
                    (Get-IntroNumConnected -Intro $hostIntro) -eq 2 -and (Get-IntroNumConnected -Intro $clientIntro) -eq 2
                })) { return $false }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_restore_resilience_verify.jsonc" `
                    -SecondarySerial $EMU2 -SecondaryScript "test_coop_restore_resilience_verify.jsonc" `
                    -Description "two-peer restore with unchanged inventory (cycle $cycle)" -TimeoutSec 90)) { return $false }
        foreach ($serial in @($EMU1, $EMU2)) {
            $pidNow = (Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "pidof", "${PACKAGE}:game") -Seconds 5).Trim()
            if (-not $pidNow) { throw "Game process exited after restore" }
            $intro = Get-GameIntrospection -Serial $serial
            $localPlayer = @($intro.multiplayer.players | Where-Object { $_.is_me })[0]
            if ($localPlayer.homing_ammo -ne 6) { throw "Restore lost valid inventory or refunded discarded gear" }
            $markers = Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "run-as", $PACKAGE, "ls", "files/tombstones") -Seconds 5
            $marker = @($markers -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -match "^engine_pending_.*_${pidNow}\.txt$" })
            if ($marker.Count -ne 1) { throw "Missing durable session marker" }
            $markerBody = Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/tombstones/$($marker[0].Trim())") -Seconds 5
            if ($markerBody -notmatch "Phase: restore complete" -or $markerBody -notmatch "Restore active: 0") { throw "Restore did not complete in this process" }
            $reports = Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "run-as", $PACKAGE, "ls", "files/tombstones") -Seconds 5
            $reports = @($reports -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -match "^crash_error_restore_.*_${pidNow}_\d+\.txt$" })
            if ($cycle -le 2) {
                if ($reports.Count -ne 1) { throw "Expected one shareable recovery report for $serial, got $($reports.Count)" }
                $report = Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/tombstones/$($reports[0].Trim())") -Seconds 5
                $expectedReason = if ($cycle -eq 1) { "missing or deleted powerup" } else { "unreadable optional section" }
                if ($report -notmatch "Save restore recovered" -or $report -notmatch $expectedReason) {
                    throw "Recovery report did not identify discarded fixture gear"
                }
            } elseif ($reports.Count) { throw "Discarded gear returned after saving and reloading" }
        }
    }
    return Wait-BidirectionalPdata -FirstSerial $EMU1 -FirstRemoteSlot 1 -SecondSerial $EMU2 -SecondRemoteSlot 0 `
        -Description "continued network updates after tolerant restore"
}

function Invoke-RestoreReportScenario {
    Write-Status "--- Restore report case: $RestoreReportCase ---" "White"
    $gamePid = (Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "pidof", "${PACKAGE}:game") -Seconds 5).Trim()
    if (-not $gamePid) { throw "No host game process for report test" }
    $files = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "run-as", $PACKAGE, "ls", "files/tombstones") -Seconds 5
    $marker = @($files -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -match "^engine_pending_.*_${gamePid}\.txt$" })
    if ($marker.Count -ne 1) { throw "No unique session marker before report test" }
    $sessionKey = $marker[0].Replace("engine_pending_", "").Replace(".txt", "")
    $reportPath = "files/tombstones/crash_error_exit_$sessionKey.txt"
    $reportScripts = @{
        unexpected_exit = "test_restore_report_unexpected_exit.jsonc"
        interrupted_restore = "test_restore_report_interrupted_restore.jsonc"
        normal_quit = "test_restore_report_normal_quit.jsonc"
        fail_after_hide = "test_restore_report_fail_after_hide.jsonc"
    }
    if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName $reportScripts[$RestoreReportCase])) { return $false }
    if ($RestoreReportCase -eq "interrupted_restore") {
        if (-not (Wait-ForCondition -Description "durable interrupted-restore evidence" -TimeoutSec 15 -PollMs 500 -Condition {
                    $body = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/tombstones/$($marker[0])") -Seconds 5
                    return $body -match "Restore active: 1" -and $body -match "injected restore interruption"
                })) { return $false }
        Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "run-as", $PACKAGE, "kill", "-9", $gamePid) -Seconds 5 | Out-Null
    }
    if ($RestoreReportCase -eq "fail_after_hide") {
        if (-not (Wait-ForCondition -Description "core restore failure returns to visible menu" -TimeoutSec 60 -PollMs 1000 -Condition {
                    $intro = Get-GameIntrospection -Serial $EMU1
                    return $intro -and -not $intro.in_game -and $intro.screen_mode -eq "menu" -and $intro.window_count -gt 0
                })) { return $false }
        $files = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "run-as", $PACKAGE, "ls", "files/tombstones") -Seconds 5
        $reports = @($files -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -match "^crash_error_restore_${sessionKey}_\d+\.txt$" })
        if ($reports.Count -ne 1) { throw "Core restore failure did not publish one report" }
        $report = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/tombstones/$($reports[0])") -Seconds 5
        if ($report -notmatch "Save restore failure" -or $report -notmatch "Visible window: 1" -or
            $report -notmatch "Failure phase: injected core restore failure after hiding window") { throw "Core failure report lacks visible recovery evidence" }
        return $true
    }
    if (-not (Wait-ForCondition -Description "host game process exits" -TimeoutSec 30 -PollMs 500 -Condition {
                $current = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "pidof", "${PACKAGE}:game") -Seconds 5
                return -not $current.Trim()
            })) { return $false }
    if (-not (Start-SetupActivity -Serial $EMU1)) { return $false }
    $report = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "run-as", $PACKAGE, "cat", $reportPath) -Seconds 5
    if ($RestoreReportCase -eq "normal_quit") {
        $files = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "run-as", $PACKAGE, "ls", "files/tombstones") -Seconds 5
        if (($files -split "`n" | ForEach-Object { $_.Trim() }) -contains "crash_error_exit_$sessionKey.txt") { throw "Intentional quit produced an unexpected-exit report" }
    } else {
        $title = if ($RestoreReportCase -eq "interrupted_restore") { "Interrupted save restore" } else { "Unexpected engine exit" }
        if ($report -notmatch $title -or $report -notmatch "injected restore interruption") { throw "Missing standalone exit report with restore evidence" }
        if (-not (Start-SetupActivity -Serial $EMU1)) { return $false }
        $again = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "run-as", $PACKAGE, "cat", $reportPath) -Seconds 5
        if ($again -ne $report) { throw "Launcher resume rewrote the completed report" }
    }
    $files = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "run-as", $PACKAGE, "ls", "files/tombstones") -Seconds 5
    if (($files -split "`n" | ForEach-Object { $_.Trim() }) -contains $marker[0]) { throw "Completed exit still has a pending session marker" }
    return $true
}

function Invoke-RestoreStatusScenario {
    if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_restore_status_host.jsonc" -SecondarySerial $EMU2 -SecondaryScript "test_coop_restore_status_client.jsonc" -Description "restore completion broadcast")) { return $false }
    if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_restore_checkpoint_host.jsonc" -SecondarySerial $EMU2 -SecondaryScript "test_coop_restore_checkpoint_client.jsonc" -Description "retained checkpoint clears restore status")) { return $false }
    if (-not (Start-DeviceGameAutomation -Serial $EMU2 -ScriptName "test_coop_restore_status_replay.jsonc")) { return $false }
    $finished = Wait-ForCondition -Description "delayed restore status packets" -TimeoutSec 30 -PollMs 500 -Condition {
        $result = Get-DeviceAutomationResult -Serial $EMU2
        return $result -and $result.result -in @("PASS", "FAIL")
    }
    $result = Get-DeviceAutomationResult -Serial $EMU2
    if (-not $finished -or -not $result -or $result.result -ne "PASS") {
        Write-DeviceAutomationDiagnostics -Serial $EMU2
        return $false
    }
    return $true
}

function Invoke-SpewRecoveryScenario {
    Write-Status "--- Death spew, process loss and repeated in-game rejoin ---" "White"
    if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName "test_coop_recovery_host.jsonc")) { return $false }
    if (-not (Start-DeviceGameAutomation -Serial $EMU2 -ScriptName "test_coop_recovery_drop.jsonc")) { return $false }
    $dropped = Wait-ForCondition -Description "host records real death spew" -TimeoutSec 30 -PollMs 500 -Condition {
        $intro = Get-GameIntrospection -Serial $EMU1
        if (-not $intro -or $intro.multiplayer.recovery.live -le 0) { return $false }
        $hostPlayer = @($intro.multiplayer.players | Where-Object { $_.is_me })[0]
        return $hostPlayer.homing_ammo -eq 4
    }
    if (-not $dropped) { return $false }
    for ($attempt = 0; $attempt -lt 2; $attempt++) {
        Adb-Dev-Timeout -Serial $EMU2 -AdbArgs @("shell", "am", "force-stop", $PACKAGE) -Seconds 10 | Out-Null
        $absent = Wait-ForCondition -Description "host notices disconnected client" -TimeoutSec 60 -PollMs 1000 -Condition {
            $intro = Get-GameIntrospection -Serial $EMU1
            return (Get-IntroNumConnected -Intro $intro) -eq 1
        }
        if (-not $absent -or -not (Start-SetupActivity -Serial $EMU2)) { return $false }
        Send-MpCommand -Serial $EMU2 -Command "lan_launch" -Extras $joinExtras
        $recovered = Wait-ForCondition -Description "rejoin restores gear exactly once (attempt $attempt)" -TimeoutSec 60 -PollMs 1000 -Condition {
            $h = Get-GameIntrospection -Serial $EMU1
            $c = Get-GameIntrospection -Serial $EMU2
            if (-not $h -or -not $c -or -not $c.in_game -or (Get-IntroNumConnected -Intro $h) -ne 2) { return $false }
            $p = @($c.multiplayer.players | Where-Object { $_.is_me })[0]
            $hostPlayer = @($h.multiplayer.players | Where-Object { $_.is_me })[0]
            return $hostPlayer.homing_ammo -eq 4 -and $p.homing_ammo -eq 2 -and ($p.primary_flags -band 8) -ne 0 -and
            $h.multiplayer.recovery.live -eq 0 -and $h.multiplayer.recovery.world_objects -eq 0 -and
            $c.multiplayer.recovery.world_objects -eq 0 -and
            $h.multiplayer.recovery.rows -gt 0 -and $c.multiplayer.recovery.rows -eq $h.multiplayer.recovery.rows
        }
        if (-not $recovered) { return $false }
    }
    Write-Status "PASS: gear returned once, no duplicate world spew after two process restarts" "Green"
    return $true
}

function Invoke-GuidebotSlotRemapRestoreScenario {
    Write-Status ""
    Write-Status "--- Guide-Bot slot-remapped coop restore scenario ---" "White"
    if (-not (Invoke-PairedGameAutomation `
                -PrimarySerial $EMU1 `
                -PrimaryScript "test_coop_guidebot_restore_save_host.jsonc" `
                -SecondarySerial $EMU2 `
                -SecondaryScript "test_coop_guidebot_restore_save_joiner.jsonc" `
                -Description "paired Guide-Bot pre-restore save automation")) {
        return $false
    }

    $script:guidebotRestoreSaveSlot = -1
    $saved = Wait-ForCondition -Description "coop autosave reaches both peers" -TimeoutSec 20 -PollMs 500 -Condition {
        $latestSlot = Get-DeviceLatestCoopAutosaveSlot -Serial $EMU1
        if ($latestSlot -lt 0) {
            return $false
        }
        $bothHaveSave = (Test-DeviceCoopSaveSlot -Serial $EMU1 -Slot $latestSlot) -and
        (Test-DeviceCoopSaveSlot -Serial $EMU2 -Slot $latestSlot -Callsign $CALLSIGN2)
        if ($bothHaveSave) {
            $script:guidebotRestoreSaveSlot = $latestSlot
        }
        return $bothHaveSave
    }
    if (-not $saved) {
        Write-Status "FAIL: a matching synchronized coop autosave was not created" "Red"
        return $false
    }
    $saveSlot = $script:guidebotRestoreSaveSlot
    Write-Status "Saved slot-0 owner and Unexplored intent to coop slot $saveSlot" "Green"

    foreach ($emu in @($EMU1, $EMU2)) {
        Adb-Dev-Timeout -Serial $emu -AdbArgs @(
            "shell", "am", "force-stop", $PACKAGE
        ) -Seconds 10 | Out-Null
        Adb-Dev-Timeout -Serial $emu -AdbArgs @(
            "shell", "run-as", $PACKAGE, "rm", "-f", "files/file_sets.json", "files/introspect.json"
        ) -Seconds 5 | Out-Null
    }

    Write-Status "Relaunching with LanJoin as host slot 0 and LanHost as joiner slot 1"
    if (-not (Start-SetupActivity -Serial $EMU2)) {
        Write-Status "FAIL: SetupActivity didn't restart on $EMU2" "Red"
        return $false
    }
    if (-not (Start-SetupActivity -Serial $EMU1)) {
        Write-Status "FAIL: SetupActivity didn't restart on $EMU1" "Red"
        return $false
    }
    if (-not (Set-DeviceCoopRestoreSlot -Serial $EMU2 -Slot $saveSlot)) {
        Write-Status "FAIL: could not stage coop restore slot on the new host" "Red"
        return $false
    }

    $newHostIp = Get-DeviceWlanIp -Serial $EMU2
    if (-not $newHostIp) {
        Write-Status "FAIL: could not get the swapped host wlan0 IP" "Red"
        return $false
    }

    Send-MpCommand -Serial $EMU2 -Command "lan_launch" -Extras @(
        "--es", "game", $Game,
        "--es", "mp_mode", "host",
        "--es", "mission", $MISSION,
        "--es", "mode", $MODE,
        "--ei", "max_players", "2",
        "--ei", "level_num", "1",
        "--ei", "difficulty", "1",
        "--es", "callsign", $CALLSIGN2
    )
    $null = Wait-ForCondition -Description "swapped host game process" -TimeoutSec 30 -PollMs 500 -Condition {
        $gamePid = Adb-Dev-Timeout -Serial $EMU2 -AdbArgs @(
            "shell", "pidof", "${PACKAGE}:game"
        ) -Seconds 5
        return $gamePid -and $gamePid -match '^\d+'
    }
    Send-MpCommand -Serial $EMU1 -Command "lan_launch" -Extras @(
        "--es", "game", $Game,
        "--es", "mp_mode", "join",
        "--es", "mission", $MISSION,
        "--es", "mode", $MODE,
        "--ei", "max_players", "2",
        "--ei", "level_num", "1",
        "--ei", "difficulty", "1",
        "--es", "callsign", $CALLSIGN1,
        "--es", "host_addr", $newHostIp,
        "--ei", "host_port", "42424"
    )

    $swappedSync = Wait-ForCondition -Description "slot-swapped LAN game sync" -TimeoutSec $TimeoutSeconds -PollMs 2000 -Condition {
        $script:swappedHostIntro = Get-GameIntrospection -Serial $EMU2
        $script:swappedJoinIntro = Get-GameIntrospection -Serial $EMU1
        $hostPlayers = Get-IntroNumConnected -Intro $script:swappedHostIntro
        $joinPlayers = Get-IntroNumConnected -Intro $script:swappedJoinIntro
        return $script:swappedHostIntro -and $script:swappedJoinIntro -and
        [bool]$script:swappedHostIntro.in_game -and [bool]$script:swappedJoinIntro.in_game -and
        [bool]$script:swappedHostIntro.is_network -and [bool]$script:swappedJoinIntro.is_network -and
        $hostPlayers -ge 2 -and $joinPlayers -ge 2
    }
    if (-not $swappedSync) {
        Write-Status "FAIL: slot-swapped peers did not synchronize" "Red"
        return $false
    }
    foreach ($intro in @($script:swappedHostIntro, $script:swappedJoinIntro)) {
        if ($intro.coop_restore.status -eq "error") {
            Write-Status "FAIL: coop restore entered an error state during synchronization" "Red"
            return $false
        }
    }

    if (-not (Invoke-PairedGameAutomation `
                -PrimarySerial $EMU2 `
                -PrimaryScript "test_coop_guidebot_restore_remap_host.jsonc" `
                -SecondarySerial $EMU1 `
                -SecondaryScript "test_coop_guidebot_restore_remap_joiner.jsonc" `
                -Description "paired slot-remapped Guide-Bot restore automation" `
                -TimeoutSec 105)) {
        return $false
    }

    $hostIntro = Get-GameIntrospection -Serial $EMU2
    $joinIntro = Get-GameIntrospection -Serial $EMU1
    if ($hostIntro.coop_restore.status -ne "idle" -or $joinIntro.coop_restore.status -ne "idle") {
        Write-Status "FAIL: coop restore status did not clear on both peers" "Red"
        return $false
    }
    $hostGuidebot = Get-IntroGuidebot -Intro $hostIntro
    $joinGuidebot = Get-IntroGuidebot -Intro $joinIntro
    $stateMatches = $hostGuidebot -and $joinGuidebot -and
    [int]$hostGuidebot.owner_player -eq 1 -and
    [int]$joinGuidebot.owner_player -eq 1 -and
    [int]$hostGuidebot.owner_generation -eq [int]$joinGuidebot.owner_generation -and
    -not [bool]$hostGuidebot.owner_is_local -and
    [bool]$joinGuidebot.owner_is_local -and
    -not [bool]$hostGuidebot.local_control_slot_matches -and
    [bool]$joinGuidebot.local_control_slot_matches -and
    $hostGuidebot.route_target_mode_name -eq "unexplored" -and
    $joinGuidebot.route_target_mode_name -eq "unexplored" -and
    [int]$joinGuidebot.route_decision.target_policy -eq 1 -and
    [int]$joinGuidebot.route_metadata_rescan_count -ge 1
    if (-not $stateMatches) {
        Write-Status "FAIL: Guide-Bot restore state differs between slot-swapped peers" "Red"
        Write-DeviceAutomationDiagnostics -Serial $EMU2
        Write-DeviceAutomationDiagnostics -Serial $EMU1
        return $false
    }

    Write-Status "Saved owner identity remapped from slot 0 to slot 1 on both peers" "Green"
    Write-Status "New owner reconstructed unexplored guidance from restored world state" "Green"
    return $true
}

# ---- Main Test Flow ----

try {

    Write-Status "=== LAN Multiplayer Integration Test ===" "White"
    Write-Status "Game: $Game | Mission: $MISSION | Mode: $MODE"
    Write-Status ""

    if ($SpewRecovery -and ($RestoreResilience -or $SpewPickup -or $SpewPartialPickup)) {
        throw "Run SpewRecovery separately: it requires the fresh game's initial inventory"
    }

    if (($GuidebotOwnership -or $GuidebotHostObserver -or $GuidebotSlotRemapRestore) -and $Game -ne "d2") {
        Write-Status "FAIL: Guide-Bot LAN scenarios currently require D2" "Red"
        exit 1
    }
    if ($GuidebotSpawn -and ($Game -ne 'd2' -or $GuidebotOwnership -or $GuidebotHostObserver -or $GuidebotSlotRemapRestore -or $HostMigration)) {
        throw 'GuidebotSpawn requires D2 and must run separately from other Guide-Bot scenarios'
    }
    if ($GuidebotSpawn -and ($InitialLevel -ne 8 -or $MissionFile -or -not $AllowSecretWarps)) {
        throw 'GuidebotSpawn requires Counterstrike level 8 with secret warps enabled'
    }
    if ($GuidebotClientRelease -and ($Game -ne 'd2' -or $InitialLevel -ne 1 -or $MissionFile -or
            $GuidebotOwnership -or $GuidebotSpawn -or $GuidebotHostObserver -or $GuidebotSlotRemapRestore -or $HostMigration)) {
        throw 'GuidebotClientRelease requires a separate Counterstrike level 1 scenario'
    }
    if (($GuidebotOwnership -and $GuidebotHostObserver) -or
        ($GuidebotOwnership -and $GuidebotSlotRemapRestore) -or
        ($GuidebotHostObserver -and $GuidebotSlotRemapRestore)) {
        Write-Status "FAIL: choose one Guide-Bot LAN scenario per run" "Red"
        exit 1
    }
    if ($HostMigration -and ($GuidebotOwnership -or $GuidebotHostObserver -or $GuidebotSlotRemapRestore)) {
        Write-Status "FAIL: run host migration separately from other Guide-Bot LAN scenarios" "Red"
        exit 1
    }
    if ($GuidebotSlotRemapRestore -and $UseRelay) {
        Write-Status "FAIL: slot-remapped restore coverage requires direct LAN" "Red"
        exit 1
    }

    # -- Step 0: Ensure both emulators are online (auto-start if needed) --
    Write-Status "Checking emulators..."
    Start-EmulatorIfNeeded -Serial $EMU1 -AvdMap $AVD_MAP
    Start-EmulatorIfNeeded -Serial $EMU2 -AvdMap $AVD_MAP
    Write-Status "Both emulators online" "Green"

    # Verify game data
    foreach ($emu in @($EMU1, $EMU2)) {
        if (-not (Ensure-StandardGameDataOnDevice -Serial $emu)) {
            Write-Status "FAIL: Could not ensure standard game data on $emu" "Red"
            exit 1
        }
    }
    Write-Status "Game data verified on both emulators" "Green"
    Initialize-EndgameContent
    if ($Flyouts) {
        $library = $FlyoutMovieLibrary
        if (-not $library) { $library = Join-Path $REPO_ROOT 'game_data/d2 1.1 data tracks from cd image tool/OTHER-L.MVL' }
        if (-not (Test-Path -LiteralPath $library)) { throw 'Pass -FlyoutMovieLibrary with an owned OTHER-L.MVL or OTHER-H.MVL' }
        $fixture = Join-Path $REPO_ROOT 'temp/coop-flyout-fixture'
        & "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $fixture | Out-Null
        & python "$PSScriptRoot/prepare_coop_endgame_fixture.py" --output $fixture --movie-library $library --movie-name esa.mve
        if ($LASTEXITCODE) { throw 'Could not extract escape movie' }
        foreach ($serial in @($EMU1, $EMU2)) {
            Stage-EndgameFile -Serial $serial -LocalPath "$fixture/esa.mve" -DevicePath 'files/d2x-redux/esa.mve'
        }
    }
    if ($MaximumExitProbe) {
        $fixtureDirectory = Join-Path $REPO_ROOT 'temp/maximum_exit_fixture'
        New-Item -ItemType Directory -Force -Path $fixtureDirectory | Out-Null
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $REPO_ROOT 'game_data/mission_files/descent_maximum_fixed.zip'))
        try {
            foreach ($name in @('max_f.hog', 'max_f.mn2')) {
                $destination = Join-Path $fixtureDirectory $name
                [IO.Compression.ZipFileExtensions]::ExtractToFile($archive.GetEntry($name), $destination, $true)
                foreach ($serial in @($EMU1, $EMU2)) {
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('push', $destination, "/data/local/tmp/$name") -Seconds 30 | Out-Null
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $PACKAGE, 'mkdir', '-p', 'files/imported/sets/default/missions') -Seconds 10 | Out-Null
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $PACKAGE, 'cp', "/data/local/tmp/$name", "files/imported/sets/default/missions/$name") -Seconds 10 | Out-Null
                }
            }
        } finally {
            $archive.Dispose()
        }
    }

    foreach ($emu in @($EMU1, $EMU2)) {
        Reset-DeviceGameState -Serial $emu
    }

    # -- Step 1: Launch SetupActivity on both --
    Write-Status ""
    Write-Status "--- Phase 1: Launch SetupActivity on both emulators ---" "White"

    if (-not (Start-SetupActivity -Serial $EMU1 -TimeoutSec $TimeoutSeconds)) {
        Write-Status "FAIL: SetupActivity didn't start on $EMU1" "Red"; Cleanup; exit 1
    }
    if (-not (Start-SetupActivity -Serial $EMU2 -TimeoutSec $TimeoutSeconds)) {
        Write-Status "FAIL: SetupActivity didn't start on $EMU2" "Red"; Cleanup; exit 1
    }
    Write-Status "SetupActivity ready on both emulators" "Green"

    if ($Game -eq 'd2') {
        foreach ($serial in @($EMU1, $EMU2)) {
            # Explicit coverage opposes the joiner's preference to the host's
            $routing = if ($GuidebotRoutingMode -eq 'Original') { 0 } else { 1 }
            if ($GuidebotRoutingMode -and $serial -eq $EMU2) { $routing = 1 - $routing }
            Adb-Dev-Timeout -Serial $serial -AdbArgs @(
                'shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
                '--es', 'command', 'write_engine_prefs', '--ei', 'guidebot_routing_mode', "$routing"
            ) -Seconds 10 | Out-Null
        }
    }

    # Other tests intentionally exercise CD audio and can leave that pilot
    # preference behind after removing their temporary source. LAN startup is
    # unrelated to audio-source coverage, so normalize both devices to MIDI.
    foreach ($serial in @($EMU1, $EMU2)) {
        Adb-Dev-Timeout -Serial $serial -AdbArgs @(
            "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
            "--es", "command", "write_music_prefs",
            "--es", "source", "midi",
            "--ez", "prefer_mission_soundtrack", "false",
            "--ei", "play_order", "0",
            "--ei", "volume", "8"
        ) -Seconds 10 | Out-Null
    }
    Start-Sleep -Seconds 1
    Write-Status "Normalized music preferences for LAN launch" "Green"

    if ($Briefings -and -not $RestoreSavePath) {
        foreach ($serial in @($EMU1, $EMU2)) {
            Adb-Dev-Timeout -Serial $serial -AdbArgs @(
                "shell", "run-as", $PACKAGE, "rm", "-f",
                "files/d1x-redux/coop_restore_slot.txt", "files/d2x-redux/coop_restore_slot.txt"
            ) -Seconds 10 | Out-Null
        }
    }

    if ($D1LevelTransition) {
        foreach ($serial in @($EMU1, $EMU2)) {
            Adb-Dev-Timeout -Serial $serial -AdbArgs @(
                'shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
                '--es', 'command', 'write_probe_debug_prefs', '--ez', 'enabled', 'true'
            ) -Seconds 10 | Out-Null
        }
    }

    if ($BriefingRestore -or $MaximumExitProbe) {
        foreach ($serial in @($EMU1, $EMU2)) {
            Adb-Dev-Timeout -Serial $serial -AdbArgs @(
                "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
                "--es", "command", "write_bool_pref",
                "--es", "key", "'dlog_coop desync_enabled'", "--ez", "value", "true"
            ) -Seconds 10 | Out-Null
        }
    }

    if ($GuidebotSlotRemapRestore -or $SavedLateJoin) {
        Write-Status "Clearing prior coop saves before restore coverage"
        foreach ($emu in @($EMU1, $EMU2)) {
            Adb-Dev-Timeout -Serial $emu -AdbArgs @(
                "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
                "--es", "command", "clear_save_files"
            ) -Seconds 10 | Out-Null
            Adb-Dev-Timeout -Serial $emu -AdbArgs @(
                "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
                "--es", "command", "write_probe_debug_prefs", "--ez", "enabled", "true"
            ) -Seconds 10 | Out-Null
            Adb-Dev-Timeout -Serial $emu -AdbArgs @(
                "shell", "run-as", $PACKAGE, "rm", "-f",
                "files/d2x-redux/coop_restore_slot.txt",
                "files/d2x-redux/coop_autosave_history.json",
                "files/d2x-redux/coop_autosave_info.json",
                "files/d2x-redux/Players/save_sets/coop/d2/coop_autosave_history.json",
                "files/d2x-redux/Players/save_sets/coop/d2/coop_autosave_info.json"
            ) -Seconds 10 | Out-Null
        }
    }

    # -- Step 2: Set up networking (relay or direct LAN) --
    Write-Status ""
    if ($UseRelay) {
        Write-Status "--- Phase 2: Set up UDP relay (legacy mode) ---" "White"

        # Pre-create firewall rules to prevent UAC prompts during test
        Ensure-FirewallRules

        # EMU1 redir: host:42500 -> EMU1:42424 (relay -> host game inbound)
        $r1del = Setup-EmulatorRedir -ConsolePort 5554 -RedirSpec "del udp:42500"
        Write-Status "  EMU1 redir cleanup: $r1del" "Gray"
        $r1 = Setup-EmulatorRedir -ConsolePort 5554 -RedirSpec "add udp:42500:42424"
        Write-Status "  EMU1 redir add: $r1" "Gray"

        # EMU2: remove stale redir
        $r2del = Setup-EmulatorRedir -ConsolePort 5556 -RedirSpec "del udp:42501"
        Write-Status "  EMU2 redir cleanup: $r2del" "Gray"

        # Start UDP relay
        $relayScript = Join-Path $PSScriptRoot "..\helpers\udp_relay.ps1"
        if (-not (Test-Path $relayScript)) {
            Write-Status "FAIL: udp_relay.ps1 not found at $relayScript" "Red"; exit 1
        }
        $relayProc = Start-Process pwsh -ArgumentList "-File", $relayScript, "-Bind", "127.0.0.1" -PassThru -NoNewWindow `
            -RedirectStandardOutput (Join-Path $REPO_ROOT "temp\udp_relay.log") `
            -RedirectStandardError (Join-Path $REPO_ROOT "temp\udp_relay_err.log")
        $script:relayProc = $relayProc
        Write-Status "UDP relay started (PID $($relayProc.Id))" "Green"
        Start-Sleep -Seconds 1
    } else {
        Write-Status "--- Phase 2: Direct LAN (emulator 36.5+ shared Wi-Fi) ---" "White"
        # Get host emulator's wlan0 IP for direct connection
        $script:DirectHostIp = Get-DeviceWlanIp -Serial $EMU1
        if (-not $script:DirectHostIp) {
            Write-Status "FAIL: Could not get wlan0 IP from $EMU1 -- is emulator 36.5+?" "Red"
            exit 1
        }
        Write-Status "Host wlan0 IP: $($script:DirectHostIp)" "Green"

        # Prove the emulator shared-Wi-Fi path is usable before attributing a
        # later timeout to the game protocol. This also waits out the brief
        # neighbor-discovery window seen after managed emulator startup.
        $directLanReachable = Wait-ForCondition -Description "Direct LAN reachability" -TimeoutSec 60 -PollMs 1000 -Condition {
            $pingResult = Adb-Dev-Timeout -Serial $EMU2 -AdbArgs @(
                "shell", "ping", "-c", "1", "-W", "2", $script:DirectHostIp
            ) -Seconds 5
            return ($pingResult -and $pingResult -match '1 (?:packets )?received')
        }
        if (-not $directLanReachable) {
            Write-Status "FAIL: $EMU2 cannot reach host $($script:DirectHostIp) over shared Wi-Fi" "Red"
            exit 1
        }
        Write-Status "Direct LAN reachability verified from $EMU2" "Green"
    }

    # -- Step 3: Launch game on both emulators via lan_launch --
    Write-Status ""
    Write-Status "--- Phase 3: Launch game via lan_launch ---" "White"

    # Start logcat capture BEFORE lan_launch so we catch all MPDIAG messages.
    $logcatFile1 = Join-Path $REPO_ROOT "temp\lan_emu1_logcat.txt"
    $logcatFile2 = Join-Path $REPO_ROOT "temp\lan_emu2_logcat.txt"
    & $ADB -s $EMU1 logcat -c 2>&1 | Out-Null
    & $ADB -s $EMU2 logcat -c 2>&1 | Out-Null
    foreach ($serial in @($EMU1, $EMU2)) {
        Adb-Dev-Timeout -Serial $serial -AdbArgs @(
            "shell", "run-as", $PACKAGE, "rm", "-f", "files/introspect.json"
        ) -Seconds 10 | Out-Null
    }
    $logcatProc1 = Start-Process -FilePath $ADB -ArgumentList "-s", $EMU1, "logcat", "-s", "DXX-MP:*", "DXX-Redux:*", "dxxredux:*", "AndroidRuntime:*", "LocalhostProxy:*" -PassThru -NoNewWindow -RedirectStandardOutput $logcatFile1 -RedirectStandardError (Join-Path $REPO_ROOT "temp\lan_emu1_logcat_err.txt")
    $logcatProc2 = Start-Process -FilePath $ADB -ArgumentList "-s", $EMU2, "logcat", "-s", "DXX-MP:*", "DXX-Redux:*", "dxxredux:*", "AndroidRuntime:*", "LocalhostProxy:*", "MatchmakingService:*" -PassThru -NoNewWindow -RedirectStandardOutput $logcatFile2 -RedirectStandardError (Join-Path $REPO_ROOT "temp\lan_emu2_logcat_err.txt")
    Start-Sleep -Seconds 1

    # Host (EMU1)
    if ($RestoreSavePath) {
        if (-not (Test-Path -LiteralPath $RestoreSavePath -PathType Leaf)) { throw "RestoreSavePath does not exist" }
        $script:ProvidedSaveHash = (Get-FileHash -LiteralPath $RestoreSavePath -Algorithm SHA256).Hash
        $providedMissionKey = if ($MISSION) { $MISSION } else { "default" }
        if ($providedMissionKey -notmatch '^[A-Za-z0-9_-]+$') { throw "Use a simple mission filename for provided-save coverage" }
        $providedGameDir = if ($Game -eq "d1") { "d1x-redux" } else { "d2x-redux" }
        $providedSaveDir = "files/$providedGameDir/Players/save_sets/coop/$providedMissionKey"
        Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("push", $RestoreSavePath, "/data/local/tmp/coop-provided-save.mg") -Seconds 30 | Out-Null
        Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "run-as", $PACKAGE, "mkdir", "-p", $providedSaveDir) -Seconds 5 | Out-Null
        Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "run-as", $PACKAGE, "cp", "/data/local/tmp/coop-provided-save.mg", "$providedSaveDir/coopsave.mg9") -Seconds 5 | Out-Null
        if (-not (Set-DeviceCoopRestoreSlot -Serial $EMU1 -Slot 9)) { throw "Could not stage supplied save" }
    }
    Write-Status "Sending lan_launch host to $EMU1..."
    $hostExtras = @(
        "--es", "game", $Game,
        "--es", "mp_mode", "host",
        "--es", "mode", $MODE,
        "--ei", "max_players", "2",
        "--ei", "level_num", "$InitialLevel",
        "--ei", "difficulty", "1",
        "--es", "callsign", $CALLSIGN1
    )
    if ($MISSION) {
        $hostExtras += @("--es", "mission", $MISSION)
    }
    if ($Briefings) {
        $hostExtras += @("--ez", "coop_briefings", "true")
    }
    if ($AllowSecretWarps) {
        $hostExtras += @("--ez", "allow_secret_warps", "true")
    }
    if ($NoCoopQol) {
        $hostExtras += @("--ez", "coop_qol", "false")
    }
    if ($GuidebotHostObserver -or $EndgameObserverHost -or $BriefingCase -eq 'observer_host') {
        $hostExtras += @("--ez", "host_observer", "true")
    }
    Send-MpCommand -Serial $EMU1 -Command "lan_launch" -Extras $hostExtras

    # Poll for host game process to appear (replaces fixed 5s sleep)
    $hostProcessReady = Wait-ForCondition -Description "Host game process" -TimeoutSec 30 -PollMs 500 -Condition {
        $gPid = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "pidof", "${PACKAGE}:game") -Seconds 5
        return ($gPid -and $gPid -match '^\d+')
    }
    if (-not $hostProcessReady) {
        Write-Status "FAIL: Host game process did not start" "Red"
        exit 1
    }

    # A process PID only proves that Android created the game process. Wait
    # until the engine has actually entered its one-player network lobby before
    # starting the joiner's finite game-info request sequence.
    $script:hostReadyIntro = $null
    $hostNetworkReady = Wait-ForCondition -Description "Host network lobby" -TimeoutSec $TimeoutSeconds -PollMs 1000 -Condition {
        $script:hostReadyIntro = Get-GameIntrospection -Serial $EMU1
        $connected = Get-IntroNumConnected -Intro $script:hostReadyIntro
        return (
            $script:hostReadyIntro -and
            [bool]$script:hostReadyIntro.is_network -and
            $connected -eq 1
        )
    }
    if (-not $hostNetworkReady) {
        Write-Status "FAIL: Host process started but did not enter the network lobby" "Red"
        exit 1
    }
    Write-Status "Host network lobby ready" "Green"

    # Joiner (EMU2)
    Write-Status "Sending lan_launch join to $EMU2..."
    $joinExtras = @(
        "--es", "game", $Game,
        "--es", "mp_mode", "join",
        "--es", "mode", $MODE,
        "--ei", "max_players", "2",
        "--ei", "level_num", "$InitialLevel",
        "--ei", "difficulty", "1",
        "--es", "callsign", $CALLSIGN2
    )
    if ($MISSION) {
        $joinExtras += @("--es", "mission", $MISSION)
    }
    if ($UseRelay) {
        # Relay mode: 10.0.2.2 = host loopback from emulator, port 42600 = relay
        $joinExtras += @("--es", "host_addr", "10.0.2.2", "--ei", "host_port", "42600")
        Write-Status "  Joiner target: 10.0.2.2:42600 (via relay)"
    } else {
        # Direct LAN: use host's wlan0 IP on the engine port
        $joinExtras += @("--es", "host_addr", $script:DirectHostIp, "--ei", "host_port", "42424")
        Write-Status "  Joiner target: $($script:DirectHostIp):42424 (direct LAN)"
    }
    if ($BriefingCase -eq 'first_join') {
        $testPassed = Invoke-BriefingRejoinScenario -FirstJoin
        Write-Status '=== BRIEFING FIRST JOIN TEST PASSED ===' 'Green'
        exit 0
    }
    Send-MpCommand -Serial $EMU2 -Command "lan_launch" -Extras $joinExtras

    # -- Step 4: Verify multiplayer launch --
    # Counterstrike level 8 has no authored pages, regardless of the travel fixture
    if ($EmptyBriefing -or ($Briefings -and $Game -eq "d2" -and $InitialLevel -eq 8 -and -not $MissionFile)) {
        if (-not (Wait-ForCondition -Description "Empty briefing uses normal level sync on both peers" -TimeoutSec 60 -PollMs 500 -Condition {
                    foreach ($serial in @($EMU1, $EMU2)) {
                        $intro = Get-GameIntrospection -Serial $serial
                        if (-not $intro -or -not $intro.in_game -or $intro.time_paused -or
                            (Get-IntroNumConnected -Intro $intro) -ne 2 -or $intro.coop_briefing.active) { return $false }
                        if (-not $intro.coop_briefing.enabled -or $intro.coop_briefing.local_total -ne 0 -or
                            $intro.coop_briefing.generation -ne 0 -or $intro.coop_briefing.presentations_started -ne 0 -or
                            $intro.coop_briefing.palette_restored) { throw "Empty briefing must not start a presentation or synchronization generation" }
                    }
                    return $true
                })) { throw "Initial empty briefing did not release both peers" }
    } elseif ($Briefings) {
        $briefingReady = Wait-ForCondition -Description "Both peers reviewing briefings" -TimeoutSec $TimeoutSeconds -PollMs 500 -Condition {
            $hostIntro = Get-GameIntrospection -Serial $EMU1
            $clientIntro = Get-GameIntrospection -Serial $EMU2
            if (-not $hostIntro -or -not $clientIntro -or
                -not $hostIntro.PSObject.Properties['coop_briefing'] -or
                -not $clientIntro.PSObject.Properties['coop_briefing']) { return $false }
            return $hostIntro -and $clientIntro -and
            $hostIntro.coop_briefing.phase -eq 6 -and $clientIntro.coop_briefing.phase -eq 6 -and
            $hostIntro.coop_briefing.presenting -and $clientIntro.coop_briefing.presenting
        }
        if (-not $briefingReady) { throw "Both peers must enter the synchronized briefing phase" }
        if ($BriefingCase -eq 'rejoin') {
            $testPassed = Invoke-BriefingRejoinScenario
            Write-Status '=== BRIEFING REJOIN TEST PASSED ===' 'Green'
            exit 0
        }
        if ($BriefingFailure) {
            $testPassed = Invoke-BriefingFailureScenario
            Write-Status '=== BRIEFING PARTICIPANT LOSS TEST PASSED ===' 'Green'
            exit 0
        }
        # Explicit filenames keep scenario ownership visible to catalog validation
        $briefingScripts = @{
            force = @('test_coop_briefing_host.jsonc', 'test_coop_briefing_client.jsonc')
            host_deadline = @('test_coop_briefing_host_deadline_host.jsonc', 'test_coop_briefing_host_deadline_client.jsonc')
            overall_deadline = @('test_coop_briefing_overall_deadline_host.jsonc', 'test_coop_briefing_overall_deadline_client.jsonc')
            release_delay = @('test_coop_briefing_release_delay_host.jsonc', 'test_coop_briefing_release_delay_client.jsonc')
            overlay_touch = @('test_coop_briefing_overlay_touch_host.jsonc', 'test_coop_briefing_client.jsonc')
            paused_force = @('test_coop_briefing_paused_force_host.jsonc', 'test_coop_briefing_paused_client.jsonc')
            paused_deadline = @('test_coop_briefing_host_deadline_host.jsonc', 'test_coop_briefing_paused_client.jsonc')
            paused_overall = @('test_coop_briefing_paused_overall_host.jsonc', 'test_coop_briefing_paused_overall_client.jsonc')
            reading = @('test_coop_briefing_reading_host.jsonc', 'test_coop_briefing_reading_client.jsonc')
            partial_skip = @('test_coop_briefing_partial_skip_host.jsonc', 'test_coop_briefing_partial_skip_client.jsonc')
            missing_movie = @('test_coop_briefing_missing_movie_host.jsonc', 'test_coop_briefing_missing_movie_client.jsonc')
            observer_host = @('test_coop_briefing_observer_host_host.jsonc', 'test_coop_briefing_observer_host_client.jsonc')
        }[$BriefingCase]
        $briefingHostScript = $briefingScripts[0]
        $briefingClientScript = $briefingScripts[1]
        if (-not (Start-DeviceGameAutomation -Serial $EMU2 -ScriptName $briefingClientScript)) {
            throw "Could not start client briefing verification"
        }
        if ($BriefingCase -like "paused_*") {
            if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName "test_coop_briefing_pause_host.jsonc")) {
                throw "Could not pause the host briefing video"
            }
            if (-not (Wait-ForCondition -Description "Client pauses the authored briefing video" -TimeoutSec 10 -PollMs 200 -Condition {
                        $state = Get-GameIntrospection -Serial $EMU2
                        return $state -and $state.movie.paused -and $state.movie.pause_window -and
                        $state.coop_briefing.local_state -eq 2 -and $state.coop_briefing.phase -eq 6
                    })) { throw "Client did not pause a real briefing video; install other-h.mvl for this case" }
            $paused = Get-GameIntrospection -Serial $EMU2
            if (-not (Wait-ForCondition -Description "Visible briefing timer advances while movie frames stay paused" -TimeoutSec 8 -PollMs 300 -Condition {
                        $state = Get-GameIntrospection -Serial $EMU2
                        if (-not $state) { return $false }
                        if (-not $state.movie.paused -or -not $state.movie.pause_window -or $state.movie.frame -ne $paused.movie.frame) {
                            throw "Movie resumed or advanced frames during the pause probe"
                        }
                        return $state.coop_briefing.seconds_remaining -le ($paused.coop_briefing.seconds_remaining - 3) -and
                        $state.coop_briefing.status -match 'remaining'
                    })) { throw "Briefing countdown stopped during movie pause" }
            Write-Status "Paused movie frame $($paused.movie.frame) stayed fixed while the visible countdown advanced"
            $hostPause = Get-DeviceAutomationResult -Serial $EMU1
            if (-not $hostPause -or $hostPause.result -ne "PASS") { throw "Host video pause did not complete" }
            if ($BriefingCase -ne 'paused_overall') {
                if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName 'test_coop_briefing_skip_movie.jsonc')) { throw 'Could not request movie Skip' }
                if (-not (Wait-ForCondition -Description 'Skip only the host movie and retain robot briefing pages' -TimeoutSec 10 -PollMs 300 -Condition {
                            $result = Get-DeviceAutomationResult -Serial $EMU1
                            return $result -and $result.result -eq 'PASS'
                        })) { throw 'Movie Skip dismissed the briefing sequence' }
            }
        }
        if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName $briefingHostScript)) {
            throw "Could not start paired briefing verification"
        }
        if ($BriefingCase -eq "overlay_touch") {
            # Use the actual fullscreen display, not the configurable render resolution
            $touchIntro = Get-GameIntrospection -Serial $EMU1
            $displayWidth = [int]$touchIntro.resolution.display_width
            $displayHeight = [int]$touchIntro.resolution.display_height
            if ($displayWidth -le 0 -or $displayHeight -le 0) { throw "Missing display dimensions for briefing touch probe" }
            $unit = [Math]::Min($displayWidth, $displayHeight) * 0.05
            # Centers from SkipButtonView and CoopBriefingOverlayView
            $skipX = [int]($displayWidth - $unit * 1.4)
            $skipY = [int]($unit * 1.4)
            $launchX = [int]($unit * 4.5)
            $launchY = [int]($displayHeight - $unit * 1.7)
            Write-Status "Touch Skip twice at ($skipX, $skipY), then deliberately launch at ($launchX, $launchY)"
            foreach ($tap in 1..2) {
                Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "input", "tap", "$skipX", "$skipY") -Seconds 5 | Out-Null
            }
            $waiting = Wait-ForCondition -Description "Repeated Skip taps leave the host waiting" -TimeoutSec 5 -PollMs 200 -Condition {
                $state = Get-GameIntrospection -Serial $EMU1
                return $state -and $state.coop_briefing.phase -eq 6 -and $state.coop_briefing.can_launch -and
                -not $state.coop_briefing.presenting -and $state.time_paused
            }
            if (-not $waiting) { throw "Skip did not enter the wait screen or its repeated tap launched the mine" }
            # Ending a drag on Launch now must not count as a fresh press on that button
            Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "input", "swipe", "$([int]($displayWidth / 2))", "$([int]($displayHeight / 2))", "$launchX", "$launchY", "350") -Seconds 5 | Out-Null
            $state = Get-GameIntrospection -Serial $EMU1
            if (-not $state.coop_briefing.can_launch -or $state.coop_briefing.phase -ne 6 -or -not $state.time_paused) {
                throw "A drag onto Launch now incorrectly ended the briefing"
            }
            Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "input", "tap", "$launchX", "$launchY") -Seconds 5 | Out-Null
        }
        $briefingPassed = Wait-ForCondition -Description "Briefing $BriefingCase launch" -TimeoutSec 150 -PollMs 500 -Condition {
            $hostResult = Get-DeviceAutomationResult -Serial $EMU1
            $clientResult = Get-DeviceAutomationResult -Serial $EMU2
            if (($hostResult -and $hostResult.result -eq "FAIL") -or
                ($clientResult -and $clientResult.result -eq "FAIL")) {
                throw "Briefing automation failed: host=$($hostResult | ConvertTo-Json -Compress) client=$($clientResult | ConvertTo-Json -Compress)"
            }
            return $hostResult -and $clientResult -and $hostResult.result -eq "PASS" -and $clientResult.result -eq "PASS"
        }
        if (-not $briefingPassed) { throw "Both peers must verify the briefing launch barrier" }
        if ($BriefingCase -like "paused_*") {
            if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_briefing_movie_closed.jsonc" `
                        -SecondarySerial $EMU2 -SecondaryScript "test_coop_briefing_movie_closed.jsonc" `
                        -Description "No movie or pause window survives launch on either peer" -TimeoutSec 20)) {
                throw "Movie or modal pause window survived synchronized launch"
            }
        }
    }
    #
    # Primary signal: both emulators reach in-game network state with two
    # connected players in game introspection.
    #
    # Fallback: if an emulator dies during 3D render, accept host MPDIAG
    # sync lines when they are available.
    #
    # In direct LAN mode, traffic goes:
    #   EMU2 -> LAN proxy -> host wlan0 IP -> host engine
    # In relay mode:
    #   EMU2 -> LAN proxy -> relay -> EMU1 redir -> host engine
    #
    # We do NOT require both emulators to survive the level load phase
    # (GPU rendering on swiftshader_indirect can crash the emulator).

    Write-Status ""
    Write-Status "--- Phase 4: Wait for multiplayer sync ---" "White"

    # Quick diagnostic: is relay still alive? (relay mode only)
    if ($UseRelay -and $script:relayProc -and $script:relayProc.HasExited) {
        Write-Status "WARN: UDP relay exited early (code $($script:relayProc.ExitCode))" "Yellow"
        $errLog = Join-Path $REPO_ROOT "temp\udp_relay_err.log"
        if (Test-Path $errLog) {
            Get-Content $errLog -ErrorAction SilentlyContinue | Select-Object -Last 5 | ForEach-Object { Write-Status "  relay err: $_" "Yellow" }
        }
    }

    # Dump early EMU2 logcat to see if game launched and proxy started
    Start-Sleep -Seconds 2
    if (Test-Path $logcatFile2) {
        $early = Get-Content $logcatFile2 -ErrorAction SilentlyContinue | Select-Object -First 20
        foreach ($l in $early) { Write-Status "  [EMU2 early] $l" "Gray" }
    }

    $script:pollCount = 0
    $syncOk = Wait-ForCondition -Description "LAN game sync" -TimeoutSec $TimeoutSeconds -PollMs 3000 -Condition {
        $script:pollCount++

        $gi1 = Get-GameIntrospection -Serial $EMU1
        $gi2 = Get-GameIntrospection -Serial $EMU2
        $script:lastGi1 = $gi1
        $script:lastGi2 = $gi2

        $hostInGame = if ($gi1) { [bool]$gi1.in_game } else { $false }
        $hostNet = if ($gi1) { [bool]$gi1.is_network } else { $false }
        $hostPlayers = if ($gi1) { Get-IntroNumConnected -Intro $gi1 } else { $null }
        $joinInGame = if ($gi2) { [bool]$gi2.in_game } else { $false }
        $joinNet = if ($gi2) { [bool]$gi2.is_network } else { $false }
        $joinPlayers = if ($gi2) { Get-IntroNumConnected -Intro $gi2 } else { $null }

        if ($gi1 -or $gi2) {
            $hostPlayersLabel = if ($null -ne $hostPlayers) { $hostPlayers } else { "?" }
            $joinPlayersLabel = if ($null -ne $joinPlayers) { $joinPlayers } else { "?" }
            Write-Status "  [poll $($script:pollCount)] host in_game=$hostInGame net=$hostNet players=$hostPlayersLabel | join in_game=$joinInGame net=$joinNet players=$joinPlayersLabel" "Gray"
            if ($hostInGame -and $hostNet -and $hostPlayers -ge 2 -and $joinInGame -and $joinNet -and $joinPlayers -ge 2) {
                return $true
            }
        } elseif (Test-Path $logcatFile1) {
            $lines = Get-Content $logcatFile1 -ErrorAction SilentlyContinue
            $hasSync = $lines | Where-Object { $_ -match 'send_sync.*sending SYNC to all' }
            $hasTwoPlayers = $lines | Where-Object { $_ -match 'N_players now 2' }
            $lastMpdiag = ($lines | Where-Object { $_ -match 'MPDIAG' } | Select-Object -Last 1)
            if ($lastMpdiag) {
                Write-Status "  [poll $($script:pollCount)] fallback MPDIAG: $($lastMpdiag.Substring([Math]::Max(0, $lastMpdiag.Length - 80)))" "Gray"
            } else {
                Write-Status "  [poll $($script:pollCount)] introspection unavailable, no MPDIAG yet" "Gray"
            }
            if ($hasSync -and $hasTwoPlayers) {
                return $true
            }
        }

        # Every 3rd poll, show EMU2 proxy/mp state for diagnostics
        if ($script:pollCount % 3 -eq 1 -and (Test-Path $logcatFile2)) {
            $emu2lines = Get-Content $logcatFile2 -ErrorAction SilentlyContinue | Where-Object { $_ -match 'LocalhostProxy|MPDIAG|lan_launch|DXX-MP' } | Select-Object -Last 3
            foreach ($l in $emu2lines) { Write-Status "  [EMU2] $l" "Gray" }
        }
        return $false
    }

    if (-not $syncOk) {
        Write-Status "FAIL: Multiplayer sync never completed" "Red"
        $gi1 = $script:lastGi1
        $gi2 = $script:lastGi2
        if ($gi1) {
            $hostPlayers = Get-IntroNumConnected -Intro $gi1
            if ($null -eq $hostPlayers) { $hostPlayers = "?" }
            Write-Status "  EMU1 state: screen=$($gi1.screen_mode) in_game=$($gi1.in_game) net=$($gi1.is_network) players=$hostPlayers" "Gray"
        }
        if ($gi2) {
            $joinPlayers = Get-IntroNumConnected -Intro $gi2
            if ($null -eq $joinPlayers) { $joinPlayers = "?" }
            Write-Status "  EMU2 state: screen=$($gi2.screen_mode) in_game=$($gi2.in_game) net=$($gi2.is_network) players=$joinPlayers" "Gray"
        }
        if (Test-Path $logcatFile1) {
            Write-Status "  EMU1 MPDIAG lines:" "Gray"
            Get-Content $logcatFile1 -ErrorAction SilentlyContinue | Where-Object { $_ -match 'MPDIAG' } | ForEach-Object { Write-Status "    $_" "Gray" }
        }
        if (Test-Path $logcatFile2) {
            Write-Status "  EMU2 all captured lines:" "Gray"
            Get-Content $logcatFile2 -ErrorAction SilentlyContinue | Select-Object -Last 30 | ForEach-Object { Write-Status "    $_" "Gray" }
        }
        Cleanup; exit 1
    }

    Write-Status "Multiplayer sync completed" "Green"

    # -- Step 5: Verify results --
    Write-Status ""
    Write-Status "--- Phase 5: Verify networking ---" "White"

    # Check relay traffic (relay mode only)
    if ($UseRelay) {
        $relayLog = Join-Path $REPO_ROOT "temp\udp_relay.log"
        $relayLines = @()
        if (Test-Path $relayLog) {
            $relayLines = Get-Content $relayLog -ErrorAction SilentlyContinue
        }
        $emu1ToEmu2 = @($relayLines | Where-Object { $_ -match 'EMU1->EMU2' }).Count
        $emu2ToEmu1 = @($relayLines | Where-Object { $_ -match 'EMU2->EMU1' }).Count
        Write-Status "Relay traffic: EMU1->EMU2: $emu1ToEmu2 packets, EMU2->EMU1: $emu2ToEmu1 packets"
    } else {
        Write-Status "Direct LAN mode -- no relay traffic to verify"
    }

    # Check captured host diagnostics from logcat
    $hostLines = @(Get-Content $logcatFile1 -ErrorAction SilentlyContinue | Where-Object { $_ -match 'MPDIAG|auto_net|lan_launch|LocalhostProxy' })
    Write-Status "Host LAN log ($($hostLines.Count) lines):" "Gray"
    foreach ($line in $hostLines) {
        Write-Status "  $line" "Gray"
    }

    $testPassed = $true
    if ($AllowSecretWarps -and $NoCoopQol) {
        $testPassed = $false
        if ($Game -ne "d2") { throw "Secret travel is a D2 option" }
        foreach ($serial in @($EMU1, $EMU2)) {
            $intro = Get-GameIntrospection -Serial $serial
            if (-not $intro -or $intro.multiplayer.coop_qol -or -not $intro.multiplayer.allow_secret_warps -or
                -not $intro.multiplayer.recovery.active) {
                throw "Secret-world recovery must be active independently of the QoL switch"
            }
        }
        $testPassed = $true
    }

    if ($CoopRewind) {
        $testPassed = $false
        $testPassed = Invoke-CoopRewindScenario -FromClient:$ClientRewind
    }

    if ($RestoreFailure) {
        $testPassed = $false
        $testPassed = Invoke-RestoreFailureScenario
    }

    if ($Flyouts) {
        $scripts = @{
            natural = @('test_coop_flyout_natural_host.jsonc', 'test_coop_flyout_natural_client.jsonc')
            deadline = @('test_coop_flyout_deadline_host.jsonc', 'test_coop_flyout_deadline_client.jsonc')
            force = @('test_coop_flyout_force_host.jsonc', 'test_coop_flyout_force_client.jsonc')
        }[$FlyoutCase]
        $hostScript, $clientScript = $scripts
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $hostScript `
                    -SecondarySerial $EMU2 -SecondaryScript $clientScript -Description "Fly-out $FlyoutCase" -TimeoutSec 90)) {
            throw "Fly-out $FlyoutCase failed"
        }
        $testPassed = $true
        Write-Status "=== CO-OP FLY-OUT TEST PASSED ===" 'Green'
        exit 0
    }

    if ($LevelRestart) {
        $testPassed = $false
        $testPassed = Invoke-CoopLevelRestartScenario
    }

    if ($CountdownSave -and -not $SecretSaveRestore) {
        $testPassed = $false
        foreach ($phase in @('seed', 'save', 'mutate', 'restore')) {
            $hostScript = @{
                seed = 'test_coop_countdown_save_seed.jsonc'
                save = 'test_coop_countdown_save_save.jsonc'
                mutate = 'test_coop_countdown_save_mutate.jsonc'
                restore = 'test_coop_countdown_save_restore.jsonc'
            }[$phase]
            $clientScript = if ($phase -eq 'restore') { 'test_coop_countdown_save_wait.jsonc' } elseif ($phase -eq 'save') { 'test_coop_countdown_save_idle.jsonc' } else { $hostScript }
            if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $hostScript `
                        -SecondarySerial $EMU2 -SecondaryScript $clientScript `
                        -Description "Countdown save: $phase" -TimeoutSec 180)) { throw "Countdown save $phase failed" }
        }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_countdown_save_verify.jsonc' `
                    -SecondarySerial $EMU2 -SecondaryScript 'test_coop_countdown_save_verify.jsonc' `
                    -Description 'Restored countdown and inventory on both peers' -TimeoutSec 30)) { throw 'Countdown save verification failed' }
        $testPassed = $true
    }

    if ($BriefingRestore -or $SecretRollback) {
        $testPassed = $false
        Assert-CoopWorldVisit -Expected 1
        Assert-CoopGameplayFences
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_mdata_capacity.jsonc" `
                    -SecondarySerial $EMU2 -SecondaryScript "test_coop_mdata_capacity.jsonc" `
                    -Description "Maximum reliable payload retry and acknowledgment" -TimeoutSec 30)) {
            throw "Maximum reliable payload did not survive a dropped initial send"
        }
        # Launcher restarts clear logcat; retain the native wire evidence first
        foreach ($serial in @($EMU1, $EMU2)) {
            $capacityOutput = Adb-Dev -Serial $serial -AdbArgs @('logcat', '-d', '-s', 'DXX:I')
            $capacityLines = if ($capacityOutput) { $capacityOutput -split '\r?\n' } else { @() }
            $capacityEvidence = @($capacityLines | Where-Object {
                    $_ -match 'Android full MDATA: packet=\d+ bytes=476 dropped=1 acknowledged=1'
                })
            if ($capacityEvidence.Count -eq 0) { throw "Missing full-payload retry evidence on $serial" }
            foreach ($line in $capacityEvidence) { Write-Status "  ${serial}: $line" }
        }
        $testPassed = $true
    }

    if ($BriefingRestore) {
        $testPassed = $false
        $beforeSave = Get-GameIntrospection -Serial $EMU1
        if (-not $beforeSave -or -not $beforeSave.PSObject.Properties['coop_campaign']) {
            throw "Missing campaign introspection before save"
        }
        $savedCampaign = $beforeSave.coop_campaign
        if ($Game -eq "d2" -and ($savedCampaign.active_level -ne $InitialLevel -or $savedCampaign.generation -lt 1)) {
            throw "D2 did not initialize its campaign context"
        }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_restore_resilience_seed.jsonc" `
                    -SecondarySerial $EMU2 -SecondaryScript "test_coop_restore_resilience_seed.jsonc" `
                    -Description "Inventory before briefing-enabled save" -TimeoutSec 30)) { throw "Could not seed restore inventory" }
        if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName "test_coop_late_join_save.jsonc")) { throw "Could not start save" }
        if (-not (Wait-ForCondition -Description "Briefing-enabled save completes" -TimeoutSec 30 -PollMs 500 -Condition {
                    $result = Get-DeviceAutomationResult -Serial $EMU1
                    return $result -and $result.result -eq "PASS"
                })) { throw "Save failed" }
        $slot = Get-DeviceLatestCoopAutosaveSlot -Serial $EMU1
        if ($slot -lt 0) { throw "Missing saved campaign" }
        foreach ($serial in @($EMU1, $EMU2)) {
            if (-not (Start-SetupActivity -Serial $serial)) { throw "Could not restart launcher" }
            Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "run-as", $PACKAGE, "rm", "-f", "files/introspect.json") -Seconds 5 | Out-Null
        }
        if (-not (Set-DeviceCoopRestoreSlot -Serial $EMU1 -Slot $slot)) { throw "Could not select resume save" }
        Send-MpCommand -Serial $EMU1 -Command "lan_launch" -Extras $hostExtras
        if (-not (Wait-ForCondition -Description "Resume host lobby" -TimeoutSec 40 -PollMs 500 -Condition {
                    $intro = Get-GameIntrospection -Serial $EMU1
                    return $intro -and $intro.is_network -and (Get-IntroNumConnected -Intro $intro) -eq 1
                })) { throw "Resume host did not reach lobby" }
        $lifecycleOutput = Adb-Dev -Serial $EMU1 -AdbArgs @("logcat", "-d", "-s", "DXX-Lifecycle", "ActivityThread")
        $lifecycleLines = if ($lifecycleOutput) { $lifecycleOutput -split '\r?\n' } else { @() }
        $creations = @($lifecycleLines | Where-Object { $_ -match 'DXX-Lifecycle.*create activity=' })
        if ($creations.Count -ne 1 -or ($lifecycleOutput -match 'ServiceConnectionLeaked|DXX-Lifecycle.*destroy activity=')) {
            throw "Cold resume recreated its game activity or leaked its service connection"
        }
        foreach ($line in $lifecycleLines | Where-Object { $_ -match 'DXX-Lifecycle' }) { Write-Status "  Resume lifecycle: $line" }
        if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName "test_coop_restore_autosave_guard.jsonc")) {
            throw "Could not arm autosave rejection check"
        }
        Send-MpCommand -Serial $EMU2 -Command "lan_launch" -Extras $joinExtras
        $script:briefingRestorePoll = 0
        # Initial startup and save application each load the mine. Give them
        # separate test budgets; neither is the briefing reading allowance
        if (-not (Wait-ForCondition -Description "Resume engines finish initial mine load" -TimeoutSec 120 -PollMs 1000 -Condition {
                    $h = Get-GameIntrospection -Serial $EMU1
                    $c = Get-GameIntrospection -Serial $EMU2
                    if (-not $h -or -not $c -or -not $h.in_game -or -not $c.in_game) { return $false }
                    return $h.coop_briefing.suppressed_for_restore -and $c.coop_briefing.suppressed_for_restore
                })) { throw "Resume did not complete initial mine loading" }
        if (-not (Wait-ForCondition -Description "Restore without briefing replay" -TimeoutSec 180 -PollMs 1000 -Condition {
                    ++$script:briefingRestorePoll
                    $complete = $true
                    foreach ($serial in @($EMU1, $EMU2)) {
                        $intro = Get-GameIntrospection -Serial $serial
                        if (-not $intro -or -not $intro.PSObject.Properties['coop_briefing']) {
                            $complete = $false
                            continue
                        }
                        if ($intro.coop_restore.status -eq "error") { throw "Native save restore failed on $serial" }
                        $localPlayers = @($intro.multiplayer.players | Where-Object { $_.is_me })
                        if (-not $localPlayers.Count) {
                            $complete = $false
                            continue
                        }
                        $localPlayer = $localPlayers[0]
                        if ($script:briefingRestorePoll % 5 -eq 1) {
                            Write-Status "  Resume $serial : in_game=$($intro.in_game) phase=$($intro.coop_briefing.phase) restore=$($intro.coop_restore.status) homing=$($localPlayer.homing_ammo)" "Gray"
                        }
                        if ($intro.coop_briefing.presentations_started -ne 0) { throw "Save resume replayed briefing content" }
                        if (-not $intro.in_game -or (Get-IntroNumConnected -Intro $intro) -ne 2 -or
                            $intro.coop_restore.status -ne "idle" -or $intro.coop_briefing.active -or $localPlayer.homing_ammo -ne 6) {
                            $complete = $false
                            continue
                        }
                        if (-not $intro.coop_briefing.enabled -or -not $intro.coop_briefing.suppressed_for_restore) {
                            throw "Resume lost the briefing setting or suppression decision"
                        }
                        if (-not $intro.PSObject.Properties['coop_campaign'] -or
                            $intro.coop_campaign.active_level -ne $savedCampaign.active_level -or
                            $intro.coop_campaign.mission -ne $savedCampaign.mission -or
                            $intro.coop_campaign.generation -ne $savedCampaign.generation -or
                            $intro.coop_campaign.entered_from -ne $savedCampaign.entered_from -or
                            $intro.coop_campaign.base_returnable -ne $savedCampaign.base_returnable -or
                            $intro.coop_campaign.dormant_worlds -ne $savedCampaign.dormant_worlds) {
                            throw "Resume did not restore the saved campaign context"
                        }
                    }
                    return $complete
                })) { throw "Both peers must restore inventory and skip presentation" }
        if (-not (Wait-ForCondition -Description "Restore transfer guard and full-width ACK verification" -TimeoutSec 30 -PollMs 500 -Condition {
                    $result = Get-DeviceAutomationResult -Serial $EMU1
                    return $result -and $result.result -in @("PASS", "FAIL")
                })) { throw "Restore transfer guard did not finish" }
        $autosaveGuard = Get-DeviceAutomationResult -Serial $EMU1
        if (-not $autosaveGuard -or $autosaveGuard.result -ne "PASS") {
            throw "Autosave rejection or full-width packet acknowledgment during restore was not verified"
        }
        Assert-CoopWorldVisit -Expected 2
        Assert-CoopGameplayFences
        $script:lastGi1 = Get-GameIntrospection -Serial $EMU1
        $script:lastGi2 = Get-GameIntrospection -Serial $EMU2
        $testPassed = $true
    }

    if ($UseRelay -and ($emu1ToEmu2 -eq 0 -or $emu2ToEmu1 -eq 0)) {
        Write-Status "FAIL: Relay did not forward traffic in both directions" "Red"
        $testPassed = $false
    }

    # Optionally check in-game state if emulators are still alive
    $gi1 = if ($script:lastGi1) { $script:lastGi1 } else { Get-GameIntrospection -Serial $EMU1 }
    $gi2 = if ($script:lastGi2) { $script:lastGi2 } else { Get-GameIntrospection -Serial $EMU2 }
    if ($gi1) {
        $hostPlayers = Get-IntroNumConnected -Intro $gi1
        if ($null -eq $hostPlayers) { $hostPlayers = "?" }
        Write-Status "Initial sync EMU1: screen=$($gi1.screen_mode) in_game=$($gi1.in_game) net=$($gi1.is_network) players=$hostPlayers game_mode=$($gi1.game_mode)"
    } else {
        Write-Status "EMU1: introspection unavailable (emulator may have crashed during level load)" "Yellow"
    }
    if ($gi2) {
        $joinPlayers = Get-IntroNumConnected -Intro $gi2
        if ($null -eq $joinPlayers) { $joinPlayers = "?" }
        Write-Status "Initial sync EMU2: screen=$($gi2.screen_mode) in_game=$($gi2.in_game) net=$($gi2.is_network) players=$joinPlayers game_mode=$($gi2.game_mode)"
    } else {
        Write-Status "EMU2: introspection unavailable (emulator may have crashed during level load)" "Yellow"
    }
    $hostFinalPlayers = Get-IntroNumConnected -Intro $gi1
    $joinFinalPlayers = Get-IntroNumConnected -Intro $gi2
    if (-not ($gi1 -and $gi1.is_network -and $hostFinalPlayers -ge 2)) {
        Write-Status "FAIL: EMU1 did not report an active two-player network game" "Red"
        $testPassed = $false
    }
    if (-not ($gi2 -and $gi2.is_network -and $joinFinalPlayers -ge 2)) {
        Write-Status "FAIL: EMU2 did not report an active two-player network game" "Red"
        $testPassed = $false
    }

    if ($testPassed) { Assert-GuidebotRoutingSelection }

    if ($testPassed -and $GuidebotOwnership) {
        $testPassed = Invoke-GuidebotOwnershipScenario
    }
    if ($testPassed -and $GuidebotSpawn) {
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_guidebot_spawn_host.jsonc' `
            -SecondarySerial $EMU2 -SecondaryScript 'test_coop_guidebot_spawn_client.jsonc' `
            -Description 'Enter secret mine, deploy missing Guide-Bot from client, repeat, dock and redeploy' -TimeoutSec 240
    }
    if ($testPassed -and $GuidebotClientRelease) {
        $releaseScripts = @{
            cage = 'test_coop_guidebot_client_cage.jsonc'
            deploy = 'test_coop_guidebot_client_deploy.jsonc'
        }
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_guidebot_client_release_host.jsonc' `
            -SecondarySerial $EMU2 -SecondaryScript $releaseScripts[$GuidebotClientRelease] `
            -Description "Client Guide-Bot release via $GuidebotClientRelease retains client ownership" -TimeoutSec 45
    }
    if ($testPassed -and $SavedLateJoin) {
        $testPassed = Invoke-SavedLateJoinScenario
    }
    if ($testPassed -and $VerifyAutomationFailure) {
        if ($Game -ne "d2") { throw "Terminal automation failure fixture requires D2" }
        foreach ($serial in @($EMU1, $EMU2)) {
            if (-not (Start-DeviceGameAutomation -Serial $serial -ScriptName "test_coop_terminal_failure.jsonc")) { throw "Could not start terminal failure probe" }
        }
        if (-not (Wait-ForCondition -Description "Final-step failures remain failed on both peers" -TimeoutSec 30 -PollMs 500 -Condition {
                    foreach ($serial in @($EMU1, $EMU2)) {
                        $result = Get-DeviceAutomationResult -Serial $serial
                        if (-not $result -or $result.result -ne "FAIL" -or
                            $result.reason -ne "Normal exits did not advance together to the next normal mine") { return $false }
                    }
                    return $true
                })) { throw "A final-step failure was lost or overwritten" }
    }
    if ($testPassed -and $MaximumExitProbe) {
        if ($Game -ne 'd2' -or $MissionFile -ne 'max_f' -or $InitialLevel -ne 18 -or -not $AllowSecretWarps) {
            throw 'MaximumExitProbe requires D2 max_f level 18 with AllowSecretWarps'
        }
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_maximum_exit_host.jsonc' `
            -SecondarySerial $EMU2 -SecondaryScript 'test_coop_maximum_exit_client.jsonc' `
            -Description 'Grant delayed client exit after a newer position beyond the doorway' -TimeoutSec 60
    }
    if ($testPassed -and $NormalPhysical) {
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_normal_exit_prepare_host.jsonc" `
                    -SecondarySerial $EMU2 -SecondaryScript "test_coop_normal_exit_prepare_client.jsonc" -Description "Prepare authored normal/secret exits and reject companion touch")) { throw "Normal exit setup failed" }
        $hostExitScript = if ($NormalExitRace) { "test_coop_normal_exit_race_host.jsonc" } else { "test_coop_normal_exit_host.jsonc" }
        $clientExitScript = if ($NormalExitRace) { "test_coop_normal_exit_race_client.jsonc" } else { "test_coop_normal_exit_client.jsonc" }
        if ($NormalReactorDeath) {
            $hostExitScript = "test_coop_normal_reactor_death_host.jsonc"
            $clientExitScript = "test_coop_normal_reactor_death_client.jsonc"
        } elseif ($NormalCountdown) {
            $hostExitScript = $clientExitScript = "test_coop_normal_countdown.jsonc"
        }
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $hostExitScript `
            -SecondarySerial $EMU2 -SecondaryScript $clientExitScript -Description "Normal mine exit/death completion and shared advancement" -TimeoutSec 240
    }
    if ($testPassed -and $SecretExitRace) {
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_world_carry.jsonc" `
                    -SecondarySerial $EMU2 -SecondaryScript "test_coop_world_carry.jsonc" -Description "Seed portable state before secret-winning exit race")) { throw "Secret exit race inventory setup failed" }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_secret_exit_race_prepare_host.jsonc" `
                    -SecondarySerial $EMU2 -SecondaryScript "test_coop_secret_exit_race_prepare_client.jsonc" -Description "Prepare competing exits with the base reactor destroyed")) { throw "Secret exit race setup failed" }
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_secret_exit_race_host.jsonc" `
            -SecondarySerial $EMU2 -SecondaryScript "test_coop_secret_exit_race_client.jsonc" -Description "Secret winner rejects an in-flight normal exit and brings both players into the secret mine" -TimeoutSec 270
    }
    if ($testPassed -and ($SecretAdvance -or $SecretEndgame)) {
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_world_carry.jsonc" `
                    -SecondarySerial $EMU2 -SecondaryScript "test_coop_world_carry.jsonc" -Description "Seed both players before secret departure")) { throw "Advance inventory setup failed" }
        $departure = if ($SecretEndgameModal) { "endgame_modal" } elseif ($SecretEndgame) { "endgame" } else { "advance" }
        $departureScripts = @{
            advance = @('test_coop_secret_advance_prepare.jsonc', 'test_coop_secret_advance_host.jsonc', 'test_coop_secret_advance_client.jsonc')
            endgame = @('test_coop_secret_endgame_prepare.jsonc', 'test_coop_secret_endgame_host.jsonc', 'test_coop_secret_endgame_client.jsonc')
            endgame_modal = @('test_coop_secret_endgame_modal_prepare.jsonc', 'test_coop_secret_endgame_modal_host.jsonc', 'test_coop_secret_endgame_modal_client.jsonc')
            endgame_host_leaves = @('test_coop_secret_endgame_modal_prepare.jsonc', 'test_coop_secret_endgame_host_leaves_host.jsonc', 'test_coop_secret_endgame_host_leaves_client.jsonc')
        }
        $prepareScript = $departureScripts[$departure][0]
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $prepareScript `
                    -SecondarySerial $EMU2 -SecondaryScript $prepareScript -Description "Prepare secret campaign $departure")) { throw "Departure setup failed" }
        if ($SecretEndgameHostLeaves) { $departure = "endgame_host_leaves" }
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $departureScripts[$departure][1] `
            -SecondarySerial $EMU2 -SecondaryScript $departureScripts[$departure][2] -Description "Complete coordinated secret campaign $departure" -TimeoutSec 300
    }
    if ($testPassed -and $TravelGate) {
        if ($Game -ne "d2" -or -not $AllowSecretWarps) { throw "Travel gate probe requires D2 secret warps enabled" }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_travel_gate_arm.jsonc" `
                    -SecondarySerial $EMU2 -SecondaryScript "test_coop_travel_gate_arm.jsonc" -Description "Arm host travel gate on both peers")) { throw "Could not arm travel gate" }
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_travel_gate_host.jsonc" `
            -SecondarySerial $EMU2 -SecondaryScript "test_coop_travel_gate_client.jsonc" -Description "Arbitrate exits, freeze reactor, warn and abort without changing mines" -TimeoutSec 90
        if ($testPassed) {
            $testPassed = Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_travel_normal_host.jsonc" `
                -SecondarySerial $EMU2 -SecondaryScript "test_coop_travel_normal_client.jsonc" -Description "Normal winner blocks secrets and grants individual exits without freezing" -TimeoutSec 60
        }
    }
    if ($testPassed -and $WorldRestore) {
        if ($Game -ne "d2" -or $BriefingRestore) { throw "World restore probe needs a fresh D2 mine" }
        $testPassed = $false
        $worldCampaign = (Get-GameIntrospection -Serial $EMU1).coop_campaign
        if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName "test_coop_world_capture.jsonc")) { throw "Could not capture world" }
        if (-not (Wait-ForCondition -Description "Raw world captured" -TimeoutSec 30 -PollMs 500 -Condition {
                    $r = Get-DeviceAutomationResult -Serial $EMU1
                    return $r -and $r.result -eq "PASS"
                })) { throw "World capture failed" }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_world_carry.jsonc" `
                    -SecondarySerial $EMU2 -SecondaryScript "test_coop_world_carry.jsonc" -Description "Change world and portable state")) { throw "World probe setup failed" }
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_world_restore_host.jsonc" `
            -SecondarySerial $EMU2 -SecondaryScript "test_coop_world_restore_client.jsonc" -Description "Restore world while retaining current players" -TimeoutSec 200
        if ($testPassed) {
            foreach ($serial in @($EMU1, $EMU2)) {
                $afterWorld = Get-GameIntrospection -Serial $serial
                if ($afterWorld.coop_campaign.active_level -ne $worldCampaign.active_level -or
                    $afterWorld.coop_campaign.generation -ne $worldCampaign.generation) { throw "World restore rolled back campaign context" }
            }
        }
    }
    if ($testPassed -and $CoopDeath) {
        $testPassed = Invoke-CoopDeathScenario
    }
    if ($testPassed -and $SecretDisconnectPhase) {
        if ($Game -ne 'd2' -or $InitialLevel -ne 8 -or -not $AllowSecretWarps) { throw 'Secret disconnect requires D2 level 8 and secret warps' }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_world_travel_prepare.jsonc' `
                    -SecondarySerial $EMU2 -SecondaryScript 'test_coop_world_travel_prepare.jsonc' -Description 'Prepare client disconnect during secret travel')) { throw 'Disconnect setup failed' }
        $disconnectScripts = @{
            freezing = 'test_coop_disconnect_freezing.jsonc'
            capturing = 'test_coop_disconnect_capturing.jsonc'
            loading = 'test_coop_disconnect_loading.jsonc'
            committed = 'test_coop_disconnect_committed.jsonc'
            release = 'test_coop_disconnect_release.jsonc'
        }
        if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName $disconnectScripts[$SecretDisconnectPhase])) { throw 'Could not start disconnect scenario' }
        $testPassed = Wait-ForCondition -Description "Host finishes secret travel after client disconnect in $SecretDisconnectPhase" -TimeoutSec 240 -Condition {
            $result = Get-DeviceAutomationResult -Serial $EMU1
            if ($result -and $result.result -eq 'FAIL') { throw "Disconnect scenario failed: $($result.reason)" }
            return $result -and $result.result -eq 'PASS'
        }
        Write-DeviceAutomationDiagnostics -Serial $EMU1
    }
    if ($testPassed -and $SecretWorld) {
        if ($Game -ne "d2" -or -not $AllowSecretWarps -or $BriefingRestore) {
            throw "Secret world probe requires a fresh D2 co-op game with secret warps enabled"
        }
        $sourceCampaign = (Get-GameIntrospection -Serial $EMU1).coop_campaign
        if ($GuidebotTravel) {
            if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_guidebot_travel_seed_host.jsonc' `
                        -SecondarySerial $EMU2 -SecondaryScript 'test_coop_guidebot_travel_seed_client.jsonc' `
                        -Description 'Release companion for client and seed carried health')) { throw 'Guidebot travel setup failed' }
        }
        if ($SecretCrossRestore) {
            if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_base_save_seed.jsonc' `
                        -SecondarySerial $EMU2 -SecondaryScript 'test_coop_base_save_seed.jsonc' `
                        -Description 'Seed distinct normal-save inventories')) { throw 'Base save seed failed' }
            if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_base_save_host.jsonc' `
                        -SecondarySerial $EMU2 -SecondaryScript 'test_coop_secret_save_idle.jsonc' `
                        -Description 'Save the normal mine before secret entry')) { throw 'Base save failed' }
        }
        if ($SecretRollback) {
            if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_world_carry.jsonc" `
                        -SecondarySerial $EMU2 -SecondaryScript "test_coop_world_carry.jsonc" -Description "Seed player state before failed secret travel")) { throw "Rollback fixture setup failed" }
            if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_world_rollback_tag.jsonc" `
                        -SecondarySerial $EMU2 -SecondaryScript "test_coop_world_rollback_tag.jsonc" -Description "Bind rollback fixture powerup on both peers before publishing its ledger")) { throw "Rollback object tagging failed" }
            if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_world_rollback_prepare_host.jsonc" `
                        -SecondarySerial $EMU2 -SecondaryScript "test_coop_world_travel_prepare.jsonc" -Description "Arm travel and waiting-peer packet loss before destination failure")) { throw "Rollback gate setup failed" }
            if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_world_rollback_host.jsonc" `
                        -SecondarySerial $EMU2 -SecondaryScript "test_coop_world_rollback_client.jsonc" -Description "Fail client after destination load, restore the source on both peers" -TimeoutSec 360)) { throw "Source checkpoint rollback failed" }
            $expectedVisit = [uint64]3 # Initial mine, failed destination, then rollback
            Assert-CoopWorldVisit -Expected $expectedVisit
            Assert-CoopGameplayFences
            if ($GuidebotTravel) {
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_guidebot_travel_released.jsonc' `
                            -SecondarySerial $EMU2 -SecondaryScript 'test_coop_guidebot_travel_navigation_client.jsonc' `
                            -Description 'Rollback preserves released companion and client control')) { throw 'Guidebot rollback failed' }
            }
        }
        $travelLegs = if ($SecretRevisit) { @("enter", "return", "revisit", "return") } else { @("enter", "return") }
        $expectedGeneration = $sourceCampaign.generation
        foreach ($leg in $travelLegs) {
            ++$expectedGeneration
            $testPassed = $false
            if ($GuidebotTravel -and $leg -eq 'revisit') {
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_guidebot_travel_docked.jsonc' `
                            -SecondarySerial $EMU2 -SecondaryScript 'test_coop_guidebot_travel_dock_client.jsonc' `
                            -Description 'Dock the travelling companion before revisiting the secret')) { throw 'Guidebot docking failed' }
            }
            if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_world_carry.jsonc" `
                        -SecondarySerial $EMU2 -SecondaryScript "test_coop_world_carry.jsonc" -Description "Seed portable state for secret $leg")) { throw "Secret portable-state setup failed" }
            $hostPrepare = if ($SecretPhysical) { "test_coop_physical_prepare_host.jsonc" } else { "test_coop_world_travel_prepare.jsonc" }
            $clientPrepare = if ($SecretPhysical) { "test_coop_physical_prepare_client.jsonc" } else { "test_coop_world_travel_prepare.jsonc" }
            $clientTravel = if ($SecretPhysical) { "test_coop_physical_travel_client.jsonc" } else { "test_coop_world_travel_client.jsonc" }
            $hostTravel = "test_coop_world_travel_host.jsonc"
            if ($SecretDying) {
                $hostDies = $leg -ne "return"
                $expectScript = if ($hostDies) { "test_coop_dying_expect_host.jsonc" } else { "test_coop_dying_expect_client.jsonc" }
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $expectScript `
                            -SecondarySerial $EMU2 -SecondaryScript $expectScript -Description "Record expected dying player before secret $leg")) { throw "Dying travel setup failed" }
                if (-not $hostDies) {
                    $hostPrepare = "test_coop_physical_prepare_client.jsonc"
                    $clientPrepare = "test_coop_physical_prepare_host.jsonc"
                }
                $hostTravel = if ($hostDies) { "test_coop_dying_travel_dead.jsonc" } else { "test_coop_dying_travel_alive.jsonc" }
                $clientTravel = if ($hostDies) { "test_coop_dying_travel_alive.jsonc" } else { "test_coop_dying_travel_dead.jsonc" }
            }
            if (($SecretReactorDeath -or $SecretCountdown) -and $leg -eq "return") {
                # Countdown expiry must not park a living player at an exit
                $hostPrepare = if ($SecretCountdown) { "test_coop_world_travel_prepare.jsonc" } else { "test_coop_physical_prepare_client.jsonc" }
                $clientPrepare = if ($SecretCountdown) { "test_coop_world_travel_prepare.jsonc" } else { "test_coop_physical_prepare_host.jsonc" }
                $hostTravel = if ($SecretCountdown) { "test_coop_reactor_countdown.jsonc" } else { "test_coop_reactor_death_host.jsonc" }
                $clientTravel = if ($SecretCountdown) { "test_coop_reactor_countdown.jsonc" } else { "test_coop_reactor_death_client.jsonc" }
            }
            if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $hostPrepare `
                        -SecondarySerial $EMU2 -SecondaryScript $clientPrepare -Description "Prepare secret $leg")) { throw "Secret gate setup failed" }
            if (($SecretReactorDeath -or $SecretCountdown) -and $leg -eq "return") {
                $reactorPrepare = if ($SecretCountdown) { "test_coop_reactor_countdown_prepare.jsonc" } else { "test_coop_reactor_death_prepare.jsonc" }
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $reactorPrepare `
                            -SecondarySerial $EMU2 -SecondaryScript $reactorPrepare -Description "Prepare secret reactor deaths before $leg")) { throw "Secret reactor setup failed" }
            }
            if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $hostTravel `
                        -SecondarySerial $EMU2 -SecondaryScript $clientTravel -Description "Host prepares, transfers and commits secret $leg" -TimeoutSec 270)) { throw "Secret world $leg failed" }
            # Introspection is serviced on an engine frame; a slow frame can
            # leave the previous visit's JSON in place after automation finishes
            if ($SecretRollback) {
                ++$expectedVisit
                Assert-CoopWorldVisit -Expected $expectedVisit
                Assert-CoopGameplayFences
            }
            if (-not (Wait-ForCondition -Description "Both committed campaigns visible after $leg" -TimeoutSec 30 -PollMs 1000 -Condition {
                        $preparedChecksum = 0
                        $checkpointChecksum = 0
                        $arrivalChecksum = 0
                        foreach ($serial in @($EMU1, $EMU2)) {
                            $state = Get-GameIntrospection -Serial $serial
                            if (-not $state) { return $false }
                            $campaign = $state.coop_campaign
                            if ($leg -ne "return") {
                                if ($campaign.active_level -ge 0 -or $campaign.entered_from -ne $sourceCampaign.active_level -or
                                    -not $campaign.base_returnable -or $campaign.generation -ne $expectedGeneration) { return $false }
                            } elseif ($campaign.active_level -ne $sourceCampaign.active_level -or $campaign.base_returnable -or
                                $campaign.generation -ne $expectedGeneration) { return $false }
                            if (-not $state.multiplayer.allow_secret_warps -or $state.coop_briefing.enabled -ne [bool]$Briefings) {
                                throw "Secret travel changed the independent server settings"
                            }
                            if (-not $Briefings -and ($state.coop_briefing.presentations_started -ne 0 -or $state.coop_briefing.active)) {
                                throw "Secret travel opened a briefing while the setting was disabled"
                            }
                            if (-not $state.coop_travel.prepared -or -not $state.coop_travel.prepared_checksum) { return $false }
                            # coop_travel.c envelope: 24 + 8*172 portable + 8 companion + 8 save framing
                            if (-not $state.coop_travel.checkpoint_ready -or -not $state.coop_travel.checkpoint_checksum -or
                                $state.coop_travel.checkpoint_size -le 1416) { return $false }
                            if ($checkpointChecksum -and $checkpointChecksum -ne $state.coop_travel.checkpoint_checksum) {
                                throw "Peers retained different source checkpoints"
                            }
                            $checkpointChecksum = $state.coop_travel.checkpoint_checksum
                            if (-not $state.coop_travel.arrivals_placed -or -not $state.coop_travel.arrival_checksum) { return $false }
                            if ($arrivalChecksum -and $arrivalChecksum -ne $state.coop_travel.arrival_checksum) {
                                throw "Peers planned different team arrival positions"
                            }
                            $arrivalChecksum = $state.coop_travel.arrival_checksum
                            if ($state.coop_travel.portable_received -ne 3 -or -not $state.coop_travel.portable_checksum) { return $false }
                            if ($preparedChecksum -and $preparedChecksum -ne $state.coop_travel.prepared_checksum) {
                                throw "Peers committed different prepared campaigns"
                            }
                            $preparedChecksum = $state.coop_travel.prepared_checksum
                        }
                        return $true
                    })) { throw "Invalid committed campaigns after secret $leg" }
            if ($GuidebotTravel) {
                $buddyCheck = if ($expectedGeneration -ge $sourceCampaign.generation + 3) { 'test_coop_guidebot_travel_docked.jsonc' } else { 'test_coop_guidebot_travel_released.jsonc' }
                $clientBuddyCheck = if ($expectedGeneration -ge $sourceCampaign.generation + 3) { $buddyCheck } else { 'test_coop_guidebot_travel_navigation_client.jsonc' }
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript $buddyCheck `
                            -SecondarySerial $EMU2 -SecondaryScript $clientBuddyCheck `
                            -Description "Verify one client-owned companion after $leg")) { throw 'Guidebot travel verification failed' }
                if ($expectedGeneration -eq $sourceCampaign.generation + 4) {
                    if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_guidebot_travel_released.jsonc' `
                                -SecondarySerial $EMU2 -SecondaryScript 'test_coop_guidebot_travel_redeploy_client.jsonc' `
                                -Description 'Redeploy and navigate after docked return')) { throw 'Guidebot redeploy after travel failed' }
                }
            }
            if ($SecretRewind -and $leg -eq "enter") {
                if (-not (Invoke-CoopRewindScenario -FromClient)) { throw "Secret client rewind failed" }
            }
            if ($SecretRestart -and $leg -eq "enter") {
                if (-not (Invoke-CoopLevelRestartScenario)) { throw "Secret level restart failed" }
            }
            if ($SecretDeath -and $leg -eq "enter") {
                if (-not (Invoke-CoopDeathScenario)) { throw "Secret death or survivor verification failed" }
            }
            if ($SecretSaveRestore -and $leg -eq "enter") {
                if ($CountdownSave -and -not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_countdown_save_seed.jsonc' `
                            -SecondarySerial $EMU2 -SecondaryScript 'test_coop_countdown_save_seed.jsonc' `
                            -Description 'Destroy the secret reactor before saving')) { throw 'Secret countdown seed failed' }
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_secret_save_seed.jsonc" `
                            -SecondarySerial $EMU2 -SecondaryScript "test_coop_secret_save_seed.jsonc" `
                            -Description "Seed distinct inventories before settled secret save")) { throw "Secret save seed failed" }
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_secret_save_host.jsonc" `
                            -SecondarySerial $EMU2 -SecondaryScript "test_coop_secret_save_idle.jsonc" `
                            -Description "Open save/load menus and save the settled secret mine")) { throw "Secret save or menu access failed" }
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_secret_save_mutate.jsonc" `
                            -SecondarySerial $EMU2 -SecondaryScript "test_coop_secret_save_mutate.jsonc" `
                            -Description "Change both inventories after secret save")) { throw "Secret save mutation failed" }
                if ($CountdownSave -and -not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_countdown_save_mutate.jsonc' `
                            -SecondarySerial $EMU2 -SecondaryScript 'test_coop_countdown_save_mutate.jsonc' `
                            -Description 'Change the secret countdown after saving')) { throw 'Secret countdown mutation failed' }
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_secret_load_host.jsonc" `
                            -SecondarySerial $EMU2 -SecondaryScript "test_coop_secret_load_client.jsonc" `
                            -Description "Restore both players and dormant base from the secret save" -TimeoutSec 210)) { throw "Secret save restore failed" }
                if ($CountdownSave -and -not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_countdown_save_verify.jsonc' `
                            -SecondarySerial $EMU2 -SecondaryScript 'test_coop_countdown_save_verify.jsonc' `
                            -Description 'Verify the restored secret reactor timer and both inventories')) { throw 'Secret countdown verification failed' }
                if ($SecretCrossRestore) {
                    if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_base_load_host.jsonc' `
                                -SecondarySerial $EMU2 -SecondaryScript 'test_coop_base_load_client.jsonc' `
                                -Description 'Load the normal save from the secret mine' -TimeoutSec 210)) { throw 'Secret-to-normal save load failed' }
                    if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_secret_load_host.jsonc' `
                                -SecondarySerial $EMU2 -SecondaryScript 'test_coop_secret_load_client.jsonc' `
                                -Description 'Load the secret save from the normal mine' -TimeoutSec 210)) { throw 'Normal-to-secret save load failed' }
                }
                if ($SecretColdResume) {
                    $secretBeforeRestart = Get-GameIntrospection -Serial $EMU1
                    $savedSecretCampaign = $secretBeforeRestart.coop_campaign | ConvertTo-Json -Depth 10 -Compress
                    if ($secretBeforeRestart.coop_campaign.active_level -ge 0 -or
                        $secretBeforeRestart.coop_campaign.worlds.Count -lt 1) { throw "Missing dormant base before cold resume" }
                    foreach ($serial in @($EMU1, $EMU2)) {
                        if (-not (Start-SetupActivity -Serial $serial)) { throw "Could not restart secret-save launcher" }
                        Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "run-as", $PACKAGE, "rm", "-f", "files/introspect.json") -Seconds 5 | Out-Null
                        Adb-Dev-Timeout -Serial $serial -AdbArgs @(
                            "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
                            "--es", "command", "write_bool_pref", "--es", "key", "'dlog_coop desync_enabled'", "--ez", "value", "true"
                        ) -Seconds 10 | Out-Null
                    }
                    if (-not (Set-DeviceCoopRestoreSlot -Serial $EMU1 -Slot 0)) { throw "Could not select the manual secret save" }
                    # Secret save selection uses normal level 1 for network startup
                    $coldHostExtras = @($hostExtras)
                    $coldHostExtras[[Array]::IndexOf($coldHostExtras, "level_num") + 1] = "1"
                    $coldJoinExtras = @($joinExtras)
                    $coldJoinExtras[[Array]::IndexOf($coldJoinExtras, "level_num") + 1] = "1"
                    Send-MpCommand -Serial $EMU1 -Command "lan_launch" -Extras $coldHostExtras
                    if (-not (Wait-ForCondition -Description "Cold secret resume host lobby" -TimeoutSec 60 -PollMs 500 -Condition {
                                $intro = Get-GameIntrospection -Serial $EMU1
                                return $intro -and $intro.is_network -and (Get-IntroNumConnected -Intro $intro) -eq 1
                            })) { throw "Cold secret resume host did not reach its lobby" }
                    Send-MpCommand -Serial $EMU2 -Command "lan_launch" -Extras $coldJoinExtras
                    if (-not (Wait-ForCondition -Description "Cold resume restores secret inventory and dormant base without briefings" -TimeoutSec 240 -PollMs 1000 -Condition {
                                $ready = $true
                                foreach ($serial in @($EMU1, $EMU2)) {
                                    $intro = Get-GameIntrospection -Serial $serial
                                    if ($intro -and $intro.current_level_num -lt 0 -and -not $intro.is_network) {
                                        throw "Peer left the network while cold-restoring the secret mine: $serial"
                                    }
                                    if (-not $intro -or -not $intro.in_game -or -not $intro.is_network -or
                                        (Get-IntroNumConnected -Intro $intro) -ne 2) { $ready = $false; continue }
                                    if ($intro.coop_restore.status -eq "error") { throw "Cold secret restore failed on $serial" }
                                    if ($intro.coop_briefing.presentations_started -ne 0) { throw "Cold secret resume replayed a briefing" }
                                    if ($intro.coop_restore.status -ne "idle" -or $intro.coop_restore.transfer_busy -or
                                        $intro.time_paused -or $intro.coop_travel.active -or $intro.coop_campaign.active_level -ge 0) { $ready = $false; continue }
                                    if (-not $intro.coop_briefing.enabled -or -not $intro.coop_briefing.suppressed_for_restore -or
                                        -not $intro.multiplayer.allow_secret_warps) { throw "Cold secret resume lost its server options" }
                                    if (($intro.coop_campaign | ConvertTo-Json -Depth 10 -Compress) -ne $savedSecretCampaign) {
                                        throw "Cold secret resume changed campaign metadata or dormant world bytes"
                                    }
                                    $slotIndex = if ($serial -eq $EMU1) { 0 } else { 1 }
                                    if ($intro.player.score -ne 4321 + $slotIndex -or
                                        $intro.player.secondary_ammo[1] -ne 6 + $slotIndex) { throw "Cold secret resume lost saved local inventory on $serial" }
                                }
                                return $ready
                            })) { throw "Both peers must cold-resume the saved secret mine" }
                    if ($CountdownSave -and -not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_countdown_save_verify.jsonc' `
                                -SecondarySerial $EMU2 -SecondaryScript 'test_coop_countdown_save_verify.jsonc' `
                                -Description 'Cold resume preserves the secret countdown and resumes its clock')) { throw 'Cold secret countdown restore failed' }
                    if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_secret_cold_resume.jsonc" `
                                -SecondarySerial $EMU2 -SecondaryScript "test_coop_secret_cold_resume.jsonc" `
                                -Description "Restore measured pre-travel expectations for the dormant base")) { throw "Cold secret return setup failed" }
                }
            }
            if ($SecretCountdown -and $leg -eq "return") {
                $hostSyncLog = (Adb-Dev -Serial $EMU1 -AdbArgs @('logcat', '-d', '-s', 'DXX-DLOG')) -join "`n"
                if ($hostSyncLog -notmatch 'test delaying host world apply:' -or
                    $hostSyncLog -notmatch 'network join deferred for coop transition: player=-?\d+ source=-\d+ requested=[1-9]\d*') {
                    throw "Countdown return did not exercise an early sync request against the destroyed source"
                }
            }
            if ($DestroyedGearRestore -and $leg -eq "return") {
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_retired_save_seed.jsonc" `
                            -SecondarySerial $EMU2 -SecondaryScript "test_coop_retired_save_seed.jsonc" `
                            -Description "Record destroyed-world recovery credit before saving")) { throw "Retired gear save seed failed" }
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_retired_save_host.jsonc" `
                            -SecondarySerial $EMU2 -SecondaryScript "test_coop_secret_save_idle.jsonc" `
                            -Description "Save the settled base with retired secret gear")) { throw "Retired gear save failed" }
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_retired_save_mutate.jsonc" `
                            -SecondarySerial $EMU2 -SecondaryScript "test_coop_retired_save_mutate.jsonc" `
                            -Description "Change both inventories after the retired gear save")) { throw "Retired gear mutation failed" }
                if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_retired_load_host.jsonc" `
                            -SecondarySerial $EMU2 -SecondaryScript "test_coop_retired_load_client.jsonc" `
                            -Description "Restore exact recovery records and both player inventories" -TimeoutSec 210)) { throw "Retired gear restore failed" }
            }
            $testPassed = $true
        }
    }
    if ($testPassed -and $RestoreResilience) {
        $testPassed = Invoke-RestoreResilienceScenario
    }
    if ($testPassed -and $RestoreSavePath) {
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_restore_resilience_verify.jsonc" `
            -SecondarySerial $EMU2 -SecondaryScript "test_coop_restore_resilience_verify.jsonc" -Description "provided cooperative save completes on both peers" -TimeoutSec 90
        if (-not $testPassed) { throw "Provided save did not restore" }
        foreach ($serial in @($EMU1, $EMU2)) {
            $gamePid = (Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "pidof", "${PACKAGE}:game") -Seconds 5).Trim()
            $files = Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "run-as", $PACKAGE, "ls", "files/tombstones") -Seconds 5
            $reports = @($files -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -match "^crash_error_restore_.*_${gamePid}_\d+\.txt$" })
            if ($reports.Count -ne 1) { throw "Expected one recovery report for provided save" }
            $body = Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/tombstones/$($reports[0])") -Seconds 5
            if ($body -notmatch "Save restore recovered" -or $body -notmatch "Visible window: 1") { throw "Provided save failed to recover visibly" }
            [IO.File]::WriteAllText((Join-Path $REPO_ROOT "temp/coop-provided-$serial-report.txt"), $body)
            $intro = Get-GameIntrospection -Serial $serial
            $rawIntro = Adb-Dev-Timeout -Serial $serial -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/introspect.json") -Seconds 5
            [IO.File]::WriteAllText((Join-Path $REPO_ROOT "temp/coop-provided-$serial-introspect.json"), $rawIntro)
            if ($intro.current_level_num -ne $InitialLevel) { throw "Provided save restored the wrong level" }
        }
        if ((Get-FileHash -LiteralPath $RestoreSavePath -Algorithm SHA256).Hash -ne $script:ProvidedSaveHash) { throw "Original supplied save changed" }
        $testPassed = Wait-BidirectionalPdata -FirstSerial $EMU1 -FirstRemoteSlot 1 -SecondSerial $EMU2 -SecondRemoteSlot 0 -Description "network updates after supplied save restore"
    }
    if ($testPassed -and $RestoreReportCase) {
        $testPassed = Invoke-RestoreReportScenario
    }
    if ($testPassed -and $RestoreStatus) {
        $testPassed = Invoke-RestoreStatusScenario
    }
    if ($testPassed -and $GuidebotHostObserver) {
        $testPassed = Invoke-GuidebotHostObserverScenario
    }
    if ($testPassed -and $GuidebotSlotRemapRestore -and $SpewPickup) {
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_pickup_seed.jsonc" -SecondarySerial $EMU2 -SecondaryScript "test_coop_pickup_trace.jsonc" -Description "nonempty recovery ledger before save" -TimeoutSec 15
    }
    if ($testPassed -and $GuidebotSlotRemapRestore) {
        $testPassed = Invoke-GuidebotSlotRemapRestoreScenario
    }
    if ($testPassed -and $HostMigration) {
        $testPassed = Invoke-HostMigrationScenario
    }

    if ($testPassed -and ($SpewPickup -or $SpewPartialPickup)) {
        $pickupHost = if ($GuidebotSlotRemapRestore) { $EMU2 } else { $EMU1 }
        $pickupClient = if ($GuidebotSlotRemapRestore) { $EMU1 } else { $EMU2 }
        $pickupScript = if ($SpewPartialPickup) { "test_coop_partial_pickup_client.jsonc" } else { "test_coop_respawn_pickup_client.jsonc" }
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $pickupHost -PrimaryScript "test_coop_respawn_pickup_host.jsonc" -SecondarySerial $pickupClient -SecondaryScript $pickupScript -Description "client death, respawn and approach to owned spew" -TimeoutSec 60
        if ($testPassed) {
            $testPassed = Wait-ForCondition -Description "client collects owned homing spew" -TimeoutSec 20 -PollMs 1000 -Condition {
                $h = Get-GameIntrospection -Serial $pickupHost
                $c = Get-GameIntrospection -Serial $pickupClient
                if (-not $h -or -not $c) { return $false }
                $remote = @($h.multiplayer.players | Where-Object { -not $_.is_me -and $_.connected -eq 1 })[0]
                $local = @($c.multiplayer.players | Where-Object { $_.is_me })[0]
                $expectedAmmo = if ($SpewPartialPickup) { $local.homing_ammo -eq 10 -and $h.multiplayer.recovery.credit_homing -eq 3 -and $c.multiplayer.recovery.credit_homing -eq 3 } else { $local.homing_ammo -ge 4 -and $local.homing_ammo -le 6 }
                return $expectedAmmo -and
                $local.homing_ammo -eq $remote.homing_ammo -and
                $h.multiplayer.recovery.epoch -eq $c.multiplayer.recovery.epoch
            }
            if (-not $testPassed) {
                foreach ($serial in @($pickupHost, $pickupClient)) {
                    $last = Get-GameIntrospection -Serial $serial
                    @{ serial = $serial; multiplayer = $last.multiplayer } | ConvertTo-Json -Depth 8 | Write-Output
                }
            }
        }
    }

    if ($testPassed -and $SpewRecovery) {
        $testPassed = Invoke-SpewRecoveryScenario
    }

    if ($testPassed -and $Endgame) {
        $fastSerial = if ($EndgameClientFirst) { $EMU2 } else { $EMU1 }
        $slowSerial = if ($EndgameClientFirst) { $EMU1 } else { $EMU2 }
        $fastContent = if ($EndgameContent -eq "builtin") { $Game } else { $EndgameContent }
        $slowContent = if ($EndgameContent -eq "builtin") { $Game } else { "custom" }
        $fastScripts = @{
            d1 = 'test_coop_endgame_d1_fast.jsonc'
            d2 = 'test_coop_endgame_d2_fast.jsonc'
            custom = 'test_coop_endgame_custom_fast.jsonc'
            missing = 'test_coop_endgame_missing_fast.jsonc'
        }
        $slowScripts = @{
            d1 = 'test_coop_endgame_d1_slow.jsonc'
            d2 = 'test_coop_endgame_d2_slow.jsonc'
            custom = 'test_coop_endgame_custom_slow.jsonc'
        }
        $observerHostScripts = @{
            d1 = 'test_coop_endgame_d1_observer_host.jsonc'
            d2 = 'test_coop_endgame_d2_observer_host.jsonc'
        }
        $fastScript = $fastScripts[$fastContent]
        $slowScript = $slowScripts[$slowContent]
        if ($EndgameBoss) { $fastScript = "test_coop_endgame_boss_host.jsonc"; $slowScript = "test_coop_endgame_boss_client.jsonc" }
        if ($EndgameObserverHost) { $fastScript = $observerHostScripts[$Game]; $slowScript = $slowScripts[$Game] }
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $fastSerial -PrimaryScript $fastScript `
            -SecondarySerial $slowSerial -SecondaryScript $slowScript `
            -Description "Independent campaign ending with $fastSerial returning first" -TimeoutSec 90 -IndependentEndgame
    }

    if ($testPassed -and $D1LevelTransition) {
        $testPassed = Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript 'test_coop_d1_transition_host.jsonc' `
            -SecondarySerial $EMU2 -SecondaryScript 'test_coop_d1_transition_client.jsonc' `
            -Description 'First Strike flyout, score, briefing and playable level 2' -TimeoutSec 180
        if ($testPassed) { Assert-PostTransitionAndroidControls }
    }

    if ($testPassed -and $BriefingPalette) {
        foreach ($serial in @($EMU1, $EMU2)) {
            $intro = Get-GameIntrospection -Serial $serial
            if (-not $intro.in_game -or -not $intro.coop_briefing.palette_changed -or
                -not $intro.coop_briefing.palette_restored) {
                throw "Expected a changed briefing palette restored to gameplay on $serial"
            }
            Write-Status "Verified changed briefing palette restored on $serial" 'Green'
        }
    }

    if ($testPassed) { Assert-GuidebotRoutingSelection }

    # Stop logcat capture
    try { if ($logcatProc1 -and -not $logcatProc1.HasExited) { Stop-Process -Id $logcatProc1.Id -Force } } catch {}
    try { if ($logcatProc2 -and -not $logcatProc2.HasExited) { Stop-Process -Id $logcatProc2.Id -Force } } catch {}

    Write-Status ""
    if ($testPassed) {
        Write-Status "=== LAN MP TEST PASSED ===" "Green"
    } else {
        Write-Status "=== LAN MP TEST FAILED ===" "Red"
    }

} finally {
    Cleanup
    if (-not $testPassed) {
        if (Test-Path $script:LogFile) {
            Get-Content $script:LogFile -ErrorAction SilentlyContinue | Write-Output
        }
    }
}

exit $(if ($testPassed) { 0 } else { 1 })

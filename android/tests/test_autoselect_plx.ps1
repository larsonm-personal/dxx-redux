#!/usr/bin/env pwsh
# test_autoselect_plx.ps1 -- Integration test for weapon autoselect file handling.
#
# Verifies:
#   1. D1 .plx weapon reorder section read/write format
#   2. D2 .plr pilot file exists and has valid binary header
#   3. App installs and setup introspection works
#
# Usage:
#   .\android\tests\test_autoselect_plx.ps1

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\..\test_helpers.ps1"

$pass = 0
$fail = 0

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if ($Condition) {
        Write-Status "  PASS: $Message" -Color Green
        $script:pass++
    } else {
        Write-Status "  FAIL: $Message" -Color Red
        $script:fail++
    }
}

Write-Status "=== Test: Weapon Autoselect File Handling ===" -Color Cyan

# -- Ensure emulator is healthy --
Ensure-EmulatorHealthy

# -- Verify app is installed --
$installed = Adb -AdbArgs @("shell", "pm", "list", "packages", $PACKAGE)
Assert-True ($installed -match $PACKAGE) "App is installed"

# -- Test 1: D1 .plx weapon reorder format --
Write-Status "--- D1 .plx weapon reorder format ---" -Color Yellow

# Read existing D1 .plx file
$d1plx = Adb -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/d1x-redux/Players/player.plx")
$hasSection = [bool]($d1plx -match "\[weapon reorder\]")
Assert-True $hasSection "D1 .plx has [weapon reorder] section"

# Parse primary and secondary lines
$primaryLine = ($d1plx -split "`n" | Where-Object { $_ -match "^primary=" }) | Select-Object -First 1
$secondaryLine = ($d1plx -split "`n" | Where-Object { $_ -match "^secondary=" }) | Select-Object -First 1

Assert-True ($null -ne $primaryLine) "D1 .plx has primary= line"
Assert-True ($null -ne $secondaryLine) "D1 .plx has secondary= line"

if ($primaryLine) {
    # D1 primary should have 7 hex values (weapon priority indices)
    $primVals = ($primaryLine -replace "primary=","").Trim() -split ","
    Assert-True ($primVals.Count -eq 7) "D1 primary has 7 entries (got $($primVals.Count))"

    # All entries should be valid hex values
    $allHex = $primVals | Where-Object { $_ -match '^0x[0-9a-fA-F]+$' }
    Assert-True ($allHex.Count -eq $primVals.Count) "D1 primary entries are all hex values"
}

if ($secondaryLine) {
    # D1 secondary should have 6 hex values
    $secVals = ($secondaryLine -replace "secondary=","").Trim() -split ","
    Assert-True ($secVals.Count -eq 6) "D1 secondary has 6 entries (got $($secVals.Count))"

    $allHex = $secVals | Where-Object { $_ -match '^0x[0-9a-fA-F]+$' }
    Assert-True ($allHex.Count -eq $secVals.Count) "D1 secondary entries are all hex values"
}

# -- Test 2: Write and verify D1 .plx round-trip --
Write-Status "--- D1 .plx round-trip test ---" -Color Yellow

# Backup original .plx
$backupPath = "files/d1x-redux/Players/player.plx.bak"
Adb -AdbArgs @("shell", "run-as", $PACKAGE, "cp", "files/d1x-redux/Players/player.plx", $backupPath) | Out-Null

# Read the current primary line to construct a valid sed pattern
$origPrimary = ($primaryLine -replace "primary=","").Trim()
# Reverse the comma-separated values for a detectable modification
$reversed = ($origPrimary -split "," | ForEach-Object { $_ }) 
[array]::Reverse($reversed)
$reversedStr = $reversed -join ","

Adb -AdbArgs @("shell", "run-as", $PACKAGE, "sed", "-i",
    "s/primary=$origPrimary/primary=$reversedStr/",
    "files/d1x-redux/Players/player.plx") | Out-Null

# Read back and verify modification
$d1plxMod = Adb -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/d1x-redux/Players/player.plx")
$modPrimary = ($d1plxMod -split "`n" | Where-Object { $_ -match "^primary=" }) | Select-Object -First 1
$hasReversed = [bool]($modPrimary -match [regex]::Escape("primary=$reversedStr"))
Assert-True $hasReversed "D1 .plx round-trip: modified primary order preserved"

# Restore original
Adb -AdbArgs @("shell", "run-as", $PACKAGE, "cp", $backupPath, "files/d1x-redux/Players/player.plx") | Out-Null
Adb -AdbArgs @("shell", "run-as", $PACKAGE, "rm", $backupPath) | Out-Null

# Verify restore
$d1plxRestored = Adb -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/d1x-redux/Players/player.plx")
$restoredPrimary = ($d1plxRestored -split "`n" | Where-Object { $_ -match "^primary=" }) | Select-Object -First 1
$isRestored = [bool]($restoredPrimary -match [regex]::Escape("primary=$origPrimary"))
Assert-True $isRestored "D1 .plx restore: original primary order intact"

# -- Test 3: D2 .plr binary header (skip if D2 not set up) --
Write-Status "--- D2 .plr binary header ---" -Color Yellow

$d2plrExists = Adb -AdbArgs @("shell", "run-as", $PACKAGE, "test", "-f", "files/d2x-redux/Players/player.plr", "&&", "echo", "yes")
if (-not ($d2plrExists -match "yes")) {
    Write-Status "  SKIP: D2 player.plr not present (D2 game data not installed)" -Color DarkGray
} else {
    # Read first 6 bytes of D2 .plr: should be DPLR signature (LE: 0x52 0x4C 0x50 0x44)
    # followed by version (LE u16)
    $hexDump = Adb -AdbArgs @("shell", "run-as", $PACKAGE, "xxd", "-l", "6", "files/d2x-redux/Players/player.plr")
    # xxd output: "00000000: 524c 5044 1300  RLPD.."
    # Bytes: R=0x52 L=0x4C P=0x50 D=0x44 = MAKE_SIG('D','P','L','R') in LE
    $hasSignature = [bool]($hexDump -match "524c 5044")
    Assert-True $hasSignature "D2 .plr has valid DPLR signature"

    if ($hexDump -match "524c 5044 (\w{2})") {
        $verByte = [Convert]::ToInt32($Matches[1], 16)
        Assert-True ($verByte -ge 17) "D2 .plr version >= 17 (got $verByte)"
    }
}

# -- Test 4: App starts and setup introspection works --
Write-Status "--- App startup and introspection ---" -Color Yellow

Adb -AdbArgs @("shell", "am", "force-stop", $PACKAGE) | Out-Null
Start-Sleep -Seconds 1
Adb -AdbArgs @("shell", "am", "start", "-n", "$PACKAGE/$ACTIVITY") | Out-Null
Start-Sleep -Seconds 3

$setup = Get-SetupIntrospection
Assert-True ($null -ne $setup) "Setup introspection returns JSON"
if ($setup) {
    Assert-True ($setup.screen -eq "setup") "Setup screen is 'setup'"
    if ($setup.d1) {
        # D1 may or may not be ready (depends on game files), just check the field exists
        Write-Status "  INFO: D1 ready = $($setup.d1.ready)" -Color Gray
    }
    if ($setup.d2) {
        Write-Status "  INFO: D2 ready = $($setup.d2.ready)" -Color Gray
    }
}

# -- Summary --
Write-Status ""
Write-Status "=== Results: $pass passed, $fail failed ===" -Color $(if ($fail -eq 0) { "Green" } else { "Red" })

if ($fail -gt 0) { exit 1 }
exit 0

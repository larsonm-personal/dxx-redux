#!/usr/bin/env pwsh

# Shared by direct automation runners and the suite; children inherit ANDROID_SERIAL
function Test-PhysicalTestTarget {
    param([string]$Serial = $env:ANDROID_SERIAL)
    return [bool]($Serial -and $Serial -notmatch '^emulator-\d+$')
}

function Get-AndroidTestTargets {
    param([Parameter(Mandatory)][string]$AdbPath)

    $targets = @([pscustomobject]@{ Serial = 'emulator-5554'; Label = 'emulator' })
    foreach ($line in @(& $AdbPath devices -l 2>$null)) {
        if ($line -notmatch '^(\S+)\s+device\s') { continue }
        $serial = $Matches[1]
        if ($serial -match '^emulator-\d+$') { continue }
        $model = if ($line -match '\bmodel:(\S+)') { $Matches[1] -replace '_', ' ' } else { $serial }
        $targets += [pscustomobject]@{ Serial = $serial; Label = "$model [$serial]" }
    }
    return $targets
}

function Select-AndroidTestTarget {
    param(
        [Parameter(Mandatory)][array]$Targets,
        [scriptblock]$ReadChoice = { Read-Host 'Choose an Android test target (Enter for emulator)' }
    )

    Write-Host ''
    Write-Host 'Android test targets' -ForegroundColor Cyan
    for ($index = 0; $index -lt $Targets.Count; $index++) {
        Write-Host "  $($index + 1). $($Targets[$index].Label)"
    }
    while ($true) {
        $choice = [string](& $ReadChoice)
        if (-not $choice.Trim()) { return $Targets[0].Serial }
        $number = 0
        if ([int]::TryParse($choice, [ref]$number) -and $number -ge 1 -and $number -le $Targets.Count) {
            return $Targets[$number - 1].Serial
        }
        Write-Host "Enter a number from 1 to $($Targets.Count)" -ForegroundColor Yellow
    }
}

function Initialize-AndroidTestTarget {
    param([string]$Serial, [switch]$NonInteractive)

    if (-not $Serial) { $Serial = $env:ANDROID_SERIAL }
    if (-not $Serial) {
        $redirected = try { [Console]::IsInputRedirected } catch { $true }
        $unattended = $NonInteractive -or $redirected -or -not [Environment]::UserInteractive -or
        ([Environment]::GetCommandLineArgs() -match '^-NonInteractive$')
        $Serial = 'emulator-5554'
        if (-not $unattended) {
            $targets = @(Get-AndroidTestTargets -AdbPath $script:ADB)
            if ($targets.Count -gt 1) { $Serial = Select-AndroidTestTarget -Targets $targets }
        }
    }
    if ($Serial -eq 'emulator') { $Serial = 'emulator-5554' }
    $env:ANDROID_SERIAL = $Serial
    if (Test-PhysicalTestTarget -Serial $Serial) {
        # Existing diagnostic build stays separate from the user's Play/sideload apps
        if (-not $env:DXX_TEST_PACKAGE) { $env:DXX_TEST_PACKAGE = 'com.dxxredux.app.nsdtest' }
    }
    $script:PACKAGE = if ($env:DXX_TEST_PACKAGE) { $env:DXX_TEST_PACKAGE } else { 'com.dxxredux.app' }
    Write-Host "Android test target: $Serial ($($script:PACKAGE))"
    return $Serial
}

function Assert-IsolatedPhysicalTestApp {
    if ((Test-PhysicalTestTarget) -and $script:PACKAGE -ne 'com.dxxredux.app.nsdtest') {
        throw 'Physical reset-state fixtures require the isolated com.dxxredux.app.nsdtest test app'
    }
}

function Wake-AndroidTestTarget {
    # Same ordinary wake/unlock path as the physical network campaign
    Adb -AdbArgs @('shell', 'input', 'keyevent', 'KEYCODE_WAKEUP') | Out-Null
    for ($attempt = 0; $attempt -lt 5; $attempt++) {
        Start-Sleep -Milliseconds 300
        $policy = Adb -AdbArgs @('shell', 'dumpsys', 'window', 'policy')
        if ($policy -match '\bshowing=true\b' -and $policy -match '\bsecure=true\b') {
            throw "Unlock $env:ANDROID_SERIAL normally before running tests"
        }
        Adb -AdbArgs @('shell', 'wm', 'dismiss-keyguard') | Out-Null
        if ($policy -and $policy -notmatch '\bshowing=true\b') {
            Adb -AdbArgs @('shell', 'input', 'keyevent', 'KEYCODE_SHIFT_LEFT') | Out-Null
            return
        }
    }
    throw "Lock screen did not dismiss on $env:ANDROID_SERIAL"
}

function Get-TestPackageAdbArguments {
    param([string[]]$Arguments)
    # Automation broadcasts must not reach a second installed distribution
    if ($Arguments.Count -ge 3 -and $Arguments[0] -eq 'shell' -and
        $Arguments[1] -eq 'am' -and $Arguments[2] -eq 'broadcast' -and
        ($Arguments -match '^com\.dxxredux\.') -and '-p' -notin $Arguments) {
        return @($Arguments) + @('-p', $script:PACKAGE)
    }
    return $Arguments
}

function Start-AndroidTestPowerSession {
    if (-not (Test-PhysicalTestTarget)) { return $null }
    $original = Adb -AdbArgs @('shell', 'settings', 'get', 'global', 'stay_on_while_plugged_in')
    if ($original -notmatch '^(\d+|null)$') { throw 'Cannot read the device stay-awake setting' }
    $session = @{ Serial = $env:ANDROID_SERIAL; Original = $original }
    Adb -AdbArgs @('shell', 'svc', 'power', 'stayon', 'true') | Out-Null
    return $session
}

function Stop-AndroidTestPowerSession {
    param($Session)
    if (-not $Session) { return }
    $arguments = if ($Session.Original -eq 'null') {
        @('shell', 'settings', 'delete', 'global', 'stay_on_while_plugged_in')
    } else {
        @('shell', 'settings', 'put', 'global', 'stay_on_while_plugged_in', $Session.Original)
    }
    Adb-Dev -Serial $Session.Serial -AdbArgs $arguments | Out-Null
    $restored = Adb-Dev -Serial $Session.Serial -AdbArgs @('shell', 'settings', 'get', 'global', 'stay_on_while_plugged_in')
    if ($restored -ne $Session.Original) {
        Write-Warning "Could not restore stay_on_while_plugged_in=$($Session.Original) on $($Session.Serial)"
    }
}

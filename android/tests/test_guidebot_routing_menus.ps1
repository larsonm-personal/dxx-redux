# Verify actual Android Guide wheel visibility across active mode changes and save restore
param([string]$Serial = 'emulator-5554', [switch]$Install)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial
$outputDir = Join-Path $REPO_ROOT 'android/temp/guidebot_routing_menus'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-MenuSteps {
    param([object[]]$Steps)
    $path = Join-Path $outputDir 'step.jsonc'
    [IO.File]::WriteAllText($path, (ConvertTo-Json -InputObject $Steps -Depth 20) + "`n")
    $deviceName = 'guidebot_routing_menu_step.jsonc'
    Adb -AdbArgs @('push', $path, "/data/local/tmp/$deviceName") | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $PACKAGE, 'cp', "/data/local/tmp/$deviceName", "files/$deviceName") | Out-Null
    $runId = [guid]::NewGuid().ToString('N')
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', $deviceName, '--es', 'run_id', $runId) | Out-Null
    if (-not (Wait-ForCondition -Description 'Guidebot menu automation' -TimeoutSec 45 -PollMs 500 -Condition {
                $raw = Adb -AdbArgs @('shell', 'run-as', $PACKAGE, 'cat', 'files/automation_result.json')
                if (-not $raw -or $raw -notmatch '^\s*\{') { return $false }
                $result = $raw | ConvertFrom-Json
                if ($result.run_id -ne $runId) { return $false }
                if ($result.result -eq 'FAIL') { throw "Menu automation failed: $raw" }
                return $result.result -eq 'PASS'
            })) { throw 'Menu automation did not finish' }
}

function Assert-GuideMenu {
    param([bool]$Enhanced, [bool]$SecretRevealed = $true)
    if (-not (Wait-ForCondition -Description "Guide menu Enhanced=$Enhanced SecretRevealed=$SecretRevealed" -TimeoutSec 15 -PollMs 500 -Condition {
                $requestId = [guid]::NewGuid().ToString('N')
                Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.INTROSPECT', '--es', 'request_id', $requestId) | Out-Null
                $raw = Adb -AdbArgs @('shell', 'run-as', $PACKAGE, 'cat', 'files/introspect_ui.json')
                if (-not $raw -or $raw -notmatch '^\s*\{') { return $false }
                $ui = $raw | ConvertFrom-Json
                if ($ui.request_id -ne $requestId -or $ui.guidebot_enhanced_routing -ne $Enhanced) { return $false }
                $bindings = @($ui.guidebot_goal_bindings)
                # Unexplored=1043, Secret=1038; retain Next, Energy, Recall and Warp
                if (($bindings -contains 1043) -ne $Enhanced -or
                    ($bindings -contains 1038) -ne ($Enhanced -and $SecretRevealed)) { return $false }
                foreach ($classic in @(1041, 1004, 1045, 1042)) {
                    if ($bindings -notcontains $classic) { return $false }
                }
                [IO.File]::WriteAllText((Join-Path $outputDir "menu-$Enhanced-$SecretRevealed.json"), ($ui | ConvertTo-Json -Depth 10) + "`n")
                return $true
            })) { throw 'Guide menu visibility did not match active routing' }
}

try {
    # Reuse the existing routing test's launch/deploy setup, stopping before its first default change
    $sourcePath = Join-Path $REPO_ROOT 'android/game_scripts/test_guidebot_routing_modes.jsonc'
    $source = ConvertFrom-JsoncText -Text (Get-Content -LiteralPath $sourcePath -Raw) -SourceName $sourcePath | ConvertFrom-Json
    $bootstrap = @()
    $boundaryFound = $false
    foreach ($step in $source) {
        if ($step.action -eq 'set_debug' -and $step.field -eq 'guidebot_routing_default') {
            $boundaryFound = $true
            break
        }
        $bootstrap += $step
    }
    if (-not $boundaryFound) { throw 'Routing test launch boundary not found' }
    $bootstrapPath = Join-Path $outputDir 'setup.jsonc'
    [IO.File]::WriteAllText($bootstrapPath, (ConvertTo-Json -InputObject $bootstrap -Depth 20) + "`n")
    $runnerArgs = @('-NoProfile', '-File', (Join-Path $REPO_ROOT 'android/helpers/run_test.ps1'), '-ScriptName', $bootstrapPath, '-Game', 'd2', '-LeaveRunning')
    if ($Install) { $runnerArgs += '-Install' }
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    & (Get-RegressionCurrentPwshPath) @runnerArgs *> (Join-Path $outputDir 'setup.log')
    if ($LASTEXITCODE -ne 0) { throw "Menu test setup failed; see $outputDir/setup.log" }

    Invoke-MenuSteps @(@{action = 'set_secret_reveal'; enabled = $true })
    Assert-GuideMenu -Enhanced $false
    Invoke-MenuSteps @(
        @{action = 'set_debug'; field = 'guidebot_routing_default'; value = 'Enhanced' },
        @{action = 'set_debug'; field = 'android_game_request'; value = 'quick_save'; post_delay_ms = 750 }
    )
    Assert-GuideMenu -Enhanced $false
    Invoke-MenuSteps @(@{action = 'set_debug'; field = 'guidebot_routing_mode'; value = 'Enhanced' })
    Assert-GuideMenu -Enhanced $true
    Invoke-MenuSteps @(@{action = 'set_secret_reveal'; enabled = $false })
    Assert-GuideMenu -Enhanced $true -SecretRevealed $false
    Invoke-MenuSteps @(
        @{action = 'set_debug'; field = 'android_game_request'; value = 'quick_load' },
        @{action = 'wait_for'; timeout_ms = 30000; expect = @{game_window_is_front = $true; 'guidebot.routing_mode_name' = 'Original' } },
        @{action = 'set_secret_reveal'; enabled = $true }
    )
    Assert-GuideMenu -Enhanced $false
    Invoke-MenuSteps @(@{action = 'set_debug'; field = 'guidebot_routing_mode'; value = 'Enhanced' })
    Assert-GuideMenu -Enhanced $true
    Write-Status 'PASS: added goals hide and return with active routing, including after save restore' 'Green'
} finally {
    Stop-AppAndWait
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial }
    else { Remove-Item Env:ANDROID_SERIAL -ErrorAction SilentlyContinue }
}

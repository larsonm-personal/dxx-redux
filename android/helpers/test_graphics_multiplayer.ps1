# Loaded by test_lan.ps1 for its provisioned-emulator graphics scenario

function Initialize-MultiplayerGraphicsFixture {
    foreach ($serial in @($EMU1, $EMU2)) {
        if ($serial -notmatch '^emulator-\d+$') { throw 'Graphics multiplayer fixtures require provisioned emulators' }
    }
    $script:GraphicsMultiplayerDirectory = Join-Path $REPO_ROOT ('android/temp/graphics-multiplayer-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    & (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts @($script:GraphicsMultiplayerDirectory) | Out-Null
    New-Item -ItemType Directory -Path $script:GraphicsMultiplayerDirectory -Force | Out-Null
    $script:GraphicsMultiplayerAccepted = [ordered]@{
        TexFilt = 1; AnisoLevel = 0; MsaaLevel = 0; MenuTexFilt = 0; HudTexFilt = 0
        ResolutionX = 640; ResolutionY = 480; AspectX = 3; AspectY = 4; ColorDepth = 0
    }
    $recordPath = Join-Path $script:GraphicsMultiplayerDirectory 'graphics_safety.json'
    @{ accepted = $script:GraphicsMultiplayerAccepted; attempt = $null; pending = @{} } |
        ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $recordPath -Encoding utf8NoBOM
    $configPath = Join-Path $script:GraphicsMultiplayerDirectory 'descent.cfg'
    $script:GraphicsMultiplayerAccepted.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" } |
        Set-Content -LiteralPath $configPath -Encoding utf8NoBOM
    foreach ($serial in @($EMU1, $EMU2)) {
        foreach ($name in @('graphics_safety.json', 'descent.cfg')) {
            Adb-Dev -Serial $serial -AdbArgs @('push', (Join-Path $script:GraphicsMultiplayerDirectory $name), "/data/local/tmp/$name") | Out-Null
            Adb-Dev -Serial $serial -AdbArgs @('shell', 'run-as', $PACKAGE, 'cp', "/data/local/tmp/$name", "files/$name") | Out-Null
        }
        foreach ($variant in @('d1x-redux', 'd2x-redux')) {
            Adb-Dev -Serial $serial -AdbArgs @('shell', 'run-as', $PACKAGE, 'mkdir', '-p', "files/$variant") | Out-Null
            Adb-Dev -Serial $serial -AdbArgs @('shell', 'run-as', $PACKAGE, 'cp', '/data/local/tmp/descent.cfg', "files/$variant/descent.cfg") | Out-Null
        }
    }
}

function Assert-MultiplayerGraphicsSnapshot {
    param($State, [int]$TextureFiltering)
    if (-not $State -or $State.graphics_safety.phase -ne 'idle' -or $State.time_paused -or
        -not $State.is_network -or (Get-IntroNumConnected -Intro $State) -ne 2) {
        throw 'Graphics decision did not return to an unpaused two-player session'
    }
    foreach ($snapshot in @('current', 'requested', 'accepted')) {
        foreach ($entry in $script:GraphicsMultiplayerAccepted.GetEnumerator()) {
            $expected = if ($entry.Key -eq 'TexFilt') { $TextureFiltering } else { $entry.Value }
            if ($State.graphics_safety.$snapshot.($entry.Key) -ne $expected) {
                throw "Graphics $snapshot $($entry.Key) was not restored/published: expected $expected"
            }
        }
    }
}

function Invoke-MultiplayerGraphicsScenario {
    param([string]$TriggerScriptName, [string]$DecisionScriptName)
    $results = @()
    foreach ($serial in @($EMU1, $EMU2)) {
        $other = if ($serial -eq $EMU1) { $EMU2 } else { $EMU1 }
        $remoteSlot = if ($serial -eq $EMU1) { 1 } else { 0 }
        foreach ($outcome in @('cancel', 'timeout', 'accept')) {
            $caseName = "$Game-$serial-$outcome"
            Write-Status "Graphics multiplayer case: $caseName"
            $before = Get-GameIntrospection -Serial $serial
            Assert-MultiplayerGraphicsSnapshot -State $before -TextureFiltering 1
            if (-not (Start-DeviceGameAutomation -Serial $serial -ScriptName $TriggerScriptName)) { throw 'Could not start multiplayer graphics trial' }
            $script:GraphicsTrialFirst = $null
            if (-not (Wait-ForCondition -Description 'Graphics challenge in a network session' -TimeoutSec 10 -PollMs 100 -Condition {
                        $script:GraphicsTrialFirst = Get-GameIntrospection -Serial $serial
                        return $script:GraphicsTrialFirst -and $script:GraphicsTrialFirst.graphics_safety.phase -eq 'challenge'
                    })) { throw 'Multiplayer trial did not become visible' }
            $first = $script:GraphicsTrialFirst
            Start-Sleep -Milliseconds 350
            $second = Get-GameIntrospection -Serial $serial
            foreach ($state in @($first, $second)) {
                if (-not $state -or $state.graphics_safety.phase -ne 'challenge' -or $state.time_paused -or
                    -not $state.is_network -or (Get-IntroNumConnected -Intro $state) -ne 2) {
                    throw 'Session paused, disconnected or trial ended before network observations completed'
                }
            }
            if ($first.graphics_safety.trial_id -ne $second.graphics_safety.trial_id -or
                $first.graphics_safety.deadline_ms -ne $second.graphics_safety.deadline_ms -or
                $second.msaa.flip_serial -le $first.msaa.flip_serial -or
                (Get-IntroPdataSequence -Intro $first -PlayerSlot $remoteSlot) -eq (Get-IntroPdataSequence -Intro $second -PlayerSlot $remoteSlot)) {
                throw 'Trial deadline changed or frame/network processing did not advance during the modal'
            }
            $first | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $script:GraphicsMultiplayerDirectory "$caseName-during-first.json") -Encoding utf8NoBOM
            $second | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $script:GraphicsMultiplayerDirectory "$caseName-during-second.json") -Encoding utf8NoBOM
            if (-not (Wait-ForCondition -Description 'Trial input assertions complete' -TimeoutSec 3 -PollMs 100 -Condition {
                        $result = Get-DeviceAutomationResult -Serial $serial
                        if ($result -and $result.result -eq 'FAIL') { throw 'Trial automation failed' }
                        return $result -and $result.result -eq 'PASS'
                    })) { throw 'Trial automation did not complete' }
            if ($outcome -ne 'timeout') {
                $decision = if ($outcome -eq 'accept') { 'A' } else { 'B' }
                if (-not (Start-DeviceGameAutomation -Serial $serial -ScriptName $DecisionScriptName -Params @{ DECISION = $decision })) { throw 'Could not decide multiplayer trial' }
                if (-not (Wait-ForCondition -Description 'Controller graphics decision completes' -TimeoutSec 8 -PollMs 100 -Condition {
                            $result = Get-DeviceAutomationResult -Serial $serial
                            if ($result -and $result.result -eq 'FAIL') { throw 'Controller decision automation failed' }
                            return $result -and $result.result -eq 'PASS'
                        })) { throw 'Controller decision did not complete' }
            }
            $script:GraphicsTrialAfter = $null
            if (-not (Wait-ForCondition -Description 'Graphics returns to network gameplay' -TimeoutSec 8 -PollMs 150 -Condition {
                        $script:GraphicsTrialAfter = Get-GameIntrospection -Serial $serial
                        return $script:GraphicsTrialAfter -and $script:GraphicsTrialAfter.graphics_safety.phase -eq 'idle'
                    })) { throw 'Graphics did not return to idle' }
            $after = $script:GraphicsTrialAfter
            $expectedTexture = if ($outcome -eq 'accept') { 2 } else { 1 }
            Assert-MultiplayerGraphicsSnapshot -State $after -TextureFiltering $expectedTexture
            $otherState = Get-GameIntrospection -Serial $other
            $otherTexture = if ($serial -eq $EMU2) { 2 } else { 1 }
            Assert-MultiplayerGraphicsSnapshot -State $otherState -TextureFiltering $otherTexture
            $after | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $script:GraphicsMultiplayerDirectory "$caseName-after.json") -Encoding utf8NoBOM
            $results += [ordered]@{ case = $caseName; passed = $true; trial_id = $first.graphics_safety.trial_id; first_flip = $first.msaa.flip_serial; second_flip = $second.msaa.flip_serial; first_packet = (Get-IntroPdataSequence -Intro $first -PlayerSlot $remoteSlot); second_packet = (Get-IntroPdataSequence -Intro $second -PlayerSlot $remoteSlot) }
            ConvertTo-Json -InputObject @($results) -Depth 20 | Set-Content (Join-Path $script:GraphicsMultiplayerDirectory 'results.json') -Encoding utf8NoBOM
        }
    }
    Write-Status "Graphics multiplayer cases passed; output: $script:GraphicsMultiplayerDirectory" 'Green'
    return $true
}

# Focused mode owned by test_lan_launch_preparation; no repeated timing campaign
function Invoke-LanStartupRecoveryChecks {
    . "$PSScriptRoot/check_lan_parallel_startup.ps1"
    $dualCases = @('host-loss-loading', 'host-loss-waiting', 'same-session-reconnect', 'cancel-reconnect', 'replay-admission', 'retag-admission', 'drop-requests', 'drop-proof', 'delay-proof', 'replay-proof', 'delay-admission', 'long-client', 'cancel-client', 'missing-third', 'failed-level-load')
    $allowed = $dualCases + @('three-player', 'three-player-lost', 'mixed-binaries')
    $selected = if ($env:DXX_RECOVERY_CASES) { @($env:DXX_RECOVERY_CASES.Split(',')) } else { $allowed }
    # A game prefix narrows a dual-peer follow-up without repeating the other engine
    foreach ($case in $selected) {
        $name = ($case -split '/', 2)[-1]
        if ($case -notin $allowed -and ($case -notmatch '^d[12]/' -or $name -notin $dualCases)) { throw "Unknown recovery case: $case" }
    }
    $failures = @()
    $blocked = @()
    $attempted = @()
    :games foreach ($game in @('d1', 'd2')) {
        foreach ($case in @($selected | Where-Object { $_ -in $dualCases -or $_ -like "$game/*" } | ForEach-Object { ($_ -split '/', 2)[-1] } | Select-Object -Unique)) {
            $attempted += "$game/$case"
            try {
                Invoke-LanParallelStartupChecks -Games @($game) -Cases @($case)
            } catch {
                $failures += "$game/$case`: $($_.Exception.Message)"
                Write-Output "RECOVERY FAIL: $($failures[-1])"
                foreach ($serial in @($hostSerial, $clientSerial)) {
                    if (-not (Test-EmulatorHealthy -Serial $serial)) {
                        $blocked += "Emulator unavailable after $game/$case`: $serial; remaining cases were not executed"
                    }
                }
                if ($blocked.Count) { break games }
            }
        }
    }
    if (-not $blocked.Count -and @($selected | Where-Object { $_ -like 'three-player*' }).Count) {
        try {
            . "$PSScriptRoot/check_lan_three_player_startup.ps1"
            Invoke-LanThreePlayerStartupChecks -Cases @($selected | Where-Object { $_ -like 'three-player*' })
        } catch { $failures += "three-player: $($_.Exception.Message)" }
    }
    if (-not $blocked.Count -and $selected -contains 'mixed-binaries') {
        try {
            . "$PSScriptRoot/check_lan_mixed_startup.ps1"
            Invoke-LanMixedStartupChecks
        } catch { $failures += "mixed-binaries: $($_.Exception.Message)" }
    }
    [ordered]@{ revision = (git -C $script:REPO_ROOT rev-parse HEAD); requested = $selected; attempted_dual_cases = $attempted; failures = $failures; blocked = $blocked; passed = ($failures.Count -eq 0 -and $blocked.Count -eq 0) } |
        ConvertTo-Json -Depth 10 | Set-Content (Join-Path $outputDirectory 'recovery-summary.json') -Encoding utf8
    if ($failures.Count -or $blocked.Count) { throw (($failures + $blocked) -join "`n") }
}

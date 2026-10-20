# Focused mode owned by test_lan_launch_preparation; no repeated timing campaign
function Invoke-LanStartupRecoveryChecks {
    . "$PSScriptRoot/check_lan_parallel_startup.ps1"
    $failures = @()
    foreach ($game in @('d1', 'd2')) {
        foreach ($case in @('host-loss-loading', 'host-loss-waiting', 'same-session-reconnect', 'replay-admission', 'retag-admission', 'drop-requests', 'drop-proof', 'delay-proof', 'replay-proof', 'delay-admission', 'long-client', 'cancel-client', 'missing-third', 'failed-level-load')) {
            try {
                Invoke-LanParallelStartupChecks -Games @($game) -Cases @($case)
            } catch {
                $failures += "$game/$case`: $($_.Exception.Message)"
                Write-Output "RECOVERY FAIL: $($failures[-1])"
            }
        }
    }
    try {
        . "$PSScriptRoot/check_lan_three_player_startup.ps1"
        Invoke-LanThreePlayerStartupChecks
    } catch { $failures += "three-player: $($_.Exception.Message)" }
    try {
        . "$PSScriptRoot/check_lan_mixed_startup.ps1"
        Invoke-LanMixedStartupChecks
    } catch { $failures += "mixed-binaries: $($_.Exception.Message)" }
    [ordered]@{ revision = (git -C $script:REPO_ROOT rev-parse HEAD); failures = $failures; passed = ($failures.Count -eq 0) } |
        ConvertTo-Json -Depth 10 | Set-Content (Join-Path $outputDirectory 'recovery-summary.json') -Encoding utf8
    if ($failures.Count) { throw ($failures -join "`n") }
}

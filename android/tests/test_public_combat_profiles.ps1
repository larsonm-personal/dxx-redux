#!/usr/bin/env pwsh
# Host-only checks for complete public combat reports; no gameplay is simulated here
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'public_combat_report.ps1')
function Assert-Rejected([string]$Label, [scriptblock]$Action) {
    try { & $Action } catch { Write-Host "PASS: rejects $Label"; return }
    throw "Accepted invalid public combat evidence: $Label"
}
foreach ($native in @($true, $false)) {
    $trace = [pscustomobject]@{
        pickups = @(1..30); drops = @(1..2160); robot_pairs = @(1..16); resource_drops = @(1..36); robot_drops = @(1..27)
        volatile_impacts = @(1..30); weapon_drops = $(if ($native) { @(1..4) } else { @(1..8) })
        robot_blasts = $(if ($native) { @(1..44) } else { @(1..40) })
        secondary_explosions = $(if ($native) { @(1..11) } else { @(1..8) })
        contact_motion = @{ motion = @(1..32); contacts = @(1..8) }
        damage = @(foreach ($difficulty in 0..4) {
                foreach ($invulnerable in 0..1) {
                    foreach ($kind in 0..2) {
                        , @($difficulty, $invulnerable, $kind, $(if ($invulnerable) { 0 } else { 16 }), 0, @(0, 0, 0), @(0, 0, 0))
                    }
                }
            })
    }
    # Supply valid case keys independently of outcome values; runtime tests own outcomes
    $expected = Get-PublicCombatCaseKeys -Native $native
    foreach ($name in $expected.Keys) {
        $rows = @(foreach ($key in $expected[$name]) {
                $values = @($key.Split(',') | ForEach-Object { [int]$_ })
                if ($name -eq 'motion') { @{ type = $values[0]; bounce = $values[1]; direction = $values[2]; speed = $values[3] } }
                elseif ($name -eq 'resource_drops') { @{ source = $values[0]; id = $values[1]; inventory = $values[2]; seed = $values[3] } }
                else { , $values }
            })
        if ($name -in @('motion', 'contacts')) { $trace.contact_motion.$name = $rows } else { $trace.$name = $rows }
    }
    Assert-PublicCombatRows -Trace $trace -Native $native
    $blast = $trace.robot_blasts[0]
    $trace.robot_blasts[0] = $trace.robot_blasts[1]
    Assert-Rejected 'same-count duplicate blast' { Assert-PublicCombatRows -Trace $trace -Native $native }
    $trace.robot_blasts[0] = $blast
    $actor = $trace.robot_blasts[0][0]
    $trace.robot_blasts[0][0] = 999
    Assert-Rejected 'unavailable blast actor' { Assert-PublicCombatRows -Trace $trace -Native $native }
    $trace.robot_blasts[0][0] = $actor
    $impact = $trace.volatile_impacts[0]
    $trace.volatile_impacts[0] = $trace.volatile_impacts[1]
    Assert-Rejected 'same-count duplicate impact' { Assert-PublicCombatRows -Trace $trace -Native $native }
    $trace.volatile_impacts[0] = $impact
    $trace.damage[0][0] = 4
    Assert-Rejected 'reordered difficulty case' { Assert-PublicCombatRows -Trace $trace -Native $native }
    $trace.damage[0][0] = 0
    $trace.damage[3][3] = 1
    Assert-Rejected 'invulnerability damage' { Assert-PublicCombatRows -Trace $trace -Native $native }
    $trace.damage[3][3] = 0
    $trace.contact_motion.contacts = @(1..7)
    Assert-Rejected 'missing contact case' { Assert-PublicCombatRows -Trace $trace -Native $native }
    $trace.contact_motion.contacts = @(foreach ($key in $expected.contacts) { , @($key.Split(',') | ForEach-Object { [int]$_ }) })
    $trace.robot_blasts = @($trace.robot_blasts | Select-Object -Skip 1)
    Assert-Rejected 'missing loaded robot blast' { Assert-PublicCombatRows -Trace $trace -Native $native }
}
foreach ($role in @('native', 'imported', 'ordinary-d2')) {
    $native = $role -ne 'ordinary-d2'
    $profile = [pscustomobject]@{
        edition = 'shareware'; geometry = 'controlled-corridor'; content_game = $(if ($native) { 'd1' } else { 'd2' })
        mission = $(if (-not $native) { 'd2demo' } elseif ($role -eq 'imported') { 'descent' } else { '' })
        first_level = $(if ($native) { 'level01.sdl' } else { 'd2leva-1.sl2' })
        robots = $(if ($native) { @(0, 1, 2, 3, 4, 14, 16, 17, 18, 19, 20) } else { @(25, 30, 31, 33, 37, 38, 39, 40, 42, 50) })
        impact_projectiles = @(0, 8, 15); pair_actors = $(if ($native) { @(0, 1) } else { @(25, 40) })
        egg_actors = $(if ($native) { @(0, 14, 20) } else { @(25, 37, 50) })
    }
    Assert-PublicCombatProfile -Profile $profile -Role $role
    $profile.robots = @($profile.robots | Select-Object -Skip 1)
    Assert-Rejected "$role omitted robot" { Assert-PublicCombatProfile -Profile $profile -Role $role }
}
Write-Host 'PASS: public combat reports reject incomplete matrices and substituted actors'

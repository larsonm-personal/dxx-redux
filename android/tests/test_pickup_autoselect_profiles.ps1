#!/usr/bin/env pwsh
# Host-only admission checks; the Linux runtime job supplies actual engine evidence
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'pickup_autoselect_report.ps1')
$rows = @(foreach ($order in 0..3) {
        foreach ($scenario in 0..10) {
            foreach ($firing in 0..3) {
                foreach ($once in 0..1) {
                    foreach ($consumed in 0..1) {
                        foreach ($pending in @(-1, 2, 3)) {
                            $state = @{ selected = 0; pending = -1; picked_up = 1; inventory = 1; flags = 0; laser_level = 0; energy = 65536; ammo = 0; rearm = 0; firing_count = 0; fusion_charge = 0 }
                            [pscustomobject]@{ case = @($order, $scenario, $firing, $once, $consumed, $pending); used = 1; acquired = $state.Clone(); released = $state.Clone(); followup = $state.Clone() }
                        }
                    }
                }
            }
        }
    })
Assert-PickupAutoselectRows -Rows $rows
function Assert-Rejected([string]$Label, [scriptblock]$Action) {
    try { & $Action } catch { Write-Host "PASS: rejects $Label"; return }
    throw "Accepted invalid pickup evidence: $Label"
}
Assert-Rejected 'empty evidence' { Assert-PickupAutoselectRows -Rows @() }
Assert-Rejected 'missing case' { Assert-PickupAutoselectRows -Rows $rows[1..2111] }
$case = $rows[0].case
$rows[0].case = $rows[1].case
Assert-Rejected 'duplicate case' { Assert-PickupAutoselectRows -Rows $rows }
$rows[0].case = $case
$rows[0].released.Remove('pending')
Assert-Rejected 'missing release state' { Assert-PickupAutoselectRows -Rows $rows }
$rows[0].released.pending = -1
$rows[0].followup.ammo = '0'
Assert-Rejected 'non-numeric inventory' { Assert-PickupAutoselectRows -Rows $rows }
$rows[0].followup.ammo = 0
Assert-PickupAutoselectRows -Rows $rows
foreach ($edition in @('retail', 'shareware')) {
    foreach ($role in @('native', 'imported', 'ordinary-d2')) {
        $d2 = $role -eq 'ordinary-d2'
        $profile = [pscustomobject]@{
            edition = $edition; content_game = $(if ($d2) { 'd2' } else { 'd1' })
            mission = $(if ($d2) { if ($edition -eq 'shareware') { 'd2demo' } else { 'd2' } } elseif ($role -eq 'imported') { 'descent' } else { '' })
            first_level = $(if ($d2) { 'd2leva-1.sl2' } else { 'level01.sdl' })
            geometry = 'controlled-corridor'; models = 56; sounds = $(if ($role -eq 'native') { 77 } else { 70 }); powerups = @(@{ id = 3 }, @{ id = 12 }, @{ id = 14 }, @{ id = 16 }, @{ id = 22 })
        }
        Assert-PickupAssetProfile -Profile $profile -DataEdition $edition -Role $role
        $profile.edition = 'wrong'
        Assert-Rejected "$edition/$role edition substitution" { Assert-PickupAssetProfile -Profile $profile -DataEdition $edition -Role $role }
        $profile.edition = $edition
        $profile.mission = 'wrong'
        Assert-Rejected "$edition/$role mission substitution" { Assert-PickupAssetProfile -Profile $profile -DataEdition $edition -Role $role }
    }
}
Write-Host 'PASS: complete pickup matrix and retail/public source profiles admitted'

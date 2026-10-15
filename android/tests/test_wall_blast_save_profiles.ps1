#!/usr/bin/env pwsh
# Host-only report admission checks; these do not replace the real runtime fixture
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'wall_blast_save_report.ps1')
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$directory = Join-Path $repo ('android/temp/wall-blast-profiles/run-' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $directory -DirectoryPrefix run-
New-Item -ItemType Directory -Path "$directory/native", "$directory/imported" -Force | Out-Null

function Write-Evidence([string]$Edition) {
    foreach ($name in @('native/wall-blast.json', 'imported/wall-blast.json', 'imported/wall-blast-d2.json')) {
        $d2 = $name -eq 'imported/wall-blast-d2.json'
        $mission = if ($d2) { if ($Edition -eq 'shareware') { 'd2demo' } else { 'd2' } } elseif ($name -eq 'imported/wall-blast.json') { 'descent' } else { '' }
        $rows = @(foreach ($elapsed in @(0, 8, 24, 25, 31)) {
                $frames = @($elapsed..32 | ForEach-Object { @{ frame = $_; flags = 1; hps = 0; doorway = 1; objects = 40; rng = 42; draws = 0 } })
                $imports = @{}
                $rejections = @{}
                foreach ($source in @("wall-blast-$elapsed.sav", "opposite-wall-blast-$elapsed.sav")) { $imports[$source] = @(); $rejections[$source] = 134 }
                @{
                    elapsed = $elapsed; uninterrupted = $frames; restored = $frames; differences = @(); endian_differences = @()
                    import_differences = $imports; rejected_mutations = $rejections; legacy_cleared = $true
                    selection = @{
                        edition = $Edition; content_game = $(if ($d2) { 'd2' } else { 'd1' }); mission = $mission
                        level = $(if ($d2) { 'd2leva-1.sl2' } else { 'level01.sdl' }); level_num = 1
                        wall = $(if ($d2) { 37 } else { 10 }); segment = 1; side = 2; clip = 6; wall_type = 1; clip_flags = 1
                    }
                }
            })
        ConvertTo-Json -InputObject $rows -Depth 20 | Set-Content -LiteralPath (Join-Path $directory $name)
    }
}

function Assert-Rejected([string]$Label, [scriptblock]$Change) {
    Write-Evidence shareware
    $path = Join-Path $directory 'imported/wall-blast.json'
    $rows = @(Get-Content -LiteralPath $path -Raw | ConvertFrom-Json)
    & $Change $rows
    ConvertTo-Json -InputObject $rows -Depth 20 | Set-Content -LiteralPath $path
    try { Assert-WallBlastSaveReport -Directory $directory -DataEdition shareware | Out-Null } catch {
        Write-Host "PASS: rejects $Label"
        return
    }
    throw "Accepted invalid save evidence: $Label"
}

foreach ($edition in @('shareware', 'retail')) {
    Write-Evidence $edition
    $report = Assert-WallBlastSaveReport -Directory $directory -DataEdition $edition
    if ($report.rejectedMutations -ne 1340) { throw 'Incomplete native v21 corruption total' }
    Write-Host "PASS: $edition report profile remains supported"
}
Assert-Rejected 'wrong edition' { param($rows) $rows[0].selection.edition = 'retail' }
Assert-Rejected 'wrong mission' { param($rows) $rows[0].selection.mission = 'd2demo' }
Assert-Rejected 'retail level in public profile' { param($rows) $rows[0].selection.level = 'level01.rdl' }
Assert-Rejected 'substituted public wall' { param($rows) $rows[0].selection.wall = 11 }
Assert-Rejected 'non-exploding blastable wall' { param($rows) $rows[0].selection.clip_flags = 0 }
Assert-Rejected 'missing phase' { param($rows) $rows[0].elapsed = 1 }
Assert-Rejected 'missing continuation frame' { param($rows) $rows[0].uninterrupted = @($rows[0].uninterrupted | Select-Object -Skip 1) }
Assert-Rejected 'restore divergence hidden by empty diff' { param($rows) $rows[0].restored[0].rng = 99 }
Assert-Rejected 'opposite-endian divergence' { param($rows) $rows[0].endian_differences = @(@{ op = 'replace'; path = '/0/rng'; value = 99 }) }
Assert-Rejected 'missing imported byte order' { param($rows) $rows[0].import_differences.PSObject.Properties.Remove('opposite-wall-blast-0.sav') }
Assert-Rejected 'native checkpoint divergence' { param($rows) $rows[0].import_differences.'wall-blast-0.sav' = @(@{ op = 'replace'; path = '/0/rng'; value = 99 }) }
Assert-Rejected 'incomplete corruption checks' { param($rows) $rows[0].rejected_mutations.'wall-blast-0.sav' = 6 }
Assert-Rejected 'missing native v21 tail checks' { param($rows) $rows[0].rejected_mutations.'wall-blast-0.sav' = 126 }
Assert-Rejected 'missing corruption byte order' { param($rows) $rows[0].rejected_mutations.PSObject.Properties.Remove('opposite-wall-blast-0.sav') }
Assert-Rejected 'missing legacy clearing' { param($rows) $rows[0].legacy_cleared = $false }
Write-Host 'PASS: wall-blast evidence rejects incomplete profiles and weakened persistence oracles'

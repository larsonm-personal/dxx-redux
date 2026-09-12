param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputRoot = Join-Path $repoRoot ("android\temp\guidebot_reactor_lifetime_test\run_" + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
$metadataRoot = Join-Path $outputRoot 'metadata'
$sourceRoot = Join-Path $repoRoot 'game_data\mission_files'
& (Join-Path $repoRoot 'android\helpers\retain-recent-artifacts.ps1') -Artifacts $outputRoot
New-Item -ItemType Directory -Path $metadataRoot -Force | Out-Null
$cases = @(
    @{ File = 'CD - Descent - Levels of the World (USA).json'; Target = 14; Level = 0 },
    @{ File = 'CD - Descent - Levels of the World (USA).json'; Target = 64; Level = 0 },
    @{ File = 'CD - Dimensions for Descent (USA).json'; Target = 43; Level = 0 },
    @{ File = 'erisreb.json'; Target = 0; Level = 1 },
    @{ File = 'freelan.json'; Target = 0; Level = 4 },
    @{ File = 'megalo.json'; Target = 1; Level = 0 }
)
# Filter copies of metadata so the test exercises just the six affected levels
foreach ($group in $cases | Group-Object File) {
    $entries = @(Get-Content -LiteralPath (Join-Path $sourceRoot $group.Name) -Raw | ConvertFrom-Json)
    $selected = @(foreach ($case in $group.Group) {
            $entry = @($entries | Where-Object target_index -eq $case.Target)[0]
            $entry.levels = @($entry.levels | Where-Object level_num -eq $case.Level)
            if ($entry.levels.Count -ne 1) { throw "Missing reactor lifetime fixture: $($group.Name)" }
            $entry
        })
    ConvertTo-Json -InputObject $selected -Depth 100 | Set-Content -LiteralPath (Join-Path $metadataRoot $group.Name) -Encoding utf8NoBOM
    $archive = Join-Path $sourceRoot ([IO.Path]::ChangeExtension($group.Name, '.zip'))
    if (Test-Path -LiteralPath $archive) { Copy-Item -LiteralPath $archive -Destination $metadataRoot -Force }
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'cd_level_metadata_sources.jsonc') -Destination $metadataRoot -Force
& (Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1') `
    -MissionMetadataRoot $metadataRoot -Repeat 2 -NoBuild:$NoBuild -MaxParallel 2 -OutputRoot (Join-Path $outputRoot 'run')
if ($LASTEXITCODE -ne 0) { throw 'Reactor lifetime simulations reported infrastructure failures' }
$summary = Get-Content -LiteralPath (Join-Path $outputRoot 'run\summary.json') -Raw | ConvertFrom-Json
if ($summary.selected_levels -ne 6) { throw 'Expected six reactor lifetime simulations' }
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $outputRoot 'run\results') -Filter '*.simulation.json') {
    $records = @(Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json)
    foreach ($level in $records.levels) {
        # Neural Intrusion still has a separate route stall after the reactor
        $allowedStatuses = if ($level.level_file -eq 'neural-i.rdl') { @('ok', 'timeout') } else { @('ok') }
        if ($level.status -notin $allowedStatuses) {
            throw "Unexpected reactor lifetime result: $($file.Name): $($level.status)"
        }
        if (-not @($level.objectives | Where-Object n -eq 'reactor').Count) {
            throw "Reactor lifetime fixture did not reach the reactor: $($file.Name)"
        }
    }
}
$megaloRuns = @(Get-ChildItem -LiteralPath (Join-Path $outputRoot 'run/results') -Filter 'megalo.json_1_0_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($megaloRuns.Count -ne 2) { throw 'Expected two Megalomania reactor-lifetime runs' }
foreach ($file in $megaloRuns) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or $result.radius.player -ne 310325 -or
        $result.radius.effective -ne $result.radius.player -or
        ($result.objectives.label -join '|') -cne 'blue key|red key|Reactor|Exit') {
        throw 'Megalomania must cross the tapered connector and finish at full player radius'
    }
}
if ((Get-FileHash $megaloRuns[0].FullName).Hash -ne (Get-FileHash $megaloRuns[1].FullName).Hash) {
    throw 'Megalomania reactor-lifetime repeats differ'
}
Write-Host 'GuideBot reactor lifetime simulations passed'

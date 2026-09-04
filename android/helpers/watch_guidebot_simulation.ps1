#!/usr/bin/env pwsh

[CmdletBinding()]
param(
    [string[]]$MissionJson,
    [int[]]$Level,
    [string]$Query = '',
    [switch]$NoBuild,
    [switch]$ListOnly,
    [switch]$PreviewPicker
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $PSCommandPath
$androidRoot = Split-Path -Parent $scriptDir
$repoRoot = Split-Path -Parent $androidRoot
$missionRoot = Join-Path $repoRoot 'game_data\mission_files'
$manualRoot = Join-Path $androidRoot 'temp\guidebot_simulation_manual'

function Get-GuidebotBrowserValue {
    param([object]$Value, [string]$Name, $Default = $null)
    $property = $Value.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) { return $Default }
    return $property.Value
}

function Get-GuidebotBrowserEntries {
    param([Parameter(Mandatory)][string]$Path)
    $text = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
    $value = $text | ConvertFrom-Json -NoEnumerate
    $entries = if ($text.TrimStart().StartsWith('[')) { @($value) } else { @($value) }
    if (@($entries | Where-Object { $null -eq $_.PSObject.Properties['levels'] }).Count) { return @() }
    return $entries
}

function Get-GuidebotBrowserIndex {
    $items = foreach ($file in @(Get-ChildItem -LiteralPath $missionRoot -Recurse -File -Filter '*.json' |
                Where-Object { $_.Name -notlike '*.simulation.json' -and $_.Name -notlike '*.tracklist.json' } |
                Sort-Object FullName)) {
        $relative = $file.FullName.Substring($missionRoot.Length).TrimStart('\', '/').Replace('\', '/')
        if ($MissionJson -and -not @($MissionJson | Where-Object {
                    $file.Name -like $_ -or $file.BaseName -like $_ -or $relative -like $_
                }).Count) { continue }
        try { $entries = @(Get-GuidebotBrowserEntries -Path $file.FullName) } catch { continue }
        $simulationPath = Join-Path $file.DirectoryName ($file.BaseName + '.simulation.json')
        $simulationEntries = @()
        if (Test-Path -LiteralPath $simulationPath -PathType Leaf) {
            try {
                $simulationValue = Get-Content -LiteralPath $simulationPath -Raw | ConvertFrom-Json -NoEnumerate
                $simulationEntries = if ($simulationValue -is [Array]) {
                    @($simulationValue | ForEach-Object { $_ })
                } else {
                    @($simulationValue)
                }
            } catch {}
        }
        foreach ($mission in $entries) {
            if ([string](Get-GuidebotBrowserValue $mission game '') -ne 'd2') { continue }
            $targetIndex = [int](Get-GuidebotBrowserValue $mission target_index 0)
            $simulation = @($simulationEntries | Where-Object { [int]$_.target_index -eq $targetIndex } | Select-Object -First 1)
            foreach ($levelRecord in @(Get-GuidebotBrowserValue $mission levels @())) {
                $levelNumber = [int](Get-GuidebotBrowserValue $levelRecord level_num 0)
                if ($Level -and $levelNumber -notin $Level) { continue }
                $levelFile = [string](Get-GuidebotBrowserValue $levelRecord level_file '')
                $prior = if ($simulation.Count) {
                    @($simulation[0].levels | Where-Object {
                            [int]$_.level_num -eq $levelNumber -and [string]$_.level_file -eq $levelFile
                        } | Select-Object -First 1)
                } else { @() }
                $missionName = [string](Get-GuidebotBrowserValue $mission mission_name $file.BaseName)
                $levelName = [string](Get-GuidebotBrowserValue $levelRecord level_name $levelFile)
                [pscustomobject]@{
                    MissionName = $missionName
                    MissionFile = [string](Get-GuidebotBrowserValue $mission mission_filename '')
                    MissionJson = $relative
                    TargetIndex = $targetIndex
                    Level = $levelNumber
                    LevelName = $levelName
                    LevelFile = $levelFile
                    StaticStatus = [string](Get-GuidebotBrowserValue $levelRecord route_status 'unknown')
                    SimulationStatus = if ($prior.Count) { [string]$prior[0].status } else { 'not_run' }
                    PriorFrames = if ($prior.Count) { Get-GuidebotBrowserValue $prior[0] total_frames $null } else { $null }
                    ExpectedObjectives = @(@(Get-GuidebotBrowserValue $levelRecord route_steps @()) |
                            Where-Object { [string]$_.kind -ne 'start' })
                    SearchText = "$missionName $($file.BaseName) $relative $targetIndex $levelNumber $levelName $levelFile"
                }
            }
        }
    }
    return @($items)
}

function Find-GuidebotBrowserItems {
    param([object[]]$Items, [string]$Text)
    $tokens = @($Text.Split(' ', [StringSplitOptions]::RemoveEmptyEntries))
    return @($Items | Where-Object {
            $item = $_
            -not @($tokens | Where-Object {
                    if ($_ -match '^-?\d+$') { return [string]$item.Level -ne $_ }
                    return $item.SearchText.IndexOf($_, [StringComparison]::OrdinalIgnoreCase) -lt 0
                }).Count
        })
}

function Write-GuidebotBrowserPickerFrame {
    param([object[]]$Matches, [string]$QueryText, [int]$Selected, [switch]$ClearScreen)

    if ($ClearScreen) { [Console]::Clear() }
    Write-Host 'GuideBot route browser' -ForegroundColor Cyan
    Write-Host "Search: $QueryText"
    Write-Host 'Type to filter, arrows/J/K move, Enter runs, Escape clears, Q quits'
    Write-Host ''
    if ($Matches.Count) {
        $pageStart = [Math]::Max(0, $Selected - 7)
        foreach ($index in $pageStart..([Math]::Min($Matches.Count - 1, $pageStart + 15))) {
            $item = $Matches[$index]
            $marker = if ($index -eq $Selected) { '>' } else { ' ' }
            $line = '{0} {1,-30} L{2,3} {3,-24} route={4,-8} sim={5}' -f $marker,
            $item.MissionName, $item.Level, $item.LevelName, $item.StaticStatus, $item.SimulationStatus
            Write-Host $line -ForegroundColor $(if ($index -eq $Selected) { 'Yellow' } else { 'Gray' })
        }
    }
    if (-not $Matches.Count) { Write-Host 'No matches' -ForegroundColor DarkYellow }
    [Console]::Out.Flush()
}

function Show-GuidebotBrowserPicker {
    param([object[]]$Items, [string]$InitialQuery)
    $queryText = $InitialQuery
    $selected = 0
    $firstPaint = $true
    while ($true) {
        $matches = @(Find-GuidebotBrowserItems -Items $Items -Text $queryText)
        if ($selected -ge $matches.Count) { $selected = [Math]::Max(0, $matches.Count - 1) }
        Write-GuidebotBrowserPickerFrame -Matches $matches -QueryText $queryText -Selected $selected `
            -ClearScreen:(-not $firstPaint)
        $firstPaint = $false
        $key = [Console]::ReadKey($true)
        if ($key.Key -eq 'Enter' -and $matches.Count) { return $matches[$selected] }
        if ($key.Key -eq 'Escape') { if ($queryText) { $queryText = ''; $selected = 0 } else { return $null }; continue }
        if ($key.Key -eq 'Backspace') { if ($queryText.Length) { $queryText = $queryText.Substring(0, $queryText.Length - 1) }; continue }
        if ($key.Key -eq 'UpArrow' -or ($key.Key -eq 'K' -and -not $queryText)) { $selected = [Math]::Max(0, $selected - 1); continue }
        if ($key.Key -eq 'DownArrow' -or ($key.Key -eq 'J' -and -not $queryText)) { $selected = [Math]::Min([Math]::Max(0, $matches.Count - 1), $selected + 1); continue }
        if ($key.Key -eq 'Q' -and -not $queryText) { return $null }
        if (-not [char]::IsControl($key.KeyChar)) { $queryText += $key.KeyChar; $selected = 0 }
    }
}

function Show-GuidebotBrowserLinePicker {
    param([object[]]$Items, [string]$InitialQuery)
    $queryText = $InitialQuery
    while ($true) {
        if (-not $queryText) { $queryText = Read-Host 'Mission or level search (Q quits)' }
        if ($queryText -eq 'q') { return $null }
        $matches = @(Find-GuidebotBrowserItems -Items $Items -Text $queryText)
        for ($index = 0; $index -lt [Math]::Min(20, $matches.Count); $index++) {
            $item = $matches[$index]
            Write-Host ('{0,2}. {1} L{2}: {3} route={4} sim={5}' -f ($index + 1),
                $item.MissionName, $item.Level, $item.LevelName, $item.StaticStatus, $item.SimulationStatus)
        }
        if (-not $matches.Count) { Write-Host 'No matches' }
        $choice = Read-Host 'Number to run, new search text, or Q'
        if ($choice -eq 'q') { return $null }
        $number = 0
        if ([int]::TryParse($choice, [ref]$number) -and $number -ge 1 -and $number -le [Math]::Min(20, $matches.Count)) {
            return $matches[$number - 1]
        }
        $queryText = $choice
    }
}

function Stop-GuidebotBrowserRun {
    param([Diagnostics.Process]$Process)
    if (-not $Process.HasExited) { $Process.Kill($true); $Process.WaitForExit() }
}

function Invoke-GuidebotBrowserBuild {
    if ($NoBuild) { return }
    Write-Host 'Building Windows GuideBot runner' -ForegroundColor Cyan
    & (Join-Path $repoRoot 'run-windows-build.ps1') -Target d2
    if ($LASTEXITCODE -ne 0) { throw "D2 build failed with exit code $LASTEXITCODE" }
}

function Invoke-GuidebotBrowserRun {
    param([object]$Item)
    Invoke-GuidebotBrowserBuild
    New-Item -ItemType Directory -Path $manualRoot -Force | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $manualRunRoot = Join-Path $manualRoot $stamp
    New-Item -ItemType Directory -Path $manualRunRoot -Force | Out-Null
    $stdout = Join-Path $manualRunRoot 'runner.stdout.log'
    $stderr = Join-Path $manualRunRoot 'runner.stderr.log'
    $runner = Join-Path $scriptDir 'regenerate_all_guidebot_simulations.ps1'
    $arguments = @('-NoProfile', '-File', $runner, '-Mode', 'Desktop', '-MissionJson', $Item.MissionJson,
        '-Level', [string]$Item.Level, '-Repeat', '1', '-NoBuild')
    $arguments += @('-OutputRoot', $manualRunRoot)
    $processArguments = @($arguments | ForEach-Object {
            $argument = [string]$_
            if ($argument -match '[\s"]') { '"' + $argument.Replace('"', '\"') + '"' } else { $argument }
        }) -join ' '
    $process = Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList $processArguments -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr -WindowStyle Hidden
    $aborted = $false
    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    Write-Host 'Launching visible Windows game. Q aborts, R restarts' -ForegroundColor Cyan
    while (-not $process.HasExited) {
        if ([Console]::KeyAvailable) {
            $key = [Console]::ReadKey($true)
            if ($key.Key -eq 'Q') { Stop-GuidebotBrowserRun -Process $process; $aborted = $true; break }
            if ($key.Key -eq 'R') { Stop-GuidebotBrowserRun -Process $process; return 'restart' }
        }
        $engineProgress = @(Get-ChildItem (Join-Path $manualRunRoot 'results') -Filter '*.progress' `
                -ErrorAction SilentlyContinue | Select-Object -First 1)
        $engineText = ''
        if ($engineProgress.Count) {
            try {
                $fields = @{}
                Get-Content -LiteralPath $engineProgress[0].FullName -ErrorAction Stop | ForEach-Object {
                    $parts = $_ -split '=', 2
                    if ($parts.Count -eq 2) { $fields[$parts[0]] = $parts[1] }
                }
                $engineText = ", frame $($fields.frames), objectives $($fields.objectives)"
            } catch {}
        }
        $progress = 'Windows route running: {0:n0}s{1}' -f $stopwatch.Elapsed.TotalSeconds, $engineText
        Write-Host "`r$($progress.PadRight([Math]::Max(1, [Console]::WindowWidth - 1)))" -NoNewline
        Start-Sleep -Milliseconds 400
    }
    Write-Host ''
    if ($aborted) { Write-Host 'Route run aborted'; return 'back' }
    $process.WaitForExit()
    Get-Content -LiteralPath $stdout, $stderr -ErrorAction SilentlyContinue | Select-Object -Last 20
    if ($process.ExitCode -ne 0) { Write-Host "Windows runner failed with exit $($process.ExitCode)" -ForegroundColor Red }
    $resultFile = @(Get-ChildItem (Join-Path $manualRunRoot 'results') -Filter '*.simulation.json' -ErrorAction SilentlyContinue |
            Select-Object -First 1)
    if ($resultFile.Count) {
        $resultDocument = Get-Content -LiteralPath $resultFile[0].FullName -Raw | ConvertFrom-Json -NoEnumerate
        $resultRecord = @(@($resultDocument) | Where-Object { [int]$_.target_index -eq $Item.TargetIndex } | Select-Object -First 1)
        $levelResult = if ($resultRecord.Count) {
            @($resultRecord[0].levels | Where-Object { [int]$_.level_num -eq $Item.Level -and $_.level_file -eq $Item.LevelFile } |
                    Select-Object -First 1)
        } else { @() }
        if ($levelResult.Count) {
            $objectiveText = @($levelResult[0].objectives | ForEach-Object { "$($_.n)=$($_.s)s" }) -join ', '
            Write-Host "Result: $($levelResult[0].status), frames=$($levelResult[0].total_frames), objectives=$objectiveText" -ForegroundColor Yellow
            Write-Host "RNG start: $($levelResult[0].rng_start | ConvertTo-Json -Compress)"
            Write-Host "RNG end:   $($levelResult[0].rng_end | ConvertTo-Json -Compress)"
            Write-Host "Checked-in headless: status=$($Item.SimulationStatus), frames=$($Item.PriorFrames)"
        }
    }
    Write-Host 'Enter repeats visible Windows, H runs headless, B returns to search, Q quits'
    while ($true) {
        $key = [Console]::ReadKey($true)
        if ($key.Key -eq 'Enter') { return 'restart' }
        if ($key.Key -eq 'B') { return 'back' }
        if ($key.Key -eq 'Q') { return 'quit' }
        if ($key.Key -eq 'H') {
            $headlessRoot = Join-Path $manualRunRoot 'headless_comparison'
            & (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson $Item.MissionJson `
                -Level $Item.Level -Repeat 2 -NoBuild -OutputRoot $headlessRoot
            Write-Host 'Headless comparison complete. Enter repeats visible Windows, B returns, Q quits'
        }
    }
}

$index = @(Get-GuidebotBrowserIndex)
$matches = @(Find-GuidebotBrowserItems -Items $index -Text $Query)
if ($ListOnly) {
    $matches | Select-Object MissionName, MissionJson, TargetIndex, Level, LevelName, LevelFile,
    StaticStatus, SimulationStatus | ConvertTo-Json -Depth 4
    exit 0
}
if ($PreviewPicker) {
    Write-GuidebotBrowserPickerFrame -Matches $matches -QueryText $Query -Selected 0
    exit 0
}
if (-not $index.Count) { throw 'No mission levels matched the supplied filters' }
$direct = $MissionJson -and $Level -and $matches.Count -eq 1
while ($true) {
    $item = if ($direct) {
        $matches[0]
        $direct = $false
    } elseif ([Console]::IsInputRedirected -or [Console]::IsOutputRedirected) {
        Show-GuidebotBrowserLinePicker -Items $index -InitialQuery $Query
    } else {
        Show-GuidebotBrowserPicker -Items $index -InitialQuery $Query
    }
    if ($null -eq $item) { break }
    Write-Host "Selected $($item.MissionName), level $($item.Level): $($item.LevelName)" -ForegroundColor Yellow
    Write-Host "Source: $($item.MissionJson), target $($item.TargetIndex), file $($item.LevelFile)"
    Write-Host "Static route: $($item.StaticStatus), prior simulation: $($item.SimulationStatus), expected objectives: $($item.ExpectedObjectives.Count)"
    do { $action = Invoke-GuidebotBrowserRun -Item $item } while ($action -eq 'restart')
    if ($action -eq 'quit') { break }
}

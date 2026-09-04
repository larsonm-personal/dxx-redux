#!/usr/bin/env pwsh

[CmdletBinding()]
param(
    [ValidateSet('Headless', 'Headed', 'Desktop')][string]$Mode = 'Headless',
    [string[]]$MissionJson,
    [int[]]$Level,
    [ValidateRange(0.000001, 1.0)][double]$SampleFraction = 1.0,
    [ValidateRange(0, [int]::MaxValue)][int]$SampleSeed = 0,
    [string]$SampleStatePath,
    [ValidateRange(1, 20)][int]$Repeat = 1,
    [ValidateRange(10, 7200)][int]$LevelTimeoutSeconds = 180,
    [string]$HogDir = 'game_data/CD images/Descent II (USA) (v1.1)/data_tracks/d2data',
    [string]$OutputRoot,
    [switch]$NoBuild,
    [switch]$WriteRegression,
    [switch]$DryRun,
    [string]$DryRunJsonOut,
    [string]$HeadlessExecutable,
    [switch]$LeaveHeadedRunning
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $PSCommandPath
$androidRoot = Split-Path -Parent $scriptDir
$repoRoot = Split-Path -Parent $androidRoot
$missionRoot = Join-Path $repoRoot 'game_data\mission_files'
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$runRoot = if ($OutputRoot) {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputRoot)
} else {
    Join-Path $androidRoot "temp\guidebot_simulation_regression\$stamp"
}
$rawRoot = Join-Path $runRoot 'raw'
$stageRoot = Join-Path $runRoot 'stages'
$logRoot = Join-Path $runRoot 'logs'
$resultRoot = Join-Path $runRoot 'results'
$exe = if ($HeadlessExecutable) {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($HeadlessExecutable)
} else {
    Join-Path $repoRoot 'buildd2\main\dxx-redux-d2-headless-route.exe'
}
$desktopExe = Join-Path $repoRoot 'buildd2\main\d2x-redux.exe'
$batchStart = [DateTime]::UtcNow
. (Join-Path $scriptDir 'guidebot_simulation_regression.ps1')
. (Join-Path $scriptDir 'runtime_targeted_sampling.ps1')
. (Join-Path $scriptDir 'cd_level_metadata_sources.ps1')

function Write-GuidebotStatus {
    param([string]$Message, [string]$Color = 'Cyan')
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Message" -ForegroundColor $Color
}

function Get-GuidebotMissionEntries {
    param([Parameter(Mandatory)][string]$Path)

    $text = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
    $value = $text | ConvertFrom-Json -NoEnumerate
    $entries = if ($text.TrimStart().StartsWith('[')) { @($value) } else { @($value) }
    if ($entries.Count -eq 0 -or @($entries | Where-Object {
                $null -eq $_.PSObject.Properties['levels'] -or
                $null -eq $_.PSObject.Properties['mission_filename']
            }).Count -gt 0) {
        throw "Not a mission metadata JSON file: $Path"
    }
    return $entries
}

function Get-GuidebotMissionFiles {
    $files = @(Get-ChildItem -LiteralPath $missionRoot -Recurse -File -Filter '*.json' |
            Where-Object {
                $_.Name -notlike '*.simulation.json' -and
                $_.Name -notlike '*.tracklist.json' -and
                $_.Name -notin @('cd_level_metadata_sources.json', 'summary.json')
            } | Sort-Object FullName)
    if ($MissionJson) {
        $files = @($files | Where-Object {
                $relative = $_.FullName.Substring($missionRoot.Length).TrimStart('\', '/').Replace('\', '/')
                foreach ($pattern in $MissionJson) {
                    if ($_.Name -like $pattern -or $_.BaseName -like $pattern -or $relative -like $pattern) { return $true }
                }
                return $false
            })
        if ($files.Count -eq 0) { throw "No mission JSON matched: $($MissionJson -join ', ')" }
    }
    return @($files | Where-Object {
            try {
                $entries = @(Get-GuidebotMissionEntries -Path $_.FullName)
                return @($entries | Where-Object { [string](Get-GuidebotPropertyValue $_ 'game' '') -eq 'd2' }).Count -gt 0
            } catch {
                return $false
            }
        })
}

function Get-GuidebotWorkItems {
    param([Parameter(Mandatory)][IO.FileInfo[]]$Files)

    $items = foreach ($file in $Files) {
        $relative = $file.FullName.Substring($missionRoot.Length).TrimStart('\', '/').Replace('\', '/')
        foreach ($mission in @(Get-GuidebotMissionEntries -Path $file.FullName)) {
            if ([string](Get-GuidebotPropertyValue $mission 'game' '') -ne 'd2') { continue }
            $targetIndex = [int](Get-GuidebotPropertyValue $mission 'target_index' 0)
            foreach ($levelRecord in @(Get-GuidebotPropertyValue $mission 'levels' @())) {
                $levelNumber = [int](Get-GuidebotPropertyValue $levelRecord 'level_num' 0)
                if ($Level -and $levelNumber -notin $Level) { continue }
                $levelFile = [string](Get-GuidebotPropertyValue $levelRecord 'level_file' '')
                $identity = "$relative|$targetIndex|$levelNumber|$levelFile"
                [pscustomobject]@{
                    Name = $identity
                    Identity = $identity
                    MetadataFile = $file
                    RelativeMetadata = $relative
                    Mission = $mission
                    Level = $levelRecord
                    LevelNumber = $levelNumber
                    EngineLevelNumber = if ($levelNumber -eq 0) { 1 } else { $levelNumber }
                    SimulationTimeLimitSeconds = Get-GuidebotSimulationTimeLimitSeconds -Level $levelRecord
                    RouteHash = Get-GuidebotRouteInputHash -Mission $mission -Level $levelRecord
                }
            }
        }
    }
    return @($items)
}

function Get-GuidebotSevenZip {
    $candidate = @(& (Join-Path $androidRoot 'get_deps\helpers\get_7zip.ps1') |
            Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } |
            Select-Object -Last 1)
    if ($candidate.Count -eq 0) { throw 'Verified 7za.exe was not found' }
    return $candidate[0]
}

function Expand-GuidebotMissionArchive {
    param(
        [Parameter(Mandatory)][IO.FileInfo]$Archive,
        [Parameter(Mandatory)][string]$Destination
    )

    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    if ($Archive.Extension.Equals('.7z', [StringComparison]::OrdinalIgnoreCase)) {
        $output = & (Get-GuidebotSevenZip) x -y "-o$Destination" -- $Archive.FullName 2>&1
        if ($LASTEXITCODE -ne 0) { throw "7z extraction failed for $($Archive.Name): $($output -join ' ')" }
        return
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory($Archive.FullName, $Destination)
}

function Copy-GuidebotFlatStage {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination
    )

    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    $used = @{}
    foreach ($file in @(Get-ChildItem -LiteralPath $Source -Recurse -File | Sort-Object FullName)) {
        $key = $file.Name.ToLowerInvariant()
        if ($used.ContainsKey($key)) { continue }
        $used[$key] = $true
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $Destination $file.Name) -Force
    }
}

function Initialize-GuidebotMissionStage {
    param([Parameter(Mandatory)][IO.FileInfo]$MetadataFile)

    if ($MetadataFile.Name -eq 'Counterstrike.json') {
        return [pscustomobject]@{ ExtraDir = ''; Source = 'builtin' }
    }
    $archive = @(
        foreach ($extension in @('.zip', '.7z')) {
            $candidate = Join-Path $MetadataFile.DirectoryName ($MetadataFile.BaseName + $extension)
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { Get-Item -LiteralPath $candidate }
        }
    ) | Select-Object -First 1
    $relativeMetadata = $MetadataFile.FullName.Substring($missionRoot.Length).TrimStart('\', '/').Replace('\', '/')
    $safeName = [regex]::Replace($MetadataFile.BaseName, '[^A-Za-z0-9_.-]+', '_') + '_' +
    (Get-GuidebotSha256 -Text $relativeMetadata).Substring(0, 10)
    $stage = Join-Path $stageRoot $safeName
    if ($archive) {
        $raw = Join-Path $rawRoot $safeName
        Expand-GuidebotMissionArchive -Archive $archive -Destination $raw
        Copy-GuidebotFlatStage -Source $raw -Destination (Join-Path $stage 'missions')
        return [pscustomobject]@{ ExtraDir = $stage; Source = $archive.FullName }
    }
    $manifestPath = Join-Path $missionRoot 'cd_level_metadata_sources.jsonc'
    if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        $cdSource = @(Resolve-CdLevelMetadataSources -RepoRoot $repoRoot -ManifestPath $manifestPath -OutputDir $missionRoot |
                Where-Object { $_.OutputPath -eq $MetadataFile.FullName } | Select-Object -First 1)
        if ($cdSource.Count -gt 0) {
            Copy-GuidebotFlatStage -Source $cdSource[0].SourceDir -Destination (Join-Path $stage 'missions')
            return [pscustomobject]@{ ExtraDir = $stage; Source = $cdSource[0].SourceDir }
        }
    }
    throw "Could not resolve mission assets for $($MetadataFile.FullName)"
}

function New-GuidebotNotRunResult {
    param(
        [Parameter(Mandatory)][object]$Mission,
        [Parameter(Mandatory)][object]$LevelRecord,
        [ValidateSet('not_run', 'stale')][string]$Status = 'not_run'
    )

    return [pscustomobject][ordered]@{
        level_num = [int](Get-GuidebotPropertyValue $LevelRecord 'level_num' 0)
        level_file = [string](Get-GuidebotPropertyValue $LevelRecord 'level_file' '')
        route_input_sha256 = Get-GuidebotRouteInputHash -Mission $Mission -Level $LevelRecord
        status = $Status
    }
}

function Test-GuidebotLevelAssetUnavailable {
    param([Parameter(Mandatory)][object]$LevelRecord)

    $problem = [string](Get-GuidebotPropertyValue $LevelRecord 'route_problem' '')
    if (-not $problem) { $problem = [string](Get-GuidebotPropertyValue $LevelRecord 'problems' '') }
    return $problem -match '(?i)level file is missing'
}

function New-GuidebotUnsupportedResult {
    param(
        [Parameter(Mandatory)][object]$Mission,
        [Parameter(Mandatory)][object]$LevelRecord
    )

    $problem = [string](Get-GuidebotPropertyValue $LevelRecord 'route_problem' '')
    if (-not $problem) { $problem = [string](Get-GuidebotPropertyValue $LevelRecord 'problems' 'level is unavailable') }
    return [pscustomobject][ordered]@{
        level_num = [int](Get-GuidebotPropertyValue $LevelRecord 'level_num' 0)
        level_file = [string](Get-GuidebotPropertyValue $LevelRecord 'level_file' '')
        route_input_sha256 = Get-GuidebotRouteInputHash -Mission $Mission -Level $LevelRecord
        status = 'unsupported'
        problem = $problem
    }
}

function New-GuidebotInfrastructureErrorResult {
    param(
        [Parameter(Mandatory)][object]$Mission,
        [Parameter(Mandatory)][object]$LevelRecord,
        [Parameter(Mandatory)][string]$Problem
    )

    return [pscustomobject][ordered]@{
        level_num = [int](Get-GuidebotPropertyValue $LevelRecord 'level_num' 0)
        level_file = [string](Get-GuidebotPropertyValue $LevelRecord 'level_file' '')
        route_input_sha256 = Get-GuidebotRouteInputHash -Mission $Mission -Level $LevelRecord
        status = 'infrastructure_error'
        problem = $Problem
    }
}

function Invoke-GuidebotHeadlessLevel {
    param(
        [Parameter(Mandatory)][object]$WorkItem,
        [Parameter(Mandatory)][object]$Stage
    )

    $missionName = [IO.Path]::GetFileNameWithoutExtension(
        [string](Get-GuidebotPropertyValue $WorkItem.Mission 'mission_filename' 'd2')
    )
    $safeIdentity = [regex]::Replace($WorkItem.Identity, '[^A-Za-z0-9_.-]+', '_')
    $referenceHash = ''
    $referenceResult = $null
    for ($run = 1; $run -le $Repeat; $run++) {
        $output = Join-Path $resultRoot "${safeIdentity}_run_${run}.json"
        $log = Join-Path $logRoot "${safeIdentity}_run_${run}.log"
        $arguments = @(
            '-hogdir', $resolvedHogDir,
            '-mission', $missionName,
            '-level', [string]$WorkItem.EngineLevelNumber,
            '-route-confirm-timeout-seconds', [string]$WorkItem.SimulationTimeLimitSeconds,
            '-route-confirm-json-out', $output
        )
        if ($Stage.ExtraDir) { $arguments += @('-extra-dir', $Stage.ExtraDir) }
        $errorLog = "$log.stderr"
        $processArguments = @($arguments | ForEach-Object {
                $argument = [string]$_
                if ($argument -match '[\s"]') { '"' + $argument.Replace('"', '\"') + '"' } else { $argument }
            }) -join ' '
        $process = Start-Process -FilePath $exe -ArgumentList $processArguments -PassThru -NoNewWindow `
            -RedirectStandardOutput $log -RedirectStandardError $errorLog
        if (-not $process.WaitForExit($LevelTimeoutSeconds * 1000)) {
            $process.Kill($true)
            $process.WaitForExit()
            throw "Route engine process timeout after $LevelTimeoutSeconds seconds for $($WorkItem.Identity), log=$log"
        }
        $exitCode = $process.ExitCode
        if (Test-Path -LiteralPath $errorLog -PathType Leaf) {
            Add-Content -LiteralPath $log -Value (Get-Content -LiteralPath $errorLog -Raw)
        }
        if ($exitCode -notin @(0, 2) -or -not (Test-Path -LiteralPath $output -PathType Leaf)) {
            throw "Route engine infrastructure failure for $($WorkItem.Identity), exit $exitCode, log=$log"
        }
        $hash = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
        $result = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
        if ($run -eq 1) {
            $referenceHash = $hash
            $referenceResult = $result
        } elseif ($hash -ne $referenceHash) {
            $nondeterministic = ConvertTo-GuidebotLevelSimulationResult -Mission $WorkItem.Mission -Level $WorkItem.Level -EngineResult $referenceResult
            $nondeterministic.status = 'nondeterministic'
            $nondeterministic | Add-Member -NotePropertyName problem -NotePropertyValue "headless repeat $run differed" -Force
            return $nondeterministic
        }
    }
    return ConvertTo-GuidebotLevelSimulationResult -Mission $WorkItem.Mission -Level $WorkItem.Level -EngineResult $referenceResult
}

function Invoke-GuidebotDesktopLevel {
    param(
        [Parameter(Mandatory)][object]$WorkItem,
        [Parameter(Mandatory)][object]$Stage
    )

    $missionName = [IO.Path]::GetFileNameWithoutExtension(
        [string](Get-GuidebotPropertyValue $WorkItem.Mission 'mission_filename' 'd2')
    )
    $safeIdentity = [regex]::Replace($WorkItem.Identity, '[^A-Za-z0-9_.-]+', '_')
    $output = Join-Path $resultRoot "${safeIdentity}_desktop.json"
    $log = Join-Path $logRoot "${safeIdentity}_desktop.log"
    $errorLog = "$log.stderr"
    $sandbox = Join-Path $runRoot 'desktop_sandbox'
    New-Item -ItemType Directory -Path $sandbox -Force | Out-Null
    $arguments = @(
        '-window', '-nomovies', '-nosound', '-nomusic',
        '-hogdir', $resolvedHogDir,
        '-mission', $missionName,
        '-level', [string]$WorkItem.EngineLevelNumber,
        '-route-confirm-timeout-seconds', [string]$WorkItem.SimulationTimeLimitSeconds,
        '-route-confirm-json-out', $output
    )
    if ($Stage.ExtraDir) { $arguments += @('-extra-dir', $Stage.ExtraDir) }
    $processArguments = @($arguments | ForEach-Object {
            $argument = [string]$_
            if ($argument -match '[\s"]') { '"' + $argument.Replace('"', '\"') + '"' } else { $argument }
        }) -join ' '
    $process = Start-Process -FilePath $desktopExe -ArgumentList $processArguments -PassThru `
        -WorkingDirectory $sandbox -RedirectStandardOutput $log -RedirectStandardError $errorLog
    $processTimeoutSeconds = [Math]::Max(
        $LevelTimeoutSeconds,
        $WorkItem.SimulationTimeLimitSeconds + 60
    )
    if (-not $process.WaitForExit($processTimeoutSeconds * 1000)) {
        $process.Kill($true)
        $process.WaitForExit()
        throw "Desktop route process timeout after $processTimeoutSeconds seconds for $($WorkItem.Identity), log=$log"
    }
    $exitCode = $process.ExitCode
    if (Test-Path -LiteralPath $errorLog -PathType Leaf) {
        Add-Content -LiteralPath $log -Value (Get-Content -LiteralPath $errorLog -Raw)
    }
    if ($exitCode -ne 0 -or -not (Test-Path -LiteralPath $output -PathType Leaf)) {
        throw "Desktop route infrastructure failure for $($WorkItem.Identity), exit $exitCode, log=$log"
    }
    $engineResult = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
    return ConvertTo-GuidebotLevelSimulationResult -Mission $WorkItem.Mission -Level $WorkItem.Level `
        -EngineResult $engineResult
}

function Get-GuidebotAdb {
    $candidate = Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
    if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    $candidate = 'C:\local\android-sdk\platform-tools\adb.exe'
    if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    return 'adb'
}

function Push-GuidebotMissionArchive {
    param([Parameter(Mandatory)][string]$ArchivePath)

    $adb = Get-GuidebotAdb
    $deviceName = [IO.Path]::GetFileName($ArchivePath)
    & $adb push $ArchivePath "/data/local/tmp/$deviceName" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Could not push headed mission archive $ArchivePath" }
    & $adb shell run-as com.dxxredux.app mkdir -p files/mission_zip_batch_cache | Out-Null
    & $adb shell run-as com.dxxredux.app cp "/data/local/tmp/$deviceName" "files/mission_zip_batch_cache/$deviceName" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Could not stage headed mission archive $deviceName" }
    & $adb shell rm -f "/data/local/tmp/$deviceName" | Out-Null
    return $deviceName
}

function New-GuidebotHeadedArchive {
    param(
        [Parameter(Mandatory)][object]$Stage,
        [Parameter(Mandatory)][object]$WorkItem
    )

    if ($Stage.Source -eq 'builtin') { return '' }
    if (Test-Path -LiteralPath $Stage.Source -PathType Leaf) { return $Stage.Source }
    $safeIdentity = [regex]::Replace($WorkItem.Identity, '[^A-Za-z0-9_.-]+', '_')
    $archive = Join-Path $runRoot "$safeIdentity.headed.zip"
    Compress-Archive -Path (Join-Path $Stage.ExtraDir '*') -DestinationPath $archive -Force
    return $archive
}

function New-GuidebotHeadedScript {
    param(
        [Parameter(Mandatory)][object]$WorkItem,
        [string]$DeviceArchiveName,
        [Parameter(Mandatory)][string]$Path
    )

    $missionName = [string](Get-GuidebotPropertyValue $WorkItem.Mission 'mission_name' '')
    if (-not $missionName) {
        $missionName = [IO.Path]::GetFileNameWithoutExtension(
            [string](Get-GuidebotPropertyValue $WorkItem.Mission 'mission_filename' 'd2')
        )
    }
    $steps = [Collections.Generic.List[object]]::new()
    $steps.Add([ordered]@{ _info = [ordered]@{
                games = @('d2')
                _deps = @(
                    [ordered]@{ file = 'descent2.hog'; sha256 = 'f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703' }
                    [ordered]@{ file = 'descent2.ham'; sha256 = '5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d' }
                    [ordered]@{ file = 'groupa.pig'; sha256 = 'facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b' }
                )
            }
        })
    $steps.Add([ordered]@{ action = 'enter_launcher' })
    $steps.Add([ordered]@{ action = 'reset_state' })
    $steps.Add([ordered]@{ action = 'clear_mods' })
    $steps.Add([ordered]@{ action = 'setup_command'; command = 'clear_save_files'; post_delay_ms = 250 })
    if ($DeviceArchiveName) {
        $steps.Add([ordered]@{
                action = 'import_mission_zip'
                file = $DeviceArchiveName
                label = 'guidebot_manual'
                display_name = $DeviceArchiveName
            })
        $steps.Add([ordered]@{ action = 'tap_button'; text = 'Descent 2'; post_delay_ms = 0 })
        $steps.Add([ordered]@{ action = 'tap_button'; text = 'Launch Descent 2'; launches_game = $true })
    } else {
        $steps.Add([ordered]@{ action = 'enter_game'; game = 'd2' })
    }
    $steps.Add([ordered]@{ action = 'wait_for'; field = 'intro_active'; value = 'true'; timeout_ms = 20000; optional = $true })
    $steps.Add([ordered]@{ action = 'skip_intro'; timeout_ms = 20000; post_delay_ms = 200 })
    $steps.Add([ordered]@{ action = 'wait_for'; field = 'screen_mode'; value = 'menu'; timeout_ms = 20000 })
    $steps.Add([ordered]@{ action = 'select'; text = 'Ok'; post_delay_ms = 100; timeout_ms = 5000; optional = $true })
    $steps.Add([ordered]@{ action = 'select'; text = 'New game'; post_delay_ms = 100; timeout_ms = 10000 })
    $steps.Add([ordered]@{ action = 'select_mission'; text = $missionName; post_delay_ms = 100; timeout_ms = 10000 })
    $steps.Add([ordered]@{ action = 'wait_ms'; ms = 300 })
    $steps.Add([ordered]@{ action = 'key'; key = 'backspace'; post_delay_ms = 100 })
    foreach ($digit in ([string]$WorkItem.EngineLevelNumber).ToCharArray()) {
        $steps.Add([ordered]@{ action = 'key'; key = [string]$digit; post_delay_ms = 100 })
    }
    $steps.Add([ordered]@{ action = 'select'; text = 'Ok'; post_delay_ms = 100; timeout_ms = 5000 })
    $steps.Add([ordered]@{ action = 'select'; text = 'Rookie'; post_delay_ms = 100; timeout_ms = 10000 })
    $steps.Add([ordered]@{ action = 'skip_briefing'; timeout_ms = 60000; post_delay_ms = 100 })
    $steps.Add([ordered]@{ action = 'wait_for'; field = 'in_game'; value = 'true'; timeout_ms = 30000 })
    $steps.Add([ordered]@{
            action = 'set_debug'
            field = 'start_route_confirmation'
            value = [string]$WorkItem.SimulationTimeLimitSeconds
        })
    $steps.Add([ordered]@{
            action = 'wait_for'
            field = 'route_confirmation_terminal'
            value = 'true'
            timeout_ms = ($WorkItem.SimulationTimeLimitSeconds + 15) * 1000
        })
    $steps.Add([ordered]@{ action = 'introspect' })
    [IO.File]::WriteAllText(
        $Path,
        (($steps | ConvertTo-Json -Depth 12) -replace "`r`n", "`n") + "`n",
        [Text.UTF8Encoding]::new($false)
    )
}

function Read-GuidebotHeadedIntrospection {
    $adb = Get-GuidebotAdb
    $text = (& $adb exec-out run-as com.dxxredux.app cat files/introspect.json) | Out-String
    if ($LASTEXITCODE -ne 0 -or -not $text.Trim()) { throw 'Could not read headed route introspection' }
    return $text | ConvertFrom-Json
}

function Invoke-GuidebotHeadedLevel {
    param(
        [Parameter(Mandatory)][object]$WorkItem,
        [Parameter(Mandatory)][object]$Stage,
        [switch]$Install
    )

    $archivePath = New-GuidebotHeadedArchive -Stage $Stage -WorkItem $WorkItem
    $referenceText = ''
    $referenceResult = $null
    for ($run = 1; $run -le $Repeat; $run++) {
        $deviceArchive = if ($archivePath) { Push-GuidebotMissionArchive -ArchivePath $archivePath } else { '' }
        $safeIdentity = [regex]::Replace($WorkItem.Identity, '[^A-Za-z0-9_.-]+', '_')
        $scriptPath = Join-Path $resultRoot "${safeIdentity}_headed_${run}.jsonc"
        New-GuidebotHeadedScript -WorkItem $WorkItem -DeviceArchiveName $deviceArchive -Path $scriptPath
        $automationTimeoutSeconds = [Math]::Max(
            $LevelTimeoutSeconds,
            $WorkItem.SimulationTimeLimitSeconds + 120
        )
        $arguments = @(
            $scriptPath,
            '-Game', 'd2',
            '-TimeoutSeconds', [string]$automationTimeoutSeconds
        )
        if ($Install -and $run -eq 1) { $arguments += '-Install' }
        if ($LeaveHeadedRunning -and $run -eq $Repeat) { $arguments += '-LeaveRunning' }
        $pwsh = (Get-Process -Id $PID).Path
        $automationLog = Join-Path $logRoot "${safeIdentity}_headed_${run}.log"
        & $pwsh -NoProfile -File (Join-Path $scriptDir 'run_test.ps1') @arguments 2>&1 |
            Tee-Object -FilePath $automationLog | ForEach-Object { Write-Host $_ }
        $automationExitCode = $LASTEXITCODE
        if ($automationExitCode -ne 0) { throw "Headed route automation failed for $($WorkItem.Identity)" }
        $introspection = Read-GuidebotHeadedIntrospection
        $engineResult = $introspection.route_confirmation
        $resultPath = Join-Path $resultRoot "${safeIdentity}_headed_${run}.json"
        [IO.File]::WriteAllText($resultPath, ($engineResult | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))
        $compactResult = ConvertTo-GuidebotLevelSimulationResult -Mission $WorkItem.Mission -Level $WorkItem.Level `
            -EngineResult $engineResult
        $normalized = ($compactResult | ConvertTo-Json -Depth 20 -Compress)
        if ($run -eq 1) {
            $referenceText = $normalized
            $referenceResult = $compactResult
        } elseif ($normalized -cne $referenceText) {
            $referenceResult.status = 'nondeterministic'
            $referenceResult | Add-Member -NotePropertyName problem -NotePropertyValue "headed repeat $run differed" -Force
            return $referenceResult
        }
    }
    return $referenceResult
}

function Get-ExistingGuidebotLevelMap {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][int]$TargetIndex
    )

    $map = @{}
    $map['__contract_current'] = $false
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $map }
    $text = [IO.File]::ReadAllText($Path)
    $value = $text | ConvertFrom-Json -NoEnumerate
    $records = if ($text.TrimStart().StartsWith('[')) { @($value) } else { @($value) }
    $record = @($records | Where-Object { [int]$_.target_index -eq $TargetIndex } | Select-Object -First 1)
    if ($record.Count -eq 0) { return $map }
    $map['__contract_current'] =
    [int](Get-GuidebotPropertyValue $record[0] 'generation' -1) -eq $script:GuidebotSimulationGeneration -and
    [int](Get-GuidebotPropertyValue $record[0] 'fixed_hz' -1) -eq $script:GuidebotSimulationFixedHz -and
    [uint32](Get-GuidebotPropertyValue $record[0] 'seed' 0) -eq $script:GuidebotSimulationSeed
    foreach ($levelRecord in @($record[0].levels)) {
        $map["$($levelRecord.level_num)|$($levelRecord.level_file)"] = $levelRecord
    }
    return $map
}

function Write-GuidebotSimulationFile {
    param(
        [Parameter(Mandatory)][IO.FileInfo]$MetadataFile,
        [Parameter(Mandatory)][hashtable]$ResultsByIdentity,
        [Parameter(Mandatory)][string]$Destination,
        [Collections.Generic.List[object]]$HeadedComparisons,
        [string]$ComparisonIdentity
    )

    $entries = @(Get-GuidebotMissionEntries -Path $MetadataFile.FullName)
    $simulationPath = Join-Path $MetadataFile.DirectoryName ($MetadataFile.BaseName + '.simulation.json')
    $relative = $MetadataFile.FullName.Substring($missionRoot.Length).TrimStart('\', '/').Replace('\', '/')
    $simulationEntries = foreach ($mission in $entries) {
        if ([string](Get-GuidebotPropertyValue $mission 'game' '') -ne 'd2') { continue }
        $targetIndex = [int](Get-GuidebotPropertyValue $mission 'target_index' 0)
        $existing = Get-ExistingGuidebotLevelMap -Path $simulationPath -TargetIndex $targetIndex
        $levels = foreach ($levelRecord in @($mission.levels)) {
            $levelNumber = [int]$levelRecord.level_num
            $levelFile = [string]$levelRecord.level_file
            $identity = "$relative|$targetIndex|$levelNumber|$levelFile"
            if ($ResultsByIdentity.ContainsKey($identity)) {
                if ($Mode -eq 'Headed' -and $null -ne $HeadedComparisons -and $identity -eq $ComparisonIdentity) {
                    $existingKey = "$levelNumber|$levelFile"
                    $canonical = if ($existing.ContainsKey($existingKey)) { $existing[$existingKey] } else { $null }
                    $headed = $ResultsByIdentity[$identity]
                    $HeadedComparisons.Add([ordered]@{
                            identity = $identity
                            canonical_present = $null -ne $canonical
                            semantic_match = $null -ne $canonical -and
                            [string]$canonical.route_input_sha256 -eq [string]$headed.route_input_sha256 -and
                            [string]$canonical.status -eq [string]$headed.status -and
                            (@($canonical.objectives.n) -join "`0") -ceq
                            (@($headed.objectives.n) -join "`0")
                            canonical_status = if ($canonical) { [string]$canonical.status } else { '' }
                            headed_status = [string]$headed.status
                            canonical_frames = if ($canonical) { Get-GuidebotPropertyValue $canonical 'total_frames' $null } else { $null }
                            headed_frames = Get-GuidebotPropertyValue $headed 'total_frames' $null
                        })
                }
                $ResultsByIdentity[$identity]
                continue
            }
            $existingKey = "$levelNumber|$levelFile"
            $currentHash = Get-GuidebotRouteInputHash -Mission $mission -Level $levelRecord
            if ($existing.ContainsKey($existingKey)) {
                $old = $existing[$existingKey]
                $currentContract = [bool]$existing['__contract_current'] -and
                [string]$old.route_input_sha256 -eq $currentHash
                if ($currentContract) { $old } else { New-GuidebotNotRunResult -Mission $mission -LevelRecord $levelRecord -Status stale }
            } else {
                New-GuidebotNotRunResult -Mission $mission -LevelRecord $levelRecord
            }
        }
        $record = New-GuidebotMissionSimulationRecord -Mission $mission -Levels @($levels)
        Test-GuidebotMissionSimulationRecord -Record $record -ThrowOnError | Out-Null
        $record
    }
    $outputValue = if ([IO.File]::ReadAllText($MetadataFile.FullName).TrimStart().StartsWith('[')) {
        @($simulationEntries)
    } else {
        $simulationEntries[0]
    }
    Write-GuidebotSimulationJson -Path $Destination -Value $outputValue
}

$hogPath = if ([IO.Path]::IsPathRooted($HogDir)) { $HogDir } else { Join-Path $repoRoot $HogDir }
$resolvedHogDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($hogPath)
$files = @(Get-GuidebotMissionFiles)
$allItems = @(Get-GuidebotWorkItems -Files $files)
$selectedItems = $allItems
if ($SampleFraction -lt 1.0) {
    if ($SampleSeed -eq 0) { $SampleSeed = Get-Random -Minimum 1 -Maximum ([int]::MaxValue) }
    $selectedItems = @(Select-RuntimeHashRingFractionItems -Items $allItems -Fraction $SampleFraction `
            -Seed ($SampleSeed -bxor 611) -StatePath $SampleStatePath `
            -RingName 'regenerate:guidebot-simulation:levels')
}

Write-GuidebotStatus "GuideBot simulation work: $($selectedItems.Count)/$($allItems.Count) levels, mode $Mode"
if ($DryRun) {
    if ($DryRunJsonOut) {
        Write-GuidebotSimulationJson -Path $DryRunJsonOut -Value @($selectedItems | ForEach-Object {
                [ordered]@{
                    identity = $_.Identity
                    engine_level = $_.EngineLevelNumber
                    simulation_time_limit_seconds = $_.SimulationTimeLimitSeconds
                    route_input_sha256 = $_.RouteHash
                }
            })
    }
    $selectedItems | Select-Object Identity, SimulationTimeLimitSeconds, RouteHash
    exit 0
}
if ($Mode -in @('Headless', 'Desktop') -and -not (Test-Path -LiteralPath $resolvedHogDir -PathType Container)) {
    throw "D2 HOG directory not found: $resolvedHogDir"
}
if ($Mode -eq 'Headed' -and $WriteRegression) {
    Write-GuidebotStatus 'Warning: headed results are noncanonical and will update regression files only because -WriteRegression was explicit' 'Yellow'
}
if (-not $NoBuild) {
    if ($Mode -in @('Headless', 'Desktop')) {
        & (Join-Path $repoRoot 'run-windows-build.ps1') -Target d2
        if ($LASTEXITCODE -ne 0) { throw "D2 build failed with exit code $LASTEXITCODE" }
    } else {
        $env:JAVA_HOME = 'C:\local\jdk-21'
        & (Join-Path $androidRoot 'gradlew.bat') -p $androidRoot :app:assembleDebug
        if ($LASTEXITCODE -ne 0) { throw "Android build failed with exit code $LASTEXITCODE" }
    }
}
if ($Mode -eq 'Headless' -and -not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Route executable not found: $exe" }
if ($Mode -eq 'Desktop' -and -not (Test-Path -LiteralPath $desktopExe -PathType Leaf)) { throw "Desktop route executable not found: $desktopExe" }
New-Item -ItemType Directory -Path $runRoot, $rawRoot, $stageRoot, $logRoot, $resultRoot -Force | Out-Null

$selectedByIdentity = @{}
foreach ($item in $selectedItems) { $selectedByIdentity[$item.Identity] = $item }
$resultsByIdentity = @{}
$infrastructureFailures = [Collections.Generic.List[object]]::new()
$changedFiles = [Collections.Generic.List[string]]::new()
$headedComparisons = [Collections.Generic.List[object]]::new()
$stageByMetadata = @{}
$index = 0
$installHeaded = $Mode -eq 'Headed' -and -not $NoBuild
foreach ($item in $selectedItems) {
    $index++
    Write-GuidebotStatus "[$index/$($selectedItems.Count)] $($item.Identity) budget=$($item.SimulationTimeLimitSeconds)s"
    $metadataKey = $item.MetadataFile.FullName.ToLowerInvariant()
    try {
        if (Test-GuidebotLevelAssetUnavailable -LevelRecord $item.Level) {
            $resultsByIdentity[$item.Identity] = New-GuidebotUnsupportedResult `
                -Mission $item.Mission -LevelRecord $item.Level
            Write-GuidebotStatus "UNSUPPORTED: $($item.Identity): $($resultsByIdentity[$item.Identity].problem)" 'Yellow'
        } else {
            if (-not $stageByMetadata.ContainsKey($metadataKey)) {
                $stageByMetadata[$metadataKey] = Initialize-GuidebotMissionStage -MetadataFile $item.MetadataFile
            }
            $resultsByIdentity[$item.Identity] = switch ($Mode) {
                'Headless' { Invoke-GuidebotHeadlessLevel -WorkItem $item -Stage $stageByMetadata[$metadataKey] }
                'Desktop' { Invoke-GuidebotDesktopLevel -WorkItem $item -Stage $stageByMetadata[$metadataKey] }
                default { Invoke-GuidebotHeadedLevel -WorkItem $item -Stage $stageByMetadata[$metadataKey] -Install:$installHeaded }
            }
            $installHeaded = $false
        }
    } catch {
        $problem = $_.Exception.Message
        Write-GuidebotStatus "FAILED: $($item.Identity): $problem" 'Red'
        $resultsByIdentity[$item.Identity] = New-GuidebotInfrastructureErrorResult `
            -Mission $item.Mission -LevelRecord $item.Level -Problem $problem
        $infrastructureFailures.Add([ordered]@{ identity = $item.Identity; problem = $problem })
    }
    $simulationPath = Join-Path $item.MetadataFile.DirectoryName ($item.MetadataFile.BaseName + '.simulation.json')
    $incrementalPath = if ($WriteRegression) {
        $simulationPath
    } else {
        Join-Path $resultRoot $item.MetadataFile.Name.Replace('.json', '.simulation.json')
    }
    Write-GuidebotSimulationFile -MetadataFile $item.MetadataFile `
        -ResultsByIdentity $resultsByIdentity -Destination $incrementalPath `
        -HeadedComparisons $headedComparisons -ComparisonIdentity $item.Identity
    if ($WriteRegression -and -not $changedFiles.Contains($simulationPath)) { $changedFiles.Add($simulationPath) }
}

foreach ($file in $files) {
    $simulationPath = Join-Path $file.DirectoryName ($file.BaseName + '.simulation.json')
    $targetPath = if ($WriteRegression) { $simulationPath } else { Join-Path $resultRoot $file.Name.Replace('.json', '.simulation.json') }
    Write-GuidebotSimulationFile -MetadataFile $file -ResultsByIdentity $resultsByIdentity `
        -Destination $targetPath
    if ($WriteRegression -and -not $changedFiles.Contains($simulationPath)) { $changedFiles.Add($simulationPath) }
}

$summary = [ordered]@{
    schema = 'dxx-guidebot-route-simulation-batch-v1'
    mode = $Mode.ToLowerInvariant()
    selected_levels = $selectedItems.Count
    total_levels = $allItems.Count
    write_regression = [bool]$WriteRegression
    changed_files = @($changedFiles)
    elapsed_seconds = [Math]::Round(([DateTime]::UtcNow - $batchStart).TotalSeconds, 3)
    selected_work_items = @($selectedItems.Identity)
    infrastructure_failures = @($infrastructureFailures)
    headed_comparisons = @($headedComparisons)
    run_root = $runRoot
}
Write-GuidebotSimulationJson -Path (Join-Path $runRoot 'summary.json') -Value $summary
Write-GuidebotStatus "GuideBot simulation complete: $($selectedItems.Count) levels" 'Green'
Write-GuidebotStatus "Output: $runRoot" 'Green'
if ($infrastructureFailures.Count) { exit 1 }

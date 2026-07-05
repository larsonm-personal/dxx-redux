#!/usr/bin/env pwsh
# Runs the reusable mission ZIP import/metadata/launch smoke test over a folder.

param(
    [string]$ZipDir = "C:\local\dxx-redux\game_data\mission_files",
    [string]$OutDir = "",
    [string[]]$Pattern = @("*.zip", "*.7z"),
    [string]$RegressionJsonDir = "",
    [switch]$Install,
    [switch]$IncludeLarge,
    [string[]]$LargeZipIncludePatterns = @("ewithin-versions.zip"),
    [switch]$NoRegressionJson,
    [long]$LargeZipBytes = 524288000,
    [int]$MaxZips = 0,
    [int]$TimeoutSeconds = 900,
    [int]$MaxEmulatorRecoveries = 5
)

$ErrorActionPreference = "Stop"
$helpersDir = Split-Path -Parent $PSCommandPath
$androidRoot = Split-Path -Parent $helpersDir
. (Join-Path $helpersDir "test_helpers.ps1")
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-7zaPath {
    $onPath = Get-Command 7za -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $onPath = Get-Command 7z -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $result = & "$androidRoot\get_deps\helpers\get_7zip.ps1"
    $candidate = @($result | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -Last 1)
    if ($candidate) { return $candidate[0] }
    throw "7za.exe not found. Run android/get_deps/helpers/get_7zip.ps1 or install 7-Zip"
}

if (-not $OutDir) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutDir = Join-Path $androidRoot "temp\mission_zip_batch\$stamp"
}

$metadataDir = Join-Path $OutDir "metadata"
$importsDir = Join-Path $OutDir "imports"
$artifactsDir = Join-Path $OutDir "artifacts"
$resolvedScriptDir = Join-Path $OutDir "resolved_scripts"
New-Item -ItemType Directory -Force -Path $metadataDir, $importsDir, $artifactsDir, $resolvedScriptDir | Out-Null
$script:LogFile = Join-Path $OutDir "batch.log"
$script:MissionZipBatchRecoveryCount = 0

function Get-SafeMissionZipLabel {
    param([Parameter(Mandatory = $true)][string]$Name)
    $stem = [IO.Path]::GetFileNameWithoutExtension($Name).ToLowerInvariant()
    $safe = ([regex]::Replace($stem, '[^a-z0-9._-]+', '_')).Trim('_')
    if (-not $safe) { $safe = "mission_zip" }
    return $safe
}

function ConvertTo-JsonStringContent {
    param([Parameter(Mandatory = $true)][string]$Value)
    $literal = $Value | ConvertTo-Json -Compress
    return $literal.Substring(1, $literal.Length - 2)
}

function ConvertTo-NormalizedJsonText {
    param([Parameter(Mandatory = $true)][string]$Text)

    $trimmed = $Text.Trim()
    if (-not $trimmed) { return $Text }
    $formatterPath = Join-Path $helpersDir "normalize_json.py"
    if (-not (Test-Path -LiteralPath $formatterPath -PathType Leaf)) {
        throw "JSON formatter not found: $formatterPath"
    }
    $python = Get-Command python -ErrorAction SilentlyContinue
    $usePyLauncher = $false
    if (-not $python) {
        $python = Get-Command py -ErrorAction SilentlyContinue
        $usePyLauncher = $true
    }
    if (-not $python) {
        throw "Python not found for JSON formatting"
    }

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $python.Source
    if ($usePyLauncher) {
        [void]$startInfo.ArgumentList.Add("-3")
    }
    [void]$startInfo.ArgumentList.Add($formatterPath)
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    $startInfo.StandardInputEncoding = $utf8NoBom
    $startInfo.StandardOutputEncoding = $utf8NoBom
    $startInfo.StandardErrorEncoding = $utf8NoBom
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true

    $process = [System.Diagnostics.Process]::Start($startInfo)
    try {
        $process.StandardInput.Write($trimmed)
        $process.StandardInput.Close()
        $json = $process.StandardOutput.ReadToEnd()
        $errorText = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            throw "JSON formatter failed with exit code $($process.ExitCode): $errorText"
        }
        $json = $json -replace "`r`n", "`n"
        return ($json.TrimEnd([char[]]@("`r", "`n")) + "`n")
    } finally {
        $process.Dispose()
    }
}

function Write-Utf8NoBomText {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Text
    )

    [System.IO.File]::WriteAllText($Path, $Text, [System.Text.UTF8Encoding]::new($false))
}

function Write-TestJsonText {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Text
    )

    try {
        Write-Utf8NoBomText -Path $Path -Text (ConvertTo-NormalizedJsonText -Text $Text)
    } catch {
        Write-Warning "JSON normalization failed for ${Path}: $($_.Exception.Message)"
        Write-Utf8NoBomText -Path $Path -Text $Text
    }
}

function Get-ShortMissionZipFailureText {
    param([Parameter(Mandatory = $true)][System.Collections.IDictionary]$Record)

    $reason = if ($Record.Contains("reason") -and $Record["reason"]) {
        [string]$Record["reason"]
    } elseif ($Record.Contains("status") -and $Record["status"]) {
        [string]$Record["status"]
    } else {
        "mission ZIP failed"
    }
    $reason = ($reason -replace '[\r\n\t]+', ' ') -replace '\s{2,}', ' '
    $reason = $reason.Trim()
    if ($reason.Length -gt 240) {
        $reason = $reason.Substring(0, 237).TrimEnd() + "..."
    }
    return $reason
}

function Write-MissionZipFailureJson {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Record
    )

    $failure = [ordered]@{
        failure_text = Get-ShortMissionZipFailureText -Record $Record
    }
    Write-TestJsonText -Path $Path -Text ($failure | ConvertTo-Json -Depth 3)
}

function Test-LargeMissionZipIncluded {
    param([Parameter(Mandatory = $true)][string]$Name)

    if ($IncludeLarge) {
        return $true
    }
    foreach ($pattern in @($LargeZipIncludePatterns)) {
        if ($pattern -and $Name -like $pattern) {
            return $true
        }
    }
    return $false
}

function Get-MissionZipRegressionJsonPath {
    param([Parameter(Mandatory = $true)][System.IO.FileInfo]$Zip)

    if ($NoRegressionJson) {
        return ""
    }
    return (Join-Path $RegressionJsonDir "$($Zip.BaseName).json")
}

function Get-MissionZipBatchSortRecord {
    param([Parameter(Mandatory = $true)][System.IO.FileInfo]$Zip)

    $jsonPath = Get-MissionZipRegressionJsonPath -Zip $Zip
    $jsonItem = if ($jsonPath) { Get-Item -LiteralPath $jsonPath -ErrorAction SilentlyContinue } else { $null }
    [pscustomobject]@{
        Zip = $Zip
        JsonPath = $jsonPath
        HasJson = $null -ne $jsonItem
        JsonLastWriteTimeUtc = if ($jsonItem) { $jsonItem.LastWriteTimeUtc } else { [datetime]::MinValue }
    }
}

function Write-MissionZipBatchOrder {
    param([Parameter(Mandatory = $true)][object[]]$Records)

    Write-Status "Mission ZIP processing order ($($Records.Count) zips)"
    $index = 1
    foreach ($record in $Records) {
        if ($record.HasJson) {
            $state = "existing JSON modified $($record.JsonLastWriteTimeUtc.ToString("yyyy-MM-dd HH:mm:ssZ"))"
        } elseif ($record.JsonPath) {
            $state = "missing JSON at $($record.JsonPath)"
        } else {
            $state = "regression JSON disabled"
        }
        Write-Host ("  {0,3}. {1} -- {2}" -f $index, $record.Zip.Name, $state)
        $index++
    }
}

function Test-AppPackageInstalled {
    $path = Adb-Timeout -AdbArgs @("shell", "pm", "path", $script:PACKAGE) -Seconds 10
    return ($path -and $path -match 'package:')
}

function Restore-MissionZipBatchDevice {
    param([Parameter(Mandatory = $true)][string]$Reason)

    $script:MissionZipBatchRecoveryCount++
    if ($script:MissionZipBatchRecoveryCount -gt $MaxEmulatorRecoveries) {
        throw "emulator recovery limit reached ($MaxEmulatorRecoveries)"
    }

    Write-Status "$Reason -- recovering emulator ($script:MissionZipBatchRecoveryCount/$MaxEmulatorRecoveries)" "Yellow"
    Restart-AdbServer
    Ensure-EmulatorHealthy | Out-Null

    Write-Status "Installing APK after emulator recovery"
    if (-not (Install-ApkOnDevice)) {
        throw "APK install failed after emulator recovery"
    }

    Write-Status "Restoring standard D1/D2 base data after emulator recovery"
    if (-not (Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps))) {
        throw "could not restore base game data after emulator recovery"
    }
}

function Ensure-MissionZipBatchDeviceReady {
    param([Parameter(Mandatory = $true)][string]$Reason)

    if ((Test-EmulatorHealthy) -and (Test-AppPackageInstalled)) {
        return
    }
    Restore-MissionZipBatchDevice -Reason $Reason
}

function Test-MissionZipNeedsEmulatorRecovery {
    param([string]$Reason)

    return $Reason -match 'SetupActivity did not become ready|adb push failed|run-as copy failed|Emulator crashed|device .*not found|device offline|protocol fault|closed'
}

function Add-MissionZipGameHints {
    param(
        [Parameter(Mandatory = $true)][string[]]$EntryNames,
        [Parameter(Mandatory = $true)][hashtable]$Counts,
        [int]$Depth = 0
    )

    foreach ($entryName in $EntryNames) {
        if (-not $entryName) { continue }
        $name = ([IO.Path]::GetFileName($entryName)).ToLowerInvariant()
        switch ([IO.Path]::GetExtension($name)) {
            ".msn" { $Counts.d1 += 10 }
            ".rdl" { $Counts.d1 += 1 }
            ".sdl" { $Counts.d1 += 1 }
            ".mn2" { $Counts.d2 += 10 }
            ".rl2" { $Counts.d2 += 1 }
            ".sl2" { $Counts.d2 += 1 }
        }
    }
}

function Add-ZipMissionGameHints {
    param(
        [Parameter(Mandatory = $true)]$Archive,
        [Parameter(Mandatory = $true)][hashtable]$Counts,
        [int]$Depth = 0
    )

    foreach ($entry in $Archive.Entries) {
        if (-not $entry.Name) { continue }
        $name = $entry.Name.ToLowerInvariant()
        switch ([IO.Path]::GetExtension($name)) {
            ".msn" { $Counts.d1 += 10 }
            ".rdl" { $Counts.d1 += 1 }
            ".sdl" { $Counts.d1 += 1 }
            ".mn2" { $Counts.d2 += 10 }
            ".rl2" { $Counts.d2 += 1 }
            ".sl2" { $Counts.d2 += 1 }
            ".zip" {
                if ($Depth -ge 2 -or $entry.Length -gt 100MB) { break }
                $stream = $entry.Open()
                $memory = New-Object System.IO.MemoryStream
                try {
                    $stream.CopyTo($memory)
                    $memory.Position = 0
                    $nested = New-Object System.IO.Compression.ZipArchive($memory, [System.IO.Compression.ZipArchiveMode]::Read, $true)
                    try {
                        Add-ZipMissionGameHints -Archive $nested -Counts $Counts -Depth ($Depth + 1)
                    } finally {
                        $nested.Dispose()
                    }
                } catch {
                    $Counts.problems += "Could not inspect nested ZIP $($entry.FullName): $($_.Exception.Message)"
                } finally {
                    $stream.Dispose()
                    $memory.Dispose()
                }
            }
        }
    }
}

function Get-MissionArchiveEntryNames {
    param([Parameter(Mandatory = $true)][string]$ArchivePath)

    $ext = [IO.Path]::GetExtension($ArchivePath).ToLowerInvariant()
    if ($ext -eq ".7z") {
        $sevenZip = Get-7zaPath
        $output = & $sevenZip l -slt -- $ArchivePath 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "7z list failed for ${ArchivePath}: $($output -join ' ')"
        }
        return @(
            $output |
                Where-Object { $_ -match '^Path = (.+)$' } |
                ForEach-Object { $Matches[1] } |
                Where-Object { $_ -and $_ -ne $ArchivePath }
        )
    }

    $archive = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        return @($archive.Entries | Where-Object { $_.Name } | ForEach-Object { $_.FullName })
    } finally {
        $archive.Dispose()
    }
}

function Get-MissionZipGameHint {
    param([Parameter(Mandatory = $true)][string]$ZipPath)

    $counts = @{ d1 = 0; d2 = 0; problems = @() }
    if ([IO.Path]::GetExtension($ZipPath).Equals(".7z", [StringComparison]::OrdinalIgnoreCase)) {
        try {
            Add-MissionZipGameHints -EntryNames (Get-MissionArchiveEntryNames -ArchivePath $ZipPath) -Counts $counts
        } catch {
            $counts.problems += "Could not inspect archive: $($_.Exception.Message)"
        }
    } else {
        $archive = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
        try {
            Add-ZipMissionGameHints -Archive $archive -Counts $counts
        } finally {
            $archive.Dispose()
        }
    }

    $game =
    if ($counts.d1 -gt 0 -and $counts.d2 -eq 0) {
        "d1"
    } elseif ($counts.d2 -gt 0 -and $counts.d1 -eq 0) {
        "d2"
    } elseif ($counts.d1 -gt 0 -and $counts.d2 -gt 0) {
        "mixed"
    } else {
        "unknown"
    }
    $reason =
    if ($game -eq "mixed") {
        "ZIP contains both D1 and D2 mission hints"
    } elseif ($game -eq "unknown") {
        "ZIP contains no D1/D2 mission descriptor or level hints"
    } elseif ($counts.problems.Count -gt 0) {
        ($counts.problems -join "; ")
    } else {
        ""
    }
    [pscustomobject]@{
        Game = $game
        D1Score = $counts.d1
        D2Score = $counts.d2
        Reason = $reason
    }
}

function Get-MissionZipLaunchButtonText {
    param([Parameter(Mandatory = $true)][ValidateSet("d1", "d2")][string]$GameId)
    if ($GameId -eq "d1") { return "Launch Descent 1" }
    return "Launch Descent 2"
}

function Get-MissionZipGameSelectButtonText {
    param([Parameter(Mandatory = $true)][ValidateSet("d1", "d2")][string]$GameId)
    if ($GameId -eq "d1") { return "Descent 1" }
    return "Descent 2"
}

function Get-MissionZipStartConfirmAction {
    param([Parameter(Mandatory = $true)][ValidateSet("d1", "d2")][string]$GameId)
    if ($GameId -eq "d1") { return "key" }
    return "select"
}

function Resolve-MissionZipTemplate {
    param(
        [Parameter(Mandatory = $true)][string]$DeviceZipName,
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][ValidateSet("d1", "d2")][string]$GameId,
        [Parameter(Mandatory = $true)][string]$GameSelectButtonText,
        [Parameter(Mandatory = $true)][string]$MissionStartConfirmAction,
        [Parameter(Mandatory = $true)][string]$LaunchButtonText,
        [Parameter(Mandatory = $true)][string]$OutputPath
    )
    $templatePath = Join-Path $androidRoot "game_scripts\test_mission_zip_batch_import_metadata_launch.json5"
    $text = Get-Content -Path $templatePath -Raw
    $text = $text.Replace('${ZIP_FILE}', (ConvertTo-JsonStringContent $DeviceZipName))
    $text = $text.Replace('${ZIP_LABEL}', (ConvertTo-JsonStringContent $Label))
    $text = $text.Replace('${ZIP_DISPLAY_NAME}', (ConvertTo-JsonStringContent $DeviceZipName))
    $text = $text.Replace('${GAME_ID}', (ConvertTo-JsonStringContent $GameId))
    $text = $text.Replace('${GAME_SELECT_BUTTON_TEXT}', (ConvertTo-JsonStringContent $GameSelectButtonText))
    $text = $text.Replace('${MISSION_START_CONFIRM_ACTION}', (ConvertTo-JsonStringContent $MissionStartConfirmAction))
    $text = $text.Replace('${LAUNCH_BUTTON_TEXT}', (ConvertTo-JsonStringContent $LaunchButtonText))
    Set-Content -Path $OutputPath -Value $text -Encoding utf8
}

function Push-AppPrivateFile {
    param(
        [Parameter(Mandatory = $true)][string]$LocalPath,
        [Parameter(Mandatory = $true)][string]$DeviceRelativePath
    )
    $deviceLeaf = Split-Path -Leaf $DeviceRelativePath
    $deviceDir = (Split-Path -Parent $DeviceRelativePath).Replace('\', '/')
    $tmp = "/data/local/tmp/$deviceLeaf"
    $oldErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $pushOutput = & $script:ADB push $LocalPath $tmp 2>&1
        $pushExit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    if ($pushExit -ne 0) {
        $pushText = ($pushOutput -join " ").Trim()
        if ($pushText) {
            throw "adb push failed for ${LocalPath}: $pushText"
        }
        throw "adb push failed for $LocalPath"
    }
    $filesDir = if ($deviceDir) { "files/$deviceDir" } else { "files" }
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "mkdir", "-p", $filesDir) | Out-Null
    $dest = "/data/data/$($script:PACKAGE)/files/$($DeviceRelativePath.Replace('\', '/'))"
    $ErrorActionPreference = "Continue"
    try {
        $copyOutput = & $script:ADB shell "run-as $($script:PACKAGE) sh -c 'cat $tmp > $dest'" 2>&1
        $copyExit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    if ($copyExit -ne 0) {
        $copyText = ($copyOutput -join " ").Trim()
        if ($copyText) {
            throw "run-as copy failed for ${DeviceRelativePath}: $copyText"
        }
        throw "run-as copy failed for $DeviceRelativePath"
    }
    & $script:ADB shell "rm -f $tmp" 2>&1 | Out-Null
}

function Save-AppTextFile {
    param(
        [Parameter(Mandatory = $true)][string]$DeviceRelativePath,
        [Parameter(Mandatory = $true)][string]$LocalPath
    )
    $devicePath = "files/$($DeviceRelativePath.Replace('\', '/'))"
    $text = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", $devicePath) -Seconds 30
    if (-not $text -or $text -match 'No such file') {
        return $false
    }
    if ([IO.Path]::GetExtension($LocalPath).Equals(".json", [StringComparison]::OrdinalIgnoreCase)) {
        Write-TestJsonText -Path $LocalPath -Text $text
    } else {
        Write-Utf8NoBomText -Path $LocalPath -Text $text
    }
    return $true
}

function Read-AppAutomationResult {
    $text = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/automation_result.json") -Seconds 5
    if (-not $text -or $text -match 'No such file') { return $null }
    try { return ($text | ConvertFrom-Json) } catch { return $null }
}

function Clear-RunArtifacts {
    param([Parameter(Mandatory = $true)][string]$Label)
    Adb -AdbArgs @(
        "shell", "run-as", $script:PACKAGE, "rm", "-f",
        "files/automation_result.json",
        "files/automation_log.jsonl",
        "files/introspect.json",
        "files/level_metadata_automation_$Label.json",
        "files/mission_zip_import_$Label.json"
    ) | Out-Null
}

if (-not (Test-Path $ZipDir)) {
    Write-Status "FAIL: ZIP directory not found: $ZipDir" "Red"
    exit 1
}
if (-not $NoRegressionJson) {
    if (-not $RegressionJsonDir) {
        $RegressionJsonDir = $ZipDir
    }
    New-Item -ItemType Directory -Force -Path $RegressionJsonDir | Out-Null
}

$zipItems = @(
    foreach ($itemPattern in @($Pattern)) {
        if ($itemPattern) {
            Get-ChildItem -Path $ZipDir -Filter $itemPattern -File
        }
    }
) | Sort-Object FullName -Unique
$zipSortRecords = @($zipItems |
        ForEach-Object { Get-MissionZipBatchSortRecord -Zip $_ } |
        Sort-Object @{ Expression = { if ($_.HasJson) { 1 } else { 0 } } }, JsonLastWriteTimeUtc, @{ Expression = { $_.Zip.Name } })
if ($zipSortRecords.Count -eq 0) {
    Write-Status "FAIL: no ZIPs matched $($Pattern -join ', ') in $ZipDir" "Red"
    exit 1
}
if ($MaxZips -gt 0 -and $zipSortRecords.Count -gt $MaxZips) {
    Write-Status "Limiting mission ZIP batch to first $MaxZips zips after ordering"
    $zipSortRecords = @($zipSortRecords | Select-Object -First $MaxZips)
}
Write-MissionZipBatchOrder -Records $zipSortRecords
$zips = @($zipSortRecords | ForEach-Object { $_.Zip })

Ensure-EmulatorHealthy | Out-Null
if ($Install) {
    Write-Status "Installing APK"
    if (-not (Install-ApkOnDevice)) {
        Write-Status "FAIL: APK install failed" "Red"
        exit 1
    }
}

Write-Status "Resolving standard D1/D2 base data"
if (-not (Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps))) {
    Write-Status "FAIL: could not provision base game data" "Red"
    exit 1
}

$results = @()
foreach ($zip in $zips) {
    $label = Get-SafeMissionZipLabel $zip.Name
    $deviceZipName = "$label$($zip.Extension.ToLowerInvariant())"
    $metadataPath = Join-Path $metadataDir "$($zip.BaseName).json"
    $importPath = Join-Path $importsDir "$($zip.BaseName).import.json"
    $artifactPrefix = Join-Path $artifactsDir $label
    $resolvedScript = Join-Path $resolvedScriptDir "$label.json5"
    $deviceScriptName = "mission_zip_batch_current.json5"
    $regressionJsonPath = Get-MissionZipRegressionJsonPath -Zip $zip
    $hash = (Get-FileHash -Algorithm SHA256 -Path $zip.FullName).Hash.ToLowerInvariant()
    $gameHint = Get-MissionZipGameHint -ZipPath $zip.FullName

    $record = [ordered]@{
        zip = $zip.FullName
        name = $zip.Name
        sha256 = $hash
        size_bytes = $zip.Length
        label = $label
        game = $gameHint.Game
        status = "pending"
        metadata_json = $metadataPath
        regression_json = $regressionJsonPath
        import_json = $importPath
        automation_result_json = "$artifactPrefix.automation_result.json"
        automation_log_jsonl = "$artifactPrefix.automation_log.jsonl"
        introspect_json = "$artifactPrefix.introspect.json"
    }

    if ($zip.Length -gt $LargeZipBytes -and -not (Test-LargeMissionZipIncluded -Name $zip.Name)) {
        Write-Status "SKIP large ZIP: $($zip.Name) ($([math]::Round($zip.Length / 1MB, 1)) MB)" "Yellow"
        $record["status"] = "skipped_large"
        $record["reason"] = "ZIP is larger than the configured batch limit"
        $results += [pscustomobject]$record
        Write-MissionZipFailureJson -Path $metadataPath -Record $record
        ($record | ConvertTo-Json -Depth 20 -Compress) | Add-Content -Path (Join-Path $OutDir "summary.jsonl") -Encoding utf8
        continue
    }
    if ($gameHint.Game -notin @("d1", "d2")) {
        $record["status"] = "skipped_$($gameHint.Game)_game"
        $record["reason"] = $gameHint.Reason
        Write-Status "SKIP $($gameHint.Game) game ZIP: $($zip.Name) -- $($gameHint.Reason)" "Yellow"
        $results += [pscustomobject]$record
        Write-MissionZipFailureJson -Path $metadataPath -Record $record
        ($record | ConvertTo-Json -Depth 20 -Compress) | Add-Content -Path (Join-Path $OutDir "summary.jsonl") -Encoding utf8
        continue
    }

    $launchButtonText = Get-MissionZipLaunchButtonText -GameId $gameHint.Game
    $gameSelectButtonText = Get-MissionZipGameSelectButtonText -GameId $gameHint.Game
    $missionStartConfirmAction = Get-MissionZipStartConfirmAction -GameId $gameHint.Game
    Ensure-MissionZipBatchDeviceReady -Reason "preparing $($zip.Name)"
    Write-Status "Running mission ZIP: $($zip.Name) ($($gameHint.Game.ToUpperInvariant()))"
    $recoverAfterRun = $false
    try {
        Resolve-MissionZipTemplate -DeviceZipName $deviceZipName -Label $label -GameId $gameHint.Game -GameSelectButtonText $gameSelectButtonText -MissionStartConfirmAction $missionStartConfirmAction -LaunchButtonText $launchButtonText -OutputPath $resolvedScript
        Push-AppPrivateFile -LocalPath $zip.FullName -DeviceRelativePath "mission_zip_batch_cache/$deviceZipName"
        Push-AppPrivateFile -LocalPath $resolvedScript -DeviceRelativePath $deviceScriptName

        Stop-AppAndWait
        Reset-GameState
        Clear-RunArtifacts -Label $label
        Adb -AdbArgs @("logcat", "-c") | Out-Null
        Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
        if (-not (Wait-SetupActivityReady)) {
            throw "SetupActivity did not become ready"
        }
        Adb -AdbArgs @(
            "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_AUTOMATE",
            "--es", "script", $deviceScriptName
        ) | Out-Null

        $passed = Watch-AutomationResult -TimeoutSeconds $TimeoutSeconds -IsLauncherScript
        $automationResult = Read-AppAutomationResult
        $record["automation_result"] = $automationResult
        $record["status"] = if ($passed) { "passed" } else { "failed" }
        if (-not $passed -and $automationResult -and $automationResult.reason) {
            $record["reason"] = $automationResult.reason
        }
        if (-not $passed) {
            $reason = if ($record["reason"]) { $record["reason"] } else { "automation failed" }
            Write-Status "FAIL: $($zip.Name): $reason" "Red"
        }
    } catch {
        $record["status"] = "failed"
        $record["reason"] = $_.Exception.Message
        $recoverAfterRun = Test-MissionZipNeedsEmulatorRecovery -Reason $record["reason"]
        Write-Status "FAIL: $($zip.Name): $($record["reason"])" "Red"
    } finally {
        $deviceHealthyAfterRun = Test-EmulatorHealthy
        if (-not $deviceHealthyAfterRun) {
            if (-not ($record.Contains("reason") -and $record["reason"])) {
                $record["reason"] = "emulator crashed or adb became unavailable"
            }
            $recoverAfterRun = $true
        }

        $metadataSaved = $false
        if ($deviceHealthyAfterRun) {
            $metadataSaved = Save-AppTextFile -DeviceRelativePath "level_metadata_automation_$label.json" -LocalPath $metadataPath
        }
        if ($record["status"] -eq "passed") {
            if (-not $metadataSaved) {
                $record["status"] = "failed"
                $record["reason"] = "metadata output file was not created"
                Write-MissionZipFailureJson -Path $metadataPath -Record $record
                if (-not $NoRegressionJson) {
                    Write-MissionZipFailureJson -Path $regressionJsonPath -Record $record
                }
            } elseif (-not $NoRegressionJson) {
                Copy-Item -Path $metadataPath -Destination $regressionJsonPath -Force
            }
        } else {
            if (-not $metadataSaved) {
                Write-MissionZipFailureJson -Path $metadataPath -Record $record
            }
        }
        if ($deviceHealthyAfterRun) {
            Save-AppTextFile -DeviceRelativePath "mission_zip_import_$label.json" -LocalPath $importPath | Out-Null
            Save-AppTextFile -DeviceRelativePath "automation_result.json" -LocalPath "$artifactPrefix.automation_result.json" | Out-Null
            Save-AppTextFile -DeviceRelativePath "automation_log.jsonl" -LocalPath "$artifactPrefix.automation_log.jsonl" | Out-Null
            Save-AppTextFile -DeviceRelativePath "introspect.json" -LocalPath "$artifactPrefix.introspect.json" | Out-Null
        }
        $results += [pscustomobject]$record
        ($record | ConvertTo-Json -Depth 20 -Compress) | Add-Content -Path (Join-Path $OutDir "summary.jsonl") -Encoding utf8
        if ($recoverAfterRun) {
            Restore-MissionZipBatchDevice -Reason "recovering after $($zip.Name)"
        }
    }
}

Write-TestJsonText -Path (Join-Path $OutDir "summary.json") -Text (ConvertTo-Json -InputObject @($results) -Depth 30)
$failed = @($results | Where-Object { $_.status -eq "failed" })
$passed = @($results | Where-Object { $_.status -eq "passed" })
$skipped = @($results | Where-Object { $_.status -like "skipped*" })
$failedSummaryPath = Join-Path $OutDir "failed_zips.txt"
if ($failed.Count -gt 0) {
    $failedLines = @()
    foreach ($item in $failed) {
        $reason = if ($item.reason) { $item.reason } else { "failed" }
        $failedLines += "$($item.name)`t$reason"
    }
    Set-Content -Path $failedSummaryPath -Value $failedLines -Encoding utf8
} else {
    Set-Content -Path $failedSummaryPath -Value @() -Encoding utf8
}
Write-Status "Mission ZIP batch complete: $($results.Count) total, $($passed.Count) passed, $($skipped.Count) skipped, $($failed.Count) failed. Output: $OutDir"
if ($failed.Count -gt 0) {
    Write-Status "Failed ZIPs:" "Red"
    foreach ($item in $failed) {
        $reason = if ($item.reason) { $item.reason } else { "failed" }
        Write-Host "  $($item.name) -- $reason" -ForegroundColor Red
    }
    Write-Status "Failed ZIP summary: $failedSummaryPath" "Yellow"
    exit 1
}
exit 0

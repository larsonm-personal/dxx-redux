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
    [switch]$MetadataOnly,
    [switch]$NoRegressionJson,
    [long]$LargeZipBytes = 524288000,
    [int]$MaxZips = 0,
    [int]$TimeoutSeconds = 900,
    [int]$SetupReadyTimeoutSeconds = 120,
    [int]$MaxEmulatorRecoveries = 5
)

$ErrorActionPreference = "Stop"
$helpersDir = Split-Path -Parent $PSCommandPath
$androidRoot = Split-Path -Parent $helpersDir
. (Join-Path $helpersDir "test_helpers.ps1")
. (Join-Path $helpersDir "bounded_extraction.ps1")
. (Join-Path $helpersDir "mission_zip_batch_recovery.ps1")
. (Join-Path $helpersDir "normalized_json_text.ps1")
if (-not $env:ANDROID_SERIAL) {
    $env:ANDROID_SERIAL = $script:PRIMARY_EMULATOR_SERIAL
}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-7zaPath {
    $result = & "$androidRoot\get_deps\helpers\get_7zip.ps1"
    $candidate = @($result | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -Last 1)
    if ($candidate) { return $candidate[0] }
    throw "Verified 7za.exe not found. Run android/get_deps/helpers/get_7zip.ps1"
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
& (Join-Path $helpersDir "retain-recent-artifacts.ps1") -Artifacts $OutDir
$script:LogFile = Join-Path $OutDir "batch.log"
$script:MissionZipBatchConsecutiveRecoveryCount = 0

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

. (Join-Path $PSScriptRoot 'atomic_text_file.ps1')

function Write-TestJsonText {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Text,
        [switch]$MissionMetadata
    )

    Write-Utf8NoBomTextAtomically -Path $Path -Text (ConvertTo-NormalizedJsonText -Text $Text -MissionMetadata:$MissionMetadata)
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

function Format-MissionZipBatchDuration {
    param([Parameter(Mandatory = $true)][TimeSpan]$Elapsed)

    $totalSeconds = [int][Math]::Floor($Elapsed.TotalSeconds)
    $hours = [int]($totalSeconds / 3600)
    $minutes = [int](($totalSeconds % 3600) / 60)
    $seconds = $totalSeconds % 60
    if ($hours -gt 0) {
        return ("{0}:{1:d2}:{2:d2}" -f $hours, $minutes, $seconds)
    }
    return ("{0}:{1:d2}" -f $minutes, $seconds)
}

function Get-MissionZipBatchCounts {
    param([Parameter(Mandatory = $true)][AllowEmptyCollection()][object[]]$Results)

    return [pscustomobject]@{
        Passed = @($Results | Where-Object { $_.status -eq "passed" }).Count
        Skipped = @($Results | Where-Object { $_.status -like "skipped*" }).Count
        Failed = @($Results | Where-Object { $_.status -eq "failed" }).Count
    }
}

function Write-MissionZipBatchStart {
    param(
        [Parameter(Mandatory = $true)][int]$Index,
        [Parameter(Mandatory = $true)][int]$Total,
        [Parameter(Mandatory = $true)][System.IO.FileInfo]$Zip,
        [Parameter(Mandatory = $true)][string]$Game,
        [Parameter(Mandatory = $true)][TimeSpan]$BatchElapsed,
        [Parameter(Mandatory = $true)][object]$Counts
    )

    $percent = if ($Total -gt 0) { [int]((($Index - 1) / $Total) * 100) } else { 0 }
    $sizeMb = [Math]::Round($Zip.Length / 1MB, 1)
    $elapsedText = Format-MissionZipBatchDuration -Elapsed $BatchElapsed
    Write-Progress -Activity "Mission ZIP metadata batch" -Status "Running ${Index}/${Total}: $($Zip.Name)" -PercentComplete $percent
    Write-Status ("[{0}/{1}] Running mission ZIP: {2} ({3}, {4} MB, elapsed {5}, passed {6}, skipped {7}, failed {8})" -f
        $Index, $Total, $Zip.Name, $Game.ToUpperInvariant(), $sizeMb, $elapsedText, $Counts.Passed, $Counts.Skipped, $Counts.Failed)
}

function Write-MissionZipBatchResult {
    param(
        [Parameter(Mandatory = $true)][int]$Index,
        [Parameter(Mandatory = $true)][int]$Total,
        [Parameter(Mandatory = $true)][System.IO.FileInfo]$Zip,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Record,
        [Parameter(Mandatory = $true)][TimeSpan]$RunElapsed,
        [Parameter(Mandatory = $true)][TimeSpan]$BatchElapsed,
        [Parameter(Mandatory = $true)][object]$Counts
    )

    $status = [string]$Record["status"]
    $color = if ($status -eq "passed") { "Green" } elseif ($status -like "skipped*") { "Yellow" } else { "Red" }
    $percent = if ($Total -gt 0) { [int](($Index / $Total) * 100) } else { 100 }
    $runText = Format-MissionZipBatchDuration -Elapsed $RunElapsed
    $elapsedText = Format-MissionZipBatchDuration -Elapsed $BatchElapsed
    Write-Progress -Activity "Mission ZIP metadata batch" -Status "$status ${Index}/${Total}: $($Zip.Name)" -PercentComplete $percent
    Write-Status ("[{0}/{1}] {2}: {3} in {4} (elapsed {5}, passed {6}, skipped {7}, failed {8})" -f
        $Index, $Total, $status.ToUpperInvariant(), $Zip.Name, $runText, $elapsedText, $Counts.Passed, $Counts.Skipped, $Counts.Failed) $color
    if ($Record.Contains("reason") -and $Record["reason"]) {
        Write-Status "  reason: $($Record["reason"])" $color
    }
    if ($Record.Contains("metadata_json") -and $Record["metadata_json"]) {
        Write-Status "  metadata: $($Record["metadata_json"])" "DarkGray"
    }
    if ($Record.Contains("regression_json") -and $Record["regression_json"]) {
        Write-Status "  regression JSON: $($Record["regression_json"])" "DarkGray"
    }
}

function Restore-MissionZipBatchDevice {
    param([Parameter(Mandatory = $true)][string]$Reason)

    $script:MissionZipBatchConsecutiveRecoveryCount++
    if ($script:MissionZipBatchConsecutiveRecoveryCount -gt $MaxEmulatorRecoveries) {
        throw "consecutive emulator recovery limit reached ($MaxEmulatorRecoveries)"
    }

    Write-Status "$Reason -- recovering emulator ($script:MissionZipBatchConsecutiveRecoveryCount/$MaxEmulatorRecoveries consecutive)" "Yellow"
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
    Reset-GameState
    if (-not (Test-StandardGameDataActive)) {
        throw "restored base game data is not active after emulator recovery"
    }
}

function Ensure-MissionZipBatchDeviceReady {
    param([Parameter(Mandatory = $true)][string]$Reason)

    if ((Test-EmulatorHealthy) -and (Test-AppPackageInstalled)) {
        return
    }
    Restore-MissionZipBatchDevice -Reason $Reason
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
        [int]$Depth = 0,
        [hashtable]$Budget = $null
    )

    if (-not $Budget) {
        $Budget = @{ Entries = 0; ExpandedBytes = 0L; Started = [DateTime]::UtcNow }
    }
    foreach ($entry in $Archive.Entries) {
        $Budget.Entries++
        if ($Budget.Entries -gt 4096) { throw 'Mission inspection exceeds 4096 entries' }
        if (-not $entry.Name) { continue }
        if ($entry.Length -gt 512MB) { throw "$($entry.FullName) exceeds 512 MiB" }
        if ($entry.CompressedLength -gt 0) {
            $quotient = [math]::Floor($entry.Length / $entry.CompressedLength)
            if ($quotient -gt 1000 -or ($quotient -eq 1000 -and ($entry.Length % $entry.CompressedLength) -gt 0)) {
                throw "$($entry.FullName) exceeds the 1000:1 expansion ratio"
            }
        }
        if ($entry.Length -gt 2GB - $Budget.ExpandedBytes) { throw 'Mission inspection exceeds 2 GiB' }
        $Budget.ExpandedBytes += $entry.Length
        if (([DateTime]::UtcNow - $Budget.Started).TotalSeconds -gt 300) { throw 'Mission inspection exceeds 300 seconds' }
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
                    $buffer = New-Object byte[] 65536
                    $written = 0L
                    while (($read = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) {
                        $written += $read
                        if ($written -gt 100MB) { throw 'Nested ZIP exceeds the 100 MiB inspection limit' }
                        if (([DateTime]::UtcNow - $Budget.Started).TotalSeconds -gt 300) { throw 'Mission inspection exceeds 300 seconds' }
                        $memory.Write($buffer, 0, $read)
                    }
                    $memory.Position = 0
                    $nested = New-Object System.IO.Compression.ZipArchive($memory, [System.IO.Compression.ZipArchiveMode]::Read, $true)
                    try {
                        Add-ZipMissionGameHints -Archive $nested -Counts $Counts -Depth ($Depth + 1) -Budget $Budget
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
        $listingRoot = Join-Path ([IO.Path]::GetTempPath()) ("dxx_7z_list_" + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $listingRoot | Out-Null
        try {
            $bounded = Invoke-BoundedExtractor -OutputDirectory $listingRoot -FilePath $sevenZip `
                -ArgumentList @('l', '-slt', '--', $ArchivePath) -TimeoutSeconds 120 -MaxDiagnosticBytes 1048576
        } finally {
            Remove-Item -LiteralPath $listingRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
        $output = $bounded.Output
        if ($bounded.ExitCode -ne 0) {
            throw "7z list failed for ${ArchivePath}: $($output -join ' ')"
        }
        $entries = @(
            $output |
                Where-Object { $_ -match '^Path = (.+)$' } |
                ForEach-Object { $Matches[1] } |
                Where-Object { $_ -and $_ -ne $ArchivePath }
        )
        if ($entries.Count -gt 4096) { throw '7z listing exceeds 4096 entries' }
        return $entries
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
        Add-MissionZipGameHints -EntryNames (Get-MissionArchiveEntryNames -ArchivePath $ZipPath) -Counts $counts
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
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [switch]$MetadataOnly
    )
    $templateName = if ($MetadataOnly) { "test_mission_zip_batch_import_metadata.jsonc" } else { "test_mission_zip_batch_import_metadata_launch.jsonc" }
    $templatePath = Join-Path $androidRoot "game_scripts\$templateName"
    $text = Get-Content -Path $templatePath -Raw
    $text = $text.Replace('${ZIP_FILE}', (ConvertTo-JsonStringContent $DeviceZipName))
    $text = $text.Replace('${ZIP_LABEL}', (ConvertTo-JsonStringContent $Label))
    $text = $text.Replace('${ZIP_DISPLAY_NAME}', (ConvertTo-JsonStringContent $DeviceZipName))
    $text = $text.Replace('${GAME_ID}', (ConvertTo-JsonStringContent $GameId))
    $text = $text.Replace('${GAME_SELECT_BUTTON_TEXT}', (ConvertTo-JsonStringContent $GameSelectButtonText))
    $text = $text.Replace('${MISSION_START_CONFIRM_ACTION}', (ConvertTo-JsonStringContent $MissionStartConfirmAction))
    $text = $text.Replace('${LAUNCH_BUTTON_TEXT}', (ConvertTo-JsonStringContent $LaunchButtonText))
    [IO.File]::WriteAllText($OutputPath, $text + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
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
        [Parameter(Mandatory = $true)][string]$LocalPath,
        [switch]$MissionMetadata
    )
    $devicePath = "files/$($DeviceRelativePath.Replace('\', '/'))"
    $text = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", $devicePath) -Seconds 30
    if (-not $text -or $text -match 'No such file') {
        return $false
    }
    if ([IO.Path]::GetExtension($LocalPath).Equals(".json", [StringComparison]::OrdinalIgnoreCase)) {
        Write-TestJsonText -Path $LocalPath -Text $text -MissionMetadata:$MissionMetadata
    } else {
        Write-Utf8NoBomTextAtomically -Path $LocalPath -Text $text
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
Write-Status "Mission ZIP batch output: $OutDir"
if ($NoRegressionJson) {
    Write-Status "Regression JSON output disabled" "Yellow"
} else {
    Write-Status "Regression JSON output: $RegressionJsonDir"
}

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
Reset-GameState
if (-not (Test-StandardGameDataActive)) {
    Write-Status "FAIL: provisioned base game data is not active" "Red"
    exit 1
}

$results = @()
$consecutiveBaseDataFailures = 0
$batchStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$zipIndex = 0
foreach ($zip in $zips) {
    $zipIndex++
    $runStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $label = Get-SafeMissionZipLabel $zip.Name
    $deviceZipName = "$label$($zip.Extension.ToLowerInvariant())"
    $metadataPath = Join-Path $metadataDir "$($zip.BaseName).json"
    $importPath = Join-Path $importsDir "$($zip.BaseName).import.json"
    $artifactPrefix = Join-Path $artifactsDir $label
    $resolvedScript = Join-Path $resolvedScriptDir "$label.jsonc"
    $deviceScriptName = "mission_zip_batch_current.jsonc"
    $regressionJsonPath = Get-MissionZipRegressionJsonPath -Zip $zip

    $record = [ordered]@{
        zip = $zip.FullName
        name = $zip.Name
        sha256 = ""
        size_bytes = 0
        label = $label
        game = "unknown"
        status = "pending"
        metadata_json = $metadataPath
        regression_json = $regressionJsonPath
        import_json = $importPath
        automation_result_json = "$artifactPrefix.automation_result.json"
        automation_log_jsonl = "$artifactPrefix.automation_log.jsonl"
        introspect_json = "$artifactPrefix.introspect.json"
    }

    Write-MissionZipBatchStart -Index $zipIndex -Total $zips.Count -Zip $zip -Game "pending" -BatchElapsed $batchStopwatch.Elapsed -Counts (Get-MissionZipBatchCounts -Results $results)
    try {
        $record["size_bytes"] = $zip.Length
    } catch {
        $record["status"] = "failed"
        $record["reason"] = "Could not stat archive: $($_.Exception.Message)"
        $results += [pscustomobject]$record
        Write-MissionZipFailureJson -Path $metadataPath -Record $record
        [IO.File]::AppendAllText((Join-Path $OutDir "summary.jsonl"), ($record | ConvertTo-Json -Depth 20 -Compress) + "`n", [Text.UTF8Encoding]::new($false))
        $runStopwatch.Stop()
        Write-MissionZipBatchResult -Index $zipIndex -Total $zips.Count -Zip $zip -Record $record -RunElapsed $runStopwatch.Elapsed -BatchElapsed $batchStopwatch.Elapsed -Counts (Get-MissionZipBatchCounts -Results $results)
        $consecutiveBaseDataFailures = 0
        continue
    }
    if ($record["size_bytes"] -gt $LargeZipBytes -and -not (Test-LargeMissionZipIncluded -Name $zip.Name)) {
        Write-Status "SKIP large ZIP: $($zip.Name) ($([math]::Round($record["size_bytes"] / 1MB, 1)) MB)" "Yellow"
        $record["status"] = "skipped_large"
        $record["reason"] = "ZIP is larger than the configured batch limit"
        $results += [pscustomobject]$record
        Write-MissionZipFailureJson -Path $metadataPath -Record $record
        [IO.File]::AppendAllText((Join-Path $OutDir "summary.jsonl"), ($record | ConvertTo-Json -Depth 20 -Compress) + "`n", [Text.UTF8Encoding]::new($false))
        $runStopwatch.Stop()
        Write-MissionZipBatchResult -Index $zipIndex -Total $zips.Count -Zip $zip -Record $record -RunElapsed $runStopwatch.Elapsed -BatchElapsed $batchStopwatch.Elapsed -Counts (Get-MissionZipBatchCounts -Results $results)
        $consecutiveBaseDataFailures = 0
        continue
    }
    try {
        $record["sha256"] = (Get-FileHash -Algorithm SHA256 -Path $zip.FullName -ErrorAction Stop).Hash.ToLowerInvariant()
        $gameHint = Get-MissionZipGameHint -ZipPath $zip.FullName
        $record["game"] = $gameHint.Game
    } catch {
        $record["status"] = "failed"
        $record["reason"] = "Archive preflight failed: $($_.Exception.Message)"
        Write-Status "FAIL: $($zip.Name): $($record["reason"])" "Red"
        $results += [pscustomobject]$record
        Write-MissionZipFailureJson -Path $metadataPath -Record $record
        [IO.File]::AppendAllText((Join-Path $OutDir "summary.jsonl"), ($record | ConvertTo-Json -Depth 20 -Compress) + "`n", [Text.UTF8Encoding]::new($false))
        $runStopwatch.Stop()
        Write-MissionZipBatchResult -Index $zipIndex -Total $zips.Count -Zip $zip -Record $record -RunElapsed $runStopwatch.Elapsed -BatchElapsed $batchStopwatch.Elapsed -Counts (Get-MissionZipBatchCounts -Results $results)
        $consecutiveBaseDataFailures = 0
        continue
    }
    if ($gameHint.Game -notin @("d1", "d2")) {
        $record["status"] = "skipped_$($gameHint.Game)_game"
        $record["reason"] = $gameHint.Reason
        Write-Status "SKIP $($gameHint.Game) game ZIP: $($zip.Name) -- $($gameHint.Reason)" "Yellow"
        $results += [pscustomobject]$record
        Write-MissionZipFailureJson -Path $metadataPath -Record $record
        [IO.File]::AppendAllText((Join-Path $OutDir "summary.jsonl"), ($record | ConvertTo-Json -Depth 20 -Compress) + "`n", [Text.UTF8Encoding]::new($false))
        $runStopwatch.Stop()
        Write-MissionZipBatchResult -Index $zipIndex -Total $zips.Count -Zip $zip -Record $record -RunElapsed $runStopwatch.Elapsed -BatchElapsed $batchStopwatch.Elapsed -Counts (Get-MissionZipBatchCounts -Results $results)
        $consecutiveBaseDataFailures = 0
        continue
    }

    $launchButtonText = Get-MissionZipLaunchButtonText -GameId $gameHint.Game
    $gameSelectButtonText = Get-MissionZipGameSelectButtonText -GameId $gameHint.Game
    $missionStartConfirmAction = Get-MissionZipStartConfirmAction -GameId $gameHint.Game
    Ensure-MissionZipBatchDeviceReady -Reason "preparing $($zip.Name)"
    $recoverAfterRun = $false
    try {
        Resolve-MissionZipTemplate -DeviceZipName $deviceZipName -Label $label -GameId $gameHint.Game -GameSelectButtonText $gameSelectButtonText -MissionStartConfirmAction $missionStartConfirmAction -LaunchButtonText $launchButtonText -OutputPath $resolvedScript -MetadataOnly:$MetadataOnly
        for ($automationAttempt = 1; $automationAttempt -le 2; $automationAttempt++) {
            Invoke-MissionZipPreparationWithRetry -ZipName $zip.Name -Prepare {
                Push-AppPrivateFile -LocalPath $zip.FullName -DeviceRelativePath "mission_zip_batch_cache/$deviceZipName"
                Push-AppPrivateFile -LocalPath $resolvedScript -DeviceRelativePath $deviceScriptName

                Stop-AppAndWait
                Reset-GameState
                if (-not (Test-StandardGameDataActive)) {
                    throw "standard D1/D2 base data is not active after game-state reset"
                }
                Clear-RunArtifacts -Label $label
                Adb -AdbArgs @("logcat", "-c") | Out-Null
                Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
                if (-not (Wait-SetupActivityReady -TimeoutSeconds $SetupReadyTimeoutSeconds)) {
                    throw "SetupActivity did not become ready"
                }
            } -Recover {
                param($Reason)
                Restore-MissionZipBatchDevice -Reason $Reason
            }
            $script:MissionZipBatchConsecutiveRecoveryCount = 0
            $runId = [Guid]::NewGuid().ToString("N")
            Adb -AdbArgs @(
                "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_AUTOMATE",
                "--es", "script", $deviceScriptName,
                "--es", "run_id", $runId
            ) | Out-Null

            $passed = Watch-AutomationResult -TimeoutSeconds $TimeoutSeconds -IsLauncherScript -ExpectedRunId $runId
            $automationResult = Read-AppAutomationResult
            if (Test-AutomationResultRunId -Result $automationResult -ExpectedRunId $runId) {
                $record["automation_result"] = $automationResult
            } else {
                $automationResult = $null
            }
            $record["status"] = if ($passed) { "passed" } else { "failed" }
            if (-not $passed -and $automationResult -and $automationResult.reason) {
                $record["reason"] = $automationResult.reason
            }
            if ($passed) {
                break
            }

            if (-not ($record.Contains("reason") -and $record["reason"]) -and -not (Test-AppMainProcessRunning)) {
                $record["reason"] = "launcher process exited before automation produced a result"
            }
            $reason = if ($record.Contains("reason") -and $record["reason"]) { $record["reason"] } else { "automation failed" }
            $recoverAfterRun = Test-MissionZipNeedsEmulatorRecovery -Reason $reason
            if ($recoverAfterRun -and $automationAttempt -lt 2) {
                Restore-MissionZipBatchDevice -Reason "retrying $($zip.Name) after automation infrastructure failure: $reason"
                $recoverAfterRun = $false
                $record.Remove("reason")
                continue
            }
            Write-Status "FAIL: $($zip.Name): $reason" "Red"
            break
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
            try {
                $metadataSaved = Save-AppTextFile -DeviceRelativePath "level_metadata_automation_$label.json" -LocalPath $metadataPath -MissionMetadata
            } catch {
                $record["status"] = "failed"
                $record["reason"] = "metadata JSON validation failed: $($_.Exception.Message)"
                Write-Status "FAIL: $($zip.Name): $($record["reason"])" "Red"
            }
        }
        if ($record["status"] -eq "passed") {
            if (-not $metadataSaved) {
                $record["status"] = "failed"
                $record["reason"] = "metadata output file was not created"
                Write-MissionZipFailureJson -Path $metadataPath -Record $record
            } elseif (-not $NoRegressionJson) {
                Write-Utf8NoBomTextAtomically -Path $regressionJsonPath -Text ([System.IO.File]::ReadAllText($metadataPath))
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
        [IO.File]::AppendAllText((Join-Path $OutDir "summary.jsonl"), ($record | ConvertTo-Json -Depth 20 -Compress) + "`n", [Text.UTF8Encoding]::new($false))
        $runStopwatch.Stop()
        Write-MissionZipBatchResult -Index $zipIndex -Total $zips.Count -Zip $zip -Record $record -RunElapsed $runStopwatch.Elapsed -BatchElapsed $batchStopwatch.Elapsed -Counts (Get-MissionZipBatchCounts -Results $results)
        if ($recoverAfterRun) {
            Restore-MissionZipBatchDevice -Reason "recovering after $($zip.Name)"
        }
    }

    if ($record["status"] -eq "failed" -and (Test-StandardGameDataFailureReason -Reason $record["reason"])) {
        $consecutiveBaseDataFailures++
    } else {
        $consecutiveBaseDataFailures = 0
    }
    if ($consecutiveBaseDataFailures -ge 2) {
        Write-Status "FAIL: aborting batch after $consecutiveBaseDataFailures consecutive missing-base-data failures" "Red"
        break
    }
}

$batchStopwatch.Stop()
Write-Progress -Activity "Mission ZIP metadata batch" -Completed
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
    [IO.File]::WriteAllText($failedSummaryPath, ($failedLines -join [Environment]::NewLine) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
} else {
    [IO.File]::WriteAllText($failedSummaryPath, '', [Text.UTF8Encoding]::new($false))
}
Write-Status "Mission ZIP batch complete: $($results.Count) total, $($passed.Count) passed, $($skipped.Count) skipped, $($failed.Count) failed. Output: $OutDir"
if ($failed.Count -gt 0) {
    Write-Status "Failed ZIPs:" "Red"
    foreach ($item in $failed) {
        $reason = if ($item.reason) { $item.reason } else { "failed" }
        Write-Host "  $($item.name) -- $reason" -ForegroundColor Red
    }
    Write-Status "Failed ZIP summary: $failedSummaryPath" "Yellow"
}
Stop-AppAndWait
if ($failed.Count -gt 0) { exit 1 }
exit 0

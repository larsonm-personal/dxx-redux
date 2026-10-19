#!/usr/bin/env pwsh
param(
    [string]$D1Executable,
    [string]$D2Executable,
    [string]$Manifest,
    [string]$OutputDirectory,
    [ValidateRange(1, 10)][int]$Repeat = 2,
    [ValidateRange(1, 600)][int]$TimeoutSeconds = 30
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$D1Executable) { $D1Executable = Join-Path $repoRoot 'buildd1/main/d1x-redux.exe' }
if (!$D2Executable) { $D2Executable = Join-Path $repoRoot 'buildd2/main/d2x-redux.exe' }
if (!$Manifest) { $Manifest = Join-Path $PSScriptRoot 'fixtures/classic_demo_compatibility.json' }
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $repoRoot ('temp/classic-demo-compatibility-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}

$d1ExecutablePath = (Resolve-Path -LiteralPath $D1Executable).Path
$d2ExecutablePath = (Resolve-Path -LiteralPath $D2Executable).Path
$manifestPath = (Resolve-Path -LiteralPath $Manifest).Path
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$spec = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($spec.schema -ne 2 -or @($spec.cases).Count -eq 0) {
    throw 'Invalid or empty classic demo compatibility manifest'
}

& (Join-Path $PSScriptRoot '../helpers/retain-recent-artifacts.ps1') -Artifacts @($outputPath)
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

function Assert-ExpectedFields {
    param(
        [Parameter(Mandatory)]$Actual,
        [Parameter(Mandatory)]$Expected,
        [Parameter(Mandatory)][string]$Label
    )

    foreach ($property in $Expected.PSObject.Properties) {
        $actualProperty = $Actual.PSObject.Properties[$property.Name]
        if (!$actualProperty -or $actualProperty.Value -cne $property.Value) {
            throw "$Label field mismatch: $($property.Name) expected=$($property.Value) actual=$($actualProperty.Value)"
        }
    }
}

function Invoke-ClassicDemoProcess {
    param(
        [Parameter(Mandatory)][string]$ExecutablePath,
        [Parameter(Mandatory)][string[]]$Arguments,
        [Parameter(Mandatory)][string]$WorkingDirectory,
        [Parameter(Mandatory)][string]$Stdout,
        [Parameter(Mandatory)][string]$Stderr,
        [Parameter(Mandatory)][string]$Label
    )

    $quotedArguments = @($Arguments | ForEach-Object { '"' + $_ + '"' })
    $process = Start-Process -FilePath $ExecutablePath -ArgumentList $quotedArguments `
        -WorkingDirectory $WorkingDirectory -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $Stdout -RedirectStandardError $Stderr
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id
        $process.WaitForExit()
        throw "Classic demo compatibility run timed out: $Label"
    }
    if ($process.ExitCode -ne 0) {
        throw "Classic demo compatibility run failed: $Label exit=$($process.ExitCode); see $WorkingDirectory"
    }
}

function Assert-D1DemoRejected {
    param(
        [Parameter(Mandatory)][string]$DemoPath,
        [Parameter(Mandatory)][string]$DataPath,
        [Parameter(Mandatory)][string]$CaseDirectory,
        [Parameter(Mandatory)][string]$Label
    )

    New-Item -ItemType Directory -Path $CaseDirectory -Force | Out-Null
    $result = Join-Path $CaseDirectory 'result.json'
    $stdout = Join-Path $CaseDirectory 'stdout.log'
    $stderr = Join-Path $CaseDirectory 'stderr.log'
    $sourceHash = (Get-FileHash -LiteralPath $DemoPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $arguments = @(
        '-hogdir', $DataPath,
        '-nomovies', '-nomusic', '-nosound',
        '-classicdemo-validate', $DemoPath, $result
    )
    $quotedArguments = @($arguments | ForEach-Object { '"' + $_ + '"' })
    $process = Start-Process -FilePath $d1ExecutablePath -ArgumentList $quotedArguments `
        -WorkingDirectory $CaseDirectory -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id
        $process.WaitForExit()
        throw "D1 malformed demo rejection timed out: $Label"
    }
    if ($process.ExitCode -eq 0) {
        throw "D1 validator accepted malformed demo: $Label"
    }
    if (!(Test-Path -LiteralPath $result -PathType Leaf)) {
        throw "D1 malformed demo rejection did not write a result: $Label"
    }
    $lines = @(Get-Content -LiteralPath $result)
    if ($lines.Count -ne 1 -or ($lines[0] | ConvertFrom-Json).status -cne 'fail') {
        throw "D1 malformed demo rejection result mismatch: $Label"
    }
    $finalSourceHash = (Get-FileHash -LiteralPath $DemoPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($finalSourceHash -cne $sourceHash) {
        throw "D1 validator changed malformed source: $Label"
    }
    if (@(Get-ChildItem -LiteralPath $CaseDirectory -Filter '__classicdemo_validate_*.tmp').Count -ne 0) {
        throw "D1 malformed demo rejection leaked temporary output: $Label"
    }
}

function Assert-D1DemoAccepted {
    param(
        [Parameter(Mandatory)][string]$DemoPath,
        [Parameter(Mandatory)][string]$DataPath,
        [Parameter(Mandatory)][string]$CaseDirectory,
        [Parameter(Mandatory)][string]$Label
    )

    New-Item -ItemType Directory -Path $CaseDirectory -Force | Out-Null
    $result = Join-Path $CaseDirectory 'result.json'
    $stdout = Join-Path $CaseDirectory 'stdout.log'
    $stderr = Join-Path $CaseDirectory 'stderr.log'
    $sourceHash = (Get-FileHash -LiteralPath $DemoPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $arguments = @(
        '-hogdir', $DataPath,
        '-nomovies', '-nomusic', '-nosound',
        '-classicdemo-validate', $DemoPath, $result
    )
    Invoke-ClassicDemoProcess -ExecutablePath $d1ExecutablePath -Arguments $arguments `
        -WorkingDirectory $CaseDirectory -Stdout $stdout -Stderr $stderr -Label $Label
    $lines = @(Get-Content -LiteralPath $result)
    if ($lines.Count -ne 1 -or ($lines[0] | ConvertFrom-Json).status -cne 'pass') {
        throw "D1 synthetic demo acceptance result mismatch: $Label"
    }
    $finalSourceHash = (Get-FileHash -LiteralPath $DemoPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($finalSourceHash -cne $sourceHash) {
        throw "D1 validator changed synthetic source: $Label"
    }
}

function Add-BytesAtOffset {
    param(
        [Parameter(Mandatory)][byte[]]$Bytes,
        [Parameter(Mandatory)][int]$Offset,
        [Parameter(Mandatory)][byte[]]$InsertedBytes
    )

    if ($Offset -lt 0 -or $Offset -gt $Bytes.Length) {
        throw "Invalid insertion offset: $Offset"
    }
    $result = New-Object byte[] ($Bytes.Length + $InsertedBytes.Length)
    [Array]::Copy($Bytes, 0, $result, 0, $Offset)
    [Array]::Copy($InsertedBytes, 0, $result, $Offset, $InsertedBytes.Length)
    [Array]::Copy($Bytes, $Offset, $result, $Offset + $InsertedBytes.Length, $Bytes.Length - $Offset)
    return $result
}

function New-ClassicDemoShortpos {
    param(
        [int16]$Segment = 0,
        [switch]$Oriented,
        [switch]$SignedBoundaries
    )

    [byte[]]$bytes = @()
    if ($Oriented) {
        if ($SignedBoundaries) {
            $bytes += [byte[]]@(128, 0, 0, 0, 128, 0, 0, 0, 128)
        } else {
            $bytes += [byte[]]@(127, 0, 0, 0, 127, 0, 0, 0, 127)
        }
    }
    $signedValue = if ($SignedBoundaries) { [int16]::MinValue } else { [int16]0 }
    foreach ($value in @($signedValue, $signedValue, $signedValue, $Segment,
            $signedValue, $signedValue, $signedValue)) {
        $bytes += [BitConverter]::GetBytes([int16]$value)
    }
    return $bytes
}

function New-ClassicDemoObjectHeader {
    param(
        [byte]$RenderType,
        [byte]$ObjectType,
        [byte]$ObjectId,
        [int16]$Segment = 0,
        [switch]$SignedBoundaries
    )

    [byte[]]$bytes = @($RenderType, $ObjectType, $ObjectId, 0)
    $bytes += [BitConverter]::GetBytes([int16]0)
    $bytes += New-ClassicDemoShortpos -Segment $Segment `
        -Oriented:($RenderType -in @(1, 4, 6)) -SignedBoundaries:$SignedBoundaries
    return $bytes
}

function New-ClassicDemoLightLaserObject {
    param([int16]$Segment = 0)

    [byte[]]$bytes = New-ClassicDemoObjectHeader -RenderType 3 -ObjectType 13 -ObjectId 0 -Segment $Segment
    $bytes += [byte[]]@(14, 0)
    $bytes += [BitConverter]::GetBytes([int](1 -shl 16))
    $bytes += [BitConverter]::GetBytes([int]0) * 3
    $bytes += [byte]255
    $bytes += [BitConverter]::GetBytes([int](1 -shl 16))
    return $bytes
}

function New-ClassicDemoInvalidModelObject {
    param([int]$ModelNum)

    [byte[]]$bytes = New-ClassicDemoObjectHeader -RenderType 1 -ObjectType 13 -ObjectId 0
    $bytes += [byte[]]@(14, 0)
    $bytes += [BitConverter]::GetBytes([int](1 -shl 16))
    $bytes += [BitConverter]::GetBytes([int]0) * 3
    $bytes += [byte]255
    $bytes += [BitConverter]::GetBytes([int](1 -shl 16))
    $bytes += [BitConverter]::GetBytes($ModelNum)
    $bytes += [BitConverter]::GetBytes([int]0)
    return $bytes
}

function New-ClassicDemoPlayerPolyObject {
    param(
        [int]$TextureOverride,
        [switch]$SignedBoundaries
    )

    [byte[]]$bytes = New-ClassicDemoObjectHeader -RenderType 1 -ObjectType 4 -ObjectId 0 `
        -SignedBoundaries:$SignedBoundaries
    $bytes += [BitConverter]::GetBytes([int]0) * 3
    $bytes += [byte]255
    $bytes += [BitConverter]::GetBytes([int]0) * 6
    $bytes += [BitConverter]::GetBytes($TextureOverride)
    return $bytes
}

function New-ClassicDemoVclipObject {
    param(
        [int]$VclipNum,
        [byte]$FrameNum
    )

    [byte[]]$bytes = New-ClassicDemoObjectHeader -RenderType 2 -ObjectType 13 -ObjectId 0
    $bytes += [byte[]]@(14, 0)
    $bytes += [BitConverter]::GetBytes([int](1 -shl 16))
    $bytes += [BitConverter]::GetBytes([int]0) * 3
    $bytes += [byte]255
    $bytes += [BitConverter]::GetBytes([int](1 -shl 16))
    $bytes += [BitConverter]::GetBytes($VclipNum)
    $bytes += [BitConverter]::GetBytes([int](1 -shl 16))
    $bytes += $FrameNum
    return $bytes
}

function New-ClassicDemoHostageObject {
    param([byte]$HostageId)

    [byte[]]$bytes = New-ClassicDemoObjectHeader -RenderType 4 -ObjectType 3 -ObjectId $HostageId
    $bytes += [BitConverter]::GetBytes([int]0) * 3
    $bytes += [byte]255
    $bytes += [BitConverter]::GetBytes([int]0)
    $bytes += [BitConverter]::GetBytes([int](1 -shl 16))
    $bytes += [byte]0
    return $bytes
}

function New-ClassicDemoAttachedExplosionObject {
    param([int16]$DeleteObject)

    [byte[]]$bytes = New-ClassicDemoObjectHeader -RenderType 3 -ObjectType 1 -ObjectId 0
    $bytes[3] = 16
    $bytes += [byte[]]@(2, 0)
    $bytes += [BitConverter]::GetBytes([int](1 -shl 16))
    $bytes += [BitConverter]::GetBytes([int]0) * 3
    $bytes += [byte]255
    $bytes += [BitConverter]::GetBytes([int]0) * 2
    $bytes += [BitConverter]::GetBytes($DeleteObject)
    return $bytes
}

function Get-D1PlayerRecordsEnd {
    param(
        [Parameter(Mandatory)][byte[]]$Bytes,
        [Parameter(Mandatory)][int]$Offset,
        [Parameter(Mandatory)][int]$Count
    )

    $cursor = $Offset
    for ($player = 0; $player -lt $Count; $player++) {
        if ($cursor -ge $Bytes.Length) { throw 'Truncated D1 player list length' }
        $length = [int]$Bytes[$cursor]
        if ($length -lt 1 -or $cursor + 1 + $length + 5 -gt $Bytes.Length -or
            $Bytes[$cursor + $length] -ne 0) {
            throw 'Invalid D1 player list record'
        }
        $cursor += 1 + $length + 5
    }
    return $cursor
}

function Get-D1MultiplayerLayout {
    param([Parameter(Mandatory)][byte[]]$Bytes)

    if ($Bytes.Length -lt 100 -or $Bytes[0] -ne 1 -or $Bytes[1] -ne 13 -or $Bytes[2] -ne 2) {
        throw 'D1 multiplayer probe source has an unsupported header'
    }
    $gameMode = [BitConverter]::ToInt32($Bytes, 7)
    if (($gameMode -band 38) -eq 0 -or ($gameMode -band 256) -ne 0) {
        throw 'D1 multiplayer probe source must be a non-team network recording'
    }
    $headerCountOffset = 11
    $headerCount = [int]$Bytes[$headerCountOffset]
    $headerPlayersEnd = Get-D1PlayerRecordsEnd -Bytes $Bytes -Offset ($headerCountOffset + 1) -Count $headerCount
    $cursor = $headerPlayersEnd + 20 + 1
    $missionLength = [int]$Bytes[$cursor]
    if ($missionLength -lt 1 -or $cursor + 1 + $missionLength + 8 -gt $Bytes.Length -or
        $Bytes[$cursor + $missionLength] -ne 0) {
        throw 'D1 multiplayer probe source has an invalid mission record'
    }
    $eventOffset = $cursor + 1 + $missionLength + 8
    $byteCount = [BitConverter]::ToUInt16($Bytes, $Bytes.Length - 4)
    $trailerStart = $Bytes.Length - 4 - $byteCount
    $trailerCountOffset = $trailerStart + 39
    if ($trailerCountOffset -ge $Bytes.Length -or $Bytes[$trailerCountOffset] -ne $headerCount) {
        throw 'D1 multiplayer probe source has an unexpected trailer player count'
    }
    $trailerPlayersEnd = Get-D1PlayerRecordsEnd -Bytes $Bytes `
        -Offset ($trailerCountOffset + 1) -Count $headerCount
    return [ordered]@{
        header_count_offset = $headerCountOffset
        header_count = $headerCount
        header_players_end = $headerPlayersEnd
        header_laser_level_offset = $cursor - 1
        header_primary_weapon_offset = $eventOffset - 2
        header_secondary_weapon_offset = $eventOffset - 1
        event_offset = $eventOffset
        trailer_frame_length_offset = $trailerStart
        trailer_laser_level_offset = $trailerStart + 38
        trailer_primary_weapon_offset = $trailerStart + 16
        trailer_secondary_weapon_offset = $trailerStart + 17
        trailer_count_offset = $trailerCountOffset
        trailer_players_end = $trailerPlayersEnd
        trailer_byte_count = $byteCount
    }
}

$receipts = @()
foreach ($case in @($spec.cases)) {
    $source = (Resolve-Path -LiteralPath (Join-Path $repoRoot $case.source_path)).Path
    $sourceItem = Get-Item -LiteralPath $source
    if ($sourceItem.Length -ne [long]$case.source_size) {
        throw "Classic demo source size mismatch: $source"
    }
    $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($sourceHash -cne [string]$case.source_sha256) {
        throw "Classic demo source hash mismatch: $source"
    }

    if ($case.mode -ceq 'inventory') {
        $receipts += [ordered]@{
            id = [string]$case.id
            source_sha256 = $sourceHash
            status = 'inventory-only'
            baseline = [string]$case.baseline
        }
        Write-Output "PASS: $($case.id) original-demo identity (known baseline target)"
        continue
    }

    $profile = $spec.profiles.PSObject.Properties[[string]$case.data_profile]
    if (!$profile) { throw "Unknown classic demo data profile: $($case.data_profile)" }
    $dataPath = (Resolve-Path -LiteralPath (Join-Path $repoRoot $profile.Value)).Path
    $caseDirectory = Join-Path $outputPath $case.id
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $runHashes = @()

    for ($run = 1; $run -le $Repeat; $run++) {
        $stdout = Join-Path $caseDirectory "run-$run.stdout.log"
        $stderr = Join-Path $caseDirectory "run-$run.stderr.log"

        if ($case.game -ceq 'd1' -and $case.mode -ceq 'validate') {
            $result = Join-Path $caseDirectory "run-$run.json"
            $arguments = @(
                '-hogdir', $dataPath,
                '-nomovies', '-nomusic', '-nosound',
                '-classicdemo-validate', $source, $result
            )
            Invoke-ClassicDemoProcess -ExecutablePath $d1ExecutablePath -Arguments $arguments `
                -WorkingDirectory $caseDirectory -Stdout $stdout -Stderr $stderr `
                -Label "$($case.id) run $run"
            $lines = @(Get-Content -LiteralPath $result)
            if ($lines.Count -ne 1) { throw "D1 validation output line count mismatch: $($case.id) run $run" }
            Assert-ExpectedFields -Actual ($lines[0] | ConvertFrom-Json) -Expected $case.result `
                -Label "$($case.id) result"
            $runHashes += (Get-FileHash -LiteralPath $result -Algorithm SHA256).Hash.ToLowerInvariant()
        } elseif ($case.game -ceq 'd2' -and $case.mode -ceq 'dump') {
            $result = Join-Path $caseDirectory "run-$run.jsonl"
            $arguments = @(
                '-hogdir', $dataPath,
                '-nomovies', '-nomusic', '-nosound',
                '-classicdemo-dump-json', $source, $result
            )
            Invoke-ClassicDemoProcess -ExecutablePath $d2ExecutablePath -Arguments $arguments `
                -WorkingDirectory $caseDirectory -Stdout $stdout -Stderr $stderr `
                -Label "$($case.id) run $run"
            $resultItem = Get-Item -LiteralPath $result
            if ($resultItem.Length -ne [long]$case.output_size) {
                throw "Decoded output size mismatch: $($case.id) run $run"
            }
            $lines = @(Get-Content -LiteralPath $result)
            if ($lines.Count -ne [int]$case.output_lines) {
                throw "Decoded output line count mismatch: $($case.id) run $run"
            }
            $outputHash = (Get-FileHash -LiteralPath $result -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($outputHash -cne [string]$case.output_sha256) {
                throw "Decoded output hash mismatch: $($case.id) run $run expected=$($case.output_sha256) actual=$outputHash"
            }
            Assert-ExpectedFields -Actual ($lines[0] | ConvertFrom-Json) -Expected $case.header `
                -Label "$($case.id) header"
            Assert-ExpectedFields -Actual ($lines[-1] | ConvertFrom-Json) -Expected $case.result `
                -Label "$($case.id) result"
            $runHashes += $outputHash
        } else {
            throw "Unsupported classic demo case: game=$($case.game) mode=$($case.mode)"
        }

        if (@(Get-ChildItem -LiteralPath $caseDirectory -Filter '*.tmp.*').Count -ne 0) {
            throw "Classic demo compatibility run leaked temporary output: $($case.id) run $run"
        }
    }

    if (@($runHashes | Select-Object -Unique).Count -ne 1) {
        throw "Classic demo output was not deterministic across $Repeat runs: $($case.id)"
    }
    $finalSourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($finalSourceHash -cne $sourceHash) {
        throw "Classic demo compatibility run changed its source: $source"
    }
    $receipts += [ordered]@{
        id = [string]$case.id
        source_sha256 = $sourceHash
        output_sha256 = $runHashes[0]
        repeat = $Repeat
        status = 'pass'
    }
    Write-Output "PASS: $($case.id) exact original-demo decode ($Repeat runs)"
}

$samePathProbe = Join-Path $outputPath 'd1-same-path-probe.dem'
Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data/CD images/Descent (Europe)/data_tracks/descent/level18.dem') `
    -Destination $samePathProbe
$samePathProbeHash = (Get-FileHash -LiteralPath $samePathProbe -Algorithm SHA256).Hash.ToLowerInvariant()
$probeData = (Resolve-Path -LiteralPath (Join-Path $repoRoot $spec.profiles.'d1-retail')).Path
$probeArguments = @(
    '-hogdir', $probeData,
    '-nomovies', '-nomusic', '-nosound',
    '-classicdemo-validate', $samePathProbe, $samePathProbe
)
$quotedProbeArguments = @($probeArguments | ForEach-Object { '"' + $_ + '"' })
$probeProcess = Start-Process -FilePath $d1ExecutablePath -ArgumentList $quotedProbeArguments `
    -WorkingDirectory $outputPath -WindowStyle Hidden -PassThru
if (!$probeProcess.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $probeProcess.Id
    $probeProcess.WaitForExit()
    throw 'D1 same-path rejection probe timed out'
}
if ($probeProcess.ExitCode -eq 0) { throw 'D1 validator accepted identical source and result paths' }
$samePathProbeFinalHash = (Get-FileHash -LiteralPath $samePathProbe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($samePathProbeFinalHash -cne $samePathProbeHash) {
    throw 'D1 validator changed its source during the same-path rejection probe'
}
Write-Output 'PASS: D1 validator rejects identical source and result paths without changing the demo'

$malformedSource = (Resolve-Path -LiteralPath (
        Join-Path $repoRoot 'game_data/CD images/Descent (Europe)/data_tracks/descent/level18.dem')).Path
$malformedSourceBytes = [IO.File]::ReadAllBytes($malformedSource)
$missionLengthOffset = 36
if ($malformedSourceBytes[$missionLengthOffset] -ne 1 -or
    $malformedSourceBytes[$missionLengthOffset + 1] -ne 0) {
    throw 'D1 malformed mission-string probe source no longer has the expected header layout'
}
$malformedCases = @(
    [ordered]@{ name = 'zero-length'; length = 0; replacement = $null; truncate = $null },
    [ordered]@{ name = 'over-capacity'; length = 10; replacement = $null; truncate = $null },
    [ordered]@{ name = 'high-bit-length'; length = 255; replacement = $null; truncate = $null },
    [ordered]@{ name = 'unterminated'; length = 1; replacement = [byte][char]'x'; truncate = $null },
    [ordered]@{ name = 'missing-length'; length = $null; replacement = $null; truncate = $missionLengthOffset },
    [ordered]@{ name = 'missing-payload'; length = 1; replacement = $null; truncate = $missionLengthOffset + 1 }
)
$malformedRoot = Join-Path $outputPath 'd1-malformed-mission-string'
foreach ($malformedCase in $malformedCases) {
    $caseDirectory = Join-Path $malformedRoot $malformedCase.name
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $demoPath = Join-Path $caseDirectory 'input.dem'
    $bytes = [byte[]]$malformedSourceBytes.Clone()
    if ($null -ne $malformedCase.length) {
        $bytes[$missionLengthOffset] = [byte]$malformedCase.length
    }
    if ($null -ne $malformedCase.replacement) {
        $bytes[$missionLengthOffset + 1] = [byte]$malformedCase.replacement
    }
    if ($null -ne $malformedCase.truncate) {
        $truncated = New-Object byte[] ([int]$malformedCase.truncate)
        [Array]::Copy($bytes, $truncated, $truncated.Length)
        $bytes = $truncated
    }
    [IO.File]::WriteAllBytes($demoPath, $bytes)
    Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
        -CaseDirectory $caseDirectory -Label $malformedCase.name
}
Write-Output "PASS: D1 validator rejects $($malformedCases.Count) malformed required mission strings"

$multiplayerSource = (Resolve-Path -LiteralPath (
        Join-Path $repoRoot 'game_data/CD images/d1 mac 2nd bin+cue/data_tracks/yep9.dem')).Path
$multiplayerBytes = [IO.File]::ReadAllBytes($multiplayerSource)
$multiplayerLayout = Get-D1MultiplayerLayout -Bytes $multiplayerBytes
$playerProbeRoot = Join-Path $outputPath 'd1-player-validation'

$invalidCounts = @(0, 9, 255)
foreach ($invalidCount in $invalidCounts) {
    foreach ($location in @('header', 'trailer')) {
        $caseDirectory = Join-Path $playerProbeRoot "$location-count-$invalidCount"
        New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
        $demoPath = Join-Path $caseDirectory 'input.dem'
        $bytes = [byte[]]$multiplayerBytes.Clone()
        $offset = if ($location -ceq 'header') {
            $multiplayerLayout.header_count_offset
        } else {
            $multiplayerLayout.trailer_count_offset
        }
        $bytes[$offset] = [byte]$invalidCount
        [IO.File]::WriteAllBytes($demoPath, $bytes)
        Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
            -CaseDirectory $caseDirectory -Label "$location player count $invalidCount"
    }
}

[byte[]]$extraPlayers = @()
for ($player = 4; $player -lt 8; $player++) {
    $extraPlayers += [byte[]]@(3, [byte][char]'p', (48 + $player), 0, 1, 0, 0, 0, 0)
}
$maxPlayersBytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
    -Offset $multiplayerLayout.trailer_players_end -InsertedBytes $extraPlayers
$maxPlayersBytes[$multiplayerLayout.trailer_count_offset] = 8
$updatedByteCount = [uint16]($multiplayerLayout.trailer_byte_count + $extraPlayers.Length)
[BitConverter]::GetBytes($updatedByteCount).CopyTo($maxPlayersBytes, $maxPlayersBytes.Length - 4)
$maxPlayersBytes = Add-BytesAtOffset -Bytes $maxPlayersBytes `
    -Offset $multiplayerLayout.header_players_end -InsertedBytes $extraPlayers
$maxPlayersBytes[$multiplayerLayout.header_count_offset] = 8
$maxPlayersDirectory = Join-Path $playerProbeRoot 'maximum-player-count'
New-Item -ItemType Directory -Path $maxPlayersDirectory -Force | Out-Null
$maxPlayersDemo = Join-Path $maxPlayersDirectory 'input.dem'
[IO.File]::WriteAllBytes($maxPlayersDemo, $maxPlayersBytes)
Assert-D1DemoAccepted -DemoPath $maxPlayersDemo -DataPath $probeData `
    -CaseDirectory $maxPlayersDirectory -Label 'maximum player count'

$playerEventCases = @(
    [ordered]@{ name = 'cloak'; prefix = [byte[]]@(29); suffix = [byte[]]@() },
    [ordered]@{ name = 'decloak'; prefix = [byte[]]@(30); suffix = [byte[]]@() },
    [ordered]@{ name = 'death'; prefix = [byte[]]@(32); suffix = [byte[]]@() },
    [ordered]@{ name = 'kill'; prefix = [byte[]]@(33); suffix = [byte[]]@(1) },
    [ordered]@{ name = 'connect-new'; prefix = [byte[]]@(34); suffix = [byte[]]@(1, 2, 120, 0) },
    [ordered]@{
        name = 'connect-existing'
        prefix = [byte[]]@(34)
        suffix = [byte[]]@(0, 2, 111, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 110, 0)
    },
    [ordered]@{ name = 'reconnect'; prefix = [byte[]]@(35); suffix = [byte[]]@() },
    [ordered]@{ name = 'disconnect'; prefix = [byte[]]@(36); suffix = [byte[]]@() },
    [ordered]@{ name = 'score'; prefix = [byte[]]@(37); suffix = [byte[]]@(1, 0, 0, 0) }
)
foreach ($eventCase in $playerEventCases) {
    foreach ($playerIndex in @(8, 255)) {
        $caseDirectory = Join-Path $playerProbeRoot "event-$($eventCase.name)-index-$playerIndex"
        New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
        $demoPath = Join-Path $caseDirectory 'input.dem'
        [byte[]]$eventBytes = $eventCase.prefix + [byte]$playerIndex + $eventCase.suffix
        $bytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
            -Offset $multiplayerLayout.event_offset -InsertedBytes $eventBytes
        [IO.File]::WriteAllBytes($demoPath, $bytes)
        Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
            -CaseDirectory $caseDirectory -Label "$($eventCase.name) player index $playerIndex"
    }

    $validDirectory = Join-Path $playerProbeRoot "event-$($eventCase.name)-index-7"
    New-Item -ItemType Directory -Path $validDirectory -Force | Out-Null
    $validDemo = Join-Path $validDirectory 'input.dem'
    [byte[]]$validEventBytes = $eventCase.prefix + [byte]7 + $eventCase.suffix
    $validBytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $validEventBytes
    [IO.File]::WriteAllBytes($validDemo, $validBytes)
    Assert-D1DemoAccepted -DemoPath $validDemo -DataPath $probeData `
        -CaseDirectory $validDirectory -Label "$($eventCase.name) maximum valid player index"
}
Write-Output "PASS: D1 validates player count boundaries and all $($playerEventCases.Count) indexed event layouts"

$counterProbeRoot = Join-Path $outputPath 'd1-counter-arithmetic'
$counterCases = @(
    [ordered]@{
        name = 'multiplayer-counters'
        bytes = [byte[]](@(32, 0, 33, 0, 1, 37, 0) +
            [BitConverter]::GetBytes([int]::MaxValue) + @(37, 0) +
            [BitConverter]::GetBytes([int]1))
    },
    [ordered]@{
        name = 'player-score'
        bytes = [byte[]](@(38) + [BitConverter]::GetBytes([int]::MaxValue) + @(38) +
            [BitConverter]::GetBytes([int]::MaxValue) + @(38) +
            [BitConverter]::GetBytes([int]1))
    }
)
foreach ($counterCase in $counterCases) {
    $caseDirectory = Join-Path $counterProbeRoot $counterCase.name
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $demoPath = Join-Path $caseDirectory 'input.dem'
    $bytes = [byte[]]$multiplayerBytes.Clone()
    if ($counterCase.name -ceq 'multiplayer-counters') {
        $firstPlayerRecordOffset = $multiplayerLayout.header_count_offset + 1
        $firstPlayerTotalsOffset = $firstPlayerRecordOffset + 2 + $bytes[$firstPlayerRecordOffset]
        [BitConverter]::GetBytes([int16]::MaxValue).CopyTo($bytes, $firstPlayerTotalsOffset)
        [BitConverter]::GetBytes([int16]::MaxValue).CopyTo($bytes, $firstPlayerTotalsOffset + 2)
    }
    $bytes = Add-BytesAtOffset -Bytes $bytes -Offset $multiplayerLayout.event_offset `
        -InsertedBytes $counterCase.bytes
    [IO.File]::WriteAllBytes($demoPath, $bytes)
    Assert-D1DemoAccepted -DemoPath $demoPath -DataPath $probeData `
        -CaseDirectory $caseDirectory -Label "$($counterCase.name) boundary arithmetic"
}
Write-Output 'PASS: D1 preserves 16-bit counter and 32-bit score boundary events'

$maxSegments = 9000
$maxObjects = 1000
$worldEventCases = @(
    [ordered]@{
        name = 'wall-hit-segment'
        invalid = [byte[]](@(8) + [BitConverter]::GetBytes([int]$maxSegments) +
            [BitConverter]::GetBytes([int]0) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]-1))
        valid = [byte[]](@(8) + [BitConverter]::GetBytes([int]($maxSegments - 1)) +
            [BitConverter]::GetBytes([int]5) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]-1))
    },
    [ordered]@{
        name = 'wall-hit-side'
        invalid = [byte[]](@(8) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]6) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]-1))
        valid = [byte[]](@(8) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]5) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]-1))
    },
    [ordered]@{
        name = 'wall-hit-player'
        invalid = [byte[]](@(8) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]0) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]8))
        valid = [byte[]](@(8) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]0) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]7))
    },
    [ordered]@{
        name = 'trigger-segment'
        invalid = [byte[]](@(9) + [BitConverter]::GetBytes([int]$maxSegments) +
            [BitConverter]::GetBytes([int]0) + [BitConverter]::GetBytes([int]0))
        valid = [byte[]](@(9) + [BitConverter]::GetBytes([int]($maxSegments - 1)) +
            [BitConverter]::GetBytes([int]5) + [BitConverter]::GetBytes([int]($maxObjects - 1)))
    },
    [ordered]@{
        name = 'trigger-side'
        invalid = [byte[]](@(9) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]6) + [BitConverter]::GetBytes([int]0))
        valid = [byte[]](@(9) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]5) + [BitConverter]::GetBytes([int]0))
    },
    [ordered]@{
        name = 'trigger-object'
        invalid = [byte[]](@(9) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]0) + [BitConverter]::GetBytes([int]$maxObjects))
        valid = [byte[]](@(9) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]0) + [BitConverter]::GetBytes([int]($maxObjects - 1)))
    },
    [ordered]@{
        name = 'wall-toggle'
        invalid = [byte[]](@(13) + [BitConverter]::GetBytes([int]$maxSegments) +
            [BitConverter]::GetBytes([int]0))
        valid = [byte[]](@(13) + [BitConverter]::GetBytes([int]($maxSegments - 1)) +
            [BitConverter]::GetBytes([int]5))
    },
    [ordered]@{
        name = 'effect-blowup'
        invalid = [byte[]](@(21) + [BitConverter]::GetBytes([int16]0) + @(6) +
            [BitConverter]::GetBytes([int]0) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]0))
        valid = [byte[]](@(21) + [BitConverter]::GetBytes([int16]($maxSegments - 1)) + @(5) +
            [BitConverter]::GetBytes([int]0) + [BitConverter]::GetBytes([int]0) +
            [BitConverter]::GetBytes([int]0))
    },
    [ordered]@{
        name = 'primary-wall-texture'
        invalid = [byte[]](@(26) + [BitConverter]::GetBytes([int16]0) + @(0) +
            [BitConverter]::GetBytes([int16]0) + @(0) + [BitConverter]::GetBytes([int16]-1))
        valid = [byte[]](@(26) + [BitConverter]::GetBytes([int16]($maxSegments - 1)) + @(5) +
            [BitConverter]::GetBytes([int16]($maxSegments - 1)) + @(5) +
            [BitConverter]::GetBytes([int16]0))
    },
    [ordered]@{
        name = 'overlay-wall-texture'
        invalid = [byte[]](@(27) + [BitConverter]::GetBytes([int16]0) + @(0) +
            [BitConverter]::GetBytes([int16]0) + @(0) + [BitConverter]::GetBytes([int16]0x3fff))
        valid = [byte[]](@(27) + [BitConverter]::GetBytes([int16]($maxSegments - 1)) + @(5) +
            [BitConverter]::GetBytes([int16]($maxSegments - 1)) + @(5) +
            [BitConverter]::GetBytes([int16]0))
    },
    [ordered]@{
        name = 'door-opening'
        invalid = [byte[]](@(41) + [BitConverter]::GetBytes([int16]$maxSegments) + @(0))
        valid = [byte[]](@(41) + [BitConverter]::GetBytes([int16]($maxSegments - 1)) + @(5))
    }
)
$worldProbeRoot = Join-Path $outputPath 'd1-world-validation'
foreach ($worldEventCase in $worldEventCases) {
    $invalidDirectory = Join-Path $worldProbeRoot "$($worldEventCase.name)-invalid"
    New-Item -ItemType Directory -Path $invalidDirectory -Force | Out-Null
    $invalidDemo = Join-Path $invalidDirectory 'input.dem'
    $invalidBytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $worldEventCase.invalid
    [IO.File]::WriteAllBytes($invalidDemo, $invalidBytes)
    Assert-D1DemoRejected -DemoPath $invalidDemo -DataPath $probeData `
        -CaseDirectory $invalidDirectory -Label "$($worldEventCase.name) invalid world index"

    $validDirectory = Join-Path $worldProbeRoot "$($worldEventCase.name)-valid-boundary"
    New-Item -ItemType Directory -Path $validDirectory -Force | Out-Null
    $validDemo = Join-Path $validDirectory 'input.dem'
    $validBytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $worldEventCase.valid
    [IO.File]::WriteAllBytes($validDemo, $validBytes)
    Assert-D1DemoAccepted -DemoPath $validDemo -DataPath $probeData `
        -CaseDirectory $validDirectory -Label "$($worldEventCase.name) valid world boundary"
}
Write-Output "PASS: D1 validates all $($worldEventCases.Count) world event index layouts"

$levelProbeRoot = Join-Path $outputPath 'd1-level-validation'
$invalidLevelEvents = @(
    [ordered]@{ name = 'zero-old'; bytes = [byte[]]@(28, 1, 0) },
    [ordered]@{ name = 'zero-new'; bytes = [byte[]]@(28, 0, 1) }
)
foreach ($levelEvent in $invalidLevelEvents) {
    $caseDirectory = Join-Path $levelProbeRoot $levelEvent.name
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $demoPath = Join-Path $caseDirectory 'input.dem'
    $bytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $levelEvent.bytes
    [IO.File]::WriteAllBytes($demoPath, $bytes)
    Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
        -CaseDirectory $caseDirectory -Label "level event $($levelEvent.name)"
}

$validLevelEventDirectory = Join-Path $levelProbeRoot 'signed-byte-boundaries'
New-Item -ItemType Directory -Path $validLevelEventDirectory -Force | Out-Null
$validLevelEventDemo = Join-Path $validLevelEventDirectory 'input.dem'
$validLevelEventBytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
    -Offset $multiplayerLayout.event_offset -InsertedBytes ([byte[]]@(28, 127, 128))
[IO.File]::WriteAllBytes($validLevelEventDemo, $validLevelEventBytes)
Assert-D1DemoAccepted -DemoPath $validLevelEventDemo -DataPath $probeData `
    -CaseDirectory $validLevelEventDirectory -Label 'nonzero signed-byte level boundaries'

foreach ($trailerLevel in @(0, 127)) {
    $caseDirectory = Join-Path $levelProbeRoot "trailer-$trailerLevel"
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $demoPath = Join-Path $caseDirectory 'input.dem'
    $bytes = [byte[]]$multiplayerBytes.Clone()
    $bytes[$bytes.Length - 2] = [byte]$trailerLevel
    [IO.File]::WriteAllBytes($demoPath, $bytes)
    if ($trailerLevel -eq 0) {
        Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
            -CaseDirectory $caseDirectory -Label 'zero trailer level'
    } else {
        Assert-D1DemoAccepted -DemoPath $demoPath -DataPath $probeData `
            -CaseDirectory $caseDirectory -Label 'nonzero trailer level'
    }
}
Write-Output 'PASS: D1 rejects impossible zero levels without narrowing nonzero custom-mission levels'

$frameLinkProbeRoot = Join-Path $outputPath 'd1-frame-link-validation'
foreach ($invalidFrameLength in @([int16]0, [int16]10, [int16]-1)) {
    $caseDirectory = Join-Path $frameLinkProbeRoot "trailer-$invalidFrameLength"
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $demoPath = Join-Path $caseDirectory 'input.dem'
    $bytes = [byte[]]$multiplayerBytes.Clone()
    [BitConverter]::GetBytes($invalidFrameLength).CopyTo(
        $bytes, $multiplayerLayout.trailer_frame_length_offset)
    [IO.File]::WriteAllBytes($demoPath, $bytes)
    Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
        -CaseDirectory $caseDirectory -Label "unsafe trailer frame link $invalidFrameLength"
}
Write-Output 'PASS: D1 rejects 3 unsafe trailer frame links without requiring exact frame sizes'

$frameMetadataCases = @(
    [ordered]@{ name = 'zero-time'; frame_count = 1; frame_time = 0 },
    [ordered]@{ name = 'negative-time'; frame_count = 1; frame_time = -1 },
    [ordered]@{ name = 'maximum-time'; frame_count = 1; frame_time = [int]::MaxValue },
    [ordered]@{ name = 'minimum-frame-count'; frame_count = [int]::MinValue; frame_time = 1 }
)
foreach ($frameMetadataCase in $frameMetadataCases) {
    $caseDirectory = Join-Path $frameLinkProbeRoot $frameMetadataCase.name
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $demoPath = Join-Path $caseDirectory 'input.dem'
    $eventBytes = [byte[]](@(2) + [BitConverter]::GetBytes([int16]11) +
        [BitConverter]::GetBytes([int]$frameMetadataCase.frame_count) +
        [BitConverter]::GetBytes([int]$frameMetadataCase.frame_time))
    $bytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $eventBytes
    [IO.File]::WriteAllBytes($demoPath, $bytes)
    Assert-D1DemoAccepted -DemoPath $demoPath -DataPath $probeData `
        -CaseDirectory $caseDirectory -Label "recoverable frame metadata $($frameMetadataCase.name)"
}
Write-Output 'PASS: D1 preserves 4 recoverable noncanonical frame metadata layouts'

$soundProbeRoot = Join-Path $outputPath 'd1-sound-validation'
$soundEventCases = @(
    [ordered]@{
        name = 'sample'
        build = { param([int]$Sound) [byte[]](@(5) + [BitConverter]::GetBytes($Sound)) }
    },
    [ordered]@{
        name = 'sample-once'
        build = { param([int]$Sound) [byte[]](@(6) + [BitConverter]::GetBytes($Sound)) }
    },
    [ordered]@{
        name = 'sample-3d'
        build = { param([int]$Sound) [byte[]](@(7) + [BitConverter]::GetBytes($Sound) +
                [BitConverter]::GetBytes([int]0) + [BitConverter]::GetBytes([int](1 -shl 16))) }
    },
    [ordered]@{
        name = 'sample-3d-once'
        build = { param([int]$Sound) [byte[]](@(11) + [BitConverter]::GetBytes($Sound) +
                [BitConverter]::GetBytes([int]0) + [BitConverter]::GetBytes([int](1 -shl 16))) }
    },
    [ordered]@{
        name = 'linked-to-object'
        build = { param([int]$Sound) [byte[]](@(43) + [BitConverter]::GetBytes($Sound) +
                [BitConverter]::GetBytes([int]0) + [BitConverter]::GetBytes([int](1 -shl 16)) +
                [BitConverter]::GetBytes([int](256 -shl 16)) + [BitConverter]::GetBytes([int]-1) +
                [BitConverter]::GetBytes([int]-1)) }
    }
)
foreach ($soundEventCase in $soundEventCases) {
    foreach ($invalidSound in @(-1, 250, [int]::MaxValue)) {
        $caseDirectory = Join-Path $soundProbeRoot "$($soundEventCase.name)-invalid-$invalidSound"
        New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
        $demoPath = Join-Path $caseDirectory 'input.dem'
        [byte[]]$eventBytes = & $soundEventCase.build $invalidSound
        $bytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
            -Offset $multiplayerLayout.event_offset -InsertedBytes $eventBytes
        [IO.File]::WriteAllBytes($demoPath, $bytes)
        Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
            -CaseDirectory $caseDirectory -Label "$($soundEventCase.name) invalid sound $invalidSound"
    }

    $validDirectory = Join-Path $soundProbeRoot "$($soundEventCase.name)-valid-boundary"
    New-Item -ItemType Directory -Path $validDirectory -Force | Out-Null
    $validDemo = Join-Path $validDirectory 'input.dem'
    [byte[]]$validEventBytes = & $soundEventCase.build 249
    $validBytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $validEventBytes
    [IO.File]::WriteAllBytes($validDemo, $validBytes)
    Assert-D1DemoAccepted -DemoPath $validDemo -DataPath $probeData `
        -CaseDirectory $validDirectory -Label "$($soundEventCase.name) maximum valid sound"
}
Write-Output "PASS: D1 validates all $($soundEventCases.Count) sound event layouts"

$opcodeProbeRoot = Join-Path $outputPath 'd1-opcode-validation'
$unknownOpcodeDirectory = Join-Path $opcodeProbeRoot 'unknown-255'
New-Item -ItemType Directory -Path $unknownOpcodeDirectory -Force | Out-Null
$unknownOpcodeDemo = Join-Path $unknownOpcodeDirectory 'input.dem'
$unknownOpcodeBytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
    -Offset $multiplayerLayout.event_offset -InsertedBytes ([byte[]]@(255))
[IO.File]::WriteAllBytes($unknownOpcodeDemo, $unknownOpcodeBytes)
Assert-D1DemoRejected -DemoPath $unknownOpcodeDemo -DataPath $probeData `
    -CaseDirectory $unknownOpcodeDirectory -Label 'unknown event opcode 255'
Write-Output 'PASS: D1 rejects an unknown event opcode at its unrecoverable boundary'

$weaponProbeRoot = Join-Path $outputPath 'd1-weapon-validation'
foreach ($weaponLocation in @('header', 'trailer')) {
    foreach ($weaponClass in @('primary', 'secondary')) {
        $caseDirectory = Join-Path $weaponProbeRoot "$weaponLocation-$weaponClass-invalid"
        New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
        $demoPath = Join-Path $caseDirectory 'input.dem'
        $bytes = [byte[]]$multiplayerBytes.Clone()
        $offsetName = "${weaponLocation}_${weaponClass}_weapon_offset"
        $bytes[$multiplayerLayout[$offsetName]] = 5
        [IO.File]::WriteAllBytes($demoPath, $bytes)
        Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
            -CaseDirectory $caseDirectory -Label "$weaponLocation $weaponClass weapon index"
    }

    $validDirectory = Join-Path $weaponProbeRoot "$weaponLocation-valid-boundaries"
    New-Item -ItemType Directory -Path $validDirectory -Force | Out-Null
    $validDemo = Join-Path $validDirectory 'input.dem'
    $validBytes = [byte[]]$multiplayerBytes.Clone()
    $validBytes[$multiplayerLayout["${weaponLocation}_primary_weapon_offset"]] = 4
    $validBytes[$multiplayerLayout["${weaponLocation}_secondary_weapon_offset"]] = 4
    [IO.File]::WriteAllBytes($validDemo, $validBytes)
    Assert-D1DemoAccepted -DemoPath $validDemo -DataPath $probeData `
        -CaseDirectory $validDirectory -Label "$weaponLocation maximum valid weapon indices"
}

$weaponEventCases = @(
    [ordered]@{ name = 'primary-new'; invalid = [byte[]]@(20, 0, 5, 0); valid = [byte[]]@(20, 0, 4, 4) },
    [ordered]@{ name = 'primary-old'; invalid = [byte[]]@(20, 0, 4, 5); valid = [byte[]]@(20, 0, 4, 4) },
    [ordered]@{ name = 'secondary-new'; invalid = [byte[]]@(20, 1, 5, 0); valid = [byte[]]@(20, 1, 4, 4) },
    [ordered]@{ name = 'secondary-old'; invalid = [byte[]]@(20, 1, 4, 5); valid = [byte[]]@(20, 255, 4, 4) }
)
foreach ($weaponEventCase in $weaponEventCases) {
    $invalidDirectory = Join-Path $weaponProbeRoot "$($weaponEventCase.name)-invalid"
    New-Item -ItemType Directory -Path $invalidDirectory -Force | Out-Null
    $invalidDemo = Join-Path $invalidDirectory 'input.dem'
    $invalidBytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $weaponEventCase.invalid
    [IO.File]::WriteAllBytes($invalidDemo, $invalidBytes)
    Assert-D1DemoRejected -DemoPath $invalidDemo -DataPath $probeData `
        -CaseDirectory $invalidDirectory -Label "$($weaponEventCase.name) invalid weapon event"

    $validDirectory = Join-Path $weaponProbeRoot "$($weaponEventCase.name)-valid"
    New-Item -ItemType Directory -Path $validDirectory -Force | Out-Null
    $validDemo = Join-Path $validDirectory 'input.dem'
    $validBytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $weaponEventCase.valid
    [IO.File]::WriteAllBytes($validDemo, $validBytes)
    Assert-D1DemoAccepted -DemoPath $validDemo -DataPath $probeData `
        -CaseDirectory $validDirectory -Label "$($weaponEventCase.name) valid weapon event"
}
Write-Output "PASS: D1 validates header, trailer, and all $($weaponEventCases.Count) weapon event layouts"

$laserProbeRoot = Join-Path $outputPath 'd1-laser-validation'
foreach ($laserLocation in @('header', 'trailer')) {
    foreach ($invalidLevel in @(4, 255)) {
        $caseDirectory = Join-Path $laserProbeRoot "$laserLocation-invalid-$invalidLevel"
        New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
        $demoPath = Join-Path $caseDirectory 'input.dem'
        $bytes = [byte[]]$multiplayerBytes.Clone()
        $bytes[$multiplayerLayout["${laserLocation}_laser_level_offset"]] = [byte]$invalidLevel
        [IO.File]::WriteAllBytes($demoPath, $bytes)
        Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
            -CaseDirectory $caseDirectory -Label "$laserLocation laser level $invalidLevel"
    }

    foreach ($validLevel in @(0, 3)) {
        $caseDirectory = Join-Path $laserProbeRoot "$laserLocation-valid-$validLevel"
        New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
        $demoPath = Join-Path $caseDirectory 'input.dem'
        $bytes = [byte[]]$multiplayerBytes.Clone()
        $bytes[$multiplayerLayout["${laserLocation}_laser_level_offset"]] = [byte]$validLevel
        [IO.File]::WriteAllBytes($demoPath, $bytes)
        Assert-D1DemoAccepted -DemoPath $demoPath -DataPath $probeData `
            -CaseDirectory $caseDirectory -Label "$laserLocation laser level $validLevel"
    }
}

$laserEventCases = @(
    [ordered]@{ name = 'negative-old'; bytes = [byte[]]@(42, 255, 0); valid = $false },
    [ordered]@{ name = 'negative-new'; bytes = [byte[]]@(42, 0, 255); valid = $false },
    [ordered]@{ name = 'oversized-old'; bytes = [byte[]]@(42, 4, 0); valid = $false },
    [ordered]@{ name = 'oversized-new'; bytes = [byte[]]@(42, 0, 4); valid = $false },
    [ordered]@{ name = 'maximum'; bytes = [byte[]]@(42, 3, 3); valid = $true }
)
foreach ($laserEventCase in $laserEventCases) {
    $caseDirectory = Join-Path $laserProbeRoot "event-$($laserEventCase.name)"
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $demoPath = Join-Path $caseDirectory 'input.dem'
    $bytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $laserEventCase.bytes
    [IO.File]::WriteAllBytes($demoPath, $bytes)
    if ($laserEventCase.valid) {
        Assert-D1DemoAccepted -DemoPath $demoPath -DataPath $probeData `
            -CaseDirectory $caseDirectory -Label "$($laserEventCase.name) laser event"
    } else {
        Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
            -CaseDirectory $caseDirectory -Label "$($laserEventCase.name) laser event"
    }
}
Write-Output "PASS: D1 validates laser levels in headers, trailers, and events"

$hostageProbeRoot = Join-Path $outputPath 'd1-hostage-validation'
foreach ($invalidHostage in @(-1, 10, [int]::MaxValue)) {
    $caseDirectory = Join-Path $hostageProbeRoot "event-invalid-$invalidHostage"
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $demoPath = Join-Path $caseDirectory 'input.dem'
    [byte[]]$eventBytes = @(10) + [BitConverter]::GetBytes([int]$invalidHostage)
    $bytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $eventBytes
    [IO.File]::WriteAllBytes($demoPath, $bytes)
    Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
        -CaseDirectory $caseDirectory -Label "invalid hostage rescue $invalidHostage"
}
foreach ($validHostage in @(0, 9)) {
    $caseDirectory = Join-Path $hostageProbeRoot "event-valid-$validHostage"
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $demoPath = Join-Path $caseDirectory 'input.dem'
    [byte[]]$eventBytes = @(10) + [BitConverter]::GetBytes([int]$validHostage)
    $bytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $eventBytes
    [IO.File]::WriteAllBytes($demoPath, $bytes)
    Assert-D1DemoAccepted -DemoPath $demoPath -DataPath $probeData `
        -CaseDirectory $caseDirectory -Label "valid hostage rescue $validHostage"
}
foreach ($hostageObjectCase in @(
        [ordered]@{ name = 'invalid-10'; id = 10; valid = $false },
        [ordered]@{ name = 'invalid-255'; id = 255; valid = $false },
        [ordered]@{ name = 'valid-9'; id = 9; valid = $true }
    )) {
    $caseDirectory = Join-Path $hostageProbeRoot "object-$($hostageObjectCase.name)"
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $demoPath = Join-Path $caseDirectory 'input.dem'
    [byte[]]$eventBytes = @(4) + (New-ClassicDemoHostageObject -HostageId $hostageObjectCase.id)
    $bytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $eventBytes
    [IO.File]::WriteAllBytes($demoPath, $bytes)
    if ($hostageObjectCase.valid) {
        Assert-D1DemoAccepted -DemoPath $demoPath -DataPath $probeData `
            -CaseDirectory $caseDirectory -Label "$($hostageObjectCase.name) hostage object"
    } else {
        Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
            -CaseDirectory $caseDirectory -Label "$($hostageObjectCase.name) hostage object"
    }
}
Write-Output 'PASS: D1 validates hostage rescue and object indices'

$objectProbeRoot = Join-Path $outputPath 'd1-object-validation'
$objectEventCases = @(
    [ordered]@{ name = 'render-type'; bytes = [byte[]]@(4, 8, 13) },
    [ordered]@{ name = 'object-type'; bytes = [byte[]]@(4, 3, 254) },
    [ordered]@{ name = 'hidden-object-type'; bytes = [byte[]]@(4, 0, 254) },
    [ordered]@{
        name = 'empty-orientation'
        bytes = [byte[]](@(4, 1, 13, 0, 0) + [BitConverter]::GetBytes([int16]0) +
            (New-Object byte[] 9) + (New-ClassicDemoShortpos))
    },
    [ordered]@{
        name = 'segment'
        bytes = [byte[]](@(4) + (New-ClassicDemoLightLaserObject -Segment $maxSegments))
    },
    [ordered]@{
        name = 'robot-id'
        bytes = [byte[]](@(4) +
            (New-ClassicDemoObjectHeader -RenderType 1 -ObjectType 2 -ObjectId 255))
    },
    [ordered]@{
        name = 'powerup-id'
        bytes = [byte[]](@(4) +
            (New-ClassicDemoObjectHeader -RenderType 3 -ObjectType 7 -ObjectId 255))
    },
    [ordered]@{
        name = 'player-id'
        bytes = [byte[]](@(4) +
            (New-ClassicDemoObjectHeader -RenderType 1 -ObjectType 4 -ObjectId 8))
    },
    [ordered]@{
        name = 'weapon-id'
        bytes = [byte[]](@(4) +
            (New-ClassicDemoObjectHeader -RenderType 3 -ObjectType 5 -ObjectId 255) +
            @(9, 0) + [BitConverter]::GetBytes([int](1 -shl 16)))
    },
    [ordered]@{
        name = 'clutter-model'
        bytes = [byte[]](@(4) +
            (New-ClassicDemoObjectHeader -RenderType 1 -ObjectType 11 -ObjectId 255))
    },
    [ordered]@{
        name = 'control-type'
        bytes = [byte[]](@(4) +
            (New-ClassicDemoObjectHeader -RenderType 3 -ObjectType 13 -ObjectId 0) +
            @(255, 0) + [BitConverter]::GetBytes([int](1 -shl 16)))
    },
    [ordered]@{
        name = 'movement-type'
        bytes = [byte[]](@(4) +
            (New-ClassicDemoObjectHeader -RenderType 3 -ObjectType 13 -ObjectId 0) +
            @(14, 255) + [BitConverter]::GetBytes([int](1 -shl 16)))
    },
    [ordered]@{
        name = 'model'
        bytes = [byte[]](@(4) + (New-ClassicDemoInvalidModelObject -ModelNum -1))
    },
    [ordered]@{
        name = 'texture-override'
        bytes = [byte[]](@(4) + (New-ClassicDemoPlayerPolyObject -TextureOverride ([int]::MaxValue)))
    },
    [ordered]@{
        name = 'vclip'
        bytes = [byte[]](@(4) + (New-ClassicDemoVclipObject -VclipNum ([int]::MaxValue) -FrameNum 0))
    },
    [ordered]@{
        name = 'vclip-frame'
        bytes = [byte[]](@(4) + (New-ClassicDemoVclipObject -VclipNum 0 -FrameNum 255))
    },
    [ordered]@{
        name = 'explosion-delete-object'
        bytes = [byte[]](@(4) + (New-ClassicDemoAttachedExplosionObject -DeleteObject $maxObjects))
    }
)
foreach ($objectEventCase in $objectEventCases) {
    $caseDirectory = Join-Path $objectProbeRoot "$($objectEventCase.name)-invalid"
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $demoPath = Join-Path $caseDirectory 'input.dem'
    $bytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $objectEventCase.bytes
    [IO.File]::WriteAllBytes($demoPath, $bytes)
    Assert-D1DemoRejected -DemoPath $demoPath -DataPath $probeData `
        -CaseDirectory $caseDirectory -Label "$($objectEventCase.name) invalid object record"
}

$validObjectEvents = @(
    [ordered]@{ name = 'hidden-unused-object'; bytes = [byte[]]@(4, 0, 255) },
    [ordered]@{
        name = 'maximum-segment'
        bytes = [byte[]](@(4) + (New-ClassicDemoLightLaserObject -Segment ($maxSegments - 1)))
    },
    [ordered]@{
        name = 'player-model'
        bytes = [byte[]](@(4) + (New-ClassicDemoPlayerPolyObject -TextureOverride -1))
    },
    [ordered]@{
        name = 'vclip'
        bytes = [byte[]](@(4) + (New-ClassicDemoVclipObject -VclipNum 0 -FrameNum 0))
    },
    [ordered]@{
        name = 'orphan-attachment'
        bytes = [byte[]](@(4) + (New-ClassicDemoAttachedExplosionObject -DeleteObject -1))
    },
    [ordered]@{
        name = 'signed-scaling-boundaries'
        bytes = [byte[]](@(22) + [BitConverter]::GetBytes([int16]::MinValue) + @(4) +
            (New-ClassicDemoPlayerPolyObject -TextureOverride -1 -SignedBoundaries))
    }
)
foreach ($validObjectEvent in $validObjectEvents) {
    $caseDirectory = Join-Path $objectProbeRoot "$($validObjectEvent.name)-valid"
    New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
    $demoPath = Join-Path $caseDirectory 'input.dem'
    $bytes = Add-BytesAtOffset -Bytes $multiplayerBytes `
        -Offset $multiplayerLayout.event_offset -InsertedBytes $validObjectEvent.bytes
    [IO.File]::WriteAllBytes($demoPath, $bytes)
    Assert-D1DemoAccepted -DemoPath $demoPath -DataPath $probeData `
        -CaseDirectory $caseDirectory -Label "$($validObjectEvent.name) valid object record"
}
Write-Output "PASS: D1 validates $($objectEventCases.Count) hostile object record layouts"

[ordered]@{
    schema = 2
    manifest = $manifestPath
    executables = [ordered]@{
        d1_sha256 = (Get-FileHash -LiteralPath $d1ExecutablePath -Algorithm SHA256).Hash.ToLowerInvariant()
        d2_sha256 = (Get-FileHash -LiteralPath $d2ExecutablePath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    cases = $receipts
    security_probes = [ordered]@{
        d1_same_path_rejected = $true
        d1_malformed_mission_strings_rejected = $malformedCases.Count
        d1_invalid_player_counts_rejected = $invalidCounts.Count * 2
        d1_maximum_player_count_accepted = 8
        d1_indexed_event_layouts_checked = $playerEventCases.Count
        d1_world_event_layouts_checked = $worldEventCases.Count
        d1_sound_event_layouts_checked = $soundEventCases.Count
        d1_weapon_event_layouts_checked = $weaponEventCases.Count
    }
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputPath 'receipt.json') -Encoding utf8NoBOM

Write-Output "PASS: classic demo compatibility corpus ($(@($receipts).Count) unique original recording(s))"

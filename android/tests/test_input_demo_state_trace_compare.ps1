#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$compareScript = Join-Path $PSScriptRoot 'compare_input_demo_state_trace.ps1'
$fixtureDir = Join-Path $repoRoot 'temp\input_demo_state_trace_compare_fixture'
$expectedPath = Join-Path $fixtureDir 'expected_state.jsonl'
$actualPath = Join-Path $fixtureDir 'actual_state.jsonl'

function Write-FixtureTrace {
    param(
        [string]$Path,
        [uint32]$SegmentLinkErrorCount = 0,
        [uint32]$ObjectSlotBucketSize = 32
    )

    $slotCounts = @(0) * 32
    $slotHashes = @(0) * 32
    $slotCounts[5] = 2
    $slotHashes[5] = $ObjectSlotHash
    @{
        type = 'frame_state'
        f = 0
        diag = [ordered]@{
            highest_object_index = 191
            live_object_count = 2
            live_object_hash = 123
            object_slot_bucket_size = $ObjectSlotBucketSize
            object_slot_counts = $slotCounts
            object_slot_hashes = $slotHashes
            segment_object_list_count = 2
            segment_object_list_hash = 111
            segment_object_link_error_count = $SegmentLinkErrorCount
        }
        state = @{}
    } | ConvertTo-Json -Compress -Depth 8 | Set-Content -LiteralPath $Path -Encoding utf8NoBOM
}

function Invoke-CompareFixture {
    $compareOutput = & $compareScript -ExpectedPath $expectedPath -ActualPath $actualPath -CompareFrameMetadata *>&1
    return @{
        ExitCode = $LASTEXITCODE
        Output = $compareOutput -join "`n"
    }
}

try {
    if (-not (Test-Path -LiteralPath $fixtureDir)) {
        New-Item -ItemType Directory -Path $fixtureDir -Force | Out-Null
    }

    Write-FixtureTrace -Path $expectedPath
    Write-FixtureTrace -Path $actualPath
    $matching = Invoke-CompareFixture
    if ($matching.ExitCode -ne 0) {
        throw "matching state trace compare failed`n$($matching.Output)"
    }

    Write-FixtureTrace -Path $actualPath -SegmentLinkErrorCount 1
    $mismatch = Invoke-CompareFixture
    if ($mismatch.ExitCode -ne 1) {
        throw "mismatched state trace compare returned $($mismatch.ExitCode)`n$($mismatch.Output)"
    }
    if ($mismatch.Output -notmatch 'stage=object_list_order' -or
        $mismatch.Output -notmatch 'segment_object_link_error_count' -or
        $mismatch.Output -notmatch 'expected_diag=') {
        throw "mismatched state trace compare missed object-list diagnostics`n$($mismatch.Output)"
    }

    Write-FixtureTrace -Path $actualPath -ObjectSlotBucketSize 64
    $objectStateMismatch = Invoke-CompareFixture
    if ($objectStateMismatch.ExitCode -ne 1) {
        throw "mismatched object-state compare returned $($objectStateMismatch.ExitCode)`n$($objectStateMismatch.Output)"
    }
    if ($objectStateMismatch.Output -notmatch 'stage=object_state' -or
        $objectStateMismatch.Output -notmatch 'object_slot_bucket_size' -or
        $objectStateMismatch.Output -notmatch 'expected_diag=') {
        throw "mismatched object-state compare missed diagnostics`n$($objectStateMismatch.Output)"
    }

    Write-Host 'PASS'
} finally {
    if (Test-Path -LiteralPath $fixtureDir) {
        Remove-Item -LiteralPath $fixtureDir -Recurse -Force
    }
}
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
        [uint32]$SegmentListHash,
        [uint32]$ObjectSlotHash = 333
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
            object_slot_bucket_size = 32
            object_slot_counts = $slotCounts
            object_slot_hashes = $slotHashes
            segment_object_list_count = 2
            segment_object_list_hash = $SegmentListHash
            segment_object_link_error_count = 0
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

    Write-FixtureTrace -Path $expectedPath -SegmentListHash 111
    Write-FixtureTrace -Path $actualPath -SegmentListHash 111
    $matching = Invoke-CompareFixture
    if ($matching.ExitCode -ne 0) {
        throw "matching state trace compare failed`n$($matching.Output)"
    }

    Write-FixtureTrace -Path $actualPath -SegmentListHash 222
    $mismatch = Invoke-CompareFixture
    if ($mismatch.ExitCode -ne 1) {
        throw "mismatched state trace compare returned $($mismatch.ExitCode)`n$($mismatch.Output)"
    }
    if ($mismatch.Output -notmatch 'stage=object_list_order' -or
        $mismatch.Output -notmatch 'segment_object_list_hash' -or
        $mismatch.Output -notmatch 'expected_diag=') {
        throw "mismatched state trace compare missed object-list diagnostics`n$($mismatch.Output)"
    }

    Write-FixtureTrace -Path $actualPath -SegmentListHash 111 -ObjectSlotHash 444
    $slotMismatch = Invoke-CompareFixture
    if ($slotMismatch.ExitCode -ne 1) {
        throw "mismatched object-slot compare returned $($slotMismatch.ExitCode)`n$($slotMismatch.Output)"
    }
    if ($slotMismatch.Output -notmatch 'stage=object_state' -or
        $slotMismatch.Output -notmatch 'object_slot_hashes\[5\]' -or
        $slotMismatch.Output -notmatch 'object_slot_range=160-191') {
        throw "mismatched object-slot compare missed bucket diagnostics`n$($slotMismatch.Output)"
    }

    Write-Host 'PASS'
} finally {
    if (Test-Path -LiteralPath $fixtureDir) {
        Remove-Item -LiteralPath $fixtureDir -Recurse -Force
    }
}
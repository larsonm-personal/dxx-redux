#!/usr/bin/env pwsh
<#!
.SYNOPSIS
Summarize Profiling category lines from an exported debug log.

.EXAMPLE
.\android\summarize-profiling-log.ps1 -Path .\android\temp_game_logs\debuglog_20260521.txt
#>

param(
    [Parameter(Mandatory = $true)]
    [string]$Path,

    [int]$TopTextureBursts = 8,
    [int]$TopTextures = 10,
    [int]$TopStorage = 10
)

$ErrorActionPreference = "Stop"

function Get-ProfileFields {
    param([string]$Line)

    $start = $Line.IndexOf("prof_v=1")
    if ($start -lt 0) {
        return $null
    }

    $payload = $Line.Substring($start)
    $fieldMatches = [regex]::Matches($payload, '(?<key>[A-Za-z0-9_]+)=(?<value>[^ ]+)')
    if ($fieldMatches.Count -eq 0) {
        return $null
    }

    $fields = @{}
    foreach ($match in $fieldMatches) {
        $fields[$match.Groups['key'].Value] = $match.Groups['value'].Value
    }
    return $fields
}

function Get-Int64Value {
    param(
        [hashtable]$Fields,
        [string]$Key
    )

    if (-not $Fields.ContainsKey($Key)) {
        return 0L
    }

    $raw = $Fields[$Key]
    if ([string]::IsNullOrEmpty($raw)) {
        return 0L
    }

    try {
        return [int64]$raw
    } catch {
        return 0L
    }
}

function Update-Aggregate {
    param(
        [hashtable]$Table,
        [string]$Key,
        [string]$Primary,
        [string]$Secondary,
        [int64]$Value
    )

    if (-not $Table.ContainsKey($Key)) {
        $Table[$Key] = [ordered]@{
            Primary = $Primary
            Secondary = $Secondary
            Count = 0
            TotalUs = 0L
            MaxUs = 0L
        }
    }

    $entry = $Table[$Key]
    $entry.Count += 1
    $entry.TotalUs += $Value
    if ($Value -gt $entry.MaxUs) {
        $entry.MaxUs = $Value
    }
}

function Update-CountTable {
    param(
        [hashtable]$Table,
        [string]$Key
    )

    $resolvedKey = if ([string]::IsNullOrEmpty($Key)) { "none" } else { $Key }
    if (-not $Table.ContainsKey($resolvedKey)) {
        $Table[$resolvedKey] = 0
    }

    $Table[$resolvedKey] += 1
}

function Write-SectionHeader {
    param([string]$Title)
    Write-Host ""
    Write-Host $Title -ForegroundColor Cyan
}

function Write-AggregateTable {
    param(
        [hashtable]$Table,
        [int]$Top,
        [string]$Label1,
        [string]$Label2
    )

    if ($Table.Count -eq 0) {
        Write-Host "No data"
        return
    }

    $rows =
    $Table.Values |
        Sort-Object -Property @{ Expression = { $_.TotalUs }; Descending = $true }, @{ Expression = { $_.MaxUs }; Descending = $true } |
        Select-Object -First $Top

    foreach ($row in $rows) {
        $avgUs = if ($row.Count -gt 0) { [math]::Round($row.TotalUs / $row.Count, 1) } else { 0 }
        Write-Host (
            "{0}={1} {2}={3} count={4} total_us={5} avg_us={6} max_us={7}" -f
            $Label1, $row.Primary,
            $Label2, $row.Secondary,
            $row.Count,
            $row.TotalUs,
            $avgUs,
            $row.MaxUs
        )
    }
}

function Write-CountTable {
    param(
        [hashtable]$Table,
        [int]$Top,
        [string]$Label
    )

    if ($Table.Count -eq 0) {
        Write-Host "No data"
        return
    }

    $rows =
    $Table.GetEnumerator() |
        Sort-Object -Property @{ Expression = { $_.Value }; Descending = $true }, @{ Expression = { $_.Key }; Descending = $false } |
        Select-Object -First $Top

    foreach ($row in $rows) {
        Write-Host ("{0}={1} count={2}" -f $Label, $row.Key, $row.Value)
    }
}

$resolvedPath = Resolve-Path -LiteralPath $Path
$lines = Get-Content -LiteralPath $resolvedPath

$frameMetrics = @("total_us", "wait_us", "sim_us", "render_us", "replay_us", "swap_us", "gpu_us", "resolve_us", "glerr_us")
$frameSums = @{}
$frameMax = @{}
foreach ($metric in $frameMetrics) {
    $frameSums[$metric] = 0L
    $frameMax[$metric] = 0L
}

$frameCount = 0
$sampleCount = 0
$textureBursts = @()
$textureTable = @{}
$textureLookupCountMetrics = @("ktx2_attempts", "png_attempts")
$textureLookupTimeMetrics = @("ktx2_set_us", "ktx2_pref_us", "ktx2_base_us", "png_set_us", "png_pref_us", "png_base_us", "png_png_us", "png_jpg_us", "png_tga_us")
$textureLookupCounts = @{}
$textureLookupTimes = @{}
foreach ($metric in $textureLookupCountMetrics) {
    $textureLookupCounts[$metric] = 0L
}
foreach ($metric in $textureLookupTimeMetrics) {
    $textureLookupTimes[$metric] = 0L
}
$textureKtx2HitTable = @{}
$texturePngHitTable = @{}
$texturePngHitExtTable = @{}
$textureLookupSeen = $false
$storageTable = @{}
$uiWindowCount = 0
$uiTotalPolls = 0L
$uiWeightedTotalUs = 0L
$uiMaxUs = 0L
$uiSlowPolls = 0L
$uiErrorPolls = 0L

foreach ($line in $lines) {
    $fields = Get-ProfileFields -Line $line
    if ($null -eq $fields) {
        continue
    }

    $type = $fields["type"]
    switch ($type) {
        "frame" {
            $frameCount += 1
            foreach ($metric in $frameMetrics) {
                $value = Get-Int64Value -Fields $fields -Key $metric
                $frameSums[$metric] += $value
                if ($value -gt $frameMax[$metric]) {
                    $frameMax[$metric] = $value
                }
            }
        }
        "summary" {
            $sampleCount += 1
        }
        "texture_burst" {
            $textureBursts += [pscustomobject]@{
                Sample = Get-Int64Value -Fields $fields -Key "sample"
                Reason = if ($fields.ContainsKey("reason")) { $fields["reason"] } else { "unknown" }
                Loads = Get-Int64Value -Fields $fields -Key "loads"
                SlowLoads = Get-Int64Value -Fields $fields -Key "slow_loads"
                SpanUs = Get-Int64Value -Fields $fields -Key "span_us"
                TotalUs = Get-Int64Value -Fields $fields -Key "total_us"
                AvgUs = Get-Int64Value -Fields $fields -Key "avg_us"
                MaxUs = Get-Int64Value -Fields $fields -Key "max_us"
                MaxName = if ($fields.ContainsKey("max_name")) { $fields["max_name"] } else { "unknown" }
                MaxSource = if ($fields.ContainsKey("max_source")) { $fields["max_source"] } else { "unknown" }
                Ktx2Loads = Get-Int64Value -Fields $fields -Key "ktx2_loads"
                PngLoads = Get-Int64Value -Fields $fields -Key "png_loads"
                StockLoads = Get-Int64Value -Fields $fields -Key "stock_loads"
                OtherLoads = Get-Int64Value -Fields $fields -Key "other_loads"
                Ktx2ReadUs = Get-Int64Value -Fields $fields -Key "ktx2_read_us"
                PngReadUs = Get-Int64Value -Fields $fields -Key "png_read_us"
                UploadUs = Get-Int64Value -Fields $fields -Key "upload_us"
                MaskUs = Get-Int64Value -Fields $fields -Key "mask_us"
            }
        }
        "texture" {
            $name = if ($fields.ContainsKey("name")) { $fields["name"] } else { "unknown" }
            $source = if ($fields.ContainsKey("source")) { $fields["source"] } else { "unknown" }
            $totalUs = Get-Int64Value -Fields $fields -Key "total_us"
            Update-Aggregate -Table $textureTable -Key "$name|$source" -Primary $name -Secondary $source -Value $totalUs

            $sawLookupField = $false
            foreach ($metric in $textureLookupCountMetrics) {
                if ($fields.ContainsKey($metric)) {
                    $textureLookupCounts[$metric] += Get-Int64Value -Fields $fields -Key $metric
                    $sawLookupField = $true
                }
            }
            foreach ($metric in $textureLookupTimeMetrics) {
                if ($fields.ContainsKey($metric)) {
                    $textureLookupTimes[$metric] += Get-Int64Value -Fields $fields -Key $metric
                    $sawLookupField = $true
                }
            }
            if ($fields.ContainsKey("ktx2_hit")) {
                Update-CountTable -Table $textureKtx2HitTable -Key $fields["ktx2_hit"]
                $sawLookupField = $true
            }
            if ($fields.ContainsKey("png_hit")) {
                Update-CountTable -Table $texturePngHitTable -Key $fields["png_hit"]
                $sawLookupField = $true
            }
            if ($fields.ContainsKey("png_hit_ext")) {
                Update-CountTable -Table $texturePngHitExtTable -Key $fields["png_hit_ext"]
                $sawLookupField = $true
            }
            if ($sawLookupField) {
                $textureLookupSeen = $true
            }
        }
        "storage" {
            $name = if ($fields.ContainsKey("name")) { $fields["name"] } else { "unknown" }
            $op = if ($fields.ContainsKey("op")) { $fields["op"] } else { "unknown" }
            $totalUs = Get-Int64Value -Fields $fields -Key "total_us"
            Update-Aggregate -Table $storageTable -Key "$name|$op" -Primary $name -Secondary $op -Value $totalUs
        }
        "ui_poll" {
            $polls = Get-Int64Value -Fields $fields -Key "polls"
            $avgUs = Get-Int64Value -Fields $fields -Key "avg_us"
            $maxUs = Get-Int64Value -Fields $fields -Key "max_us"
            $slowPolls = Get-Int64Value -Fields $fields -Key "slow_polls"
            $errors = Get-Int64Value -Fields $fields -Key "errors"

            $uiWindowCount += 1
            $uiTotalPolls += $polls
            $uiWeightedTotalUs += $avgUs * $polls
            $uiSlowPolls += $slowPolls
            $uiErrorPolls += $errors
            if ($maxUs -gt $uiMaxUs) {
                $uiMaxUs = $maxUs
            }
        }
    }
}

if ($frameCount -eq 0 -and $textureBursts.Count -eq 0 -and $textureTable.Count -eq 0 -and $storageTable.Count -eq 0 -and $uiWindowCount -eq 0) {
    Write-Host "No profiling lines found in $resolvedPath" -ForegroundColor Yellow
    exit 1
}

Write-Host "Profiling summary for $resolvedPath" -ForegroundColor Green
Write-Host "samples=$sampleCount frames=$frameCount texture_bursts=$($textureBursts.Count) texture_events=$($textureTable.Count) storage_events=$($storageTable.Count) ui_windows=$uiWindowCount"

Write-SectionHeader "Frame metrics"
if ($frameCount -eq 0) {
    Write-Host "No frame lines"
} else {
    foreach ($metric in $frameMetrics) {
        $avgUs = [math]::Round($frameSums[$metric] / $frameCount, 1)
        Write-Host ("metric={0} avg_us={1} max_us={2}" -f $metric, $avgUs, $frameMax[$metric])
    }
}

Write-SectionHeader "Texture bursts"
if ($textureBursts.Count -eq 0) {
    Write-Host "No texture_burst lines"
} else {
    $topBursts =
    $textureBursts |
        Sort-Object -Property @{ Expression = { $_.TotalUs }; Descending = $true }, @{ Expression = { $_.MaxUs }; Descending = $true } |
        Select-Object -First $TopTextureBursts

    $burstRank = 0
    foreach ($burst in $topBursts) {
        $burstRank += 1
        Write-Host (
            "rank={0} sample={1} reason={2} loads={3} slow_loads={4} total_us={5} span_us={6} avg_us={7} max_us={8} max_name={9} max_source={10} ktx2_loads={11} png_loads={12} stock_loads={13} upload_us={14} ktx2_read_us={15} png_read_us={16} mask_us={17}" -f
            $burstRank,
            $burst.Sample,
            $burst.Reason,
            $burst.Loads,
            $burst.SlowLoads,
            $burst.TotalUs,
            $burst.SpanUs,
            $burst.AvgUs,
            $burst.MaxUs,
            $burst.MaxName,
            $burst.MaxSource,
            $burst.Ktx2Loads,
            $burst.PngLoads,
            $burst.StockLoads,
            $burst.UploadUs,
            $burst.Ktx2ReadUs,
            $burst.PngReadUs,
            $burst.MaskUs
        )
    }
}

Write-SectionHeader "Textures"
Write-AggregateTable -Table $textureTable -Top $TopTextures -Label1 "name" -Label2 "source"

Write-SectionHeader "Texture lookup"
if (-not $textureLookupSeen) {
    Write-Host "No deep lookup fields"
} else {
    foreach ($metric in $textureLookupCountMetrics) {
        Write-Host ("metric={0} total={1}" -f $metric, $textureLookupCounts[$metric])
    }
    foreach ($metric in $textureLookupTimeMetrics) {
        Write-Host ("metric={0} total_us={1}" -f $metric, $textureLookupTimes[$metric])
    }
    Write-Host "ktx2_hit distribution"
    Write-CountTable -Table $textureKtx2HitTable -Top 10 -Label "ktx2_hit"
    Write-Host "png_hit distribution"
    Write-CountTable -Table $texturePngHitTable -Top 10 -Label "png_hit"
    Write-Host "png_hit_ext distribution"
    Write-CountTable -Table $texturePngHitExtTable -Top 10 -Label "png_hit_ext"
}

Write-SectionHeader "Storage"
Write-AggregateTable -Table $storageTable -Top $TopStorage -Label1 "name" -Label2 "op"

Write-SectionHeader "UI poll"
if ($uiWindowCount -eq 0 -or $uiTotalPolls -eq 0) {
    Write-Host "No ui_poll lines"
} else {
    $uiAvgUs = [math]::Round($uiWeightedTotalUs / $uiTotalPolls, 1)
    Write-Host "windows=$uiWindowCount polls=$uiTotalPolls avg_us=$uiAvgUs max_us=$uiMaxUs slow_polls=$uiSlowPolls errors=$uiErrorPolls"
}
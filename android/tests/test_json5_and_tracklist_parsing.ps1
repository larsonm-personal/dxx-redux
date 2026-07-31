#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
. (Join-Path $repoRoot 'game_data\fingerprint_mission_zip_music.ps1') -BudgetTestOnly -SkipAcoustId
$tempRoot = Join-Path $repoRoot 'android\temp\json5_and_tracklist_parsing'

function Assert-Equal {
    param($Expected, $Actual, [string]$Message)
    if ($Expected -cne $Actual) {
        throw "$Message. Expected '$Expected', got '$Actual'"
    }
}

if (Test-Path -LiteralPath $tempRoot) {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $tempRoot | Out-Null

try {
    $zipPath = Join-Path $tempRoot 'mission.zip'
    [IO.File]::WriteAllBytes($zipPath, [byte[]]::new(0))
    $tracklistPath = Join-Path $tempRoot 'mission.tracklist.json'
    $tracklist = @'
{
  "schema": "dxx-mission-tracklist-v1",
  "tracks": [
    {
      "filename": "music//track.ogg",
      "title": "https://example.invalid/song"
    },
    {
      "original_name": "quoted.ogg",
      "title": "Quote: \"x\" and slash \\ and /* literal */"
    }
  ]
}
'@
    [IO.File]::WriteAllText($tracklistPath, $tracklist, [Text.UTF8Encoding]::new($false))
    $lookup = Read-MissionTracklist -ZipFile (Get-Item -LiteralPath $zipPath)
    Assert-Equal 'https://example.invalid/song' $lookup['filename:music//track.ogg'] `
        'Strict tracklist parsing must preserve URL and double-slash strings'
    Assert-Equal 'Quote: "x" and slash \ and /* literal */' $lookup['original_name:quoted.ogg'] `
        'Strict tracklist parsing must preserve escapes and block-comment markers'

    [IO.File]::WriteAllText($tracklistPath, '{"schema":"dxx-mission-tracklist-v1",// comment' + "`n" +
        '"tracks":[]}', [Text.UTF8Encoding]::new($false))
    $strictRejectedComment = $false
    try {
        Read-MissionTracklist -ZipFile (Get-Item -LiteralPath $zipPath) | Out-Null
    } catch {
        $strictRejectedComment = $true
    }
    if (-not $strictRejectedComment) {
        throw 'Strict .tracklist.json parsing accepted a JSON5 comment'
    }

    $tracklistFiles = @(
        Get-ChildItem -LiteralPath (Join-Path $repoRoot 'game_data\mission_files') `
            -Filter '*.tracklist.json' -File
    )
    foreach ($tracklistFile in $tracklistFiles) {
        $current = Read-StrictJsonFile -Path $tracklistFile.FullName
        Assert-Equal 'dxx-mission-tracklist-v1' ([string]$current.schema) `
            "Current tracklist has an unsupported schema: $($tracklistFile.Name)"
    }

    $json5Path = Join-Path $tempRoot 'supported.json5'
    $json5 = @'
{
  // Real line comment.
  "url": "https://example.invalid/a//b",
  "literal": "/* not a comment */",
  "comma_closer": "x,}",
  "escaped": "quote: \" and slash: \\",
  /* Real block
     comment. */
  "items": [
    "value,]",
  ],
}
'@
    [IO.File]::WriteAllText($json5Path, $json5, [Text.UTF8Encoding]::new($false))
    $parsed = Read-Json5File -Path $json5Path
    Assert-Equal 'https://example.invalid/a//b' $parsed.url 'JSON5 parsing must preserve URL strings'
    Assert-Equal '/* not a comment */' $parsed.literal 'JSON5 parsing must preserve comment markers in strings'
    Assert-Equal 'x,}' $parsed.comma_closer 'JSON5 parsing must preserve comma-closer strings'
    Assert-Equal 'quote: " and slash: \' $parsed.escaped 'JSON5 parsing must preserve escaped characters'
    Assert-Equal 'value,]' $parsed.items[0] 'JSON5 parsing must preserve array-closer strings'

    [IO.File]::WriteAllText($json5Path, '{"value":1, /* unterminated', [Text.UTF8Encoding]::new($false))
    $unterminatedRejected = $false
    try {
        Read-Json5File -Path $json5Path | Out-Null
    } catch {
        $unterminatedRejected = $_.Exception.Message -like '*Unterminated block comment*'
    }
    if (-not $unterminatedRejected) {
        throw 'Malformed unterminated JSON5 block comment did not fail clearly'
    }
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "JSON5 and strict tracklist parsing tests passed ($($tracklistFiles.Count) current tracklists)"

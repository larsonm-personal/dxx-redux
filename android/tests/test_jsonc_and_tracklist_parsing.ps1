#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
. (Join-Path $repoRoot 'game_data\fingerprint_mission_zip_music.ps1') -BudgetTestOnly -SkipAcoustId
$tempRoot = Join-Path $repoRoot 'android\temp\jsonc_and_tracklist_parsing'

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
        throw 'Strict .tracklist.json parsing accepted a JSONC comment'
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

    $jsoncPath = Join-Path $tempRoot 'supported.jsonc'
    $jsonc = @'
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
    [IO.File]::WriteAllText($jsoncPath, $jsonc, [Text.UTF8Encoding]::new($false))
    $parsed = Read-JsoncFile -Path $jsoncPath
    Assert-Equal 'https://example.invalid/a//b' $parsed.url 'JSONC parsing must preserve URL strings'
    Assert-Equal '/* not a comment */' $parsed.literal 'JSONC parsing must preserve comment markers in strings'
    Assert-Equal 'x,}' $parsed.comma_closer 'JSONC parsing must preserve comma-closer strings'
    Assert-Equal 'quote: " and slash: \' $parsed.escaped 'JSONC parsing must preserve escaped characters'
    Assert-Equal 'value,]' $parsed.items[0] 'JSONC parsing must preserve array-closer strings'

    [IO.File]::WriteAllText($jsoncPath, '{"value":1, /* unterminated', [Text.UTF8Encoding]::new($false))
    $unterminatedRejected = $false
    try {
        Read-JsoncFile -Path $jsoncPath | Out-Null
    } catch {
        $unterminatedRejected = $_.Exception.Message -like '*Unterminated block comment*'
    }
    if (-not $unterminatedRejected) {
        throw 'Malformed unterminated JSONC block comment did not fail clearly'
    }
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "JSONC and strict tracklist parsing tests passed ($($tracklistFiles.Count) current tracklists)"

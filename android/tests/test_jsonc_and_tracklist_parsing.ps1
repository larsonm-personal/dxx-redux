#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
. (Join-Path $repoRoot 'game_data\fingerprint_mission_zip_music.ps1') -BudgetTestOnly -SkipAcoustId
. (Join-Path $repoRoot 'android\helpers\mission_archive_sources.ps1')
. (Join-Path $repoRoot 'android/helpers/test_helpers.ps1')
$tempRoot = Join-Path $repoRoot 'android\temp\jsonc_and_tracklist_parsing'

function Assert-Equal {
    param($Expected, $Actual, [string]$Message)
    if ($Expected -cne $Actual) {
        throw "$Message. Expected '$Expected', got '$Actual'"
    }
}

function Invoke-JsoncReaderAssignment {
    param([string]$Script, [string]$Variable, [string]$Path)

    # Execute actual reader statements without the generator's build/network/publication work
    $ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $repoRoot $Script), [ref]$null, [ref]$null)
    $assignments = @($ast.FindAll({ param($node)
                $node -is [Management.Automation.Language.AssignmentStatementAst] -and
                $node.Left -is [Management.Automation.Language.VariableExpressionAst]
            }, $true))
    $reader = @($assignments | Where-Object { $_.Left.VariablePath.UserPath -eq $Variable })
    Assert-Equal 1 $reader.Count "Expected exactly one $Variable reader in $Script"
    $configPath = $infoFile = $dbPath = $Path
    $file = $_ = Get-Item -LiteralPath $Path
    . ([scriptblock]::Create($reader[0].Extent.Text))
    return Get-Variable -Name $Variable -ValueOnly
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

    # Sidecar validation requires checked-in JSON, not optional archive payloads
    $tracklistFiles = @(Get-AvailableMissionArchiveSources -Sources (Get-MissionArchiveSources -RepoRoot $repoRoot) -Extensions @('.json') |
            ForEach-Object { Get-MissionTracklistFiles -Source $_ })
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

    $commentedNumber = "{`"value`"/* before colon */:/* before value */12,`r`n// line comment`r`n}"
    $cleanNumber = ConvertFrom-JsoncText -Text $commentedNumber
    Assert-Equal 12 ($cleanNumber | ConvertFrom-Json).value 'Comments may separate complete structural tokens'
    Assert-Equal 2 ([regex]::Matches($cleanNumber, "`r`n").Count) 'Comment removal must preserve CR/LF pairs'

    foreach ($splitNumber in @('1/* gap */2', '1/* gap */.2', '1/* gap */e2', '1e/* gap */2')) {
        [IO.File]::WriteAllText($jsoncPath, ('{"value":' + $splitNumber + '}'), [Text.UTF8Encoding]::new($false))
        $rejected = $false
        try { Read-JsoncFile -Path $jsoncPath | Out-Null } catch { $rejected = $true }
        if (-not $rejected) { throw "JSONC parsing joined a split number: $splitNumber" }

        [IO.File]::WriteAllText($jsoncPath, ('[{"_info":{"timeout_seconds":' + $splitNumber + '}}]'),
            [Text.UTF8Encoding]::new($false))
        if ($null -ne (Get-TestScriptInfo -ScriptPath $jsoncPath)) {
            throw "Actual script metadata extraction accepted a split timeout: $splitNumber"
        }
    }
    [IO.File]::WriteAllText($jsoncPath, '[{"_info":{"timeout_seconds":/* before */12/* after */}}]',
        [Text.UTF8Encoding]::new($false))
    Assert-Equal 12 (Get-TestScriptInfo -ScriptPath $jsoncPath).timeout_seconds `
        'Actual script metadata extraction must retain valid commented timeouts'

    $literal = 'https://example.invalid/a//b,} /* literal */ quote " and slash \ and ,]'
    $readerRecord = [ordered]@{
        album = $literal
        api_key = $literal
        versions = @([ordered]@{ file = 'sample.hog'; sha256 = ('a' * 64); version = $literal })
        tracks = @([ordered]@{
                filename = 'sample.ogg'; chromaprint = 'fingerprint'; duration_ms = 1000
                acoustid_name = $literal; acoustid_album = $literal; acoustid_score = 0.95; acoustid_recording_id = 'reviewed'
            })
        _info = @{
            _deps = @(@{ file = 'direct.hog'; sha256 = ('a' * 64) },
                @{ file = '${ASSET_FILE}.hog'; sha256 = '${ASSET}'; target = 'missions/selected.hog' })
            params = @{ asset = @{ options = @{ first = @{ ASSET = ('b' * 64) }; second = @{ ASSET = ('c' * 64) } } } }
        }
    }
    $readerJson = $readerRecord | ConvertTo-Json -Depth 20
    [IO.File]::WriteAllText($jsoncPath, ("// Reader fixture`n" + $readerJson.Substring(0, $readerJson.Length - 1) + ',}'),
        [Text.UTF8Encoding]::new($false))
    foreach ($readerCase in @(
            @('game_data/fingerprint_music_packs.ps1', 'cfg'),
            @('game_data/fingerprint_music_packs.ps1', 'existing'),
            @('game_data/update_known_discs_albums.ps1', 'db'),
            @('game_data/update_known_discs_albums.ps1', 'sourceInfo'),
            @('game_data/update_known_discs_albums.ps1', 'info'),
            @('game_data/fingerprint_disc_tracks.ps1', 'cfg'),
            @('android/tests/test_fpcalc_and_acoustid.ps1', 'cfg'))) {
        $read = Invoke-JsoncReaderAssignment -Script $readerCase[0] -Variable $readerCase[1] -Path $jsoncPath
        Assert-Equal ($readerRecord | ConvertTo-Json -Depth 20 -Compress) ($read | ConvertTo-Json -Depth 20 -Compress) `
            "Actual $($readerCase -join ':') reader must preserve every field and reviewed lookup value"
    }
    foreach ($source in @('game_data/generate_game_data_index.ps1', 'game_data/hash_assets.ps1')) {
        $ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $repoRoot $source), [ref]$null, [ref]$null)
        foreach ($definition in $ast.FindAll({ param($node)
                    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
                    $node.Name -in @('Add-RequiredHash', 'Get-DeclaredGameDataHash', 'Read-JsoncVersions')
                }, $true)) { . ([scriptblock]::Create($definition.Extent.Text)) }
    }
    Assert-Equal ((@(('a' * 64), ('b' * 64), ('c' * 64))) -join ',') `
    ((@(Get-DeclaredGameDataHash -Path $jsoncPath) | Sort-Object) -join ',') `
        'Actual index dependency extraction must retain exactly direct and parameter option hash sets'
    Assert-Equal ($readerRecord.versions | ConvertTo-Json -Depth 20 -Compress) `
    ((Read-JsoncVersions -path $jsoncPath) | ConvertTo-Json -Depth 20 -Compress) `
        'Actual asset version reader must preserve complete version records'

    [IO.File]::WriteAllText($jsoncPath, ('[' + $readerJson + ']'), [Text.UTF8Encoding]::new($false))
    $deps = @(Get-ScriptDeps -ScriptPath $jsoncPath -Vars @{ ASSET = ('b' * 64); ASSET_FILE = 'selected' })
    Assert-Equal 2 $deps.Count 'Actual dependency reader must retain declarations containing quoted URL metadata'
    Assert-Equal 'direct.hog' $deps[0].file 'Direct dependency filename must remain unchanged'
    Assert-Equal ('a' * 64) $deps[0].sha256 'Direct dependency hash must remain unchanged'
    Assert-Equal 'selected.hog' $deps[1].file 'Dependency filename substitution must remain unchanged'
    Assert-Equal ('b' * 64) $deps[1].sha256 'Dependency hash substitution must remain unchanged'
    Assert-Equal 'missions/selected.hog' $deps[1].target 'Dependency target must remain unchanged'

    $indexRoot = Join-Path $tempRoot 'index-repository'
    $indexGameData = Join-Path $indexRoot 'game_data'
    $indexHelpers = Join-Path $indexRoot 'android/helpers'
    $indexScripts = Join-Path $indexRoot 'android/game_scripts'
    New-Item -ItemType Directory -Path $indexGameData, $indexHelpers, $indexScripts -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data/generate_game_data_index.ps1') -Destination $indexGameData
    foreach ($helper in @('jsonc.ps1', 'powershell_compat.ps1')) {
        Copy-Item -LiteralPath (Join-Path $repoRoot "android/helpers/$helper") -Destination $indexHelpers
    }
    $assetHashes = @()
    foreach ($number in 1..4) {
        $assetPath = Join-Path $indexGameData "asset$number.hog"
        [IO.File]::WriteAllBytes($assetPath, [byte[]]@(1, $number))
        $assetHashes += (Get-FileHash -LiteralPath $assetPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $indexDeclaration = $readerJson.Replace(('a' * 64), $assetHashes[0]).Replace(('b' * 64), $assetHashes[1]).Replace(('c' * 64), $assetHashes[2])
    [IO.File]::WriteAllText((Join-Path $indexScripts 'test_quoted_dependencies.jsonc'), ('[' + $indexDeclaration + ']'),
        [Text.UTF8Encoding]::new($false))
    & (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $indexGameData 'generate_game_data_index.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Actual isolated game data index generation failed' }
    $indexRows = @(Get-Content -LiteralPath (Join-Path $indexGameData 'game_data_index.txt') |
            Where-Object { $_ -and -not $_.StartsWith('#') })
    $expectedRows = @(foreach ($number in 1..3) { "$($assetHashes[$number - 1])  game_data/asset$number.hog" })
    Assert-Equal ($expectedRows -join "`n") ($indexRows -join "`n") `
        'Actual index generation must publish exactly direct and option-dependent identities, excluding unrequested files'

    $templatePath = Join-Path $tempRoot 'template.jsonc'
    $payload = 'quoted "text" and C:\music\track $1 $& $$' + "`n" + [char]0x97f3 + [char]0x697d
    $template = @(
        @{ _info = @{ vars = @{ d1 = @{ RAW = 'default'; WRAPPED = 'prefix ${RAW} suffix'; FILTER = 'd1' } }
                params = @{ CHOICE = @{ options = @{ selected = @{ RAW = $payload }; other = @{ RAW = 'other'; FILTER = 'd2' } } } }
            }
        },
        @{ action = 'log'; when = '${FILTER}'; message = '${WRAPPED}'; choice = '${CHOICE}'; '${RAW}' = 'property name stays literal'
            nested = @{ text = '${RAW}'; number = 7; flag = $true; absent = $null
                empty = @(); one = @(@{ text = '${RAW}' }); arrays = @(@(), @(1, 2)); nullable = @($null, $false, 3)
            }
        },
        @{ action = 'log'; when = 'd2'; message = '${MISSING_IN_EXCLUDED_STEP}' }
    )
    [IO.File]::WriteAllText($templatePath, (ConvertTo-Json -InputObject $template -Depth 20))
    $resolved = Resolve-TestScript -ScriptPath $templatePath -GameId d1 -Params @{ CHOICE = 'selected' }
    $resolvedText = [IO.File]::ReadAllText($resolved)
    $steps = @($resolvedText | ConvertFrom-Json)
    Assert-Equal 1 $steps.Count 'One selected template step must retain root array shape'
    Assert-Equal "prefix $payload suffix" $steps[0].message 'Template values must be resolved before JSON escaping'
    Assert-Equal $payload $steps[0].nested.text 'Nested template strings must retain exact characters'
    Assert-Equal 'selected' $steps[0].choice 'A parameter selection must also be available as a string value'
    Assert-Equal 'property name stays literal' $steps[0].'${RAW}' 'Template substitution must not rewrite property names'
    Assert-Equal 7 $steps[0].nested.number 'Numeric properties must retain their type/value'
    Assert-Equal $true $steps[0].nested.flag 'Boolean properties must retain their type/value'
    Assert-Equal $null $steps[0].nested.absent 'Null properties must remain null'
    Assert-Equal 0 $steps[0].nested.empty.Count 'Empty nested array must remain an array'
    Assert-Equal 1 $steps[0].nested.one.Count 'Singleton nested array must retain its shape'
    Assert-Equal $payload $steps[0].nested.one[0].text 'Strings inside nested arrays must resolve'
    Assert-Equal 2 $steps[0].nested.arrays.Count 'Arrays of arrays must not flatten'
    Assert-Equal 0 $steps[0].nested.arrays[0].Count 'An empty child array must not disappear'
    Assert-Equal '1,2' ($steps[0].nested.arrays[1] -join ',') 'A many-value child array must not flatten'
    Assert-Equal 3 $steps[0].nested.nullable.Count 'Null array elements must not disappear'
    Assert-Equal $null $steps[0].nested.nullable[0] 'Null array elements must remain null'
    Assert-Equal $false $steps[0].nested.nullable[1] 'Boolean array elements must keep their type/value'
    Assert-Equal $false ($steps[0].PSObject.Properties.Name -contains 'when') 'Selected when must be removed'
    if (-not $resolvedText.TrimStart().StartsWith('[') -or $steps[0].nested.number -is [string]) {
        throw 'Resolved JSON must retain array root and original scalar types'
    }
    $beforeText = $resolvedText
    $beforeTime = [IO.File]::GetLastWriteTimeUtc($resolved)
    $template[0]._info.vars.d1.CHOICE = 'default'
    foreach ($case in @(
            @{ RAW = '${UNDEFINED}'; OTHER = ''; Error = 'Unresolved template variable' },
            @{ RAW = '${RAW}'; OTHER = ''; Error = 'Cyclic template variable' },
            @{ RAW = '${OTHER}'; OTHER = '${RAW}'; Error = 'Cyclic template variable' })) {
        $template[0]._info.vars.d1.RAW = $case.RAW
        $template[0]._info.vars.d1.OTHER = $case.OTHER
        [IO.File]::WriteAllText($templatePath, (ConvertTo-Json -InputObject $template -Depth 20))
        $rejected = $false
        try { Resolve-TestScript -ScriptPath $templatePath -GameId d1 | Out-Null }
        catch { $rejected = $_.Exception.Message -like "*$($case.Error)*" }
        Assert-Equal $true $rejected 'Missing/cyclic evaluated variables must fail clearly'
        Assert-Equal $beforeText ([IO.File]::ReadAllText($resolved)) 'Failed resolution must preserve prior output bytes'
        Assert-Equal $beforeTime ([IO.File]::GetLastWriteTimeUtc($resolved)) 'Failed resolution must preserve prior output time'
    }
    $template[0]._info.vars.d1.RAW = 'default'
    $template[0]._info.vars.d1.FILTER = '${UNDEFINED_WHEN}'
    [IO.File]::WriteAllText($templatePath, (ConvertTo-Json -InputObject $template -Depth 20))
    $rejected = $false
    try { Resolve-TestScript -ScriptPath $templatePath -GameId d1 | Out-Null }
    catch { $rejected = $_.Exception.Message -like '*Unresolved template variable*' }
    Assert-Equal $true $rejected 'Unresolved filtering values must fail before selecting steps'
    Assert-Equal $beforeText ([IO.File]::ReadAllText($resolved)) 'Failed when resolution must preserve prior output'
    $template[0]._info.vars.d1.FILTER = 'd1'
    [IO.File]::WriteAllText($templatePath, (ConvertTo-Json -InputObject $template -Depth 20))
    $resolved = Resolve-TestScript -ScriptPath $templatePath -GameId d1 -Params @{ CHOICE = 'other' }
    Assert-Equal 0 @([IO.File]::ReadAllText($resolved) | ConvertFrom-Json).Count `
        'A selected parameter option must override the game filtering variable'
    foreach ($count in @(0, 1, 2)) {
        $plain = @(@{ _info = @{ games = @('d1') } }, @{ action = 'log'; when = 'd1'; message = 'plain $ value' },
            @{ action = 'wait_ms'; when = 'd1'; ms = 10 })
        for ($i = 1; $i -le 2; $i++) { if ($i -gt $count) { $plain[$i].when = 'd2' } }
        [IO.File]::WriteAllText($templatePath, (ConvertTo-Json -InputObject $plain -Depth 10))
        $resolved = Resolve-TestScript -ScriptPath $templatePath -GameId d1
        $json = [IO.File]::ReadAllText($resolved)
        Assert-Equal $count @($json | ConvertFrom-Json).Count 'Filtered template count must be exact'
        Assert-Equal $true ($json.TrimStart().StartsWith('[')) 'Zero/one/many steps must retain root array shape'
    }

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

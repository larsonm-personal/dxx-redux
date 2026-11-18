#!/usr/bin/env pwsh
# Derived strip GPU selection and native save through stock, with protected app state
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$FixtureRoot,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [ValidateSet('door', 'blastable', 'exploding')][string]$Kind = 'door'
)
$ErrorActionPreference = 'Stop'
$fixtures = @{
    door = @('stripdr', 'door14#0.tga', '77dcb6ea5922aa33c3689d2810910845263725a4bc20490f0264eb9c0edccd0c', 'strip doors')
    blastable = @('stripbl', 'door14#0.tga', '52410a065cb8f1f7751cb50cce4dabcc258770af86534fa6ed7b721601cb458d', 'blast strips')
    exploding = @('stripex', 'wall01#0.tga', 'b1da676085adc36a61bf27eee8e18467f5c8e933af4b46d6392fddb997028211', 'explode strips')
}
$mission, $texture, $hogHash, $title = $fixtures[$Kind]
$hog = Join-Path $FixtureRoot "missions/$mission.hog"
$descriptor = Join-Path $FixtureRoot "missions/$mission.mn2"
$pixels = Join-Path $FixtureRoot "mods/$mission/textures/$texture"
if ((Get-FileHash -LiteralPath $hog -Algorithm SHA256).Hash -ne $hogHash -or
    (Get-FileHash -LiteralPath $pixels -Algorithm SHA256).Hash -ne '61acd14d16bcc724bd15e4bea627bfb150f92f90f66bcbe60b3bc25e62b493c1' -or
    (Get-Content -LiteralPath $descriptor -Raw).Replace("`r", '').Trim() -ne "d2x-name = Derived XL $title`nnum_levels = 1`nneptunxl.rl2") {
    throw 'Requires the exact derived wall strip fixture from xl_close_doors'
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Wall strip runtime requires a fresh output directory' }
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $OutputDirectory
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
function Invoke-StripAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Wall strip adb failed: $result" }
    return $result
}
function Invoke-StripPrivate {
    param([string]$Command)
    Invoke-StripAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-wall-strip-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    "files/d2x-redux/missions/$mission.hog", "files/d2x-redux/missions/$mission.mn2", "files/d2x-redux/mods/$mission")
$ready = $false
try {
    Invoke-StripAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-StripPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-StripPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-StripPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-StripPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$OutputDirectory/before.sha256" -Encoding utf8
    $ready = $true
    foreach ($entry in @(@($hog, 'hog'), @($descriptor, 'mn2'), @($pixels, 'tga'))) {
        Invoke-StripAdb -Arguments @('push', (Resolve-Path -LiteralPath $entry[0]).Path, "/data/local/tmp/xl-wall-strip.$($entry[1])") | Out-Null
    }
    Invoke-StripPrivate "mkdir -p files/d2x-redux/missions files/d2x-redux/mods/$mission/textures && cp /data/local/tmp/xl-wall-strip.hog files/d2x-redux/missions/$mission.hog && cp /data/local/tmp/xl-wall-strip.mn2 files/d2x-redux/missions/$mission.mn2 && cp /data/local/tmp/xl-wall-strip.tga files/d2x-redux/mods/$mission/textures/$texture" | Out-Null
    Invoke-StripAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_wall_strip_runtime.jsonc' -Params @{STRIP_KIND = $Kind } -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-StripAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$OutputDirectory/logcat.txt" -Encoding utf8
    Invoke-StripPrivate 'cat files/automation_result.json' | Set-Content "$OutputDirectory/result.json" -Encoding utf8
    Invoke-StripPrivate 'cat files/automation_log.jsonl' | Set-Content "$OutputDirectory/steps.jsonl" -Encoding utf8
    Invoke-StripPrivate 'cat files/introspect.json' | Set-Content "$OutputDirectory/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Wall strip runtime failed ($testExit); see $OutputDirectory" }
    $result = Get-Content "$OutputDirectory/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete wall strip runtime result' }
    Write-Host "PASS: XL $Kind strip GPU, native lifecycle and disk restore through stock ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-StripAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths only; preserve unrelated missions and assets
            foreach ($path in $paths) { Invoke-StripPrivate "rm -rf $path" | Out-Null }
            Invoke-StripPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-StripPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$OutputDirectory/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Wall strip state restoration differs; backup retained at $backup" }
            Invoke-StripPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-StripAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-wall-strip.hog', '/data/local/tmp/xl-wall-strip.mn2', '/data/local/tmp/xl-wall-strip.tga') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}

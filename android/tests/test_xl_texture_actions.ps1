#!/usr/bin/env pwsh
# XL texture events, normal renderer/collision and disk/rewind; no traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '6e561cc28ed0b0641e8501ac8c6033baa4dc974f01205d6a3fd18008cbebe22f'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'cf87743d75f566c8dbc227d25b7c094d84d9779027f594586ee81cde0998e8ae') {
    throw 'Requires the pinned derived fixture from xl_texture_actions'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-texture-actions-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-TextureAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Texture adb failed: $result" }
    return $result
}
function Invoke-TexturePrivate {
    param([string]$Command)
    Invoke-TextureAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-texture-actions-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/xtexture.hog', 'files/d2x-redux/missions/xtexture.mn2')
$ready = $false
try {
    Invoke-TextureAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-TexturePrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-TexturePrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-TexturePrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-TexturePrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-TextureAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-texture-actions.hog') | Out-Null
    Invoke-TextureAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-texture-actions.mn2') | Out-Null
    Invoke-TexturePrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-texture-actions.hog files/d2x-redux/missions/xtexture.hog && cp /data/local/tmp/xl-texture-actions.mn2 files/d2x-redux/missions/xtexture.mn2' | Out-Null
    Invoke-TextureAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_texture_actions.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-TextureAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-TexturePrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-TexturePrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-TexturePrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Texture runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Texture runtime result' }
    Invoke-TextureAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-texture-actions.png') | Out-Null
    Invoke-TextureAdb -Arguments @('pull', '/data/local/tmp/xl-texture-actions.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Texture Android renderer/collision/actions, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-TextureAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-TexturePrivate "rm -rf $path" | Out-Null }
            Invoke-TexturePrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-TexturePrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Texture test state restoration differs; backup retained at $backup" }
            Invoke-TexturePrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-TextureAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-texture-actions.hog', '/data/local/tmp/xl-texture-actions.mn2', '/data/local/tmp/xl-texture-actions.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}

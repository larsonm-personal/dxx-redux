#!/usr/bin/env pwsh
# Serial provisioned-emulator integration with independent Android view-state observation
param(
    [switch]$Install,
    [ValidateSet('d1', 'd2')][string]$Game,
    [string]$Serial = 'emulator-5554'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'test_helpers.ps1')
if ($Serial -notmatch '^emulator-[0-9]+$') { throw 'This reset-state fixture is restricted to provisioned emulators' }
$outputDirectory = Join-Path $script:REPO_ROOT ('android/temp/graphics-black-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts @($outputDirectory)
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial
$games = if ($Game) { @($Game) } else { @('d1', 'd2') }
$results = [System.Collections.Generic.List[object]]::new()
$child = $null

function Invoke-BlackDevice {
    param([string[]]$Arguments)
    $output = & $script:ADB -s $Serial @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "adb failed: $($Arguments -join ' '): $output" }
    return ($output -join "`n")
}

function Read-BlackJson {
    param([string]$Path)
    return (Invoke-BlackDevice @('shell', 'run-as', $script:PACKAGE, 'cat', $Path) | ConvertFrom-Json)
}

try {
    foreach ($gameId in $games) {
        $observed = $false
        $originalPid = ''
        $accepted = $null
        $arguments = @('-NoProfile', '-File', (Join-Path $PSScriptRoot 'run_test.ps1'), '-ScriptName',
            'test_graphics_black_output.jsonc', '-Game', $gameId, '-LeaveRunning', '-TimeoutSeconds', '180')
        if ($Install) { $arguments += '-Install'; $Install = $false }
        $child = Start-Process -FilePath (Get-Command pwsh).Source -ArgumentList $arguments -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $outputDirectory "$gameId-test.txt") `
            -RedirectStandardError (Join-Path $outputDirectory "$gameId-stderr.txt")
        $timer = [Diagnostics.Stopwatch]::StartNew()
        while (-not $child.HasExited -and $timer.Elapsed.TotalSeconds -lt 210) {
            if (-not $observed) {
                $requestId = [guid]::NewGuid().ToString('N')
                Invoke-BlackDevice @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.INTROSPECT', '--es', 'request_id', $requestId) | Out-Null
                $state = $null
                try { $state = Read-BlackJson 'files/introspect.json' } catch { }
                if ($state.graphics_safety.debug_black_output.active -and $state.graphics_safety.debug_black_output.valid_frames -ge 1) {
                    $record = Read-BlackJson 'files/graphics_safety.json'
                    $ui = Read-BlackJson 'files/introspect_ui.json'
                    if ($ui.request_id -eq $requestId -and $record.attempt.phase -eq 3 -and
                        $record.attempt.trial_id -eq $state.graphics_safety.trial_id) {
                        if (-not $ui.graphics_confirmation_shown -or -not $ui.graphics_confirmation_attached) {
                            throw 'Kotlin confirmation was not visible and attached while engine output was black'
                        }
                        $state | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory "$gameId-black-state.json") -Encoding utf8NoBOM
                        $ui | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $outputDirectory "$gameId-black-ui.json") -Encoding utf8NoBOM
                        $originalPid = Invoke-BlackDevice @('shell', 'pidof', "$($script:PACKAGE):game")
                        $accepted = $state.graphics_safety.accepted
                        $observed = $true
                    }
                }
            }
            Start-Sleep -Milliseconds 200
            $child.Refresh()
        }
        if (-not $child.HasExited) { throw "Black-output integration timed out: $gameId" }
        $child.WaitForExit()
        if ($child.ExitCode -ne 0) { throw "Native black-output integration failed: $gameId; see $outputDirectory" }
        if (-not $observed) { throw 'No independent view-state observation during proven black output' }
        Invoke-BlackDevice @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.INTROSPECT') | Out-Null
        Start-Sleep -Milliseconds 300
        $after = Read-BlackJson 'files/introspect.json'
        $after | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory "$gameId-after.json") -Encoding utf8NoBOM
        Invoke-BlackDevice @('logcat', '-d') | Set-Content (Join-Path $outputDirectory "$gameId-logcat.txt") -Encoding utf8NoBOM
        if ((Invoke-BlackDevice @('shell', 'pidof', "$($script:PACKAGE):game")) -ne $originalPid) {
            throw 'Responsive black output unexpectedly required process recovery'
        }
        $black = $after.graphics_safety.debug_black_output
        if ($black.active -or $black.frames -lt 2 -or $black.frames -ne $black.valid_frames) {
            throw 'Not every injected frame proved black pixels and exact GL state restoration'
        }
        foreach ($field in $accepted.PSObject.Properties) {
            foreach ($snapshot in @($after.graphics_safety.current, $after.graphics_safety.accepted, $after.graphics_safety.requested)) {
                if ($snapshot.($field.Name) -ne $field.Value) { throw "Protected tuple did not restore: $($field.Name)" }
            }
        }
        foreach ($path in @('files/descent.cfg', 'files/d1x-redux/descent.cfg', 'files/d2x-redux/descent.cfg')) {
            $text = Invoke-BlackDevice @('shell', 'run-as', $script:PACKAGE, 'cat', $path)
            foreach ($field in $accepted.PSObject.Properties) {
                $fieldMatches = [regex]::Matches($text, '(?m)^' + [regex]::Escape($field.Name) + '=(-?[0-9]+)\r?$')
                if ($fieldMatches.Count -ne 1 -or [int]$fieldMatches[0].Groups[1].Value -ne $field.Value) {
                    throw "Mirrored accepted value differs: $path $($field.Name)"
                }
            }
        }
        $results.Add([ordered]@{ game = $gameId; passed = $true; black_frames = $black.frames; same_process = $true; mirrored_configs = $true })
        $results | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $outputDirectory 'results.json') -Encoding utf8NoBOM
        Write-Output "PASS: $gameId, $($black.frames) proven black frames, visible Kotlin modal, live full rollback and pre-armed failure capture"
        $child = $null
    }
    Write-Output "Graphics black-output tests passed; output: $outputDirectory"
} finally {
    if ($child -and -not $child.HasExited) { Stop-Process -Id $child.Id -ErrorAction SilentlyContinue }
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial }
    else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}

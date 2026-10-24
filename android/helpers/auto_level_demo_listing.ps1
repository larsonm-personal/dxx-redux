# Local recording-runner boundary; leave legacy Adb callers unchanged
function Save-AutoLevelAdbDiagnostic {
    param([string]$Path, [string]$Stdout, $Diagnostic, [string]$Serial, [string]$Package)
    $record = [ordered]@{ serial = $Serial; package = $Package; stdout = $Stdout.Substring(0, [Math]::Min($Stdout.Length, 8192)); diagnostic = $Diagnostic }
    $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Path -Encoding utf8NoBOM
}

function Get-AutoLevelRecordingPaths {
    param([ValidateSet('d1', 'd2')][string]$Game, [string]$Package, [string]$Serial, [string]$OutputDirectory)
    $remoteDir = "files/${Game}x-redux/input_demo_recordings/new"
    $diagnostic = $null
    $stdout = Adb-Timeout -AdbArgs @('shell', 'run-as', $Package, 'find', $remoteDir, '-type', 'f', '-name', '*.dximdemo') -Seconds 30 -DiagnosticResult ([ref]$diagnostic)
    Save-AutoLevelAdbDiagnostic -Path (Join-Path $OutputDirectory 'recording-listing.json') -Stdout ([string]$stdout) -Diagnostic $diagnostic -Serial $Serial -Package $Package
    if ($diagnostic.timed_out) { throw 'Recording listing timed out' }
    if ($diagnostic.error) { throw "Recording listing process error: $($diagnostic.error)" }
    if ($null -eq $diagnostic.exit_code -or $diagnostic.exit_code -ne 0) {
        throw "Recording listing failed with exit code '$($diagnostic.exit_code)'; see recording-listing.json"
    }
    $sources = @(([string]$stdout -split '\r?\n') | Where-Object { $_.Trim() })
    # Only direct filenames from this game's staging directory may be exported
    $pattern = '^' + [regex]::Escape($remoteDir) + '/[^/\\\x00-\x1f]+\.dximdemo$'
    foreach ($source in $sources) {
        if ($source -cnotmatch $pattern) { throw "Invalid recording path in listing: $source" }
    }
    if ($sources.Count -ne 2) { throw "Expected two new $Game recordings, got $($sources.Count)" }
    if ($sources[0] -ceq $sources[1]) { throw 'Duplicate recording path in listing' }
    return $sources
}

function Save-AutoLevelScenarioFailure {
    param([string]$OutputDirectory, [string]$Package, [string]$Serial)
    foreach ($name in @('automation_result.json', 'automation_log.jsonl')) {
        $diagnostic = $null
        $stdout = Adb-Timeout -AdbArgs @('exec-out', 'run-as', $Package, 'tail', '-c', '8192', "files/$name") -Seconds 5 -DiagnosticResult ([ref]$diagnostic)
        Save-AutoLevelAdbDiagnostic -Path (Join-Path $OutputDirectory "$name.diagnostic.json") -Stdout ([string]$stdout) -Diagnostic $diagnostic -Serial $Serial -Package $Package
    }
}

#!/usr/bin/env pwsh
[CmdletBinding()]
param(
    [string]$BuildDirectory = 'android/tests/build',
    [string]$ReferenceDirectory = 'game_data/music/dos-references',
    [string]$OutputDirectory = 'android/temp/dos_midi_parity',
    [ValidateSet('all', '2', '7', '8')][string]$Level = 'all',
    [switch]$SkipBuild,
    [switch]$SyntheticOnly
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. "$PSScriptRoot/../helpers/test_env.ps1"
. "$PSScriptRoot/../helpers/headless_process_pool.ps1"
. "$PSScriptRoot/../helpers/atomic_text_file.ps1"
. "$PSScriptRoot/../helpers/output_disk_space.ps1"

function Resolve-CheckoutPath {
    param([string]$Path)
    if (-not [IO.Path]::IsPathRooted($Path)) { $Path = Join-Path $repoRoot $Path }
    return [IO.Path]::GetFullPath($Path)
}

function Invoke-MidiProcess {
    param([string]$Name, [string]$Exe, [string[]]$Arguments, [int]$TimeoutSeconds)
    $execution = @{ Result = $null }
    $task = [pscustomobject]@{ FilePath = $Exe; Arguments = $Arguments; WorkingDirectory = $repoRoot; TimeoutSeconds = $TimeoutSeconds }
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        Write-Utf8NoBomTextAtomically -Path (Join-Path $runDir "$Name.stdout.log") -Text $result.StandardOutput
        Write-Utf8NoBomTextAtomically -Path (Join-Path $runDir "$Name.stderr.log") -Text $result.StandardError
        $execution.Result = $result
    }
    if (-not $execution.Result -or $execution.Result.TimedOut -or $execution.Result.ExitCode -ne 0) {
        throw "$Name failed (exit=$($execution.Result.ExitCode), timeout=$($execution.Result.TimedOut)); see $runDir"
    }
    return $execution.Result.StandardOutput
}

$BuildDirectory = Resolve-CheckoutPath $BuildDirectory
$ReferenceDirectory = Resolve-CheckoutPath $ReferenceDirectory
$OutputDirectory = Resolve-CheckoutPath $OutputDirectory
$runDir = Join-Path $OutputDirectory ('run_' + [guid]::NewGuid().ToString('N'))
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $runDir -DirectoryPrefix run_
Assert-OutputDiskSpace -Paths @($BuildDirectory, $runDir)
New-Item -ItemType Directory -Path $runDir | Out-Null
$producerLock = $null
$summary = [ordered]@{ status = 'FAIL'; synthetic = 'NOT_RUN'; references = @(); error = $null }
$exitCode = 1
try {
    $producerLock = [IO.File]::Open((Join-Path $runDir 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    Write-Host "DOS MIDI diagnostics: $runDir"
    $cmake = Resolve-RegressionCMakePath -RepoRoot $repoRoot -BuildDir $BuildDirectory
    if (-not $cmake) { throw 'CMake is required for host MIDI tests' }
    $ctest = Resolve-RegressionBuildTool -Directory (Split-Path $cmake) -BaseName ctest
    if (-not $ctest) { throw "CTest was not found beside $cmake" }
    if (-not $SkipBuild) {
        Invoke-MidiProcess -Name configure -Exe $cmake -Arguments @('-S', "$repoRoot/android/app/src/main/cpp/extract", '-B', $BuildDirectory) -TimeoutSeconds 180 | Out-Null
        Invoke-MidiProcess -Name build -Exe $cmake -Arguments @('--build', $BuildDirectory, '--config', 'Release', '--parallel', '2', '--target', 'hmp_midi_export', 'midi_tsf_render', 'test_hmp_android_shared', 'test_midi_seek_timeline') -TimeoutSeconds 300 | Out-Null
    }
    $exporter = Resolve-RegressionBuildTool -Directory (Join-Path $BuildDirectory Release) -BaseName hmp_midi_export
    if (-not $exporter) { $exporter = Resolve-RegressionBuildTool -Directory $BuildDirectory -BaseName hmp_midi_export }
    if (-not $exporter) { throw "MIDI exporter is missing from $BuildDirectory; run without -SkipBuild" }
    $requiredTests = @('hmp_android_shared_tests', 'midi_seek_timeline_tests', 'hmp_playback_tests')
    $testArgs = @('--test-dir', $BuildDirectory, '-C', 'Release', '-R', ('^(' + ($requiredTests -join '|') + ')$'))
    $catalog = Invoke-MidiProcess -Name catalog -Exe $ctest -Arguments ($testArgs + @('--show-only=json-v1')) -TimeoutSeconds 30 | ConvertFrom-Json
    foreach ($name in $requiredTests) {
        if ($name -notin @($catalog.tests.name)) { throw "Required synthetic test is not registered: $name" }
    }
    Invoke-MidiProcess -Name synthetic -Exe $ctest -Arguments ($testArgs + @('--output-on-failure', '--no-tests=error', '--timeout', '60')) -TimeoutSeconds 210 | Write-Host
    $summary.synthetic = 'PASS'
    $missing = @()
    if (-not $SyntheticOnly) {
        $python = Resolve-RegressionPythonCommand
        if (-not $python) { throw 'Python 3 is required for DOS MIDI reference parity' }
        $levels = if ($Level -eq 'all') { @(2, 7, 8) } else { @([int]$Level) }
        foreach ($number in $levels) {
            $name = 'descent14-game{0:D2}' -f $number
            $fixture = Join-Path $PSScriptRoot "fixtures/dos-midi/$name.json"
            $spec = Get-Content -LiteralPath $fixture -Raw | ConvertFrom-Json
            $reference = Join-Path $ReferenceDirectory $name
            $absent = @(@($spec.hmp, $spec.capture) | Where-Object { -not (Test-Path -LiteralPath (Join-Path $reference $_) -PathType Leaf) })
            if ($absent.Count) {
                $missing += $name
                $summary.references += [ordered]@{ name = $name; status = 'SKIP'; missing = $absent }
                continue
            }
            Invoke-MidiProcess -Name $name -Exe $python.Path -Arguments (@($python.PrefixArguments) + @('-B', "$PSScriptRoot/run_dos_midi_parity.py", '--exporter', $exporter, '--fixture', $fixture, '--reference', $reference, '--output', (Join-Path $runDir $name))) -TimeoutSeconds 240 | Write-Host
            $summary.references += [ordered]@{ name = $name; status = 'PASS' }
        }
    }
    if ($missing.Count) {
        $summary.status = 'SKIP'
        $exitCode = 2
        Write-Host "RESULT: SKIP (synthetic MIDI tests passed; missing DOS reference captures: $($missing -join ', '))"
    } else {
        $summary.status = 'PASS'
        $exitCode = 0
        Write-Host 'RESULT: PASS (requested MIDI tests passed)'
    }
} catch {
    $summary.error = $_.Exception.Message
    Write-Host "RESULT: FAIL ($($summary.error))"
} finally {
    try {
        Write-Utf8NoBomTextAtomically -Path (Join-Path $runDir 'summary.json') -Text (($summary | ConvertTo-Json -Depth 10) + "`n")
    } finally {
        if ($producerLock) { $producerLock.Dispose() }
    }
}
exit $exitCode

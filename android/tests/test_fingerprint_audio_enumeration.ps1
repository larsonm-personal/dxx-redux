$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/fingerprint_audio_results.ps1')
. (Join-Path $repoRoot 'android/helpers/test_env.ps1')

$buildDir = Join-Path $repoRoot 'android/tests/build'
$fingerprintAudio = Resolve-RegressionBuildTool -Directory (Join-Path $buildDir 'Release') -BaseName 'fingerprint_audio'
if (-not $fingerprintAudio) {
    $fingerprintAudio = Resolve-RegressionBuildTool -Directory $buildDir -BaseName 'fingerprint_audio'
}
if (-not $fingerprintAudio) {
    throw 'fingerprint_audio is not built'
}
$fingerprintAudioTest = Resolve-RegressionBuildTool -Directory (Join-Path $buildDir 'Release') -BaseName 'test_fingerprint_audio_enumeration'
if (-not $fingerprintAudioTest) {
    $fingerprintAudioTest = Resolve-RegressionBuildTool -Directory $buildDir -BaseName 'test_fingerprint_audio_enumeration'
}
if (-not $fingerprintAudioTest) {
    throw 'test_fingerprint_audio_enumeration is not built'
}

$workRoot = Join-Path $repoRoot "android/temp/fingerprint_audio_enumeration_$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $workRoot | Out-Null

function Invoke-FingerprintAudioTest {
    param(
        [Parameter(Mandatory)]
        [string]$Directory,
        [int]$FailAfter = -1
    )

    $savedInjection = [Environment]::GetEnvironmentVariable('DXX_FINGERPRINT_AUDIO_TEST_FAIL_AFTER')
    try {
        if ($FailAfter -ge 0) {
            [Environment]::SetEnvironmentVariable('DXX_FINGERPRINT_AUDIO_TEST_FAIL_AFTER', [string]$FailAfter)
        } else {
            [Environment]::SetEnvironmentVariable('DXX_FINGERPRINT_AUDIO_TEST_FAIL_AFTER', $null)
        }
        $executable = if ($FailAfter -ge 0) { $fingerprintAudioTest } else { $fingerprintAudio }
        $startInfo = [Diagnostics.ProcessStartInfo]::new($executable)
        $startInfo.ArgumentList.Add($Directory)
        $startInfo.UseShellExecute = $false
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $process = [Diagnostics.Process]::Start($startInfo)
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        return [PSCustomObject]@{
            ExitCode = $process.ExitCode
            Stdout   = $stdout
            Stderr   = $stderr
        }
    } finally {
        [Environment]::SetEnvironmentVariable('DXX_FINGERPRINT_AUDIO_TEST_FAIL_AFTER', $savedInjection)
    }
}

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Assert-ResultValidationFails {
    param([string[]]$ExpectedNames, [object[]]$Results, [string]$Case)
    $failed = $false
    try {
        Assert-DxxFingerprintAudioResults -ExpectedNames $ExpectedNames -Results $Results
    } catch {
        $failed = $true
    }
    Assert-Condition $failed "$Case did not fail"
}

try {
    $emptyDir = Join-Path $workRoot 'empty'
    New-Item -ItemType Directory -Path $emptyDir | Out-Null
    $empty = Invoke-FingerprintAudioTest -Directory $emptyDir
    Assert-Condition ($empty.ExitCode -eq 0) 'Readable empty directory failed'
    Assert-Condition ($empty.Stdout.Trim() -eq '[]') 'Readable empty directory did not produce []'

    $missing = Invoke-FingerprintAudioTest -Directory (Join-Path $workRoot 'missing')
    Assert-Condition ($missing.ExitCode -ne 0) 'Missing directory succeeded'
    Assert-Condition ($missing.Stderr -match 'Directory enumeration failed') 'Missing directory lacked an explicit error'
    Assert-Condition ([string]::IsNullOrWhiteSpace($missing.Stdout)) 'Missing directory produced success JSON'

    $notDirectoryPath = Join-Path $workRoot 'not_a_directory'
    [IO.File]::WriteAllText($notDirectoryPath, 'not a directory')
    $notDirectory = Invoke-FingerprintAudioTest -Directory $notDirectoryPath
    Assert-Condition ($notDirectory.ExitCode -ne 0) 'Non-directory path succeeded'
    Assert-Condition ($notDirectory.Stderr -match 'Directory enumeration failed') 'Non-directory path lacked an explicit error'

    $unicodeDir = Join-Path $workRoot 'audio_音楽'
    New-Item -ItemType Directory -Path $unicodeDir | Out-Null
    $testAudio = Join-Path $buildDir '_deps/chromaprint-src/tests/data/test.mp3'
    if (-not (Test-Path -LiteralPath $testAudio -PathType Leaf)) {
        throw "Chromaprint test audio is missing: $testAudio"
    }
    $unicodeName = 'canción_音楽.MP3'
    Copy-Item -LiteralPath $testAudio -Destination (Join-Path $unicodeDir $unicodeName)
    $unicode = Invoke-FingerprintAudioTest -Directory $unicodeDir
    Assert-Condition ($unicode.ExitCode -eq 0) 'Unicode audio path failed'
    $unicodeResults = @($unicode.Stdout | ConvertFrom-Json)
    Assert-DxxFingerprintAudioResults -ExpectedNames @($unicodeName) -Results $unicodeResults
    Assert-Condition ($unicodeResults.Count -eq 1) 'Unicode audio file was not reported exactly once'

    Copy-Item -LiteralPath $testAudio -Destination (Join-Path $unicodeDir 'second.mp3')
    $injected = Invoke-FingerprintAudioTest -Directory $unicodeDir -FailAfter 1
    Assert-Condition ($injected.ExitCode -ne 0) 'Injected enumeration failure succeeded'
    Assert-Condition ($injected.Stderr -match 'Injected directory enumeration failure') 'Injected failure lacked an explicit error'
    Assert-Condition ([string]::IsNullOrWhiteSpace($injected.Stdout)) 'Injected failure produced partial JSON'

    $limitDir = Join-Path $workRoot 'limit'
    New-Item -ItemType Directory -Path $limitDir | Out-Null
    for ($i = 0; $i -lt 4096; $i++) {
        [IO.File]::WriteAllBytes((Join-Path $limitDir ('track_{0:D4}.mp3' -f $i)), [byte[]]::new(0))
    }
    $atLimit = Invoke-FingerprintAudioTest -Directory $limitDir
    Assert-Condition ($atLimit.Stderr -match 'Found 4096 audio files') 'Exact-limit directory was not fully enumerated'
    Assert-Condition ($atLimit.Stderr -notmatch 'more than 4096') 'Exact-limit directory was rejected as over capacity'
    [IO.File]::WriteAllBytes((Join-Path $limitDir 'track_4096.mp3'), [byte[]]::new(0))
    $overLimit = Invoke-FingerprintAudioTest -Directory $limitDir
    Assert-Condition ($overLimit.ExitCode -ne 0) 'One-over directory succeeded'
    Assert-Condition ($overLimit.Stderr -match 'more than 4096') 'One-over directory lacked a capacity error'
    Assert-Condition ([string]::IsNullOrWhiteSpace($overLimit.Stdout)) 'One-over directory produced partial JSON'

    $validResult = [PSCustomObject]@{ filename = 'one.ogg'; chromaprint = 'encoded'; duration_ms = 1000 }
    Assert-DxxFingerprintAudioResults -ExpectedNames @('one.ogg') -Results @($validResult)
    Assert-ResultValidationFails -ExpectedNames @('one.ogg', 'two.ogg') -Results @($validResult) -Case 'missing result'
    Assert-ResultValidationFails -ExpectedNames @('one.ogg') -Results @($validResult, $validResult) -Case 'duplicate result'
    $unexpectedResult = [PSCustomObject]@{ filename = 'other.ogg'; chromaprint = 'encoded'; duration_ms = 1000 }
    Assert-ResultValidationFails -ExpectedNames @('one.ogg') -Results @($unexpectedResult) -Case 'unexpected result'
    $incompleteResult = [PSCustomObject]@{ filename = 'one.ogg'; chromaprint = ''; duration_ms = 0 }
    Assert-ResultValidationFails -ExpectedNames @('one.ogg') -Results @($incompleteResult) -Case 'incomplete result'

    Write-Host 'fingerprint audio enumeration tests passed'
} finally {
    Remove-Item -LiteralPath $workRoot -Recurse -Force -ErrorAction SilentlyContinue
    $global:LASTEXITCODE = 0
}

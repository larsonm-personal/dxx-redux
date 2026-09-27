$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android\helpers\bounded_extraction.ps1')

$testRoot = Join-Path $repoRoot "android\temp\bounded_python_$([Guid]::NewGuid().ToString('N'))"
$oldPath = $env:Path
$isWindowsHost = (Get-HostPlatform) -eq 'Windows'
$originalHostPlatformFunction = ${function:Get-HostPlatform}
$fixtureLock = $null
$oldRuntime = $env:DXX_BOUNDED_PYTHON_RUNTIME
$oldRuntimeHash = $env:DXX_BOUNDED_PYTHON_SHA256
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $testRoot -DirectoryPrefix 'bounded_python_'
New-Item -ItemType Directory -Path $testRoot | Out-Null
try {
    $fixtureLock = [IO.File]::Open((Join-Path $testRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $hostileRoot = Join-Path $testRoot 'hostile path'
    New-Item -ItemType Directory -Path $hostileRoot | Out-Null
    $sentinel = Join-Path $testRoot 'path-runtime-invoked.txt'
    if ($isWindowsHost) {
        Set-Content -LiteralPath (Join-Path $hostileRoot 'python.cmd') `
            -Value "@echo invoked>`"$sentinel`"`r`n@exit /b 77" -NoNewline
    } else {
        foreach ($name in @('python', 'python3')) {
            $hostile = Join-Path $hostileRoot $name
            Set-Content -LiteralPath $hostile -Value "#!/bin/sh`nprintf invoked > '$sentinel'`nexit 77" -NoNewline
            & chmod +x $hostile
            if ($LASTEXITCODE -ne 0) { throw 'Could not prepare hostile PATH fixture' }
        }
    }
    $env:Path = "$hostileRoot$([IO.Path]::PathSeparator)$oldPath"

    $env:DXX_BOUNDED_PYTHON_RUNTIME = $null
    $env:DXX_BOUNDED_PYTHON_SHA256 = $null
    $repositoryRuntime = Resolve-BoundedPythonRuntime
    if ($repositoryRuntime.Provenance -cne 'repository-pinned-tree' -or
        $repositoryRuntime.Version -cne '3.12.8') {
        throw 'Repository runtime did not report pinned provenance and version'
    }
    if (Test-Path -LiteralPath $sentinel) {
        throw 'Hostile PATH Python was invoked'
    }
    Write-Host 'PASS: repository runtime ignores hostile PATH precedence'

    $explicitInstall = Join-Path $testRoot 'explicit runtime [verified]'
    New-Item -ItemType Directory -Path $explicitInstall | Out-Null
    $explicitRoot = Join-Path $explicitInstall 'python'
    $runtimeRoot = if ($isWindowsHost) { Split-Path $repositoryRuntime.Path } else { Split-Path (Split-Path $repositoryRuntime.Path) }
    Copy-Item -LiteralPath $runtimeRoot -Destination $explicitRoot -Recurse
    $relativeExecutable = if ($isWindowsHost) { 'python.exe' } else { 'bin/python3.12' }
    $explicitPath = Join-Path $explicitRoot $relativeExecutable
    $explicitHash = (Get-FileHash -LiteralPath $explicitPath -Algorithm SHA256).Hash
    $explicitRuntime = Resolve-BoundedPythonRuntime -RuntimePath $explicitPath `
        -ExpectedSha256 $explicitHash
    if ($explicitRuntime.Provenance -cne 'explicit-sha256' -or
        $explicitRuntime.Path -cne (Resolve-Path -LiteralPath $explicitPath).Path) {
        throw 'Explicit runtime identity was not preserved'
    }
    Write-Host 'PASS: explicit runtime with spaces is admitted by SHA-256'

    # Exercise implicit tree admission in an isolated checkout, without altering
    # the shared installed runtime or substituting executable-only verification
    $fixtureRepo = Join-Path $testRoot 'fixture checkout'
    foreach ($relative in @('android/helpers/bounded_extraction.ps1', 'android/helpers/verified_dependencies.ps1',
            'android/get_deps/helpers/Get-DepPlatform.ps1', 'android/get_deps/tool_versions.conf')) {
        $destination = Join-Path $fixtureRepo $relative
        New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $repoRoot $relative) -Destination $destination
    }
    Set-Content -LiteralPath (Join-Path $fixtureRepo 'dependency_base.txt') -Value $testRoot
    $directoryKey = if ($isWindowsHost) { 'PYTHON_ORACLE_DIR_NAME' } else { 'PYTHON_BOUNDED_LINUX_DIR_NAME' }
    Add-Content -LiteralPath (Join-Path $fixtureRepo 'android/get_deps/tool_versions.conf') -Value "$directoryKey=explicit runtime [verified]"
    $probe = Join-Path $fixtureRepo 'probe.ps1'
    Set-Content -LiteralPath $probe -Value @'
$ErrorActionPreference = 'Stop'
try {
    . (Join-Path $PSScriptRoot 'android/helpers/bounded_extraction.ps1')
    Resolve-BoundedPythonRuntime | Out-Null
} catch { Write-Host $_; exit 1 }
'@
    $pwshPath = (Get-Process -Id $PID).Path
    & $pwshPath -NoProfile -File $probe
    if ($LASTEXITCODE -ne 0) { throw 'Copied repository runtime was not admitted' }
    $tampered = Join-Path $explicitRoot 'unexpected-module.py'
    Set-Content -LiteralPath $tampered -Value 'raise RuntimeError("unexpected")'
    $rejection = @(& $pwshPath -NoProfile -File $probe) -join "`n"
    if ($LASTEXITCODE -eq 0 -or $rejection -notmatch 'tree SHA-256 mismatch') { throw 'Changed repository runtime tree was accepted' }
    Remove-Item -LiteralPath $tampered
    Write-Host 'PASS: changed repository runtime tree is rejected before execution'

    foreach ($case in @(
            [pscustomobject]@{ Name = 'missing runtime'; Path = (Join-Path $testRoot 'missing python.exe'); Hash = $explicitHash },
            [pscustomobject]@{ Name = 'missing digest'; Path = $explicitPath; Hash = '' },
            [pscustomobject]@{ Name = 'damaged runtime'; Path = $explicitPath; Hash = ('0' * 64) }
        )) {
        $rejected = $false
        try {
            Resolve-BoundedPythonRuntime -RuntimePath $case.Path -ExpectedSha256 $case.Hash | Out-Null
        } catch {
            $rejected = $true
        }
        if (-not $rejected) { throw "Runtime policy accepted $($case.Name)" }
        Write-Host "PASS: rejected $($case.Name)"
    }

    $wrongVersionPath = Join-Path $testRoot $(if ($isWindowsHost) { 'wrong version.cmd' } else { 'wrong version' })
    if ($isWindowsHost) {
        Set-Content -LiteralPath $wrongVersionPath -Value '@echo {"executable":"C:/wrong/python.exe","version":"3.11.0"}' -NoNewline
    } else {
        Set-Content -LiteralPath $wrongVersionPath -Value "#!/bin/sh`nprintf '%s\n' '{`"executable`":`"/wrong/python`",`"version`":`"3.11.0`"}'" -NoNewline
        & chmod +x $wrongVersionPath
        if ($LASTEXITCODE -ne 0) { throw 'Could not prepare wrong-version fixture' }
    }
    $wrongVersionHash = (Get-FileHash -LiteralPath $wrongVersionPath -Algorithm SHA256).Hash
    $wrongVersionRejected = $false
    try {
        Resolve-BoundedPythonRuntime -RuntimePath $wrongVersionPath `
            -ExpectedSha256 $wrongVersionHash | Out-Null
    } catch {
        $wrongVersionRejected = $true
    }
    if (-not $wrongVersionRejected) { throw 'Runtime policy accepted the wrong Python version' }
    Write-Host 'PASS: rejected wrong Python version'

    Set-Item Function:Get-HostPlatform -Value { 'UnsupportedTest' }
    $nonWindowsRejected = $false
    try {
        Resolve-BoundedPythonRuntime | Out-Null
    } catch {
        $nonWindowsRejected = $true
    }
    if (-not $nonWindowsRejected) {
        throw 'Unsupported host accepted an implicit runtime'
    }
    $env:DXX_BOUNDED_PYTHON_RUNTIME = $explicitPath
    $env:DXX_BOUNDED_PYTHON_SHA256 = $explicitHash
    $nonWindowsExplicit = Resolve-BoundedPythonRuntime
    if ($nonWindowsExplicit.Provenance -cne 'explicit-sha256') {
        throw 'Unsupported-host explicit runtime admission failed'
    }
    Set-Item Function:Get-HostPlatform -Value $originalHostPlatformFunction
    Write-Host 'PASS: unsupported-host policy requires explicit admission'

    $outputRoot = Join-Path $testRoot 'bounded output [space]'
    New-Item -ItemType Directory -Path $outputRoot | Out-Null
    $childCode = "import pathlib,sys; pathlib.Path(sys.argv[1], 'quoted ok').write_text('ok', encoding='ascii')"
    $result = Invoke-BoundedExtractor -OutputDirectory $outputRoot -FilePath $explicitPath `
        -ArgumentList @('-I', '-B', '-c', $childCode, $outputRoot) `
        -PythonRuntimePath $explicitPath -PythonRuntimeSha256 $explicitHash
    if ($result.ExitCode -ne 0 -or
        (Get-Content -LiteralPath (Join-Path $outputRoot 'quoted ok') -Raw) -cne 'ok') {
        throw "Bounded extraction through a spaced runtime path failed: $($result.Output -join [Environment]::NewLine)"
    }
    Write-Host 'PASS: bounded extraction preserves spaced runtime and output arguments'

    $env:DXX_BOUNDED_PYTHON_RUNTIME = $null
    $env:DXX_BOUNDED_PYTHON_SHA256 = $null
    Resolve-BoundedPythonRuntime | Out-Null
    if (Test-Path -LiteralPath $sentinel) { throw 'Hostile PATH Python was invoked during extraction' }
    Write-Host 'PASS: repository runtime tree remains admitted after probing and extraction'
    & $repositoryRuntime.Path -I -B (Join-Path $PSScriptRoot 'test_run_bounded_extractor.py')
    if ($LASTEXITCODE -ne 0) { throw 'Bounded extractor supervisor regression suite failed' }
    Resolve-BoundedPythonRuntime | Out-Null
    Write-Host 'bounded Python runtime tests passed'
} finally {
    $env:Path = $oldPath
    Set-Item Function:Get-HostPlatform -Value $originalHostPlatformFunction
    $env:DXX_BOUNDED_PYTHON_RUNTIME = $oldRuntime
    $env:DXX_BOUNDED_PYTHON_SHA256 = $oldRuntimeHash
    if ($fixtureLock) { $fixtureLock.Dispose() }
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}

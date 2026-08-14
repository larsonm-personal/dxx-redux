$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android\helpers\bounded_extraction.ps1')

$testRoot = Join-Path $repoRoot "android\temp\bounded python $([Guid]::NewGuid().ToString('N'))"
$oldPath = $env:Path
$oldOs = $env:OS
$oldRuntime = $env:DXX_BOUNDED_PYTHON_RUNTIME
$oldRuntimeHash = $env:DXX_BOUNDED_PYTHON_SHA256
New-Item -ItemType Directory -Path $testRoot | Out-Null
try {
    $hostileRoot = Join-Path $testRoot 'hostile path'
    New-Item -ItemType Directory -Path $hostileRoot | Out-Null
    $sentinel = Join-Path $testRoot 'path-runtime-invoked.txt'
    Set-Content -LiteralPath (Join-Path $hostileRoot 'python.cmd') `
        -Value "@echo invoked>`"$sentinel`"`r`n@exit /b 77" -NoNewline
    $env:Path = "$hostileRoot$([IO.Path]::PathSeparator)$oldPath"

    $repositoryRuntime = Resolve-BoundedPythonRuntime
    if ($repositoryRuntime.Provenance -cne 'repository-pinned-tree' -or
        $repositoryRuntime.Version -cne '3.12.8') {
        throw 'Repository runtime did not report pinned provenance and version'
    }
    if (Test-Path -LiteralPath $sentinel) {
        throw 'Hostile PATH Python was invoked'
    }
    Write-Host 'PASS: repository runtime ignores hostile PATH precedence'

    $explicitRoot = Join-Path $testRoot 'explicit runtime [verified]'
    Copy-Item -LiteralPath (Split-Path $repositoryRuntime.Path) -Destination $explicitRoot -Recurse
    $explicitPath = Join-Path $explicitRoot 'python.exe'
    $explicitHash = (Get-FileHash -LiteralPath $explicitPath -Algorithm SHA256).Hash
    $explicitRuntime = Resolve-BoundedPythonRuntime -RuntimePath $explicitPath `
        -ExpectedSha256 $explicitHash
    if ($explicitRuntime.Provenance -cne 'explicit-sha256' -or
        $explicitRuntime.Path -cne (Resolve-Path -LiteralPath $explicitPath).Path) {
        throw 'Explicit runtime identity was not preserved'
    }
    Write-Host 'PASS: explicit runtime with spaces is admitted by SHA-256'

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

    $wrongVersionPath = Join-Path $testRoot 'wrong version.cmd'
    Set-Content -LiteralPath $wrongVersionPath -Value '@echo {"executable":"C:/wrong/python.exe","version":"3.11.0"}' -NoNewline
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

    $env:OS = 'NonWindowsTest'
    $nonWindowsRejected = $false
    try {
        Resolve-BoundedPythonRuntime | Out-Null
    } catch {
        $nonWindowsRejected = $true
    }
    if (-not $nonWindowsRejected) {
        throw 'Non-Windows resolution accepted an implicit runtime'
    }
    $env:DXX_BOUNDED_PYTHON_RUNTIME = $explicitPath
    $env:DXX_BOUNDED_PYTHON_SHA256 = $explicitHash
    $nonWindowsExplicit = Resolve-BoundedPythonRuntime
    if ($nonWindowsExplicit.Provenance -cne 'explicit-sha256') {
        throw 'Non-Windows explicit runtime admission failed'
    }
    $env:OS = $oldOs
    Write-Host 'PASS: non-Windows policy requires explicit admission'

    $outputRoot = Join-Path $testRoot 'bounded output [space]'
    New-Item -ItemType Directory -Path $outputRoot | Out-Null
    $childCode = 'import pathlib,sys; pathlib.Path(sys.argv[1], "quoted ok").write_text("ok", encoding="ascii")'
    $result = Invoke-BoundedExtractor -OutputDirectory $outputRoot -FilePath $explicitPath `
        -ArgumentList @('-I', '-c', $childCode, $outputRoot) `
        -PythonRuntimePath $explicitPath -PythonRuntimeSha256 $explicitHash
    if ($result.ExitCode -ne 0 -or
        (Get-Content -LiteralPath (Join-Path $outputRoot 'quoted ok') -Raw) -cne 'ok') {
        throw "Bounded extraction through a spaced runtime path failed: $($result.Output -join [Environment]::NewLine)"
    }
    Write-Host 'PASS: bounded extraction preserves spaced runtime and output arguments'

    Write-Host 'bounded Python runtime tests passed'
} finally {
    $env:Path = $oldPath
    $env:OS = $oldOs
    $env:DXX_BOUNDED_PYTHON_RUNTIME = $oldRuntime
    $env:DXX_BOUNDED_PYTHON_SHA256 = $oldRuntimeHash
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}

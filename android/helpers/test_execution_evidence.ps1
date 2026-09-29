# Bounded latest observations; logs and owner-specific case reports remain separate
. (Join-Path $PSScriptRoot 'atomic_text_file.ps1')
. (Join-Path $PSScriptRoot 'powershell_compat.ps1')

function New-TestExecutionEvidenceContext {
    param([string]$RepositoryRoot, [string]$ReportDir, [string]$ReportPath)
    $os = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { 'windows' } elseif ([Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Linux)) { 'linux' } else { 'other' }
    if ($os -eq 'windows') {
        # RuntimeInformation properties can be unavailable in Windows PowerShell
        # Prefer the native OS architecture even when running a 32-bit shell
        $nativeArchitecture = if ($env:PROCESSOR_ARCHITEW6432) { $env:PROCESSOR_ARCHITEW6432 } else { $env:PROCESSOR_ARCHITECTURE }
        $architecture = switch ($nativeArchitecture) {
            'AMD64' { 'x64' }
            'ARM64' { 'arm64' }
            'ARM' { 'arm' }
            'x86' { 'x86' }
            default { if ([Environment]::Is64BitOperatingSystem) { 'x64' } else { 'x86' } }
        }
        $osDescription = [Environment]::OSVersion.VersionString
    } else {
        $architecture = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString().ToLowerInvariant()
        $osDescription = [Runtime.InteropServices.RuntimeInformation]::OSDescription
    }
    $commit = (& git -C $RepositoryRoot rev-parse HEAD 2>$null | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Cannot record test evidence without repository identity' }
    $changes = @(& git -C $RepositoryRoot status --porcelain --untracked-files=normal 2>$null)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect test evidence worktree state' }
    return @{
        Path = Join-Path $ReportDir "execution_evidence_${os}_${architecture}.json"
        RunId = [guid]::NewGuid().ToString('N')
        RepositoryRoot = $RepositoryRoot
        ReportPath = [IO.Path]::GetFullPath($ReportPath)
        HostKey = "${os}_${architecture}"
        Commit = $commit
        Dirty = $changes.Count -gt 0
        Runtime = "$osDescription; PowerShell $($PSVersionTable.PSVersion)"
    }
}

function New-TestExecutionObservation {
    param([hashtable]$Context, [hashtable]$Test, [hashtable]$Result, [string]$StartedUtc, [string]$SourceSha256)
    $status = if ($Result) { [string]$Result.Status } else { 'RUNNING' }
    if (-not $Result) { $Result = @{} }
    $arguments = @($Test.Arguments) | ConvertTo-Json -Compress
    if ($arguments.Length -gt 4096) { throw 'Test arguments exceed evidence limit' }
    $reason = [string]$Result.Reason
    if ($reason.Length -gt 2048) { $reason = $reason.Substring(0, 2048) }
    return [ordered]@{
        name = $Test.Name
        type = $Test.Type
        requires = $Test.Requires
        status = $status
        started_utc = $StartedUtc
        observed_utc = [DateTime]::UtcNow.ToString('o')
        run_id = $Context.RunId
        commit = $Context.Commit
        dirty = $Context.Dirty
        runtime = $Context.Runtime
        source = (Get-CompatibleRelativePath -BasePath $Context.RepositoryRoot -TargetPath $Test.Path).Replace('\', '/')
        source_sha256 = $SourceSha256
        arguments = $arguments
        exit_code = if ($Result.ContainsKey('ExitCode')) { $Result.ExitCode } else { $null }
        elapsed = [string]$Result.Elapsed
        reason = $reason
        log = [string]$Result.LogFile
        report = $Context.ReportPath
    }
}

function Assert-TestExecutionObservation {
    param($Observation)
    if (-not $Observation.name -or ([string]$Observation.name).Length -gt 512 -or $Observation.status -notin @('RUNNING', 'PASS', 'FAIL', 'TIMEOUT', 'SKIP', 'NOT_RUN')) { throw 'Invalid test observation' }
    # ConvertFrom-Json may turn ISO strings into DateTime on newer PowerShell
    foreach ($field in @('started_utc', 'observed_utc')) {
        if (-not $Observation.$field) { throw "Missing observation timestamp: $field" }
        $Observation.$field = ([DateTimeOffset]$Observation.$field).UtcDateTime.ToString('o')
    }
}

function Write-TestExecutionEvidence {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$HostKey,
        [Parameter(Mandatory)][object[]]$Observations,
        [ValidateRange(1, 1024)][int]$MaxEntries = 1024,
        [ValidateRange(1024, 4194304)][int]$MaxBytes = 4194304
    )
    $fullPath = [IO.Path]::GetFullPath($Path)
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($fullPath)) -Force | Out-Null
    $lock = $null
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    try {
        while (-not $lock) {
            try { $lock = [IO.File]::Open("$fullPath.lock", [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
            catch [IO.IOException] {
                if ([DateTime]::UtcNow -ge $deadline) { throw 'Timed out waiting for test evidence writer' }
                Start-Sleep -Milliseconds 50
            }
        }
        $entries = @{}
        $evicted = 0
        if (Test-Path -LiteralPath $fullPath) {
            if ((Get-Item -LiteralPath $fullPath).Length -gt 4194304) { throw 'Existing test evidence exceeds its size limit' }
            $previous = Get-Content -LiteralPath $fullPath -Raw | ConvertFrom-Json
            if ($previous.schema -ne 1 -or $previous.host_key -cne $HostKey -or -not $previous.PSObject.Properties['observations']) { throw 'Invalid test evidence schema or host' }
            $evicted = [long]$previous.evicted_observations
            foreach ($item in $previous.observations) {
                Assert-TestExecutionObservation -Observation $item
                if ($entries.ContainsKey([string]$item.name)) { throw 'Duplicate test evidence entries' }
                $entries[[string]$item.name] = $item
            }
        }
        foreach ($item in $Observations) {
            Assert-TestExecutionObservation -Observation $item
            $prior = $entries[[string]$item.name]
            # A late completion from an older invocation must not hide a newer attempt
            if ($prior -and ([string]::CompareOrdinal($prior.started_utc, $item.started_utc) -gt 0 -or
                    ($prior.started_utc -ceq $item.started_utc -and [string]::CompareOrdinal($prior.observed_utc, $item.observed_utc) -gt 0))) { continue }
            $entries[[string]$item.name] = $item
        }
        $ordered = @($entries.Values | ForEach-Object { [pscustomobject]$_ } | Sort-Object -Property started_utc, observed_utc, name -Descending)
        if ($ordered.Count -gt $MaxEntries) {
            $evicted += $ordered.Count - $MaxEntries
            $ordered = @($ordered | Select-Object -First $MaxEntries)
        }
        do {
            $document = [ordered]@{ schema = 1; host_key = $HostKey; evicted_observations = $evicted; observations = $ordered }
            $json = ($document | ConvertTo-Json -Depth 8) + "`n"
            if ([Text.Encoding]::UTF8.GetByteCount($json) -le $MaxBytes) { break }
            if ($ordered.Count -le 1) { throw 'One test observation exceeds the evidence size limit' }
            $ordered = @($ordered | Select-Object -First ($ordered.Count - 1))
            $evicted++
        } while ($true)
        Write-Utf8NoBomTextAtomically -Path $fullPath -Text $json
        # Reap publication leftovers from killed writers while holding their lock
        $orphanPattern = '^\.' + [regex]::Escape([IO.Path]::GetFileName($fullPath)) + '\.[0-9a-f]{32}\.(tmp|bak)$'
        foreach ($file in Get-ChildItem -LiteralPath ([IO.Path]::GetDirectoryName($fullPath)) -File -Force) {
            if ($file.Name -cmatch $orphanPattern) { Remove-Item -LiteralPath $file.FullName -Force }
        }
    } finally {
        if ($lock) { $lock.Dispose() }
    }
}

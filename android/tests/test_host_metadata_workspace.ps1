#!/usr/bin/env pwsh
param([switch]$Integration)

$ErrorActionPreference = 'Stop'
$androidRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $androidRoot 'helpers/host_metadata_workspace.ps1')
. (Join-Path $androidRoot 'helpers/atomic_text_file.ps1')
$root = Join-Path $androidRoot ('temp/metadata-workspace-test-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $root | Out-Null
    # Exercise atomic publication while a Windows reader temporarily denies deletion
    if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
        Add-Type -TypeDefinition @'
public static class AtomicPublicationReader {
    public static async System.Threading.Tasks.Task ReleaseLater(System.IDisposable reader) {
        await System.Threading.Tasks.Task.Delay(250);
        reader.Dispose();
    }
}
'@
        $publication = Join-Path $root 'publication.json'
        [IO.File]::WriteAllText($publication, 'old')
        $reader = [IO.File]::Open($publication, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        $release = [AtomicPublicationReader]::ReleaseLater($reader)
        try { Write-Utf8NoBomTextAtomically -Path $publication -Text 'new' }
        finally { $release.GetAwaiter().GetResult() | Out-Null; $reader.Dispose() }
        if ([IO.File]::ReadAllText($publication) -ne 'new') { throw 'Transient reader prevented atomic publication' }
        $reader = [IO.File]::Open($publication, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        $rejected = $false
        try {
            try { Write-Utf8NoBomTextAtomically -Path $publication -Text 'must not publish' }
            catch { $rejected = $true }
        } finally { $reader.Dispose() }
        if (-not $rejected -or [IO.File]::ReadAllText($publication) -ne 'new') {
            throw 'Persistent reader did not preserve the previous publication'
        }
        if (@(Get-ChildItem -LiteralPath $root -Force | Where-Object Name -Match '\.(tmp|bak)$').Count) {
            throw 'Atomic publication left scratch files behind'
        }
    }
    foreach ($path in @('raw/mission/mission.hog', 'stages/mission/mission.hog',
            'raw/mission.metadata.json', 'metadata/mission.json', 'logs/mission.log', 'summary.json')) {
        $full = Join-Path $root $path
        New-Item -ItemType Directory -Path (Split-Path -Parent $full) -Force | Out-Null
        [IO.File]::WriteAllText($full, 'preserve diagnostics, discard payloads')
    }
    Remove-HostMetadataPayloads -RunRoot $root -Paths @((Join-Path $root 'raw/mission'), (Join-Path $root 'stages/mission'))
    foreach ($path in @('raw/mission', 'stages/mission')) {
        if (Test-Path -LiteralPath (Join-Path $root $path)) { throw "Payload survived: $path" }
    }
    foreach ($path in @('raw/mission.metadata.json', 'metadata/mission.json', 'logs/mission.log', 'summary.json')) {
        if (-not (Test-Path -LiteralPath (Join-Path $root $path))) { throw "Diagnostic lost: $path" }
    }

    # A crashed worker may leave partially extracted files and no summary
    $worker = Join-Path $root 'workers/0001'
    New-Item -ItemType Directory -Path (Join-Path $worker 'raw/partial'), (Join-Path $worker 'stages/partial') -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $worker 'raw/partial/data.hog'), 'partial')
    Remove-HostMetadataPayloads -RunRoot $worker
    if (Test-Path -LiteralPath (Join-Path $worker 'raw/partial')) { throw 'Crashed worker payload survived' }
    Remove-HostMetadataPayloads -RunRoot $worker

    foreach ($path in @($root, (Join-Path $root 'raw'), (Join-Path $root 'metadata'), (Join-Path $root 'raw/../logs'))) {
        $rejected = $false
        try { Remove-HostMetadataPayloads -RunRoot $root -Paths @($path) } catch { $rejected = $true }
        if (-not $rejected) { throw "Unsafe cleanup accepted: $path" }
    }

    # Successful route cases keep evidence but release their copied engines and assets
    $engine = Join-Path $root 'engine'
    New-Item -ItemType Directory -Path $engine -Force | Out-Null
    foreach ($name in @('worker.exe', 'runtime.dll', 'worker', 'runtime.so.1', 'changed-worker', 'settings.json')) {
        [IO.File]::WriteAllText((Join-Path $engine $name), 'fixture')
    }
    $manifest = @('worker.exe', 'runtime.dll', 'worker', 'runtime.so.1', 'changed-worker' | ForEach-Object {
            @{ name = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $engine $_) -Algorithm SHA256).Hash }
        })
    [IO.File]::WriteAllText((Join-Path $engine 'files.json'), (ConvertTo-Json -InputObject $manifest))
    [IO.File]::WriteAllText((Join-Path $engine 'changed-worker'), 'preserve a modified snapshot')
    Remove-GuidebotTestPayloads -RepoRoot (Split-Path $androidRoot) -OutputText "[12:00:00] Output: $root"
    foreach ($name in @('worker.exe', 'runtime.dll', 'worker', 'runtime.so.1')) {
        if (Test-Path -LiteralPath (Join-Path $engine $name)) { throw "Route executable copy survived: $name" }
    }
    foreach ($path in @('engine/files.json', 'engine/settings.json', 'engine/changed-worker', 'logs/mission.log', 'summary.json')) {
        if (-not (Test-Path -LiteralPath (Join-Path $root $path))) { throw "Route evidence lost: $path" }
    }
    foreach ($unsafeName in @('../summary.json', '..\summary.json', 'files.json', 'settings.json')) {
        $invalidManifest = @(@{ name = $unsafeName; sha256 = ('a' * 64) })
        [IO.File]::WriteAllText((Join-Path $engine 'files.json'), (ConvertTo-Json -InputObject $invalidManifest))
        $rejected = $false
        try {
            Remove-GuidebotTestPayloads -RepoRoot (Split-Path $androidRoot) -OutputText "[12:00:00] Output: $root"
        } catch { $rejected = $true }
        if (-not $rejected) { throw "Unsafe engine manifest accepted: $unsafeName" }
    }
    [IO.File]::WriteAllText((Join-Path $engine 'files.json'), (ConvertTo-Json -InputObject $manifest))
    $rejected = $false
    try {
        Remove-GuidebotTestPayloads -RepoRoot (Split-Path $androidRoot) -OutputText "[12:00:00] Output: $PSScriptRoot"
    } catch { $rejected = $true }
    if (-not $rejected) { throw 'Route cleanup accepted a source directory' }

    # Inject the failure reported by the user without filling the real disk
    function Write-Utf8NoBomTextAtomically {
        param($Path, $Text)
        throw [IO.IOException]::new('There is not enough space on the disk')
    }
    Write-HostMetadataWorkerLog -Path (Join-Path $worker 'worker.log') -Text 'original extraction failure' -WarningVariable warningText
    if (-not ($warningText -match 'not enough space')) { throw 'Log failure was not reported' }

    # Verify cleanup precedes disk writes in the actual generator paths
    $scriptText = Get-Content (Join-Path $androidRoot 'helpers/regenerate_all_mission_metadata_host.ps1') -Raw
    if ($scriptText -notmatch 'Remove-HostMetadataPayloads -RunRoot \$task.WorkerRoot[\s\S]*?Write-HostMetadataWorkerLog') {
        throw 'Parent must release crashed worker payloads before writing its log'
    }
    Write-Output 'PASS: payload cleanup preserves diagnostics, rejects unsafe paths, and tolerates disk-full worker logs'

    if ($Integration) {
        $repoRoot = Split-Path -Parent $androidRoot
        $broken = Join-Path $root 'broken.zip'
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $zip = [IO.Compression.ZipFile]::Open($broken, [IO.Compression.ZipArchiveMode]::Create)
        try {
            # Duplicate names force extraction to fail after writing the first payload
            foreach ($index in 1..2) {
                $stream = $zip.CreateEntry('partial.hog').Open()
                try { $stream.WriteByte($index) } finally { $stream.Dispose() }
            }
        } finally { $zip.Dispose() }
        # Run the generator's real failure/finally block after a partial extraction
        & {
            . (Join-Path $androidRoot 'helpers/mission_archive_variants.ps1')
            $ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $androidRoot 'helpers/regenerate_all_mission_metadata_host.ps1'), [ref]$null, [ref]$null)
            $handler = $ast.Find({ param($node)
                    $node -is [Management.Automation.Language.TryStatementAst] -and
                    $node.Finally -and $node.Finally.Extent.Text -match 'Paths @\(\$rawArchiveDir, \$stageDir\)'
                }, $true)
            $outDir = Join-Path $root 'failure'
            $rawArchiveDir = Join-Path $outDir 'raw/broken'
            $stageDir = Join-Path $outDir 'stages/broken'
            New-Item -ItemType Directory -Path $rawArchiveDir, $stageDir -Force | Out-Null
            $record = [ordered]@{ status = 'failed'; reason = 'disk full' }
            $runStopwatch = [Diagnostics.Stopwatch]::StartNew()
            $results = @()
            $index = 1
            $archives = @($broken)
            $archive = Get-Item -LiteralPath $broken
            $metadataPath = Join-Path $outDir 'failure.json'
            function Write-FailureJson {
                param($Path, $Reason)
                if ((Test-Path -LiteralPath $rawArchiveDir) -or (Test-Path -LiteralPath $stageDir)) {
                    throw 'Failure JSON was written before payload cleanup'
                }
                [IO.File]::WriteAllText($Path, $Reason)
            }
            function Write-SummaryRecord { param($Record) }
            function Write-Status { param($Message, $Color) }
            try { Expand-MissionZipContent -ArchivePath $broken -Destination $rawArchiveDir }
            catch {
                $record.reason = $_.Exception.Message
                if (-not (Test-Path -LiteralPath (Join-Path $rawArchiveDir 'partial.hog'))) { throw 'Fixture did not partially extract' }
            } finally {
                $body = $handler.Finally.Extent.Text
                . ([scriptblock]::Create($body.Substring(1, $body.Length - 2)))
            }
            if (-not (Test-Path -LiteralPath $metadataPath)) { throw 'Failed extraction produced no diagnostic' }
        }
        $output = Join-Path $root 'batch'
        $archives = @(
            (Join-Path $repoRoot 'game_data/mission_files/chromium.zip'),
            (Join-Path $repoRoot 'game_data/mission_files/d1secret.zip')
        )
        & (Join-Path $androidRoot 'helpers/regenerate_all_mission_metadata_host.ps1') `
            -NoBuild -NoRegressionCopy -ArchivePaths $archives -MaxParallel 2 -OutputRoot $output
        if ($LASTEXITCODE -ne 0) { throw 'Real parallel metadata batch failed' }
        $summary = @(Get-Content (Join-Path $output 'summary.json') -Raw | ConvertFrom-Json)
        if (@($summary | Where-Object status -eq passed).Count -ne 2 -or
            @($summary | Where-Object status -eq failed).Count -ne 0) { throw 'Unexpected integration batch results' }
        $payloadDirectories = @(Get-ChildItem (Join-Path $output 'workers') -Directory | ForEach-Object {
                foreach ($name in @('raw', 'stages')) {
                    Get-ChildItem -LiteralPath (Join-Path $_.FullName $name) -Directory
                }
            })
        if ($payloadDirectories.Count -ne 0) { throw 'Finished batch retained extracted or staged payload directories' }
        Write-Output 'PASS: real parallel batch preserves successful metadata and cleans successful/failed worker payloads'
    }

} finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}

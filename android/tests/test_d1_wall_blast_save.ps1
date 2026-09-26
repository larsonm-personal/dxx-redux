param(
    [Parameter(Mandatory)][string]$NativeExe,
    [Parameter(Mandatory)][string]$ImportedExe,
    [Parameter(Mandatory)][string]$D1Assets,
    [Parameter(Mandatory)][string]$D2Assets,
    [Parameter(Mandatory)][string]$Output,
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 120
)
$ErrorActionPreference = 'Stop'
$nativePath = (Resolve-Path -LiteralPath $NativeExe).Path
$importedPath = (Resolve-Path -LiteralPath $ImportedExe).Path
$d1Path = (Resolve-Path -LiteralPath $D1Assets).Path
$d2Path = (Resolve-Path -LiteralPath $D2Assets).Path
$outputPath = [IO.Path]::GetFullPath($Output)
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts @($outputPath)
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
Get-FileHash -LiteralPath $nativePath, $importedPath | ConvertTo-Json | Set-Content "$outputPath/binaries.json"
foreach ($case in @(@{Name = 'native'; Exe = $nativePath }, @{Name = 'imported'; Exe = $importedPath })) {
    $directory = Join-Path $outputPath $case.Name
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $traceArgs = @('--wall-blast-save-trace', $d1Path, $d2Path)
    if ($case.Name -eq 'imported') { $traceArgs += "$outputPath/native" }
    $quotedArgs = @($traceArgs | ForEach-Object { '"' + $_ + '"' })
    $testProcess = Start-Process -FilePath $case.Exe -ArgumentList $quotedArgs -WorkingDirectory $directory -WindowStyle Hidden -PassThru -RedirectStandardOutput "$directory/stdout.log" -RedirectStandardError "$directory/stderr.log"
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (!$testProcess.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -ge $deadline) {
            Stop-Process -Id $testProcess.Id
            $testProcess.WaitForExit()
            throw "$($case.Name) timed out; see $directory"
        }
    }
    Set-Content "$directory/exit.txt" $testProcess.ExitCode
    if ($testProcess.ExitCode -ne 0) { throw "$($case.Name) failed: $($testProcess.ExitCode), see $directory/stderr.log" }
}
$failures = @()
foreach ($name in @('native/wall-blast.json', 'imported/wall-blast.json', 'imported/wall-blast-d2.json')) {
    $rows = @(Get-Content -LiteralPath "$outputPath/$name" -Raw | ConvertFrom-Json)
    if (($rows.elapsed -join ',') -ne '0,8,24,25,31') { throw "Incomplete wall-blast cases: $name" }
    foreach ($row in $rows) {
        if ($row.differences.Count -or $row.endian_differences.Count) { $failures += "$name elapsed=$($row.elapsed) save/endian divergence" }
        if ($name -eq 'imported/wall-blast.json') {
            if (@($row.import_differences.PSObject.Properties).Count -ne 2) { throw 'Missing both native-checkpoint byte orders' }
            foreach ($entry in $row.import_differences.PSObject.Properties) {
                if ($entry.Value.Count) { $failures += "$name elapsed=$($row.elapsed) $($entry.Name) import divergence" }
            }
        }
    }
}
$native = @(Get-Content "$outputPath/native/wall-blast.json" -Raw | ConvertFrom-Json)
$imported = @(Get-Content "$outputPath/imported/wall-blast.json" -Raw | ConvertFrom-Json)
for ($i = 0; $i -lt $native.Count; ++$i) {
    if (($native[$i].uninterrupted | ConvertTo-Json -Depth 10 -Compress) -cne ($imported[$i].uninterrupted | ConvertTo-Json -Depth 10 -Compress)) {
        $failures += "Native/imported uninterrupted blast differs at elapsed=$($native[$i].elapsed)"
    }
}
[ordered]@{phases = $native.elapsed; failures = $failures; nativeImportByteOrders = 2; ordinaryD2Control = $true } | ConvertTo-Json -Depth 4 | Set-Content "$outputPath/report.json"
if ($failures.Count) { throw "Wall-blast persistence failed; see $outputPath/report.json" }
Write-Output 'PASS: native, imported and ordinary D2 wall-blast saves, native checkpoint import and opposite-endian restores'

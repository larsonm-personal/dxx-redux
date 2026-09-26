param(
    [Parameter(Mandatory)][string]$NativeExe,
    [Parameter(Mandatory)][string]$ImportedExe,
    [Parameter(Mandatory)][string]$D1Assets,
    [Parameter(Mandatory)][string]$D2Assets,
    [Parameter(Mandatory)][string]$Output,
    [string]$D2Baseline,
    [ValidateSet('pickup', 'selection', 'profile')][string]$Mode = 'pickup',
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
    Push-Location $directory
    try {
        $traceArgs = @("--$Mode-autoselect-trace", $d1Path, $d2Path)
        if ($Mode -eq 'profile') {
            if ($case.Name -eq 'imported') { $traceArgs += "$outputPath/native/weapon-order.dximdemo" }
        }
        $quotedArgs = @($traceArgs | ForEach-Object { '"' + $_ + '"' })
        $testProcess = Start-Process -FilePath $case.Exe -ArgumentList $quotedArgs -WorkingDirectory $directory -WindowStyle Hidden -PassThru -RedirectStandardOutput "$directory/stdout.log" -RedirectStandardError "$directory/stderr.log"
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while (!$testProcess.WaitForExit(1000)) {
            if ([DateTime]::UtcNow -ge $deadline) {
                $testProcess.Refresh()
                [ordered]@{pid = $testProcess.Id; cpuSeconds = $testProcess.TotalProcessorTime.TotalSeconds; timeoutSeconds = $TimeoutSeconds } | ConvertTo-Json | Set-Content timeout.json
                Stop-Process -Id $testProcess.Id
                $testProcess.WaitForExit()
                throw "$($case.Name) timed out; see $directory"
            }
        }
        $code = $testProcess.ExitCode
        Set-Content run.log -Value ((Get-Content stdout.log, stderr.log) -join [Environment]::NewLine)
        Set-Content exit.txt $code
        if ($code -ne 0) { throw "$($case.Name) failed: $code, see $directory/run.log" }
    } finally { Pop-Location }
}
$native = @(Get-Content "$outputPath/native/rules.json" -Raw | ConvertFrom-Json)
$imported = @(Get-Content "$outputPath/imported/rules.json" -Raw | ConvertFrom-Json)
if ($native.Count -ne $imported.Count) { throw "$Mode case counts differ" }
$different = @()
for ($index = 0; $index -lt $native.Count; ++$index) {
    if (($native[$index] | ConvertTo-Json -Depth 20 -Compress) -cne ($imported[$index] | ConvertTo-Json -Depth 20 -Compress)) {
        $different += $index
    }
}
$d2Matches = if ($D2Baseline) {
    (Get-Content -LiteralPath $D2Baseline -Raw) -ceq (Get-Content "$outputPath/imported/rules-d2.json" -Raw)
} else { $null }
$report = [ordered]@{cases = $native.Count; mismatches = $different.Count; d2MatchesBaseline = $d2Matches }
if ($different.Count) {
    $report.firstExpected = $native[$different[0]]
    $report.firstActual = $imported[$different[0]]
}
$report | ConvertTo-Json -Depth 20 | Set-Content "$outputPath/report.json"
if ($different.Count -or $d2Matches -eq $false) { throw "$Mode parity failed; see $outputPath/report.json" }
Write-Output "PASS: $($native.Count) $Mode cases match native D1"

param(
    [Parameter(Mandatory)][string]$ImportedExe,
    [Parameter(Mandatory)][string]$Output,
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 120,
    [string]$D1Assets,
    [string]$D2Assets,
    [switch]$AssetIdentity,
    [string]$GameExe
)
$ErrorActionPreference = 'Stop'
$executable = (Resolve-Path -LiteralPath $ImportedExe).Path
$outputPath = [IO.Path]::GetFullPath($Output)
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts @($outputPath)
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
Get-FileHash -LiteralPath $executable | ConvertTo-Json | Set-Content "$outputPath/binary.json"
$testArgs = @('--classic-trigger-demo')
if ($AssetIdentity -and (!$D1Assets -or !$D2Assets)) { throw 'Asset identity tests require both D1Assets and D2Assets' }
if ($D1Assets -or $D2Assets) {
    if (!$D1Assets -or !$D2Assets) { throw 'Mission recording tests require both D1Assets and D2Assets' }
    $mode = if ($AssetIdentity) { '--classic-asset-missions' } else { '--classic-trigger-missions' }
    $testArgs = @($mode, (Resolve-Path -LiteralPath $D1Assets).Path, (Resolve-Path -LiteralPath $D2Assets).Path)
}
$quotedArgs = @($testArgs | ForEach-Object { '"' + $_ + '"' })
$testProcess = Start-Process -FilePath $executable -ArgumentList $quotedArgs -WorkingDirectory $outputPath -WindowStyle Hidden -PassThru -RedirectStandardOutput "$outputPath/stdout.log" -RedirectStandardError "$outputPath/stderr.log"
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
while (!$testProcess.WaitForExit(1000)) {
    if ([DateTime]::UtcNow -ge $deadline) {
        $testProcess.Refresh()
        [ordered]@{pid = $testProcess.Id; cpuSeconds = $testProcess.TotalProcessorTime.TotalSeconds; timeoutSeconds = $TimeoutSeconds } | ConvertTo-Json | Set-Content "$outputPath/timeout.json"
        Stop-Process -Id $testProcess.Id
        $testProcess.WaitForExit()
        throw "Classic trigger test timed out; see $outputPath"
    }
}
Set-Content "$outputPath/exit.txt" $testProcess.ExitCode
Get-Content "$outputPath/stdout.log", "$outputPath/stderr.log"
if ($testProcess.ExitCode -ne 0) { throw "Classic trigger test failed: $($testProcess.ExitCode), see $outputPath" }
if ($GameExe) {
    if (!$AssetIdentity) { throw 'GameExe requires AssetIdentity to produce the native console fixture' }
    $gameExecutable = (Resolve-Path -LiteralPath $GameExe).Path
    $cliDirectory = Join-Path $outputPath 'cli'
    New-Item -ItemType Directory -Path $cliDirectory -Force | Out-Null
    Copy-Item -LiteralPath $gameExecutable -Destination $cliDirectory
    Get-ChildItem -LiteralPath (Split-Path $gameExecutable -Parent) -Filter '*.dll' | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $cliDirectory
    }
    $manifest = Get-Content (Join-Path $outputPath 'classic-custom-cli.json') -Raw | ConvertFrom-Json
    $customPath = Join-Path $cliDirectory ([IO.Path]::GetFileName($manifest.custom_name))
    Copy-Item -LiteralPath (Join-Path $outputPath $manifest.source) -Destination $customPath
    $source = Join-Path $outputPath $manifest.demo
    $sourceHash = (Get-FileHash -LiteralPath $source).Hash
    $cliExecutable = Join-Path $cliDirectory (Split-Path $gameExecutable -Leaf)
    Get-FileHash -LiteralPath $cliExecutable | ConvertTo-Json | Set-Content "$cliDirectory/binary.json"
    foreach ($scenario in @('same', 'missing')) {
        if ($scenario -eq 'missing') { Remove-Item -LiteralPath $customPath }
        $resultPath = Join-Path $cliDirectory "$scenario.jsonl"
        Set-Content -LiteralPath $resultPath -Value 'keep'
        $cliArgs = @('-d1', '-sound11k', '-hogdir', (Resolve-Path -LiteralPath $D1Assets).Path, '-nomovies', '-classicdemo-dump-json', $source, $resultPath)
        $quotedCliArgs = @($cliArgs | ForEach-Object { '"' + $_ + '"' })
        $cliProcess = Start-Process -FilePath $cliExecutable -ArgumentList $quotedCliArgs -WorkingDirectory $cliDirectory -WindowStyle Hidden -PassThru -RedirectStandardOutput "$cliDirectory/$scenario.stdout.log" -RedirectStandardError "$cliDirectory/$scenario.stderr.log"
        if (!$cliProcess.WaitForExit(30000)) {
            Stop-Process -Id $cliProcess.Id
            $cliProcess.WaitForExit()
            throw "Native console export timed out: $scenario, see $cliDirectory"
        }
        Set-Content "$cliDirectory/$scenario.exit.txt" $cliProcess.ExitCode
        if ($scenario -eq 'same') {
            if ($cliProcess.ExitCode -ne 0) { throw "Native console export failed: $($cliProcess.ExitCode), see $cliDirectory" }
            $records = @(Get-Content -LiteralPath $resultPath | ForEach-Object { $_ | ConvertFrom-Json })
            if ($records[0].game_type -ne 4 -or $records[-1].type -ne 'result') { throw 'Native console export is incomplete' }
        } elseif ($cliProcess.ExitCode -ne 1 -or (Get-Content -LiteralPath $resultPath -Raw).Trim() -ne 'keep') {
            throw 'Missing custom definitions did not preserve console export output'
        }
        if ((Get-FileHash -LiteralPath $source).Hash -cne $sourceHash) { throw 'Console export changed its input' }
    }
    Write-Output 'PASS: actual native console export and source rejection'
}
Write-Output "PASS: $($testArgs[0])"

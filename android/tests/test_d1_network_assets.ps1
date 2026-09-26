param(
    [Parameter(Mandatory)][string]$ImportedExe,
    [Parameter(Mandatory)][string]$D1Assets,
    [Parameter(Mandatory)][string]$D2Assets,
    [Parameter(Mandatory)][string]$Output,
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 120
)
$ErrorActionPreference = 'Stop'
$executable = (Resolve-Path -LiteralPath $ImportedExe).Path
$outputPath = [IO.Path]::GetFullPath($Output)
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts @($outputPath)
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
Get-FileHash -LiteralPath $executable | ConvertTo-Json | Set-Content "$outputPath/binary.json"
$testArguments = @('--network-asset-missions', (Resolve-Path -LiteralPath $D1Assets).Path, (Resolve-Path -LiteralPath $D2Assets).Path)
$quotedArguments = @($testArguments | ForEach-Object { '"' + $_ + '"' })
$testProcess = Start-Process -FilePath $executable -ArgumentList $quotedArguments -WorkingDirectory $outputPath -WindowStyle Hidden -PassThru -RedirectStandardOutput "$outputPath/stdout.log" -RedirectStandardError "$outputPath/stderr.log"
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
while (!$testProcess.WaitForExit(1000)) {
    if ([DateTime]::UtcNow -ge $deadline) {
        Stop-Process -Id $testProcess.Id
        $testProcess.WaitForExit()
        throw "Network asset test timed out; see $outputPath"
    }
}
Set-Content "$outputPath/exit.txt" $testProcess.ExitCode
Get-Content "$outputPath/stdout.log", "$outputPath/stderr.log"
if ($testProcess.ExitCode -ne 0) { throw "Network asset test failed: $($testProcess.ExitCode), see $outputPath" }
Write-Output 'PASS: D1 network asset admission'

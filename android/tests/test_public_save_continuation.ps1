param(
    [Parameter(Mandatory)][string]$D1Exe,
    [Parameter(Mandatory)][string]$D2Exe,
    [string]$D1Assets,
    [string]$D2Assets,
    [string]$Output,
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $repo 'android/helpers/shareware_test_data.ps1')
. (Join-Path $repo 'android/helpers/test_host_platform.ps1')
$d1ExePath = (Resolve-Path -LiteralPath $D1Exe).Path
$d2ExePath = (Resolve-Path -LiteralPath $D2Exe).Path
$d1Path = Resolve-SharewareTestData -RepoRoot $repo -Game d1 -DataDir $D1Assets
$d2Path = Resolve-SharewareTestData -RepoRoot $repo -Game d2 -DataDir $D2Assets
if (-not $Output) { $Output = Join-Path $repo 'temp/public-save-continuation' }
$outputRoot = [IO.Path]::GetFullPath($Output)
foreach ($source in @($d1Path, $d2Path)) {
    $prefix = $source.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    if ($outputRoot.Equals($prefix, [StringComparison]::OrdinalIgnoreCase) -or
        $outputRoot.StartsWith($prefix + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Save test output must be outside the supplied game data directories'
    }
}
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts @($outputRoot)
# Never admit stale reports, saved games or loose mission overrides from an earlier run
$outputPath = Join-Path $outputRoot ('run-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
Get-FileHash -LiteralPath $d1ExePath, $d2ExePath | ConvertTo-Json | Set-Content "$outputPath/binaries.json"
$identities = @()
foreach ($game in @('d1', 'd2')) {
    $config = Get-SharewareTestGameConfig -RepoRoot $repo -Game $game
    $source = if ($game -eq 'd1') { $d1Path } else { $d2Path }
    $staged = Join-Path $outputPath "assets/$game"
    New-Item -ItemType Directory -Path $staged -Force | Out-Null
    foreach ($file in $config.RequiredFiles) {
        $original = @(Get-ChildItem -LiteralPath $source -File | Where-Object Name -IEQ $file)
        Copy-Item -LiteralPath $original[0].FullName -Destination (Join-Path $staged $file)
    }
    Assert-SharewareTestDataDirectory -Config $config -DataDir $staged | Out-Null
    $identities += @{ game = $game; archive = $config.Archive; archiveSha256 = $config.ArchiveSha256; files = $config.RequiredHashes }
    if ($game -eq 'd1') { $d1Path = $staged } else { $d2Path = $staged }
}
$identities | ConvertTo-Json -Depth 6 | Set-Content "$outputPath/assets.json"

foreach ($case in @(@{Name = 'd1-flight'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'flight' }, @{Name = 'd2-flight'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'flight' }, @{Name = 'd1-door'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'door' })) {
    $directory = Join-Path $outputPath $case.Name
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $traceArgs = @('--public-save-continuation', $case.Assets, $case.Scenario)
    $quotedArgs = @($traceArgs | ForEach-Object { '"' + $_ + '"' })
    $parameters = @{
        FilePath = $case.Exe; ArgumentList = $quotedArgs; WorkingDirectory = $directory; PassThru = $true
        RedirectStandardOutput = "$directory/stdout.log"; RedirectStandardError = "$directory/stderr.log"
    }
    if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { $parameters.WindowStyle = 'Hidden' }
    $testProcess = Start-Process @parameters
    try {
        if (-not $testProcess.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-Process -Id $testProcess.Id -Force
            $testProcess.WaitForExit()
            Set-Content "$directory/timeout.txt" "Exceeded $TimeoutSeconds seconds"
            throw "$($case.Name) timed out; see $directory"
        }
        Set-Content "$directory/exit.txt" $testProcess.ExitCode
        if ($testProcess.ExitCode -ne 0) { throw "$($case.Name) failed: $($testProcess.ExitCode), see $directory/stderr.log" }
    } finally {
        if (-not $testProcess.HasExited) { Stop-Process -Id $testProcess.Id -Force }
        $testProcess.Dispose()
    }
}
$python = Resolve-RegressionPythonCommand
if (-not $python) { throw 'Python 3 is required for save continuation verification' }
$prefix = @($python.PrefixArguments)
& $python.Path @prefix (Join-Path $PSScriptRoot 'public_save_continuation.py') --output $outputPath
if ($LASTEXITCODE -ne 0) { throw "Ordinary save persistence failed; see $outputPath/report.json" }
Write-Output "PASS: D1/D2 immediate ordinary-save persistence and selected flight continuation; full trajectories UNQUALIFIED; $outputPath/report.json"

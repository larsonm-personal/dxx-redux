param(
    [Parameter(Mandatory)][string]$NativeExe,
    [Parameter(Mandatory)][string]$ImportedExe,
    [string]$D1Assets,
    [string]$D2Assets,
    [string]$Output,
    [ValidateSet('shareware', 'retail')][string]$DataEdition = 'shareware',
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 120
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $repo 'android/helpers/shareware_test_data.ps1')
. (Join-Path $PSScriptRoot 'wall_blast_save_report.ps1')
$nativePath = (Resolve-Path -LiteralPath $NativeExe).Path
$importedPath = (Resolve-Path -LiteralPath $ImportedExe).Path
if ($DataEdition -eq 'shareware') {
    $d1Path = Resolve-SharewareTestData -RepoRoot $repo -Game d1 -DataDir $D1Assets
    $d2Path = Resolve-SharewareTestData -RepoRoot $repo -Game d2 -DataDir $D2Assets
} else {
    if (-not $D1Assets -or -not $D2Assets) { throw 'Retail wall-blast tests require -D1Assets and -D2Assets' }
    $d1Path = (Resolve-Path -LiteralPath $D1Assets).Path
    $d2Path = (Resolve-Path -LiteralPath $D2Assets).Path
}
if (-not $Output) { $Output = Join-Path $repo "temp/wall-blast-save-$DataEdition" }
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
Get-FileHash -LiteralPath $nativePath, $importedPath | ConvertTo-Json | Set-Content "$outputPath/binaries.json"
if ($DataEdition -eq 'shareware') {
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
}
foreach ($case in @(@{Name = 'native'; Exe = $nativePath }, @{Name = 'imported'; Exe = $importedPath })) {
    $directory = Join-Path $outputPath $case.Name
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $mode = if ($DataEdition -eq 'shareware') { '--wall-blast-save-shareware-trace' } else { '--wall-blast-save-trace' }
    $traceArgs = @($mode, $d1Path, $d2Path)
    if ($case.Name -eq 'imported') { $traceArgs += "$outputPath/native" }
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
Assert-WallBlastSaveReport -Directory $outputPath -DataEdition $DataEdition | Out-Null
Write-Output "PASS: native, imported and ordinary D2 wall-blast saves, both byte orders and 1260 atomic corruption checks; $outputPath/report.json"

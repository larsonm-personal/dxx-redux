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

$cases = @(
    @{Name = 'd1-flight'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'flight' }
    @{Name = 'd2-flight'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'flight' }
    @{Name = 'd1-door'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'door' }
    @{Name = 'd2-countdown'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'countdown'; Mode = '--active-save-runtime' }
    @{Name = 'd2-robot-death'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'robot-death'; Mode = '--active-save-runtime' }
    # Load each persisted guided identity in a separate process with the same checkpoint directory
    @{Name = 'd2-guided-active'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'active'; Mode = '--guided-save-runtime' }
    @{Name = 'd2-guided-released'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'released'; Mode = '--guided-save-runtime' }
    @{Name = 'd2-guided-multiple'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'multiple'; Mode = '--guided-save-runtime' }
    @{Name = 'd2-guided-stale'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'stale'; Mode = '--guided-save-runtime' }
    @{Name = 'd2-guided-parent'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'parent'; Mode = '--guided-save-runtime' }
    @{Name = 'd2-guided-none'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'none'; Mode = '--guided-save-runtime' }
    @{Name = 'd2-guided-format'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'format'; Mode = '--guided-save-runtime' }
    @{Name = 'd2-guided-load-active'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'load-active'; Mode = '--guided-save-runtime'; WorkDirectory = 'd2-guided-active' }
    @{Name = 'd2-guided-load-released'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'load-released'; Mode = '--guided-save-runtime'; WorkDirectory = 'd2-guided-released' }
    # Keep normal and no-wiggle controls separate; cold loads consume their own saved references
    @{Name = 'd1-wiggle-normal'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'normal'; Mode = '--wiggle-save-runtime' }
    @{Name = 'd1-wiggle-normal-load'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'load'; Mode = '--wiggle-save-runtime'; WorkDirectory = 'd1-wiggle-normal' }
    @{Name = 'd1-wiggle-no-wiggle'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'no-wiggle'; Mode = '--wiggle-save-runtime' }
    @{Name = 'd1-wiggle-no-wiggle-load'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'load'; Mode = '--wiggle-save-runtime'; WorkDirectory = 'd1-wiggle-no-wiggle' }
    @{Name = 'd2-wiggle-normal'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'normal'; Mode = '--wiggle-save-runtime' }
    @{Name = 'd2-wiggle-normal-load'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'load'; Mode = '--wiggle-save-runtime'; WorkDirectory = 'd2-wiggle-normal' }
    @{Name = 'd2-wiggle-no-wiggle'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'no-wiggle'; Mode = '--wiggle-save-runtime' }
    @{Name = 'd2-wiggle-no-wiggle-load'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'load'; Mode = '--wiggle-save-runtime'; WorkDirectory = 'd2-wiggle-no-wiggle' }
    @{Name = 'd2-wiggle-level-record'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'level-record'; Mode = '--wiggle-save-runtime' }
    @{Name = 'd2-wiggle-level-load'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'level-load'; Mode = '--wiggle-save-runtime'; WorkDirectory = 'd2-wiggle-level-record' }
    @{Name = 'd1-fusion-idle'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'idle'; Mode = '--fusion-save-runtime' }
    @{Name = 'd1-fusion-due'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'due'; Mode = '--fusion-save-runtime' }
    @{Name = 'd1-fusion-past'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'past'; Mode = '--fusion-save-runtime' }
    @{Name = 'd1-fusion-soon'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'soon'; Mode = '--fusion-save-runtime' }
    @{Name = 'd1-fusion-future'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'future'; Mode = '--fusion-save-runtime' }
    @{Name = 'd1-fusion-inactive-charged'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'inactive-charged'; Mode = '--fusion-save-runtime' }
    @{Name = 'd1-fusion-nonfusion'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'nonfusion'; Mode = '--fusion-save-runtime' }
    @{Name = 'd2-fusion-idle'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'idle'; Mode = '--fusion-save-runtime' }
    @{Name = 'd2-fusion-due'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'due'; Mode = '--fusion-save-runtime' }
    @{Name = 'd2-fusion-past'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'past'; Mode = '--fusion-save-runtime' }
    @{Name = 'd2-fusion-soon'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'soon'; Mode = '--fusion-save-runtime' }
    @{Name = 'd2-fusion-future'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'future'; Mode = '--fusion-save-runtime' }
    @{Name = 'd2-fusion-inactive-charged'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'inactive-charged'; Mode = '--fusion-save-runtime' }
    @{Name = 'd2-fusion-nonfusion'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'nonfusion'; Mode = '--fusion-save-runtime' }
    # The D2 consumer uses only public D1 assets and the native D1 producer checkpoints
    @{Name = 'd1-translation-record'; Exe = $d1ExePath; Assets = $d1Path; Scenario = '.'; Mode = '--d1-save-translation-runtime' }
    @{Name = 'd2-translation-load'; Exe = $d2ExePath; Assets = $d1Path; Scenario = (Join-Path $outputPath 'd1-translation-record'); Mode = '--d1-save-translation-runtime' }
    # Native epoch cold loads consume the checkpoint and reference from their producer process
    @{Name = 'd1-epoch-ordinary'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'ordinary'; Mode = '--save-epoch-runtime' }
    @{Name = 'd1-epoch-high'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'high'; Mode = '--save-epoch-runtime' }
    @{Name = 'd1-epoch-repeat'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'repeat'; Mode = '--save-epoch-runtime' }
    @{Name = 'd1-epoch-endian'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'endian'; Mode = '--save-epoch-runtime' }
    @{Name = 'd1-epoch-malformed'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'malformed'; Mode = '--save-epoch-runtime' }
    @{Name = 'd1-epoch-replay'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'replay'; Mode = '--save-epoch-runtime' }
    @{Name = 'd1-epoch-save'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'save'; Mode = '--save-epoch-runtime' }
    @{Name = 'd1-epoch-load'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'load'; Mode = '--save-epoch-runtime'; WorkDirectory = 'd1-epoch-save' }
    @{Name = 'd2-epoch-ordinary'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'ordinary'; Mode = '--save-epoch-runtime' }
    @{Name = 'd2-epoch-high'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'high'; Mode = '--save-epoch-runtime' }
    @{Name = 'd2-epoch-repeat'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'repeat'; Mode = '--save-epoch-runtime' }
    @{Name = 'd2-epoch-endian'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'endian'; Mode = '--save-epoch-runtime' }
    @{Name = 'd2-epoch-malformed'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'malformed'; Mode = '--save-epoch-runtime' }
    @{Name = 'd2-epoch-replay'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'replay'; Mode = '--save-epoch-runtime' }
    @{Name = 'd2-epoch-save'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'save'; Mode = '--save-epoch-runtime' }
    @{Name = 'd2-epoch-load'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'load'; Mode = '--save-epoch-runtime'; WorkDirectory = 'd2-epoch-save' }
    @{Name = 'd2-epoch-ai-phase'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'ai-phase'; Mode = '--save-epoch-runtime' }
    @{Name = 'd2-epoch-secret'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'secret'; Mode = '--save-epoch-runtime' }
    @{Name = 'd2-epoch-flash'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'flash'; Mode = '--save-epoch-runtime' }
    @{Name = 'd1-timer-cooldowns'; Exe = $d1ExePath; Assets = $d1Path; Scenario = 'cooldowns'; Mode = '--timer-save-runtime' }
    @{Name = 'd2-timer-pickup'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'pickup'; Mode = '--timer-save-runtime' }
    @{Name = 'd2-timer-cooldowns'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'cooldowns'; Mode = '--timer-save-runtime' }
    @{Name = 'd2-timer-seismic'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'seismic'; Mode = '--timer-save-runtime' }
    @{Name = 'd2-timer-ambient'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'ambient'; Mode = '--timer-save-runtime' }
    # Imported D1 timer boundaries run in D2 with only the staged public D1 assets
    @{Name = 'imported-d1-timer-pickup'; Exe = $d2ExePath; Assets = $d1Path; Scenario = 'pickup'; Mode = '--timer-save-runtime'; ExtraArguments = @('--imported-d1') }
    @{Name = 'imported-d1-timer-cooldowns'; Exe = $d2ExePath; Assets = $d1Path; Scenario = 'cooldowns'; Mode = '--timer-save-runtime'; ExtraArguments = @('--imported-d1') }
    @{Name = 'd2-timer-seismic-level'; Exe = $d2ExePath; Assets = $d2Path; Scenario = 'seismic-level'; Mode = '--timer-save-runtime' }
)
foreach ($case in $cases) {
    $directory = Join-Path $outputPath $case.Name
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $workingDirectory = if ($case.ContainsKey('WorkDirectory')) { Join-Path $outputPath $case.WorkDirectory } else { $directory }
    if (-not (Test-Path -LiteralPath $workingDirectory -PathType Container)) { throw "Missing checkpoint directory for $($case.Name): $workingDirectory" }
    $mode = if ($case.ContainsKey('Mode')) { $case.Mode } else { '--public-save-continuation' }
    $traceArgs = @($mode, $case.Assets, $case.Scenario)
    if ($case.ContainsKey('ExtraArguments')) { $traceArgs += @($case.ExtraArguments) }
    $quotedArgs = @($traceArgs | ForEach-Object { '"' + $_ + '"' })
    $parameters = @{
        FilePath = $case.Exe; ArgumentList = $quotedArgs; WorkingDirectory = $workingDirectory; PassThru = $true
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
Write-Output "PASS: D2 countdown/robot-death runtime, guided/wiggle/Fusion runtime, native D1 translation and epoch/timer boundaries; D1/D2 immediate ordinary-save persistence and selected flight continuation; full trajectories UNQUALIFIED; $outputPath/report.json"

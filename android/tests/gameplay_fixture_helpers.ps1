# Isolated loaded-asset fixture runs shared by the pickup and gameplay rule probes
function Initialize-GameplayFixtureRun {
    param([string]$Repository, [string]$Output, [string]$DataEdition, [string]$D1Assets, [string]$D2Assets)
    . (Join-Path $Repository 'android/helpers/shareware_test_data.ps1')
    if ($DataEdition -eq 'shareware') {
        $d1 = Resolve-SharewareTestData -RepoRoot $Repository -Game d1 -DataDir $D1Assets
        $d2 = Resolve-SharewareTestData -RepoRoot $Repository -Game d2 -DataDir $D2Assets
    } else {
        if (-not $D1Assets -or -not $D2Assets) { throw 'Retail gameplay fixtures require both asset directories' }
        $d1 = (Resolve-Path -LiteralPath $D1Assets).Path
        $d2 = (Resolve-Path -LiteralPath $D2Assets).Path
    }
    $root = [IO.Path]::GetFullPath($Output)
    foreach ($source in @($d1, $d2)) {
        $prefix = $source.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
        if ($root.Equals($prefix, [StringComparison]::OrdinalIgnoreCase) -or
            $root.StartsWith($prefix + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
            $prefix.StartsWith($root.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Gameplay fixture output must be outside the supplied game data directories'
        }
    }
    & (Join-Path $Repository 'android/helpers/retain-recent-artifacts.ps1') -Artifacts @($root) | Out-Host
    $run = Join-Path $root ('run-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    if ($DataEdition -eq 'shareware') {
        $identities = @()
        foreach ($game in @('d1', 'd2')) {
            $config = Get-SharewareTestGameConfig -RepoRoot $Repository -Game $game
            $source = if ($game -eq 'd1') { $d1 } else { $d2 }
            $stage = Join-Path $run "assets/$game"
            New-Item -ItemType Directory -Path $stage | Out-Null
            foreach ($file in $config.RequiredFiles) {
                $original = @(Get-ChildItem -LiteralPath $source -File | Where-Object Name -IEQ $file)
                Copy-Item -LiteralPath $original[0].FullName -Destination (Join-Path $stage $file)
            }
            Assert-SharewareTestDataDirectory -Config $config -DataDir $stage | Out-Null
            $identities += @{ game = $game; archive = $config.Archive; archiveSha256 = $config.ArchiveSha256; files = $config.RequiredHashes }
            if ($game -eq 'd1') { $d1 = $stage } else { $d2 = $stage }
        }
        $identities | ConvertTo-Json -Depth 6 | Set-Content "$run/assets.json"
    }
    return @{ Output = $run; D1Assets = $d1; D2Assets = $d2 }
}

function Invoke-GameplayFixtureProcess {
    param([string]$Executable, [string[]]$Arguments, [string]$Directory, [int]$TimeoutSeconds)
    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $parameters = @{
        FilePath = $Executable; ArgumentList = @($Arguments | ForEach-Object { '"' + $_ + '"' })
        WorkingDirectory = $Directory; PassThru = $true
        RedirectStandardOutput = "$Directory/stdout.log"; RedirectStandardError = "$Directory/stderr.log"
    }
    if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { $parameters.WindowStyle = 'Hidden' }
    $process = Start-Process @parameters
    try {
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
            Set-Content "$Directory/timeout.txt" "Exceeded $TimeoutSeconds seconds"
            throw "Gameplay fixture timed out; see $Directory"
        }
        Set-Content "$Directory/exit.txt" $process.ExitCode
        if ($process.ExitCode -ne 0) { throw "Gameplay fixture failed: $($process.ExitCode); see $Directory/stderr.log" }
    } finally {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
        $process.Dispose()
    }
}

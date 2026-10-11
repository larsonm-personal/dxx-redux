#!/usr/bin/env pwsh
param([switch]$RequireData)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/shareware_test_data.ps1')
$testRoot = Join-Path $repoRoot "android/temp/shareware_data_$([Guid]::NewGuid().ToString('N'))"

function Assert-Rejected {
    param([scriptblock]$Action, [string]$Expected, [string]$Label)
    try { & $Action | Out-Null } catch {
        if ($_.Exception.Message -notlike "*$Expected*") { throw }
        Write-Host "PASS: $Label"
        return
    }
    throw "Expected rejection: $Label"
}

& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $testRoot -DirectoryPrefix shareware_data_
New-Item -ItemType Directory -Path $testRoot | Out-Null
$lock = [IO.File]::Open((Join-Path $testRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    foreach ($game in @('d1', 'd2')) {
        $config = Get-SharewareTestGameConfig -RepoRoot $repoRoot -Game $game
        $expectedMission = if ($game -eq 'd1') { 'd1' } else { 'd2demo' }
        $expectedLevel = if ($game -eq 'd1') { 'level01.sdl' } else { 'd2leva-1.sl2' }
        if ($config.Name -cne $game -or $config.Mission -cne $expectedMission -or
            $config.FirstLevel -cne $expectedLevel -or $config.Edition -cne 'shareware' -or
            $config.RequiredHashes.Count -ne $config.RequiredFiles.Count -or
            $config.DefaultDataDirs.Count -ne 1 -or $config.DefaultDataDirs[0] -cne $config.DataDir) {
            throw "Invalid public fixture configuration: $game"
        }
        $offers = Get-Content -LiteralPath (Join-Path $repoRoot 'android/app/src/main/java/com/dxxredux/app/SetupGameFiles.kt') -Raw
        if (-not $offers.Contains("demo_installers/$($config.Archive)")) {
            throw "Public fixture no longer matches a production download offer: $game"
        }
        Assert-Rejected { Resolve-SharewareTestData -RepoRoot $repoRoot -Game $game -DataDir (Join-Path $testRoot "missing-$game") } `
            'setup_shareware_test_data.ps1' "$game missing fixture fails offline with setup guidance"
        if (Test-Path -LiteralPath (Join-Path $testRoot "missing-$game")) { throw 'Offline resolver wrote missing data' }
        if ($RequireData -or (Test-Path -LiteralPath $config.DataDir -PathType Container)) {
            $resolved = Resolve-SharewareTestData -RepoRoot $repoRoot -Game $game
            if ($resolved -cne (Resolve-Path -LiteralPath $config.DataDir).Path) { throw 'Resolved the wrong fixture' }
            Write-Host "PASS: $game public files match pinned package output hashes"
        } else {
            Write-Host "SKIP: optional real $game fixture verification (run setup_shareware_test_data.ps1 -Download)"
        }
    }

    $setupScript = Join-Path $repoRoot 'android/helpers/setup_shareware_test_data.ps1'
    $emptyCache = Join-Path $testRoot 'empty-cache'
    $setupRoot = Join-Path $testRoot 'setup-output'
    $unusedBuild = Join-Path $testRoot 'unused-build'
    Assert-Rejected {
        & $setupScript -Game d1 -ArchiveDirectory $emptyCache -DataRoot $setupRoot -BuildDirectory $unusedBuild
    } 'Rerun with -Download' 'setup defaults to offline and does not fetch missing packages'
    if ((Test-Path -LiteralPath $emptyCache) -or (Test-Path -LiteralPath $unusedBuild)) {
        throw 'Offline missing-package setup unexpectedly downloaded or built tools'
    }
    New-Item -ItemType Directory -Path $emptyCache | Out-Null
    $badArchive = Join-Path $emptyCache 'desc14sw.exe'
    [IO.File]::WriteAllText($badArchive, 'corrupt cached package')
    Assert-Rejected {
        & $setupScript -Game d1 -ArchiveDirectory $emptyCache -DataRoot $setupRoot -BuildDirectory $unusedBuild
    } 'SHA-256 mismatch' 'corrupt cached package is rejected before building or extracting'
    if (Test-Path -LiteralPath $unusedBuild) { throw 'Corrupt package setup unexpectedly built a tool' }
    if (Test-Path -LiteralPath (Join-Path $setupRoot 'd1')) { throw 'Corrupt package setup published an output tree' }

    # Exercise fail-closed resolution with small fixtures, without downloading game data
    $directory = Join-Path $testRoot 'synthetic'
    New-Item -ItemType Directory -Path $directory | Out-Null
    $file = Join-Path $directory 'D2DEMO.HOG'
    [IO.File]::WriteAllBytes($file, [byte[]](1, 2, 3, 4))
    $config = @{
        Name = 'd2'
        RequiredHashes = @(@{ File = 'd2demo.hog'; Size = 4L; Sha256 = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash })
    }
    Assert-SharewareTestDataDirectory -Config $config -DataDir $directory | Out-Null
    Write-Host 'PASS: case-insensitive source names are validated'
    [IO.File]::WriteAllBytes($file, [byte[]](1, 2, 3, 5))
    Assert-Rejected { Assert-SharewareTestDataDirectory -Config $config -DataDir $directory } 'SHA-256 mismatch' 'same-size source mutation is rejected'
    [IO.File]::WriteAllBytes($file, [byte[]](1, 2, 3))
    Assert-Rejected { Assert-SharewareTestDataDirectory -Config $config -DataDir $directory } 'size mismatch' 'truncated source is rejected'
    [IO.File]::WriteAllBytes($file, [byte[]](1, 2, 3, 4))
    $retail = Join-Path $directory 'DESCENT2.HOG'
    [IO.File]::WriteAllText($retail, 'retail')
    Assert-Rejected { Assert-SharewareTestDataDirectory -Config $config -DataDir $directory } 'Retail file conflicts' 'retail data cannot shadow the public demo'
    Remove-Item -LiteralPath $retail
    $looseLevel = Join-Path $directory 'd2leva-1.sl2'
    [IO.File]::WriteAllText($looseLevel, 'loose level override')
    Assert-Rejected { Assert-SharewareTestDataDirectory -Config $config -DataDir $directory } 'Unexpected shareware fixture entry' 'loose overrides cannot shadow pinned archive content'
    Remove-Item -LiteralPath $looseLevel
    $missionDirectory = Join-Path $directory 'missions'
    New-Item -ItemType Directory -Path $missionDirectory | Out-Null
    Assert-Rejected { Assert-SharewareTestDataDirectory -Config $config -DataDir $directory } 'Unexpected shareware fixture entry' 'extra content directories are rejected'
    Remove-Item -LiteralPath $missionDirectory
    $duplicate = Join-Path $directory 'd2demo.hog'
    if (-not (Test-Path -LiteralPath $duplicate)) {
        Copy-Item -LiteralPath $file -Destination $duplicate
        Assert-Rejected { Assert-SharewareTestDataDirectory -Config $config -DataDir $directory } 'Ambiguous shareware filename' 'case-colliding source files are rejected'
        Remove-Item -LiteralPath $duplicate
    }
    Remove-Item -LiteralPath $file
    Assert-Rejected { Assert-SharewareTestDataDirectory -Config $config -DataDir $directory } 'Required shareware file missing' 'missing required file is rejected'
    Write-Host 'Public shareware fixture tests passed'
} finally {
    $lock.Dispose()
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}

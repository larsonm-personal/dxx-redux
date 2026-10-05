#!/usr/bin/env pwsh
# Exercise the publishing wrapper without Gradle, credentials or network access
#Requires -Version 5.1
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
$testRoot = Join-Path $repoRoot ('android/temp/publish_menu_tests/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $testRoot -DirectoryPrefix run_
$androidDir = Join-Path $testRoot 'android'
New-Item -ItemType Directory -Path (Join-Path $androidDir 'helpers') -Force | Out-Null
Copy-Item (Join-Path $repoRoot 'android/0_upload_to_test.ps1') $androidDir
[IO.File]::WriteAllText((Join-Path $androidDir 'play-store-credentials.json'), '{}')

$authFixture = @'
function Get-PlayStoreAccessToken {
    param($Credentials)
    Record-PublishEvent @{ event = 'auth' }
    return 'fixture-token'
}
function Get-TrackVersionCode {
    param($BaseUrl, $EditId, $Headers, $TrackName)
    Record-PublishEvent @{ event = 'track'; track = $TrackName }
    return $global:publishMenuConfig.deployedCode
}
'@
[IO.File]::WriteAllText((Join-Path $androidDir 'helpers/playstore-auth.ps1'), $authFixture)
$buildFixture = @'
param($BuildType, $VersionCode, $OutputPath)
Record-PublishEvent @{ event = 'build'; buildType = $BuildType; versionCode = $VersionCode; path = $OutputPath }
$global:LASTEXITCODE = 0
if ($global:publishMenuConfig.failure -eq 'build') { $global:LASTEXITCODE = 9; return }
New-Item -ItemType Directory -Path (Split-Path $OutputPath) -Force | Out-Null
[IO.File]::WriteAllText($OutputPath, 'fixture-aab')
'@
[IO.File]::WriteAllText((Join-Path $androidDir '1_build_aab_apk.ps1'), $buildFixture)
$deployFixture = @'
param($TrackName, $AabPath)
Record-PublishEvent @{ event = 'deploy'; track = $TrackName; path = $AabPath }
$global:LASTEXITCODE = 0
if ($global:publishMenuConfig.failure -eq 'deploy') { $global:LASTEXITCODE = 8 }
'@
[IO.File]::WriteAllText((Join-Path $androidDir '2_deploy-playstore.ps1'), $deployFixture)
$releaseFixture = @'
param($Version, [switch]$BuildOnly)
Record-PublishEvent @{ event = 'release'; version = $Version; buildOnly = [bool]$BuildOnly }
$global:LASTEXITCODE = 0
if ($global:publishMenuConfig.failure -eq 'release') { $global:LASTEXITCODE = 7 }
'@
[IO.File]::WriteAllText((Join-Path $androidDir 'release-github.ps1'), $releaseFixture)
$driver = @'
param([string]$ConfigPath)
$ErrorActionPreference = 'Stop'
$global:publishMenuConfig = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json
$global:publishMenuResponses = [Collections.Generic.Queue[string]]::new()
foreach ($response in $global:publishMenuConfig.responses) { $global:publishMenuResponses.Enqueue($response) }
$global:publishMenuEvents = $ConfigPath + '.events'
function Record-PublishEvent {
    param($Event)
    [IO.File]::AppendAllText($global:publishMenuEvents, ($Event | ConvertTo-Json -Compress) + "`n")
}
function Read-Host {
    param($Prompt)
    Record-PublishEvent @{ event = 'prompt'; prompt = $Prompt }
    if ($global:publishMenuResponses.Count -eq 0) { throw 'Unexpected late prompt' }
    return $global:publishMenuResponses.Dequeue()
}
function git {
    Record-PublishEvent @{ event = 'git' }
    $global:LASTEXITCODE = 0
    return '100'
}
function Invoke-RestMethod {
    param($Uri, $Method, $Headers, $ContentType, $Body, $TimeoutSec)
    if ($Uri -notlike 'https://androidpublisher.googleapis.com/androidpublisher/v3/applications/com.dxxrevival.app/edits*') {
        throw "Unexpected Play Store application URL: $Uri"
    }
    Record-PublishEvent @{ event = 'rest'; method = $Method }
    if ($Method -eq 'POST') { return @{ id = 'fixture-edit' } }
    if ($Method -ne 'DELETE') { throw 'Unexpected remote operation' }
}
$parameters = @{}
foreach ($property in $global:publishMenuConfig.parameters.PSObject.Properties) { $parameters[$property.Name] = $property.Value }
if ($global:publishMenuConfig.noCredentials) {
    Remove-Item -LiteralPath (Join-Path $PSScriptRoot 'android/play-store-credentials.json')
}
$global:LASTEXITCODE = 0
& (Join-Path $PSScriptRoot 'android/0_upload_to_test.ps1') @parameters
exit $LASTEXITCODE
'@
$driverPath = Join-Path $testRoot 'driver.ps1'
[IO.File]::WriteAllText($driverPath, $driver)

$cases = @(
    @{ name = 'default'; responses = @(''); order = 'prompt,git,auth,rest,track,rest,build,deploy' },
    @{ name = 'release-only'; responses = @('2', '1.2.0'); noCredentials = $true; order = 'prompt,prompt,release' },
    @{ name = 'both'; responses = @('3', '1.2.0'); order = 'prompt,prompt,git,auth,rest,track,rest,build,deploy,release' },
    @{ name = 'invalid-choices'; responses = @('oops', '4', '3', '', 'v1.2.0', '01.2.0', '1.2.0-rc.1'); order = 'prompt,prompt,prompt,prompt,prompt,prompt,prompt,git,auth,rest,track,rest,build,deploy,release' },
    @{ name = 'unattended-both'; parameters = @{ Action = '3'; ReleaseVersion = '1.2.0' }; order = 'git,auth,rest,track,rest,build,deploy,release' },
    @{ name = 'unattended-release'; parameters = @{ Action = '2'; ReleaseVersion = '1.2.0' }; order = 'release' },
    @{ name = 'early-version'; parameters = @{ Action = '3' }; responses = @('1.2.0'); order = 'prompt,git,auth,rest,track,rest,build,deploy,release' },
    @{ name = 'legacy-parameters'; parameters = @{ BuildType = '2'; TrackName = 'alpha' }; order = 'git,auth,rest,track,rest,build,deploy' },
    @{ name = 'build-only'; parameters = @{ BuildOnly = $true }; order = 'git,build' },
    @{ name = 'both-build-only'; parameters = @{ Action = '3'; ReleaseVersion = '1.2.0'; BuildOnly = $true }; order = 'git,build,release' },
    @{ name = 'release-build-only'; parameters = @{ Action = '2'; ReleaseVersion = '1.2.0'; BuildOnly = $true }; order = 'release' },
    @{ name = 'build-fails'; parameters = @{ Action = '3'; ReleaseVersion = '1.2.0' }; failure = 'build'; order = 'git,auth,rest,track,rest,build'; exit = 1 },
    @{ name = 'deploy-fails'; parameters = @{ Action = '3'; ReleaseVersion = '1.2.0' }; failure = 'deploy'; order = 'git,auth,rest,track,rest,build,deploy'; exit = 1 },
    @{ name = 'release-fails'; parameters = @{ Action = '3'; ReleaseVersion = '1.2.0' }; failure = 'release'; order = 'git,auth,rest,track,rest,build,deploy,release'; exit = 1 },
    @{ name = 'invalid-version'; parameters = @{ Action = '3'; ReleaseVersion = 'v1.2.0' }; order = ''; exit = 1 },
    @{ name = 'wrong-destination'; parameters = @{ Action = '1'; ReleaseVersion = '1.2.0' }; order = ''; exit = 1 },
    @{ name = 'rev-limit'; parameters = @{ Action = '3'; ReleaseVersion = '1.2.0' }; deployedCode = 1009; order = 'git,auth,rest,track,rest'; exit = 1 }
)
foreach ($case in $cases) {
    $config = @{ parameters = @{}; responses = @(); failure = ''; deployedCode = 1002; noCredentials = $false }
    foreach ($key in @('parameters', 'responses', 'failure', 'deployedCode', 'noCredentials')) {
        if ($case.ContainsKey($key)) { $config[$key] = $case[$key] }
    }
    [IO.File]::WriteAllText((Join-Path $androidDir 'play-store-credentials.json'), '{}')
    $configPath = Join-Path $testRoot ($case.name + '.json')
    [IO.File]::WriteAllText($configPath, ($config | ConvertTo-Json -Depth 4))
    $execution = @{ Result = $null }
    $task = [pscustomobject]@{ FilePath = Get-RegressionCurrentPwshPath; Arguments = @('-NoProfile', '-NonInteractive', '-File', $driverPath, '-ConfigPath', $configPath); WorkingDirectory = $testRoot; TimeoutSeconds = 15 }
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($Task, $Result)
        $execution.Result = $Result
    }
    $expectedExit = if ($case.ContainsKey('exit')) { $case.exit } else { 0 }
    if ($execution.Result.TimedOut -or $execution.Result.ExitCode -ne $expectedExit) {
        throw "$($case.name) failed: $($execution.Result.StandardOutput)`n$($execution.Result.StandardError)"
    }
    $events = if (Test-Path -LiteralPath ($configPath + '.events')) {
        @(Get-Content -LiteralPath ($configPath + '.events') | ForEach-Object { $_ | ConvertFrom-Json })
    } else { @() }
    if (($events.event -join ',') -ne $case.order) { throw "$($case.name) wrong workflow order: $($events.event -join ',')" }
    $build = @($events | Where-Object event -EQ 'build')
    $deploy = @($events | Where-Object event -EQ 'deploy')
    $release = @($events | Where-Object event -EQ 'release')
    if ($build.Count) {
        $expectedType = if ($config.parameters.ContainsKey('BuildType')) { $config.parameters.BuildType } else { '3' }
        $expectedCode = if ($config.parameters.BuildOnly) { '1000' } else { '1003' }
        if ($build[0].buildType -ne $expectedType -or $build[0].versionCode -ne $expectedCode) { throw "$($case.name) lost build type or Play revision" }
    }
    if ($deploy.Count) {
        $expectedTrack = if ($config.parameters.ContainsKey('TrackName')) { $config.parameters.TrackName } else { 'internal' }
        if ($deploy[0].path -ne $build[0].path -or $deploy[0].track -ne $expectedTrack) { throw "$($case.name) deployed the wrong AAB or track" }
    }
    if ($release.Count) {
        $expectedVersion = if ($case.name -eq 'invalid-choices') { '1.2.0-rc.1' } else { '1.2.0' }
        if ($release[0].version -ne $expectedVersion -or $release[0].buildOnly -ne [bool]$config.parameters.BuildOnly) { throw "$($case.name) lost release version or BuildOnly" }
    }
    Write-Host "PASS $($case.name)"
}
Write-Host "Passed $($cases.Count) publishing menu scenarios on PowerShell $($PSVersionTable.PSVersion)"

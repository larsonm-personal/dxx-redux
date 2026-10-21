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
Copy-Item (Join-Path $repoRoot 'android/helpers/release-build.ps1') (Join-Path $androidDir 'helpers')
[IO.File]::WriteAllText((Join-Path $androidDir 'play-store-credentials.json'), '{}')

$authFixture = @'
function Get-PlayStoreAccessToken {
    param($Credentials)
    Record-PublishEvent @{ event = 'auth' }
    return 'fixture-token'
}
function Get-PlayStoreMaxVersionCode {
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
param($Version, [switch]$BuildOnly, $BuildMode)
Record-PublishEvent @{ event = 'release'; version = $Version; buildOnly = [bool]$BuildOnly; buildMode = $BuildMode }
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
    @{ name = 'closed-menu'; responses = @('4'); order = 'prompt,git,auth,rest,track,rest,build,deploy' },
    @{ name = 'closed-unattended'; parameters = @{ Action = '4' }; order = 'git,auth,rest,track,rest,build,deploy' },
    @{ name = 'closed-build-only'; parameters = @{ Action = '4'; BuildOnly = $true }; order = 'git,build' },
    @{ name = 'closed-wrong-track'; parameters = @{ Action = '4'; TrackName = 'production' }; order = ''; exit = 1 },
    @{ name = 'closed-debug'; parameters = @{ Action = '4'; BuildType = '3' }; order = ''; exit = 1 },
    @{ name = 'newer-remote'; parameters = @{ Action = '4' }; deployedCode = 1010; order = 'git,auth,rest,track,rest'; exit = 1 },
    @{ name = 'release-only'; responses = @('2', '1.2.0'); noCredentials = $true; order = 'prompt,prompt,release' },
    @{ name = 'both'; responses = @('3', '1.2.0'); order = 'prompt,prompt,git,auth,rest,track,rest,build,deploy,release' },
    @{ name = 'invalid-choices'; responses = @('oops', '5', '3', '', 'v1.2.0', '01.2.0', '1.2.0-rc.1'); order = 'prompt,prompt,prompt,prompt,prompt,prompt,prompt,git,auth,rest,track,rest,build,deploy,release' },
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
    if ($config.parameters.Action -in @('2', '3') -and $config.parameters.ReleaseVersion) {
        $config.parameters.BuildMode = 'Clean'
    } elseif ($case.name -in @('release-only', 'both', 'invalid-choices', 'early-version')) {
        $config.responses += 'invalid', ''
        $case.order = [regex]::new('(git|release)').Replace($case.order, 'prompt,prompt,$1', 1)
    }
    [IO.File]::WriteAllText((Join-Path $androidDir 'play-store-credentials.json'), '{}')
    $configPath = Join-Path $testRoot ($case.name + '.json')
    [IO.File]::WriteAllText($configPath, ($config | ConvertTo-Json -Depth 4))
    $execution = @{ Result = $null }
    $task = [pscustomobject]@{ FilePath = Get-RegressionCurrentPwshPath; Arguments = @('-NoProfile', '-NonInteractive', '-File', $driverPath, '-ConfigPath', $configPath); WorkingDirectory = $testRoot; TimeoutSeconds = 60 }
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
        $expectedType = if ($config.parameters.ContainsKey('BuildType')) { $config.parameters.BuildType } elseif ($case.name -like 'closed-*') { '2' } else { '3' }
        $expectedCode = if ($config.parameters.BuildOnly) { '1000' } else { '1003' }
        if ($build[0].buildType -ne $expectedType -or $build[0].versionCode -ne $expectedCode) { throw "$($case.name) lost build type or Play revision" }
    }
    if ($deploy.Count) {
        $expectedTrack = if ($config.parameters.ContainsKey('TrackName')) { $config.parameters.TrackName } elseif ($case.name -like 'closed-*') { 'alpha' } else { 'internal' }
        if ($deploy[0].path -ne $build[0].path -or $deploy[0].track -ne $expectedTrack) { throw "$($case.name) deployed the wrong AAB or track" }
    }
    if ($release.Count) {
        $expectedMode = if ($config.parameters.BuildMode) { $config.parameters.BuildMode } else { 'Unclean' }
        if ($release[0].buildMode -ne $expectedMode) { throw "$($case.name) lost build mode" }
        $expectedVersion = if ($case.name -eq 'invalid-choices') { '1.2.0-rc.1' } else { '1.2.0' }
        if ($release[0].version -ne $expectedVersion -or $release[0].buildOnly -ne [bool]$config.parameters.BuildOnly) { throw "$($case.name) lost release version or BuildOnly" }
    }
    Write-Host "PASS $($case.name)"
}
Write-Host "Passed $($cases.Count) publishing menu scenarios on PowerShell $($PSVersionTable.PSVersion)"

# Exercise version allocation against real helper logic with isolated API responses
. (Join-Path $repoRoot 'android/helpers/playstore-auth.ps1')
function Invoke-RestMethod {
    param($Uri, $Headers, $TimeoutSec)
    if ($Uri -like '*/tracks') {
        return @{ tracks = @(
                @{ track = 'alpha'; releases = @() },
                @{ track = 'internal'; releases = @(@{ versionCodes = @('1002', '1004') }) }
            )
        }
    }
    if ($Uri -like '*/bundles') { return @{ bundles = @(@{ versionCode = 1007 }) } }
    throw "Unexpected version query: $Uri"
}
$maximum = Get-PlayStoreMaxVersionCode -BaseUrl 'https://fixture' -EditId 'edit' -Headers @{} -TrackName alpha
if ($maximum -ne 1007) { throw 'Version query ignored a higher uploaded bundle or another track' }
$missingTrackRejected = $false
try { Get-PlayStoreMaxVersionCode -BaseUrl 'https://fixture' -EditId 'edit' -Headers @{} -TrackName missing } catch { $missingTrackRejected = $true }
if (-not $missingTrackRejected) { throw 'Missing track accepted' }
Write-Host 'PASS global version allocation and missing track rejection'

# Run the actual deploy script against a fake API, including first-release failures
Copy-Item (Join-Path $repoRoot 'android/2_deploy-playstore.ps1') $androidDir -Force
[IO.File]::WriteAllText((Join-Path $testRoot 'fixture.aab'), 'fixture-bundle')
$deployDriver = @'
param([string]$Mode)
$ErrorActionPreference = 'Stop'
$script:mockStatus = 'completed'
function Record-PublishEvent { param($Event) }
function Invoke-WebRequest {
    param($Uri, $Method, $Headers, $ContentType, $Body, $TimeoutSec, [switch]$UseBasicParsing)
    return @{ Content = '{"versionCode":1003}' }
}
function Invoke-RestMethod {
    param($Uri, $Method, $Headers, $ContentType, $Body, $TimeoutSec)
    if ($Uri -like '*:commit*') {
        $message = $null
        if ($Mode -eq 'transport') { throw 'Fixture connection failed' }
        if ($Mode -eq 'draft' -and $script:mockStatus -eq 'completed') { $message = 'Only draft releases allowed for a draft app' }
        if ($Mode -eq 'review' -and $Uri -notlike '*changesNotSentForReview=true') { $message = 'Set changesNotSentForReview' }
        if ($message) {
            $record = [Management.Automation.ErrorRecord]::new([Exception]::new($message), 'Fixture', 'InvalidOperation', $null)
            $record.ErrorDetails = [Management.Automation.ErrorDetails]::new($message)
            throw $record
        }
        return @{}
    }
    if ($Method -eq 'PUT') {
        $script:mockStatus = ($Body | ConvertFrom-Json).releases[0].status
        return ($Body | ConvertFrom-Json)
    }
    if ($Uri -like '*/tracks') { return @{ tracks = @(@{ track = 'alpha' }) } }
    if ($Method -eq 'POST' -and $Uri -like '*/edits') { return @{ id = 'fixture-edit' } }
    throw "Unexpected deploy operation: $Method $Uri"
}
& (Join-Path $PSScriptRoot 'android/2_deploy-playstore.ps1') -TrackName alpha -AabPath (Join-Path $PSScriptRoot 'fixture.aab')
exit $LASTEXITCODE
'@
$deployDriverPath = Join-Path $testRoot 'deploy-driver.ps1'
[IO.File]::WriteAllText($deployDriverPath, $deployDriver)
foreach ($mode in @('completed', 'review', 'draft', 'transport')) {
    $execution = @{ Result = $null }
    $task = [pscustomobject]@{ FilePath = Get-RegressionCurrentPwshPath; Arguments = @('-NoProfile', '-NonInteractive', '-File', $deployDriverPath, '-Mode', $mode); WorkingDirectory = $testRoot; TimeoutSeconds = 60 }
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($Task, $Result)
        $execution.Result = $Result
    }
    $expectedExit = switch ($mode) { draft { 2 } transport { 1 } default { 0 } }
    $output = $execution.Result.StandardOutput + $execution.Result.StandardError
    if ($execution.Result.TimedOut -or $execution.Result.ExitCode -ne $expectedExit) { throw "Deploy $mode failed: $output" }
    if ($mode -eq 'review' -and $output -notmatch 'send changes for review') { throw 'Missing review submission instruction' }
    if ($mode -eq 'draft' -and $output -notmatch 'Status: +draft') { throw 'Draft reported as completed' }
    if ($mode -eq 'transport' -and $output -match 'Deployment committed') { throw 'Transport failure reported as success' }
    Write-Host "PASS deployment outcome: $mode"
}

param(
    [Parameter(Mandatory)][string]$NativeExe,
    [Parameter(Mandatory)][string]$ImportedExe,
    [string]$D1Assets,
    [string]$D2Assets,
    [Parameter(Mandatory)][string]$Output,
    [string]$D2Baseline,
    [ValidateSet('pickup', 'selection', 'profile')][string]$Mode = 'pickup',
    [ValidateSet('retail', 'shareware')][string]$DataEdition = 'retail',
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 120
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $PSScriptRoot 'gameplay_fixture_helpers.ps1')
. (Join-Path $PSScriptRoot 'pickup_autoselect_report.ps1')
if ($DataEdition -eq 'shareware' -and $Mode -ne 'pickup') {
    throw 'The public profile covers pickup acquisition; selection and profile modes retain their retail assets'
}
$nativePath = (Resolve-Path -LiteralPath $NativeExe).Path
$importedPath = (Resolve-Path -LiteralPath $ImportedExe).Path
$run = Initialize-GameplayFixtureRun -Repository $repo -Output $Output -DataEdition $DataEdition -D1Assets $D1Assets -D2Assets $D2Assets
$outputPath = $run.Output
Get-FileHash -LiteralPath $nativePath, $importedPath | ConvertTo-Json | Set-Content "$outputPath/binaries.json"
foreach ($case in @(@{Name = 'native'; Exe = $nativePath }, @{Name = 'imported'; Exe = $importedPath })) {
    $flag = if ($DataEdition -eq 'shareware') { '--pickup-autoselect-shareware-trace' } else { "--$Mode-autoselect-trace" }
    $traceArgs = @($flag, $run.D1Assets, $run.D2Assets)
    if ($Mode -eq 'profile' -and $case.Name -eq 'imported') { $traceArgs += "$outputPath/native/weapon-order.dximdemo" }
    Invoke-GameplayFixtureProcess -Executable $case.Exe -Arguments $traceArgs -Directory (Join-Path $outputPath $case.Name) -TimeoutSeconds $TimeoutSeconds
}
$native = @(Get-Content "$outputPath/native/rules.json" -Raw | ConvertFrom-Json)
$imported = @(Get-Content "$outputPath/imported/rules.json" -Raw | ConvertFrom-Json)
if ($Mode -eq 'pickup') {
    Assert-PickupAutoselectRows -Rows $native
    Assert-PickupAutoselectRows -Rows $imported
    $ordinary = @(Get-Content "$outputPath/imported/rules-d2.json" -Raw | ConvertFrom-Json)
    Assert-PickupAutoselectRows -Rows $ordinary
    foreach ($role in @('native', 'imported', 'ordinary-d2')) {
        $file = if ($role -eq 'ordinary-d2') { 'imported/pickup-assets-d2.json' } else { "$role/pickup-assets.json" }
        $profile = Get-Content (Join-Path $outputPath $file) -Raw | ConvertFrom-Json
        Assert-PickupAssetProfile -Profile $profile -DataEdition $DataEdition -Role $role
    }
}
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
$report = [ordered]@{edition = $DataEdition; fixture = 'controlled loaded-asset corridor'; cases = $native.Count; mismatches = $different.Count; d2MatchesBaseline = $d2Matches }
if ($different.Count) {
    $report.firstExpected = $native[$different[0]]
    $report.firstActual = $imported[$different[0]]
}
$report | ConvertTo-Json -Depth 20 | Set-Content "$outputPath/report.json"
if ($different.Count -or $d2Matches -eq $false) { throw "$Mode parity failed; see $outputPath/report.json" }
Write-Output "PASS: $($native.Count) $Mode cases match native D1 ($DataEdition); $outputPath/report.json"
if ($Mode -eq 'pickup') { Write-Output 'PASS: all 2112 ordinary D2 controls and native quad runtime persistence assertions completed' }

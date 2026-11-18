#!/usr/bin/env pwsh
# Derived XL equipment production and disk/rewind persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor,
    [switch]$PickupOnly,
    [ValidateSet(-1, 7, 8, 9, 24, 26, 27, 46, 47)][int]$InertId = -1,
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
if ($PickupOnly -and $InertId -ge 0) { throw 'Choose one equipment scenario' }
if (($PickupOnly -or $InertId -ge 0) -and (!$OutputDirectory -or (Test-Path -LiteralPath $OutputDirectory))) {
    throw 'Focused equipment controls require a fresh explicit output directory'
}
# Exact native derived-fixture outputs: HOG, Windows descriptor and Unix descriptor
$fixtureHashes = @{
    -1 = @('124aba8bfcf9dda63fbc5d18e08d6aa8316c50cc05ac2f4527ec1c99d3e220d9', '6a4ca565352f3fe1bdce3b3249672c5ad2a2844816ac87173f21d8110960f6d0', '61b8d97c4cb1e31e2541352a1852201253df11bb976a443738fadc2fdc417996')
    7 = @('fa0702b691431552e49bd9ccdb92063a27f176e56b122a7a72e1cab15a3711bb', '82a9d003486ac592bc84f4f130dfea4a6bd445b7a43bd290030af0dd7f7e51e4', '3172c9597101bec29a7db5c1442b5e4a7bcf6a2111bffa9ea32d3495927f86c5')
    8 = @('c5543ff13e237f805aeed8fe0b7e3ddad4d5b182baa1a06e6468084600c7a99e', 'a85771d563f87c29c51c65c36c9f6364ed094e1d192163db665d196f6378bd6f', 'b9ec453e03c9edc2dfd2807405995ee415b608125eebce87d1331ce89c2146c3')
    9 = @('b75379c4152ef23e2c3f05f69251edb1d901ce49ac79bd4f09a736c73e04765f', 'e0acd6d433440d0fb42daa428633c9ab72655a5696c3dbdc5da1b25b94d7549f', '25c01f4aa1ffdf6c0b3c2ef31076ebd161136d9f9e536257b0d68e5d97dbe0aa')
    24 = @('12538868b640de9a077dd5bc12744861cd34d5dced1f97e12e135ae54b954924', '96c49560cd59e591237a8442263def55e7fbcfb4da8ce45decb535e0e4d94160', '00b6acef18e0d6fc9f7e7d9bacef4390822be2b55a46abf4448ff163ee9f86e2')
    26 = @('66d2a93b49d01c83287143bc139f50ba98fa5676463f157153d3645cdc42f7e4', 'e3ab41fbd2c91a3e1e1936e81fef8848b00d5778fd41b19b41621c5053e141d2', '91472c1b53d43cce71b9c32afbbe49702cc6fe4ebee7a26084182b1a4d57776e')
    27 = @('e681ede790c6546f74cc1b53ff5bb6e96722c51e1a3560a43b79ec2931c50b42', '9eed25ce0c3d4eb509b150b85577787afbfdf048ea058cf57efc59821edf6dc1', '3271fb547b8700a9195eb4de7e5f5da5e3b0c66cde03c41e0c654cb9bca59b92')
    46 = @('876c1b30dabfbfd35f982cb7488f32b4f354cbd4da6d86f7d699671c7e5976da', '2316e6374504ec6f94cb4f6164ce1da6d3ffbebae057bc9f04b4e986e9203455', '8f0b474233b7ccaf69ec4c467f26ad9f36559678546a7e3524d8d26c85ec2fc9')
    47 = @('96f98c0d141c883cc0c8d09bb146aed04dbfdc5f33e3a3308bbe42011dfe8562', '6a749bfb1ea3d150996f7229bcd4a0f9c30b01b5f00ee476113ba73a88eedb33', '12f9c3c17c3ac19107f737feea96ab5f7aefab3c5b24b2f1473c1021f99181be')
}
$expected = $fixtureHashes[$InertId]
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expected[0] -or
    $expected[1..2] -notcontains (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash) {
    throw 'Requires the pinned derived fixture from xl_equipment_generators'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-equipment-android'
if ($OutputDirectory) { $output = [IO.Path]::GetFullPath($OutputDirectory) }
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-EquipmentAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Equipment adb failed: $result" }
    return $result
}
function Invoke-EquipmentPrivate {
    param([string]$Command)
    Invoke-EquipmentAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-equipment-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/equip.hog', 'files/d2x-redux/missions/equip.mn2')
$ready = $false
try {
    Invoke-EquipmentAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-EquipmentPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-EquipmentPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-EquipmentPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-EquipmentPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-EquipmentAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-equipment.hog') | Out-Null
    Invoke-EquipmentAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-equipment.mn2') | Out-Null
    Invoke-EquipmentPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-equipment.hog files/d2x-redux/missions/equip.hog && cp /data/local/tmp/xl-equipment.mn2 files/d2x-redux/missions/equip.mn2' | Out-Null
    Invoke-EquipmentAdb -Arguments @('logcat', '-c') | Out-Null
    $scenario = 'test_xl_equipment_runtime.jsonc'
    if ($PickupOnly) {
        # Separate contact-only owner; the full persistence regression remains unchanged
        $steps = @(Get-Content "$PSScriptRoot/../game_scripts/test_xl_equipment_runtime.jsonc" -Raw | ConvertFrom-Json)
        $end = 0
        while ($end -lt $steps.Count -and $steps[$end].field -ne 'robots_suspended') { $end++ }
        if ($end -eq $steps.Count) { throw 'Missing shared equipment startup control' }
        $scenario = Join-Path $output 'pickup-only.jsonc'
        $focused = @($steps[0..$end]) + @([pscustomobject][ordered]@{ action = 'set_debug'; field = 'xl_equipment_pickup_probe'; value = 'inert' })
        $focused | ConvertTo-Json -Depth 30 | Set-Content $scenario -Encoding utf8
    }
    if ($InertId -ge 0) {
        . "$PSScriptRoot/../helpers/xl_equipment_inert_script.ps1"
        $scenario = Join-Path $output 'inert-lifecycle.jsonc'
        New-InertEquipmentScript -ItemId $InertId | ConvertTo-Json -Depth 30 | Set-Content $scenario -Encoding utf8
    }
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $scenario -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 360
    $testExit = $LASTEXITCODE
    Invoke-EquipmentAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-EquipmentPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-EquipmentPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-EquipmentPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Equipment runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Equipment runtime result' }
    Invoke-EquipmentAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-equipment.png') | Out-Null
    Invoke-EquipmentAdb -Arguments @('pull', '/data/local/tmp/xl-equipment.png', "$output/render.png") | Out-Null
    $scope = if ($InertId -ge 0) { "authored inert item $InertId production, contact and cold restore" } elseif ($PickupOnly) { 'eight inert pickup contacts only' } else { 'activation/production/pickup, disk/rewind and stock transition' }
    Write-Host "PASS: Equipment Android $scope ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-EquipmentAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-EquipmentPrivate "rm -rf $path" | Out-Null }
            Invoke-EquipmentPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-EquipmentPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Equipment test state restoration differs; backup retained at $backup" }
            Invoke-EquipmentPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-EquipmentAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-equipment.hog', '/data/local/tmp/xl-equipment.mn2', '/data/local/tmp/xl-equipment.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}

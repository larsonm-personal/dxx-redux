$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "..\xfing_minimal_dxa_lib.ps1")

$script:Uud2spHamPatchPath = "patches/d2/ham_patch.rfc6902.json"
$script:Uud2spHamPatchSummaryPath = "patches/d2/ham_patch_summary.json"
$script:Uud2spRobotRecordSize = 480
$script:Uud2spWeaponRecordSize = 125
$script:Uud2spEclipRecordSize = 130
$script:Uud2spWclipRecordSize = 126
$script:Uud2spMaxBitmapFiles = 2620

$script:Uud2spRobotFields = [ordered]@{}
function Add-Uud2spRobotPatchField {
    param([string]$Name, [int]$Offset, [string]$Type)

    $script:Uud2spRobotFields[$Name] = [pscustomobject]@{ Offset = $Offset; Type = $Type }
}

for ($gunIndex = 0; $gunIndex -lt 8; $gunIndex++) {
    Add-Uud2spRobotPatchField -Name "GunPoint$($gunIndex)X" -Offset (4 + ($gunIndex * 12)) -Type "int32"
    Add-Uud2spRobotPatchField -Name "GunPoint$($gunIndex)Y" -Offset (8 + ($gunIndex * 12)) -Type "int32"
    Add-Uud2spRobotPatchField -Name "GunPoint$($gunIndex)Z" -Offset (12 + ($gunIndex * 12)) -Type "int32"
}
for ($gunIndex = 0; $gunIndex -lt 8; $gunIndex++) {
    Add-Uud2spRobotPatchField -Name "GunSubmodel$gunIndex" -Offset (100 + $gunIndex) -Type "byte"
}
Add-Uud2spRobotPatchField -Name "Exp1Vclip" -Offset 108 -Type "int16"
Add-Uud2spRobotPatchField -Name "Exp1Sound" -Offset 110 -Type "int16"
Add-Uud2spRobotPatchField -Name "Exp2Vclip" -Offset 112 -Type "int16"
Add-Uud2spRobotPatchField -Name "Exp2Sound" -Offset 114 -Type "int16"
Add-Uud2spRobotPatchField -Name "WeaponType" -Offset 116 -Type "byte"
Add-Uud2spRobotPatchField -Name "WeaponType2" -Offset 117 -Type "byte"
Add-Uud2spRobotPatchField -Name "NGuns" -Offset 118 -Type "byte"
Add-Uud2spRobotPatchField -Name "ContainsId" -Offset 119 -Type "byte"
Add-Uud2spRobotPatchField -Name "ContainsCount" -Offset 120 -Type "byte"
Add-Uud2spRobotPatchField -Name "ContainsProb" -Offset 121 -Type "byte"
Add-Uud2spRobotPatchField -Name "ContainsType" -Offset 122 -Type "byte"
Add-Uud2spRobotPatchField -Name "Kamikaze" -Offset 123 -Type "byte"
Add-Uud2spRobotPatchField -Name "ScoreValue" -Offset 124 -Type "int16"
Add-Uud2spRobotPatchField -Name "Badass" -Offset 126 -Type "byte"
Add-Uud2spRobotPatchField -Name "EnergyDrain" -Offset 127 -Type "byte"
Add-Uud2spRobotPatchField -Name "Lighting" -Offset 128 -Type "int32"
Add-Uud2spRobotPatchField -Name "Strength" -Offset 132 -Type "int32"
Add-Uud2spRobotPatchField -Name "Mass" -Offset 136 -Type "int32"
Add-Uud2spRobotPatchField -Name "Drag" -Offset 140 -Type "int32"

$robotArrayOffsets = [ordered]@{
    FieldOfView = 144
    FiringWait = 164
    FiringWait2 = 184
    TurnTime = 204
    MaxSpeed = 224
    CircleDistance = 244
}
foreach ($arrayName in $robotArrayOffsets.Keys) {
    for ($difficulty = 0; $difficulty -lt 5; $difficulty++) {
        Add-Uud2spRobotPatchField -Name "$arrayName$difficulty" -Offset ($robotArrayOffsets[$arrayName] + ($difficulty * 4)) -Type "int32"
    }
}
for ($difficulty = 0; $difficulty -lt 5; $difficulty++) {
    Add-Uud2spRobotPatchField -Name "RapidfireCount$difficulty" -Offset (264 + $difficulty) -Type "byte"
    Add-Uud2spRobotPatchField -Name "EvadeSpeed$difficulty" -Offset (269 + $difficulty) -Type "byte"
}
Add-Uud2spRobotPatchField -Name "CloakType" -Offset 274 -Type "byte"
Add-Uud2spRobotPatchField -Name "AttackType" -Offset 275 -Type "byte"
Add-Uud2spRobotPatchField -Name "SeeSound" -Offset 276 -Type "byte"
Add-Uud2spRobotPatchField -Name "AttackSound" -Offset 277 -Type "byte"
Add-Uud2spRobotPatchField -Name "ClawSound" -Offset 278 -Type "byte"
Add-Uud2spRobotPatchField -Name "TauntSound" -Offset 279 -Type "byte"
Add-Uud2spRobotPatchField -Name "BossFlag" -Offset 280 -Type "byte"
Add-Uud2spRobotPatchField -Name "Companion" -Offset 281 -Type "byte"
Add-Uud2spRobotPatchField -Name "SmartBlobs" -Offset 282 -Type "byte"
Add-Uud2spRobotPatchField -Name "EnergyBlobs" -Offset 283 -Type "byte"
Add-Uud2spRobotPatchField -Name "Thief" -Offset 284 -Type "byte"
Add-Uud2spRobotPatchField -Name "Pursuit" -Offset 285 -Type "byte"
Add-Uud2spRobotPatchField -Name "Lightcast" -Offset 286 -Type "byte"
Add-Uud2spRobotPatchField -Name "DeathRoll" -Offset 287 -Type "byte"
Add-Uud2spRobotPatchField -Name "Flags" -Offset 288 -Type "byte"
Add-Uud2spRobotPatchField -Name "DeathrollSound" -Offset 292 -Type "byte"
Add-Uud2spRobotPatchField -Name "Glow" -Offset 293 -Type "byte"
Add-Uud2spRobotPatchField -Name "Behavior" -Offset 294 -Type "byte"
Add-Uud2spRobotPatchField -Name "Aim" -Offset 295 -Type "byte"
for ($gunIndex = 0; $gunIndex -lt 9; $gunIndex++) {
    for ($stateIndex = 0; $stateIndex -lt 5; $stateIndex++) {
        $animOffset = 296 + ((($gunIndex * 5) + $stateIndex) * 4)
        Add-Uud2spRobotPatchField -Name "AnimState$($gunIndex)_$($stateIndex)Joints" -Offset $animOffset -Type "int16"
        Add-Uud2spRobotPatchField -Name "AnimState$($gunIndex)_$($stateIndex)Offset" -Offset ($animOffset + 2) -Type "int16"
    }
}
Add-Uud2spRobotPatchField -Name "Always0xabcd" -Offset 476 -Type "int32"

$script:Uud2spWeaponFields = [ordered]@{
    FlashSound = [pscustomobject]@{ Offset = 8; Type = "int16" }
    RobotHitSound = [pscustomobject]@{ Offset = 12; Type = "int16" }
    WallHitSound = [pscustomobject]@{ Offset = 16; Type = "int16" }
}

$script:Uud2spEclipFields = [ordered]@{
    FrameTime = [pscustomobject]@{ Offset = 8; Type = "int32" }
    Sound = [pscustomobject]@{ Offset = 118; Type = "int32" }
}

$script:Uud2spWclipFields = [ordered]@{
    PlayTime = [pscustomobject]@{ Offset = 0; Type = "int32" }
    OpenSound = [pscustomobject]@{ Offset = 106; Type = "int16" }
    CloseSound = [pscustomobject]@{ Offset = 108; Type = "int16" }
}

function Read-Uud2spLe16At {
    param([byte[]]$Bytes, [int]$Offset)

    return [BitConverter]::ToInt16($Bytes, $Offset)
}

function Read-Uud2spLe32At {
    param([byte[]]$Bytes, [int]$Offset)

    return [BitConverter]::ToInt32($Bytes, $Offset)
}

function Write-Uud2spLe16At {
    param([byte[]]$Bytes, [int]$Offset, [int]$Value)

    [BitConverter]::GetBytes([int16]$Value).CopyTo($Bytes, $Offset)
}

function Write-Uud2spLe32At {
    param([byte[]]$Bytes, [int]$Offset, [int]$Value)

    [BitConverter]::GetBytes([int32]$Value).CopyTo($Bytes, $Offset)
}

function Test-Uud2spRange {
    param(
        [string]$Name,
        [long]$Value,
        [long]$Min,
        [long]$Max
    )

    if ($Value -lt $Min -or $Value -gt $Max) {
        throw "$Name value $Value is outside $Min..$Max"
    }
}

function Get-Uud2spHamLayout {
    param([byte[]]$Bytes)

    $hamId = Read-Uud2spLe32At $Bytes 0
    $version = Read-Uud2spLe32At $Bytes 4
    if ($hamId -ne 558711112 -or $version -ne 3) {
        throw "Not a supported D2 HAM id=$hamId version=$version"
    }

    $position = 8
    $textureCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($textureCount * 2) + ($textureCount * 20)

    $soundsStart = $position
    $soundCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($soundCount * 2)

    $vclipStart = $position
    $vclipCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($vclipCount * 82)

    $eclipStart = $position
    $eclipCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($eclipCount * $script:Uud2spEclipRecordSize)

    $wclipStart = $position
    $wclipCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($wclipCount * $script:Uud2spWclipRecordSize)

    $robotStart = $position
    $robotCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($robotCount * $script:Uud2spRobotRecordSize)

    $robotJointStart = $position
    $robotJointCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($robotJointCount * 8)

    $weaponStart = $position
    $weaponCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($weaponCount * $script:Uud2spWeaponRecordSize)

    $powerupStart = $position
    $powerupCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($powerupCount * 16)

    $polyStart = $position
    $polyCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($polyCount * 734)
    for ($index = 0; $index -lt $polyCount; $index++) {
        $modelDataSize = Read-Uud2spLe32At $Bytes ($polyStart + 4 + ($index * 734) + 4)
        $position += $modelDataSize
    }

    $position += $polyCount * 8
    $gaugeStart = $position
    $gaugeCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($gaugeCount * 4)

    $objBitmapStart = $position
    $objBitmapCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($objBitmapCount * 4)

    $playerShipStart = $position
    $position += 132

    $cockpitStart = $position
    $cockpitCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($cockpitCount * 2)

    $firstMultiBitmapStart = $position
    $position += 4

    $reactorStart = $position
    $reactorCount = Read-Uud2spLe32At $Bytes $position
    $position += 4 + ($reactorCount * 772)

    $markerModelStart = $position
    $position += 4

    return [pscustomobject]@{
        TextureCount = $textureCount
        SoundsStart = $soundsStart
        SoundCount = $soundCount
        VclipStart = $vclipStart
        VclipCount = $vclipCount
        EclipStart = $eclipStart
        EclipCount = $eclipCount
        WclipStart = $wclipStart
        WclipCount = $wclipCount
        RobotStart = $robotStart
        RobotCount = $robotCount
        RobotJointStart = $robotJointStart
        RobotJointCount = $robotJointCount
        WeaponStart = $weaponStart
        WeaponCount = $weaponCount
        PowerupStart = $powerupStart
        PowerupCount = $powerupCount
        PolyStart = $polyStart
        PolyCount = $polyCount
        GaugeStart = $gaugeStart
        GaugeCount = $gaugeCount
        ObjBitmapStart = $objBitmapStart
        ObjBitmapCount = $objBitmapCount
        PlayerShipStart = $playerShipStart
        CockpitStart = $cockpitStart
        CockpitCount = $cockpitCount
        FirstMultiBitmapStart = $firstMultiBitmapStart
        ReactorStart = $reactorStart
        ReactorCount = $reactorCount
        MarkerModelStart = $markerModelStart
        MainEnd = $position
    }
}

function Get-Uud2spFieldDescriptor {
    param(
        $Layout,
        [string]$Section,
        [int]$Index,
        [string]$Field
    )

    switch ($Section) {
        "sounds" {
            if ($Index -lt 0 -or $Index -ge $Layout.SoundCount) { throw "Sound index out of range: $Index" }
            if ($Field -eq "Sound") {
                return [pscustomobject]@{ Offset = $Layout.SoundsStart + 4 + $Index; Type = "byte" }
            }
            if ($Field -eq "AltSound") {
                return [pscustomobject]@{ Offset = $Layout.SoundsStart + 4 + $Layout.SoundCount + $Index; Type = "byte" }
            }
        }
        "eclips" {
            if ($Index -lt 0 -or $Index -ge $Layout.EclipCount) { throw "Eclip index out of range: $Index" }
            if ($script:Uud2spEclipFields.Contains($Field)) {
                $fieldInfo = $script:Uud2spEclipFields[$Field]
                return [pscustomobject]@{ Offset = $Layout.EclipStart + 4 + ($Index * $script:Uud2spEclipRecordSize) + $fieldInfo.Offset; Type = $fieldInfo.Type }
            }
        }
        "wclips" {
            if ($Index -lt 0 -or $Index -ge $Layout.WclipCount) { throw "Wclip index out of range: $Index" }
            if ($script:Uud2spWclipFields.Contains($Field)) {
                $fieldInfo = $script:Uud2spWclipFields[$Field]
                return [pscustomobject]@{ Offset = $Layout.WclipStart + 4 + ($Index * $script:Uud2spWclipRecordSize) + $fieldInfo.Offset; Type = $fieldInfo.Type }
            }
        }
        "robots" {
            if ($Index -lt 0 -or $Index -ge $Layout.RobotCount) { throw "Robot index out of range: $Index" }
            if ($script:Uud2spRobotFields.Contains($Field)) {
                $fieldInfo = $script:Uud2spRobotFields[$Field]
                return [pscustomobject]@{ Offset = $Layout.RobotStart + 4 + ($Index * $script:Uud2spRobotRecordSize) + $fieldInfo.Offset; Type = $fieldInfo.Type }
            }
        }
        "weapons" {
            if ($Index -lt 0 -or $Index -ge $Layout.WeaponCount) { throw "Weapon index out of range: $Index" }
            if ($script:Uud2spWeaponFields.Contains($Field)) {
                $fieldInfo = $script:Uud2spWeaponFields[$Field]
                return [pscustomobject]@{ Offset = $Layout.WeaponStart + 4 + ($Index * $script:Uud2spWeaponRecordSize) + $fieldInfo.Offset; Type = $fieldInfo.Type }
            }
        }
        "objBitmaps" {
            if ($Index -lt 0 -or $Index -ge $Layout.ObjBitmapCount) { throw "Object bitmap index out of range: $Index" }
            if ($Field -eq "Bitmap") {
                return [pscustomobject]@{ Offset = $Layout.ObjBitmapStart + 4 + ($Index * 2); Type = "int16" }
            }
            if ($Field -eq "Pointer") {
                return [pscustomobject]@{ Offset = $Layout.ObjBitmapStart + 4 + ($Layout.ObjBitmapCount * 2) + ($Index * 2); Type = "int16" }
            }
        }
    }
    throw "Unsupported HAM field path /sections/$Section/$Index/$Field"
}

function Read-Uud2spHamFieldValue {
    param([byte[]]$Bytes, $Layout, [string]$Section, [int]$Index, [string]$Field)

    $descriptor = Get-Uud2spFieldDescriptor -Layout $Layout -Section $Section -Index $Index -Field $Field
    switch ($descriptor.Type) {
        "byte" { return [int]$Bytes[$descriptor.Offset] }
        "int16" { return Read-Uud2spLe16At $Bytes $descriptor.Offset }
        "int32" { return Read-Uud2spLe32At $Bytes $descriptor.Offset }
    }
    throw "Unsupported field type $($descriptor.Type)"
}

function Write-Uud2spHamFieldValue {
    param([byte[]]$Bytes, $Layout, [string]$Section, [int]$Index, [string]$Field, [long]$Value)

    $descriptor = Get-Uud2spFieldDescriptor -Layout $Layout -Section $Section -Index $Index -Field $Field
    switch ($descriptor.Type) {
        "byte" {
            Test-Uud2spRange -Name "/sections/$Section/$Index/$Field" -Value $Value -Min 0 -Max 255
            $Bytes[$descriptor.Offset] = [byte]$Value
            return
        }
        "int16" {
            Test-Uud2spRange -Name "/sections/$Section/$Index/$Field" -Value $Value -Min -32768 -Max 32767
            Write-Uud2spLe16At -Bytes $Bytes -Offset $descriptor.Offset -Value $Value
            return
        }
        "int32" {
            Test-Uud2spRange -Name "/sections/$Section/$Index/$Field" -Value $Value -Min ([int32]::MinValue) -Max ([int32]::MaxValue)
            Write-Uud2spLe32At -Bytes $Bytes -Offset $descriptor.Offset -Value $Value
            return
        }
    }
    throw "Unsupported field type $($descriptor.Type)"
}

function New-Uud2spChangedFieldRow {
    param(
        [string]$Section,
        [int]$Index,
        [string]$Field,
        [int]$BaseValue,
        [int]$PatchValue
    )

    return [pscustomobject]@{
        section = $Section
        index = $Index
        field = $Field
        base = $BaseValue
        patch = $PatchValue
    }
}

function Add-Uud2spChangedFieldRows {
    param(
        [System.Collections.Generic.List[object]]$Rows,
        [byte[]]$BaseBytes,
        $BaseLayout,
        [byte[]]$PatchBytes,
        $PatchLayout,
        [string]$Section,
        [int]$Count,
        [string[]]$Fields
    )

    for ($index = 0; $index -lt $Count; $index++) {
        foreach ($field in $Fields) {
            $baseValue = Read-Uud2spHamFieldValue -Bytes $BaseBytes -Layout $BaseLayout -Section $Section -Index $index -Field $field
            $patchValue = Read-Uud2spHamFieldValue -Bytes $PatchBytes -Layout $PatchLayout -Section $Section -Index $index -Field $field
            if ($baseValue -ne $patchValue) {
                $Rows.Add((New-Uud2spChangedFieldRow -Section $Section -Index $index -Field $field -BaseValue $baseValue -PatchValue $patchValue))
            }
        }
    }
}

function ConvertTo-Uud2spJsonPatchPath {
    param($Row)

    return "/sections/$($Row.section)/$($Row.index)/$($Row.field)"
}

function ConvertTo-Uud2spJsonPatch {
    param([object[]]$Rows)

    $ops = @()
    foreach ($row in $Rows) {
        $path = ConvertTo-Uud2spJsonPatchPath $row
        $ops += New-XfingJsonPatchOperation -Op "test" -Path $path -Value $row.base
        $ops += New-XfingJsonPatchOperation -Op "replace" -Path $path -Value $row.patch
    }
    return @($ops)
}

function Copy-Uud2spBytes {
    param([byte[]]$Bytes)

    $copy = [byte[]]::new($Bytes.Length)
    [Array]::Copy($Bytes, $copy, $Bytes.Length)
    Write-Output -InputObject $copy -NoEnumerate
}

function Split-Uud2spPatchPath {
    param([string]$Path)

    if ($Path -notmatch '^/sections/([^/]+)/([0-9]+)/([^/]+)$') {
        throw "Unsupported UUD2SP HAM patch path: $Path"
    }
    return [pscustomobject]@{ Section = $Matches[1]; Index = [int]$Matches[2]; Field = $Matches[3] }
}

function Apply-Uud2spHamPatchOperations {
    param(
        [byte[]]$BaseBytes,
        [object[]]$PatchOperations
    )

    $bytes = Copy-Uud2spBytes $BaseBytes
    $layout = Get-Uud2spHamLayout $bytes
    foreach ($op in $PatchOperations) {
        $path = Split-Uud2spPatchPath $op.path
        $opName = [string]$op.op
        if ($opName -eq "test") {
            $actual = Read-Uud2spHamFieldValue -Bytes $bytes -Layout $layout -Section $path.Section -Index $path.Index -Field $path.Field
            if ($actual -ne [int]$op.value) {
                throw "HAM patch test failed at $($op.path): expected $($op.value), actual $actual"
            }
        } elseif ($opName -eq "replace" -or $opName -eq "add") {
            Write-Uud2spHamFieldValue -Bytes $bytes -Layout $layout -Section $path.Section -Index $path.Index -Field $path.Field -Value ([int]$op.value)
        } else {
            throw "Unsupported UUD2SP HAM patch op: $opName"
        }
    }
    Write-Output -InputObject $bytes -NoEnumerate
}

function Get-Uud2spFirstByteDifference {
    param([byte[]]$Left, [byte[]]$Right, [int]$Length)

    for ($index = 0; $index -lt $Length; $index++) {
        if ($Left[$index] -ne $Right[$index]) {
            return $index
        }
    }
    return -1
}

function New-Uud2spHamPatchAnalysis {
    param(
        [string]$BaseHamPath,
        [string]$PatchedHamPath
    )

    $baseBytes = [IO.File]::ReadAllBytes($BaseHamPath)
    $patchBytes = [IO.File]::ReadAllBytes($PatchedHamPath)
    $baseLayout = Get-Uud2spHamLayout $baseBytes
    $patchLayout = Get-Uud2spHamLayout $patchBytes
    if ($baseLayout.MainEnd -ne $patchLayout.MainEnd) {
        throw "Base and patched HAM main data lengths differ: $($baseLayout.MainEnd) vs $($patchLayout.MainEnd)"
    }
    if ($patchBytes.Length -lt $baseBytes.Length) {
        throw "Patched HAM is shorter than the base HAM"
    }
    $baseBitmapXlatBytes = $baseBytes.Length - $baseLayout.MainEnd
    if ($baseBitmapXlatBytes -lt 0 -or ($baseBitmapXlatBytes % 2) -ne 0) {
        throw "Base HAM GameBitmapXlat tail has an invalid byte length: $baseBitmapXlatBytes"
    }
    $baseBitmapXlatCount = [int]($baseBitmapXlatBytes / 2)
    if ($baseBitmapXlatCount -gt $script:Uud2spMaxBitmapFiles) {
        throw "Base HAM GameBitmapXlat count $baseBitmapXlatCount exceeds $script:Uud2spMaxBitmapFiles"
    }

    $rows = [System.Collections.Generic.List[object]]::new()
    Add-Uud2spChangedFieldRows -Rows $rows -BaseBytes $baseBytes -BaseLayout $baseLayout -PatchBytes $patchBytes -PatchLayout $patchLayout -Section "sounds" -Count $baseLayout.SoundCount -Fields @("Sound", "AltSound")
    Add-Uud2spChangedFieldRows -Rows $rows -BaseBytes $baseBytes -BaseLayout $baseLayout -PatchBytes $patchBytes -PatchLayout $patchLayout -Section "eclips" -Count $baseLayout.EclipCount -Fields @($script:Uud2spEclipFields.Keys)
    Add-Uud2spChangedFieldRows -Rows $rows -BaseBytes $baseBytes -BaseLayout $baseLayout -PatchBytes $patchBytes -PatchLayout $patchLayout -Section "wclips" -Count $baseLayout.WclipCount -Fields @($script:Uud2spWclipFields.Keys)
    Add-Uud2spChangedFieldRows -Rows $rows -BaseBytes $baseBytes -BaseLayout $baseLayout -PatchBytes $patchBytes -PatchLayout $patchLayout -Section "robots" -Count $baseLayout.RobotCount -Fields @($script:Uud2spRobotFields.Keys)
    Add-Uud2spChangedFieldRows -Rows $rows -BaseBytes $baseBytes -BaseLayout $baseLayout -PatchBytes $patchBytes -PatchLayout $patchLayout -Section "weapons" -Count $baseLayout.WeaponCount -Fields @($script:Uud2spWeaponFields.Keys)
    Add-Uud2spChangedFieldRows -Rows $rows -BaseBytes $baseBytes -BaseLayout $baseLayout -PatchBytes $patchBytes -PatchLayout $patchLayout -Section "objBitmaps" -Count $baseLayout.ObjBitmapCount -Fields @("Bitmap", "Pointer")

    $patchOps = ConvertTo-Uud2spJsonPatch -Rows @($rows)
    $generatedBytes = Apply-Uud2spHamPatchOperations -BaseBytes $baseBytes -PatchOperations $patchOps
    $firstDifference = Get-Uud2spFirstByteDifference -Left $generatedBytes -Right $patchBytes -Length $baseBytes.Length
    if ($firstDifference -ge 0) {
        throw "Generated HAM does not match the patched HAM retail-length prefix at byte $firstDifference"
    }

    return [pscustomobject]@{
        baseSha256 = Get-XfingSha256ForBytes -Bytes $baseBytes
        baseSize = $baseBytes.Length
        patchSha256 = Get-XfingSha256ForBytes -Bytes $patchBytes
        patchSize = $patchBytes.Length
        hamMainLength = $baseLayout.MainEnd
        bitmapXlatOffset = $baseLayout.MainEnd
        bitmapXlatCount = $baseBitmapXlatCount
        engineReadableMainLength = $baseLayout.MainEnd
        engineComparableLength = $baseBytes.Length
        engineComparableSha256 = Get-XfingSha256ForBytes -Bytes $patchBytes -Offset 0 -Length $baseBytes.Length
        generatedComparableSha256 = Get-XfingSha256ForBytes -Bytes $generatedBytes
        ignoredTrailerOffset = $baseBytes.Length
        ignoredTrailerLength = $patchBytes.Length - $baseBytes.Length
        ignoredTrailerSha256 = if ($patchBytes.Length -gt $baseBytes.Length) { Get-XfingSha256ForBytes -Bytes $patchBytes -Offset $baseBytes.Length -Length ($patchBytes.Length - $baseBytes.Length) } else { $null }
        counts = [pscustomobject]@{
            textures = $baseLayout.TextureCount
            sounds = $baseLayout.SoundCount
            vclips = $baseLayout.VclipCount
            eclips = $baseLayout.EclipCount
            wclips = $baseLayout.WclipCount
            robots = $baseLayout.RobotCount
            robotJoints = $baseLayout.RobotJointCount
            weapons = $baseLayout.WeaponCount
            powerups = $baseLayout.PowerupCount
            polygonModels = $baseLayout.PolyCount
            objectBitmaps = $baseLayout.ObjBitmapCount
            reactors = $baseLayout.ReactorCount
        }
        rows = @($rows)
        patchOperations = @($patchOps)
        changedFieldCount = @($rows).Count
        operationCount = @($patchOps).Count
    }
}
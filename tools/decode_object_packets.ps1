<#
.SYNOPSIS
Decode UPID_OBJECT_DATA packet hex dumps from DXX multiplayer PKTDUMP logs

.DESCRIPTION
Parses PKTDUMP lines from logcat output and decodes the binary object sync
protocol used during multiplayer rejoin. Shows object type, id, segment,
shields, and position for each transmitted object. Identifies truncated
packets and lost objects.

.PARAMETER LogFile
Path to a log file containing PKTDUMP lines

.PARAMETER Hex
A single raw hex string to decode directly

.PARAMETER Diff
When processing a log file, compare TX vs RX to show lost objects

.EXAMPLE
.\decode_object_packets.ps1 -LogFile temp\host_log.txt
.\decode_object_packets.ps1 -LogFile temp\combined_log.txt -Diff
.\decode_object_packets.ps1 -Hex "0b01020304..."
#>
param(
    [Parameter(Position = 0)]
    [string]$LogFile,
    [string]$Hex,
    [switch]$Diff
)

Set-StrictMode -Version Latest

# --- Constants ---
$UPID_OBJECT_DATA = 0x0b
$SIZEOF_OBJECT_RW = 264
$PER_OBJ_HEADER = 9  # 4 (local objnum) + 1 (owner) + 4 (remote objnum)

$OBJ_TYPE_NAMES = @{
    0 = "WALL"; 1 = "FIREBALL"; 2 = "ROBOT"; 3 = "HOSTAGE"; 4 = "PLAYER"
    5 = "WEAPON"; 6 = "CAMERA"; 7 = "POWERUP"; 8 = "DEBRIS"; 9 = "CNTRLCEN"
    10 = "FLARE"; 11 = "CLUTTER"; 12 = "GHOST"; 13 = "LIGHT"; 14 = "COOP"
    15 = "MARKER"; 255 = "NONE"
}

# object_rw field offsets (packed, no WORDS_NEED_ALIGNMENT):
# int signature        @ 0  (4)
# ubyte type           @ 4  (1)
# ubyte id             @ 5  (1)
# short next           @ 6  (2)
# short prev           @ 8  (2)
# ubyte control_type   @ 10 (1)
# ubyte movement_type  @ 11 (1)
# ubyte render_type    @ 12 (1)
# ubyte flags          @ 13 (1)
# short segnum         @ 14 (2)
# short attached_obj   @ 16 (2)
# vms_vector pos       @ 18 (12) = 3x int32
# fix size             @ 66 (4)
# fix shields          @ 70 (4)

function ConvertFrom-HexBytes {
    param([string]$HexString)
    $bytes = [byte[]]::new($HexString.Length / 2)
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        $bytes[$i] = [Convert]::ToByte($HexString.Substring($i * 2, 2), 16)
    }
    return $bytes
}

function Get-LEInt32 {
    param([byte[]]$Data, [int]$Offset)
    return [BitConverter]::ToInt32($Data, $Offset)
}

function Get-LEUInt32 {
    param([byte[]]$Data, [int]$Offset)
    return [BitConverter]::ToUInt32($Data, $Offset)
}

function Get-LEInt16 {
    param([byte[]]$Data, [int]$Offset)
    return [BitConverter]::ToInt16($Data, $Offset)
}

function ConvertFrom-Fix {
    param([int]$FixVal)
    return [math]::Round($FixVal / 65536.0, 2)
}

function Decode-ObjectRwFields {
    param([byte[]]$Data, [int]$Offset)
    if (($Offset + 94) -gt $Data.Length) {
        return @{ Error = "body too short (avail=$($Data.Length - $Offset))" }
    }
    $typeNum = $Data[$Offset + 4]
    $typeName = if ($OBJ_TYPE_NAMES.ContainsKey([int]$typeNum)) { $OBJ_TYPE_NAMES[[int]$typeNum] } else { "?$typeNum" }
    $segnum = Get-LEInt16 $Data ($Offset + 14)
    $px = ConvertFrom-Fix (Get-LEInt32 $Data ($Offset + 18))
    $py = ConvertFrom-Fix (Get-LEInt32 $Data ($Offset + 22))
    $pz = ConvertFrom-Fix (Get-LEInt32 $Data ($Offset + 26))
    $shields = ConvertFrom-Fix (Get-LEInt32 $Data ($Offset + 70))
    return @{
        Sig         = Get-LEInt32 $Data $Offset
        Type        = $typeName
        TypeNum     = [int]$typeNum
        Id          = [int]$Data[$Offset + 5]
        Segnum      = $segnum
        CtrlType    = [int]$Data[$Offset + 10]
        MoveType    = [int]$Data[$Offset + 11]
        RenderType  = [int]$Data[$Offset + 12]
        Flags       = [int]$Data[$Offset + 13]
        Pos         = "($px, $py, $pz)"
        Shields     = $shields
    }
}

function Decode-Packet {
    param([byte[]]$Data, [string]$Label = "")

    $result = [ordered]@{
        Label        = $Label
        RawLen       = $Data.Length
        Objects      = [System.Collections.ArrayList]::new()
        Truncated    = $false
        TruncatedAt  = ""
    }

    if ($Data.Length -lt 9) {
        $result.Error = "packet too short ($($Data.Length) < 9)"
        return $result
    }

    $pktType = $Data[0]
    $token = Get-LEUInt32 $Data 1
    $nobj = Get-LEInt32 $Data 5
    $result.PktType = $pktType
    $result.Token = "0x{0:x8}" -f $token
    $result.NObjDeclared = $nobj

    if ($pktType -ne $UPID_OBJECT_DATA) {
        $result.Error = "not UPID_OBJECT_DATA (got 0x{0:x2})" -f $pktType
        return $result
    }

    $loc = 9
    $parsed = 0

    for ($i = 0; $i -lt $nobj; $i++) {
        if (($loc + $PER_OBJ_HEADER) -gt $Data.Length) {
            $result.Truncated = $true
            $result.TruncatedAt = "header $i/$nobj, loc=$loc, avail=$($Data.Length)"
            break
        }

        $localObjnum = Get-LEInt32 $Data $loc;       $loc += 4
        $owner = [sbyte]$Data[$loc];                  $loc += 1
        $remoteObjnum = Get-LEInt32 $Data $loc;       $loc += 4

        $entry = [ordered]@{ EntryIdx = $i }

        if ($localObjnum -eq -1) {
            $entry.Marker = "INIT"
            $entry.PlayerNum = $owner
            $parsed++
            [void]$result.Objects.Add($entry)
            continue
        }
        elseif ($localObjnum -eq -2) {
            $entry.Marker = "END"
            $entry.PlayerNum = $owner
            $entry.TotalObjCount = $remoteObjnum
            $parsed++
            [void]$result.Objects.Add($entry)
            continue
        }

        $entry.LocalObjnum = $localObjnum
        $entry.Owner = $owner
        $entry.RemoteObjnum = $remoteObjnum

        if (($loc + $SIZEOF_OBJECT_RW) -gt $Data.Length) {
            $result.Truncated = $true
            $result.TruncatedAt = "body $i/$nobj, loc=$loc, need=$($loc + $SIZEOF_OBJECT_RW), avail=$($Data.Length)"
            $entry.BodyTruncated = $true
            # partial decode if we have at least 16 bytes
            $avail = $Data.Length - $loc
            if ($avail -ge 16) {
                $entry.PartialType = if ($OBJ_TYPE_NAMES.ContainsKey([int]$Data[$loc + 4])) { $OBJ_TYPE_NAMES[[int]$Data[$loc + 4]] } else { "?$($Data[$loc+4])" }
                $entry.PartialId = [int]$Data[$loc + 5]
                $entry.PartialSegnum = Get-LEInt16 $Data ($loc + 14)
            }
            $parsed++
            [void]$result.Objects.Add($entry)
            break
        }

        $fields = Decode-ObjectRwFields $Data $loc
        $loc += $SIZEOF_OBJECT_RW
        foreach ($k in $fields.Keys) { $entry[$k] = $fields[$k] }
        $parsed++
        [void]$result.Objects.Add($entry)
    }

    $result.ObjectsParsed = $parsed
    return $result
}

function Format-ObjectEntry {
    param($Obj, [string]$Indent = "  ")
    if ($Obj.Contains("Marker")) {
        if ($Obj.Marker -eq "INIT") {
            return "$Indent[INIT] player_num=$($Obj.PlayerNum)"
        }
        return "$Indent[END] player_num=$($Obj.PlayerNum) total_count=$($Obj.TotalObjCount)"
    }

    $local = "{0,4}" -f $Obj.LocalObjnum
    $remote = "{0,4}" -f $Obj.RemoteObjnum
    $owner = "{0,2}" -f $Obj.Owner

    if ($Obj.Contains("BodyTruncated") -and $Obj.BodyTruncated) {
        $pt = if ($Obj.Contains("PartialType")) { $Obj.PartialType } else { "?" }
        $pi = if ($Obj.Contains("PartialId")) { $Obj.PartialId } else { "?" }
        $ps = if ($Obj.Contains("PartialSegnum")) { $Obj.PartialSegnum } else { "?" }
        return "${Indent}local=$local remote=$remote owner=$owner TRUNCATED (partial: type=$pt id=$pi seg=$ps)"
    }

    $type = "{0,-10}" -f $Obj.Type
    $id = "{0,3}" -f $Obj.Id
    $seg = "{0,4}" -f $Obj.Segnum
    $sh = "{0,8:F1}" -f $Obj.Shields
    return "${Indent}local=$local remote=$remote owner=$owner type=$type id=$id seg=$seg shields=$sh"
}

function Format-Packet {
    param($Pkt)
    $lines = [System.Collections.ArrayList]::new()
    [void]$lines.Add("--- $($Pkt.Label) ---  len=$($Pkt.RawLen)  token=$($Pkt.Token)  nobj=$($Pkt.NObjDeclared)")
    if ($Pkt.Contains("Error")) {
        [void]$lines.Add("  ERROR: $($Pkt.Error)")
        return $lines -join "`n"
    }
    foreach ($obj in $Pkt.Objects) {
        [void]$lines.Add((Format-ObjectEntry $obj))
    }
    if ($Pkt.Truncated) {
        [void]$lines.Add("  *** TRUNCATED: $($Pkt.TruncatedAt)")
    }
    return $lines -join "`n"
}

function Get-PktDumpLines {
    param([string]$LogText)
    $pattern = 'PKTDUMP\s+(TX|RX)\s+len=(\d+)\s+([0-9a-fA-F]+)'
    $results = [System.Collections.ArrayList]::new()
    foreach ($line in $LogText -split "`n") {
        if ($line -match $pattern) {
            [void]$results.Add(@{
                Direction   = $Matches[1]
                DeclaredLen = [int]$Matches[2]
                HexData     = $Matches[3]
            })
        }
    }
    return $results
}

function Show-Diff {
    param($TxPackets, $RxPackets)
    Write-Host "`n=== TX vs RX Comparison ===" -ForegroundColor Cyan
    # Collect TX objects
    $txObjects = [ordered]@{}
    $txEnd = $null
    foreach ($pkt in $TxPackets) {
        foreach ($obj in $pkt.Objects) {
            if ($obj.Contains("Marker")) {
                if ($obj.Marker -eq "END") { $txEnd = $obj }
                continue
            }
            $txObjects[$obj.LocalObjnum] = $obj
        }
    }
    # Collect RX objects
    $rxObjects = [ordered]@{}
    $rxTruncCount = 0
    foreach ($pkt in $RxPackets) {
        foreach ($obj in $pkt.Objects) {
            if ($obj.Contains("Marker")) { continue }
            if ($obj.Contains("BodyTruncated") -and $obj.BodyTruncated) {
                $rxTruncCount++
                continue
            }
            $rxObjects[$obj.LocalObjnum] = $obj
        }
    }

    Write-Host "TX sent $($txObjects.Count) objects"
    if ($txEnd) { Write-Host "TX END marker: total_count=$($txEnd.TotalObjCount)" }
    Write-Host "RX received $($rxObjects.Count) complete objects, $rxTruncCount partially truncated"

    $missing = [System.Collections.ArrayList]::new()
    foreach ($key in $txObjects.Keys) {
        if (-not $rxObjects.Contains($key)) {
            [void]$missing.Add($txObjects[$key])
        }
    }

    if ($missing.Count -gt 0) {
        Write-Host "`n--- $($missing.Count) objects LOST (sent but not received) ---" -ForegroundColor Red
        foreach ($obj in $missing) {
            Write-Host (Format-ObjectEntry $obj)
        }
    }
    else {
        Write-Host "`nNo objects lost" -ForegroundColor Green
    }

    $truncPkts = @($RxPackets | Where-Object { $_.Truncated })
    if ($truncPkts.Count -gt 0) {
        Write-Host "`n--- $($truncPkts.Count) RX packets were truncated ---" -ForegroundColor Yellow
        foreach ($p in $truncPkts) {
            Write-Host "  $($p.Label): declared=$($p.NObjDeclared), truncated at: $($p.TruncatedAt)"
        }
    }
}

# --- Main ---

if ($Hex) {
    $data = ConvertFrom-HexBytes $Hex
    $pkt = Decode-Packet $data "CLI"
    Write-Host (Format-Packet $pkt)
    exit 0
}

if (-not $LogFile) {
    Write-Host @"
Usage:
  .\decode_object_packets.ps1 -LogFile <logfile>        -- decode all PKTDUMP lines
  .\decode_object_packets.ps1 -LogFile <logfile> -Diff   -- compare TX vs RX
  .\decode_object_packets.ps1 -Hex <hexstring>           -- decode a single hex packet
"@
    exit 1
}

if (-not (Test-Path $LogFile)) {
    Write-Host "File not found: $LogFile"
    exit 1
}

$logText = Get-Content $LogFile -Raw -Encoding utf8
$entries = Get-PktDumpLines $logText

if ($entries.Count -eq 0) {
    Write-Host "No PKTDUMP lines found in $LogFile"
    exit 1
}

Write-Host "Found $($entries.Count) PKTDUMP lines`n"

$txPackets = [System.Collections.ArrayList]::new()
$rxPackets = [System.Collections.ArrayList]::new()

for ($idx = 0; $idx -lt $entries.Count; $idx++) {
    $e = $entries[$idx]
    $actualBytes = $e.HexData.Length / 2
    $label = "PKT#$idx $($e.Direction) (declared=$($e.DeclaredLen), hex_bytes=$actualBytes)"
    $data = ConvertFrom-HexBytes $e.HexData
    $pkt = Decode-Packet $data $label

    if ($e.DeclaredLen -ne $actualBytes) {
        $pkt.LenMismatch = "declared=$($e.DeclaredLen) actual_hex=$actualBytes"
    }

    if ($e.Direction -eq "TX") {
        [void]$txPackets.Add($pkt)
    }
    else {
        [void]$rxPackets.Add($pkt)
    }

    if (-not $Diff) {
        Write-Host (Format-Packet $pkt)
        if ($pkt.Contains("LenMismatch")) {
            Write-Host "  !! LENGTH MISMATCH: $($pkt.LenMismatch)" -ForegroundColor Red
        }
        Write-Host ""
    }
}

if ($Diff -and $txPackets.Count -gt 0 -and $rxPackets.Count -gt 0) {
    Show-Diff $txPackets $rxPackets
}
elseif ($Diff) {
    Write-Host "Need both TX and RX packets for diff mode"
    foreach ($pkt in ($txPackets + $rxPackets)) {
        Write-Host (Format-Packet $pkt)
        Write-Host ""
    }
}

# Summary
Write-Host "`n=== Summary ===" -ForegroundColor Cyan
Write-Host "TX packets: $($txPackets.Count)"
Write-Host "RX packets: $($rxPackets.Count)"
$txObjCount = ($txPackets | ForEach-Object {
    ($_.Objects | Where-Object { -not $_.Contains("Marker") -and -not ($_.Contains("BodyTruncated") -and $_.BodyTruncated) }).Count
} | Measure-Object -Sum).Sum
$rxObjCount = ($rxPackets | ForEach-Object {
    ($_.Objects | Where-Object { -not $_.Contains("Marker") -and -not ($_.Contains("BodyTruncated") -and $_.BodyTruncated) }).Count
} | Measure-Object -Sum).Sum
$rxTrunc = @($rxPackets | Where-Object { $_.Truncated }).Count
Write-Host "TX objects: $txObjCount"
Write-Host "RX objects: $rxObjCount ($rxTrunc packets truncated)"
if ($txObjCount -gt $rxObjCount) {
    Write-Host "LOST: $($txObjCount - $rxObjCount) objects" -ForegroundColor Red
}

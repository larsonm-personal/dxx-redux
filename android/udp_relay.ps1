# udp_relay.ps1 -- UDP relay to bridge two Android emulators for MP testing.
#
# Pure PowerShell/.NET reimplementation of udp_relay.py. No Python dependency.
#
# Setup:
#   EMU1 redir: host:42500 -> EMU1:42424  (host game, inbound only)
#   EMU2: no redir needed (redir blocks outbound from game port 42424)
#
# Joiner (EMU2) connects to 10.0.2.2:42600 (this relay).
# Relay forwards to EMU1 via redir (127.0.0.1:42500).
# EMU1 replies come back through the redir session to this relay.
# Relay forwards replies to EMU2 via its learned NAT address.

param(
    [string]$Bind = "0.0.0.0",
    [int]$Port = 42600,
    [string]$Emu1Host = "127.0.0.1",
    [int]$Emu1Port = 42500
)

$sock = New-Object System.Net.Sockets.UdpClient
$sock.Client.SetSocketOption(
    [System.Net.Sockets.SocketOptionLevel]::Socket,
    [System.Net.Sockets.SocketOptionName]::ReuseAddress, $true)
$sock.Client.Bind([System.Net.IPEndPoint]::new(
        [System.Net.IPAddress]::Parse($Bind), $Port))
$sock.Client.ReceiveTimeout = 1000

$emu1Redir = [System.Net.IPEndPoint]::new(
    [System.Net.IPAddress]::Parse($Emu1Host), $Emu1Port)
$emu2Addr = $null
$emu1RedirAddr = $null
$pktCount = 0
$t0 = [System.Diagnostics.Stopwatch]::StartNew()

Write-Host "UDP relay listening on ${Bind}:${Port}"
Write-Host "  EMU1 (host)  redir: ${Emu1Host}:${Emu1Port}"
Write-Host "  EMU2 (join)  via NAT (no redir)"

try {
    while ($true) {
        $remoteEP = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
        try {
            $data = $sock.Receive([ref]$remoteEP)
        } catch [System.Net.Sockets.SocketException] {
            if ($_.Exception.SocketErrorCode -eq [System.Net.Sockets.SocketError]::TimedOut) {
                continue
            }
            throw
        }
        $pktCount++
        $elapsed = "{0:F1}" -f $t0.Elapsed.TotalSeconds
        $pid0 = $data[0]

        if ($null -eq $emu2Addr) {
            $emu2Addr = $remoteEP
            Write-Host "  [$pktCount] +${elapsed}s EMU2 joiner identified: $remoteEP"
        }

        if ($remoteEP.Equals($emu2Addr)) {
            $sock.Send($data, $data.Length, $emu1Redir) | Out-Null
            if ($pktCount -le 200 -or $pktCount % 50 -eq 0) {
                Write-Host "  [$pktCount] +${elapsed}s EMU2->EMU1: $($data.Length) bytes (pid=$pid0)"
            }
        } else {
            if ($null -eq $emu1RedirAddr) {
                $emu1RedirAddr = $remoteEP
                Write-Host "  [$pktCount] +${elapsed}s EMU1 host identified: $remoteEP"
            }
            if ($null -ne $emu2Addr) {
                $sock.Send($data, $data.Length, $emu2Addr) | Out-Null
            }
            if ($pktCount -le 200 -or $pktCount % 50 -eq 0) {
                Write-Host "  [$pktCount] +${elapsed}s EMU1->EMU2: $($data.Length) bytes (pid=$pid0)"
            }
        }
    }
} finally {
    $sock.Close()
    Write-Host "Relay stopped"
}

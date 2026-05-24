#!/usr/bin/env pwsh
# test_lan_discovery.ps1 -- LAN lobby discovery test bot
# Sends ANNOUNCE packets on UDP port 42400 so the Android app can discover them.
# Also listens for JOIN/LEAVE/READY packets from clients.
#
# Usage:
#   .\android\tests\test_lan_discovery.ps1 [-Callsign "TestHost"] [-Game "d2"] [-Mission "Counterstrike!"] [-Mode "coop"]
#
# When running with the Android emulator:
#   The emulator's network is bridged. Broadcasts from the host reach the emulator
#   if both are on the same subnet. However, 255.255.255.255 broadcasts from the
#   emulator may not reach the host. To test, this script listens on 0.0.0.0 and
#   also sends directed packets to 10.0.2.15 (default emulator IP) if specified.

param(
    [string]$Callsign = "TestHost",
    [string]$Game = "d2",
    [string]$Mission = "Counterstrike!",
    [string]$Mode = "coop",
    [int]$MaxPlayers = 4,
    [int]$Port = 42400,
    [int]$DurationSec = 30
)

$ErrorActionPreference = "Stop"

$lobbyId = [guid]::NewGuid().ToString()
$players = @(@{ callsign = $Callsign; address = "127.0.0.1"; ready = $false })

function Build-Announce {
    $obj = @{
        type = "ANNOUNCE"
        lobby_id = $lobbyId
        callsign = $Callsign
        game = $Game
        mission = $Mission
        mode = $Mode
        player_count = $players.Count
        max_players = $MaxPlayers
    }
    return ($obj | ConvertTo-Json -Compress)
}

function Build-PlayerList {
    $obj = @{
        type = "PLAYER_LIST"
        lobby_id = $lobbyId
        players = $players
    }
    return ($obj | ConvertTo-Json -Compress)
}

Write-Host "LAN Discovery Test Bot"
Write-Host "  Lobby ID: $lobbyId"
Write-Host "  Callsign: $Callsign"
Write-Host "  Game: $Game, Mission: $Mission, Mode: $Mode"
Write-Host "  Broadcasting on UDP port $Port for ${DurationSec}s"
Write-Host ""

$udp = New-Object System.Net.Sockets.UdpClient
$udp.Client.SetSocketOption(
    [System.Net.Sockets.SocketOptionLevel]::Socket,
    [System.Net.Sockets.SocketOptionName]::ReuseAddress, $true)
$udp.Client.SetSocketOption(
    [System.Net.Sockets.SocketOptionLevel]::Socket,
    [System.Net.Sockets.SocketOptionName]::Broadcast, $true)
$udp.Client.Bind([System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, $Port))
$udp.Client.ReceiveTimeout = 500

$broadcastEp = [System.Net.IPEndPoint]::new(
    [System.Net.IPAddress]::Broadcast, $Port)

$deadline = (Get-Date).AddSeconds($DurationSec)
$lastAnnounce = [datetime]::MinValue
$announceInterval = [timespan]::FromSeconds(3)

try {
    while ((Get-Date) -lt $deadline) {
        # Broadcast announce every 3 seconds
        if (((Get-Date) - $lastAnnounce) -ge $announceInterval) {
            $json = Build-Announce
            $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
            try {
                $udp.Send($bytes, $bytes.Length, $broadcastEp) | Out-Null
            } catch {
                # Broadcast may fail on some network configs, ignore
            }
            $lastAnnounce = Get-Date
            Write-Host "[$(Get-Date -Format 'HH:mm:ss')] ANNOUNCE sent ($($players.Count) players)"
        }

        # Listen for incoming packets
        try {
            $remoteEp = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
            $recvBytes = $udp.Receive([ref]$remoteEp)
            $text = [System.Text.Encoding]::UTF8.GetString($recvBytes)
            $msg = $text | ConvertFrom-Json

            $sender = $remoteEp.Address.ToString()
            Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Received $($msg.type) from $sender"

            switch ($msg.type) {
                "JOIN" {
                    if ($msg.lobby_id -eq $lobbyId) {
                        $existing = $players | Where-Object { $_.callsign -eq $msg.callsign }
                        if (-not $existing -and $players.Count -lt $MaxPlayers) {
                            $players += @{ callsign = $msg.callsign; address = $sender; ready = $false }
                            Write-Host "  -> Player joined: $($msg.callsign) from $sender"
                            # Send player list update to all
                            $plJson = Build-PlayerList
                            $plBytes = [System.Text.Encoding]::UTF8.GetBytes($plJson)
                            foreach ($p in $players) {
                                if ($p.address -ne "127.0.0.1") {
                                    $ep = [System.Net.IPEndPoint]::new(
                                        [System.Net.IPAddress]::Parse($p.address), $Port)
                                    $udp.Send($plBytes, $plBytes.Length, $ep) | Out-Null
                                }
                            }
                        }
                    }
                }
                "LEAVE" {
                    if ($msg.lobby_id -eq $lobbyId) {
                        $players = @($players | Where-Object { $_.callsign -ne $msg.callsign })
                        Write-Host "  -> Player left: $($msg.callsign)"
                    }
                }
                "READY" {
                    if ($msg.lobby_id -eq $lobbyId) {
                        $players = @($players | ForEach-Object {
                                if ($_.callsign -eq $msg.callsign) {
                                    $_.ready = $msg.ready
                                }
                                $_
                            })
                        Write-Host "  -> Ready update: $($msg.callsign) = $($msg.ready)"
                    }
                }
                "PING" {
                    # Respond with PONG
                    $pong = @{ type = "PONG"; lobby_id = $msg.lobby_id; ts = $msg.ts } | ConvertTo-Json -Compress
                    $pongBytes = [System.Text.Encoding]::UTF8.GetBytes($pong)
                    $ep = [System.Net.IPEndPoint]::new(
                        [System.Net.IPAddress]::Parse($sender), $Port)
                    $udp.Send($pongBytes, $pongBytes.Length, $ep) | Out-Null
                }
                "ANNOUNCE" {
                    # Another host on the network, just log it
                    Write-Host "  -> Another host: $($msg.callsign) ($($msg.mission))"
                }
            }
        } catch [System.Net.Sockets.SocketException] {
            # Timeout, normal
        }
    }
} finally {
    $udp.Close()
    Write-Host ""
    Write-Host "Test bot stopped"
}

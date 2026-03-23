<#
.SYNOPSIS
    WebSocket test bot that connects to the matchmaking server as a second player.
    Use this alongside the emulator to test two-client interactions.

.DESCRIPTION
    Connects to the server, authenticates, and either creates a lobby or joins one.
    Stays connected so the emulator client can see this player in the lobby list,
    join the lobby, exchange chat messages, etc.

.PARAMETER ServerUrl
    WebSocket URL of the matchmaking server (default: ws://127.0.0.1:9000/ws)

.PARAMETER Callsign
    Player name for this bot (default: BotPlayer)

.PARAMETER Action
    What the bot should do after connecting:
      list    - list lobbies and exit
      create  - create a lobby and wait for others to join
      join    - join the first available lobby
      idle    - just connect and sit in the browser
    (default: create)

.PARAMETER Game
    Game type for lobby creation (default: d2)

.PARAMETER Mission
    Mission name for lobby creation (default: Counterstrike!)

.EXAMPLE
    .\test_bot_client.ps1
    .\test_bot_client.ps1 -Callsign "Player2" -Action join
    .\test_bot_client.ps1 -Action list
#>

param(
    [string]$ServerUrl = "ws://127.0.0.1:9000/ws",
    [string]$Callsign = "BotPlayer",
    [ValidateSet("list", "create", "join", "idle")]
    [string]$Action = "create",
    [string]$Game = "d2",
    [string]$Mission = "Counterstrike!"
)

$ErrorActionPreference = "Stop"

# --- Check/start matchmaking server ---
$serverHost = ([Uri]$ServerUrl).Host
$serverPort = ([Uri]$ServerUrl).Port
$script:autoServerProc = $null

function Test-ServerReachable {
    try {
        $tcp = [System.Net.Sockets.TcpClient]::new()
        $tcp.Connect($serverHost, $serverPort)
        $tcp.Close()
        return $true
    } catch { return $false }
}

if (-not (Test-ServerReachable)) {
    # Always auto-start server when not reachable
        $repoRoot = Split-Path (Split-Path $PSScriptRoot)
        $serverDir = Join-Path $repoRoot "server"
        $serverBin = Join-Path $serverDir "target\release\dxx-matchmaking.exe"
        if (-not (Test-Path $serverBin)) {
            $serverBin = Join-Path $serverDir "target\debug\dxx-matchmaking.exe"
        }
        if (-not (Test-Path $serverBin)) {
            Write-Host "Building matchmaking server..." -ForegroundColor Yellow
            Push-Location $serverDir
            & cargo build --release 2>&1 | Out-Null
            Pop-Location
            $serverBin = Join-Path $serverDir "target\release\dxx-matchmaking.exe"
        }
        if (Test-Path $serverBin) {
            $psi = New-Object System.Diagnostics.ProcessStartInfo
            $psi.FileName = $serverBin
            $psi.WorkingDirectory = $serverDir
            $psi.RedirectStandardOutput = $true
            $psi.RedirectStandardError = $true
            $psi.UseShellExecute = $false
            $psi.CreateNoWindow = $true
            $psi.EnvironmentVariables["SKIP_GPGS_VERIFY"] = "true"
            $psi.EnvironmentVariables["RUST_LOG"] = "info"
            $script:autoServerProc = [System.Diagnostics.Process]::Start($psi)
            $sw = [System.Diagnostics.Stopwatch]::StartNew()
            while ($sw.Elapsed.TotalSeconds -lt 10) {
                if (Test-ServerReachable) { break }
                Start-Sleep -Seconds 1
            }
            if (Test-ServerReachable) {
                Write-Host "Auto-started server (PID $($script:autoServerProc.Id))" -ForegroundColor Green
            } else {
                Write-Host "FAIL: Could not auto-start server" -ForegroundColor Red
                try { $script:autoServerProc.Kill() } catch {}
                exit 1
            }
        } else {
            Write-Host "FAIL: No server binary found. Run 'cargo build' in server/" -ForegroundColor Red
            exit 1
        }
}

# --- WebSocket helpers ---

function Connect-WebSocket {
    param([string]$Url)
    $ws = [System.Net.WebSockets.ClientWebSocket]::new()
    $ws.Options.KeepAliveInterval = [TimeSpan]::FromSeconds(30)
    $uri = [Uri]::new($Url)
    $ct = [System.Threading.CancellationToken]::None
    $null = $ws.ConnectAsync($uri, $ct).GetAwaiter().GetResult()
    return $ws
}

function Send-WsMessage {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Ws,
        [string]$Json
    )
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Json)
    $seg = [ArraySegment[byte]]::new($bytes)
    $ct = [System.Threading.CancellationToken]::None
    $null = $Ws.SendAsync($seg, [System.Net.WebSockets.WebSocketMessageType]::Text, $true, $ct).GetAwaiter().GetResult()
    Write-Host "TX: $Json" -ForegroundColor DarkGray
}

function Receive-WsMessage {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Ws,
        [int]$TimeoutMs = 0  # 0 = no timeout (blocks until message or close)
    )
    $buf = [byte[]]::new(65536)
    $seg = [ArraySegment[byte]]::new($buf)
    # NOTE: Using CancellationToken with a timeout will ABORT the WebSocket when
    # cancelled. Only use timeouts for short-lived drain operations, not main loops.
    if ($TimeoutMs -gt 0) {
        $cts = [System.Threading.CancellationTokenSource]::new($TimeoutMs)
        $token = $cts.Token
    } else {
        $cts = $null
        $token = [System.Threading.CancellationToken]::None
    }
    try {
        $result = $Ws.ReceiveAsync($seg, $token).GetAwaiter().GetResult()
    } catch [System.OperationCanceledException] {
        return $null
    } catch {
        Write-Host "ReceiveAsync error: $($_.Exception.GetType().Name): $($_.Exception.Message)" -ForegroundColor Red
        return $null
    } finally {
        if ($null -ne $cts) { $cts.Dispose() }
    }
    if ($result.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Close) {
        return $null
    }
    $text = [System.Text.Encoding]::UTF8.GetString($buf, 0, $result.Count)
    Write-Host "RX: $text" -ForegroundColor Cyan
    return ($text | ConvertFrom-Json)
}

function Send-AndReceive {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Ws,
        [hashtable]$Msg
    )
    $json = $Msg | ConvertTo-Json -Compress -Depth 10
    Send-WsMessage -Ws $Ws -Json $json
    return (Receive-WsMessage -Ws $Ws)
}

# Drain all pending messages (non-blocking), return them as an array.
# WARNING: timeout-based drain will abort the WebSocket if it fires. Only use
# this when you're done with the connection or will reconnect.
function Drain-Messages {
    param([System.Net.WebSockets.ClientWebSocket]$Ws)
    $msgs = @()
    while ($true) {
        $m = Receive-WsMessage -Ws $Ws -TimeoutMs 500
        if ($null -eq $m) { break }
        $msgs += $m
    }
    return $msgs
}

# --- Main ---

Write-Host "=== DXX-Redux Test Bot ===" -ForegroundColor Green
Write-Host "Server:   $ServerUrl"
Write-Host "Callsign: $Callsign"
Write-Host "Action:   $Action"
Write-Host ""

# Connect
Write-Host "Connecting..." -ForegroundColor Yellow
$ws = Connect-WebSocket -Url $ServerUrl
Write-Host "Connected." -ForegroundColor Green

# Authenticate
$devToken = "dev-bot-$([guid]::NewGuid().ToString())"
$authMsg = @{
    type = "AUTHENTICATE"
    protocol_version = 1
    client_version = "bot-0.1.0"
    play_games_token = $devToken
    callsign = $Callsign
    platform = "bot"
    auth_method = "gpgs"
}
Send-WsMessage -Ws $ws -Json ($authMsg | ConvertTo-Json -Compress -Depth 10)

# Read welcome bundle (AUTH_OK, MOTD, SERVER_STATUS, LOBBY_LIST, FRIEND_LIST)
Write-Host "Waiting for auth..." -ForegroundColor Yellow
$playerId = $null
$welcomeMsgs = @()
for ($i = 0; $i -lt 10; $i++) {
    $m = Receive-WsMessage -Ws $ws
    if ($null -eq $m) { break }
    $welcomeMsgs += $m
    if ($m.type -eq "AUTH_OK") {
        $playerId = $m.player_id
        Write-Host "Authenticated! Player ID: $playerId" -ForegroundColor Green
    }
    if ($m.type -eq "AUTH_FAIL") {
        Write-Host "Auth failed: $($m.reason)" -ForegroundColor Red
        exit 1
    }
    # Welcome bundle is 5 msgs: AUTH_OK, MOTD, SERVER_STATUS, LOBBY_LIST, FRIEND_LIST
    if ($welcomeMsgs.Count -ge 5 -and $null -ne $playerId) { break }
}

if ($null -eq $playerId) {
    Write-Host "Failed to authenticate (no AUTH_OK received)" -ForegroundColor Red
    exit 1
}

# Show welcome info
foreach ($m in $welcomeMsgs) {
    switch ($m.type) {
        "SERVER_STATUS" {
            Write-Host "Server: $($m.online_players) online, $($m.active_games_count) games" -ForegroundColor DarkCyan
        }
        "MOTD" {
            Write-Host "MOTD: $($m.message)" -ForegroundColor DarkCyan
        }
    }
}

# Execute action
switch ($Action) {
    "list" {
        $resp = Send-AndReceive -Ws $ws -Msg @{ type = "LIST_LOBBIES" }
        if ($resp.type -eq "LOBBY_LIST") {
            if ($resp.lobbies.Count -eq 0) {
                Write-Host "No lobbies found." -ForegroundColor Yellow
            } else {
                Write-Host "Lobbies ($($resp.lobbies.Count)):" -ForegroundColor Green
                foreach ($lobby in $resp.lobbies) {
                    $joinable = if ($lobby.joinable) { "joinable" } else { "full" }
                    Write-Host "  $($lobby.host_callsign) - $($lobby.mission) ($($lobby.mode)) [$($lobby.player_count)/$($lobby.max_players)] $joinable"
                }
            }
        }
    }

    "create" {
        Write-Host "Creating lobby..." -ForegroundColor Yellow
        $createMsg = @{
            type = "CREATE_LOBBY"
            game = $Game
            mission = $Mission
            mode = "anarchy"
            max_players = 4
        }
        $resp = Send-AndReceive -Ws $ws -Msg $createMsg
        if ($resp.type -eq "LOBBY_UPDATE") {
            Write-Host "Lobby created: $($resp.lobby_id)" -ForegroundColor Green
            Write-Host "Players: $($resp.players.Count)" -ForegroundColor Green
            Write-Host ""
            Write-Host "Waiting for other players to join... (Ctrl+C to quit)" -ForegroundColor Yellow
            Write-Host ""

            # Stay connected and react to lobby events (blocks on receive, no timeout)
            while ($ws.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
                $m = Receive-WsMessage -Ws $ws
                if ($null -eq $m) { continue }
                switch ($m.type) {
                    "LOBBY_UPDATE" {
                        Write-Host "Lobby update: $($m.players.Count) players" -ForegroundColor Green
                        foreach ($p in $m.players) {
                            $readyStr = if ($p.ready) { "[READY]" } else { "[not ready]" }
                            Write-Host "  $($p.callsign) $readyStr"
                        }
                    }
                    "MESSAGE_RECEIVED" {
                        Write-Host "Chat from $($m.from_callsign): $($m.text)" -ForegroundColor Magenta
                        # Auto-reply
                        $replyMsg = @{
                            type = "SEND_MESSAGE"
                            target_player_id = $m.from_player_id
                            text = "Bot auto-reply: got your message!"
                        }
                        Send-WsMessage -Ws $ws -Json ($replyMsg | ConvertTo-Json -Compress -Depth 10)
                        # MESSAGE_SENT ack will arrive in the next loop iteration
                    }
                    "GAME_STARTING" {
                        Write-Host "Game starting! Mission: $($m.mission)" -ForegroundColor Green
                    }
                    "ERROR" {
                        Write-Host "Error: $($m.code) - $($m.message)" -ForegroundColor Red
                    }
                    default {
                        Write-Host "[$($m.type)]" -ForegroundColor DarkGray
                    }
                }
            }
        }
    }

    "join" {
        $resp = Send-AndReceive -Ws $ws -Msg @{ type = "LIST_LOBBIES" }
        if ($resp.type -ne "LOBBY_LIST" -or $resp.lobbies.Count -eq 0) {
            Write-Host "No lobbies to join." -ForegroundColor Yellow
            exit 0
        }
        $target = $resp.lobbies | Where-Object { $_.joinable } | Select-Object -First 1
        if ($null -eq $target) {
            Write-Host "No joinable lobbies found." -ForegroundColor Yellow
            exit 0
        }
        Write-Host "Joining lobby hosted by $($target.host_callsign)..." -ForegroundColor Yellow
        $joinMsg = @{
            type = "JOIN_LOBBY"
            lobby_id = $target.lobby_id
        }
        Send-WsMessage -Ws $ws -Json ($joinMsg | ConvertTo-Json -Compress -Depth 10)

        # Listen for updates (blocks on receive, no timeout)
        Write-Host "In lobby. Listening... (Ctrl+C to quit)" -ForegroundColor Yellow
        while ($ws.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
            $m = Receive-WsMessage -Ws $ws
            if ($null -eq $m) { continue }
            switch ($m.type) {
                "LOBBY_UPDATE" {
                    Write-Host "Lobby update: $($m.players.Count) players" -ForegroundColor Green
                    foreach ($p in $m.players) {
                        $readyStr = if ($p.ready) { "[READY]" } else { "[not ready]" }
                        Write-Host "  $($p.callsign) $readyStr"
                    }
                }
                "MESSAGE_RECEIVED" {
                    Write-Host "Chat from $($m.from_callsign): $($m.text)" -ForegroundColor Magenta
                }
                "GAME_STARTING" {
                    Write-Host "Game starting! Mission: $($m.mission)" -ForegroundColor Green
                }
                "ERROR" {
                    Write-Host "Error: $($m.code) - $($m.message)" -ForegroundColor Red
                }
                default {
                    Write-Host "[$($m.type)]" -ForegroundColor DarkGray
                }
            }
        }
    }

    "idle" {
        Write-Host "Connected and idle. Listening... (Ctrl+C to quit)" -ForegroundColor Yellow
        while ($ws.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
            $m = Receive-WsMessage -Ws $ws
            if ($null -eq $m) { continue }
            Write-Host "[$($m.type)]" -ForegroundColor DarkGray
        }
    }
}

Write-Host "Bot disconnected." -ForegroundColor Yellow

# Cleanup auto-started server
if ($script:autoServerProc -and -not $script:autoServerProc.HasExited) {
    Write-Host "Stopping auto-started server (PID $($script:autoServerProc.Id))..." -ForegroundColor Yellow
    try { $script:autoServerProc.Kill(); $script:autoServerProc.WaitForExit(5000) } catch {}
}

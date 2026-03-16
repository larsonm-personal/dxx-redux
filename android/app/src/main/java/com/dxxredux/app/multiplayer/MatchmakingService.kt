package com.dxxredux.app.multiplayer

import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import java.util.concurrent.TimeUnit
import kotlin.math.min

private const val TAG = "MatchmakingService"

object MatchmakingService {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val state get() = MatchmakingStateHolder

    private val client =
        OkHttpClient
            .Builder()
            .pingInterval(30, TimeUnit.SECONDS)
            .readTimeout(0, TimeUnit.SECONDS) // no read timeout for websocket
            .build()

    private var webSocket: WebSocket? = null
    private var reconnectJob: Job? = null
    private var reconnectAttempt = 0
    private var manualDisconnect = false

    fun connect(
        serverUrl: String,
        callsign: String,
    ) {
        manualDisconnect = false
        reconnectJob?.cancel()
        webSocket?.close(NetworkConstants.CLOSE_NORMAL, null)

        state.update {
            it.copy(
                status = ConnectionStatus.CONNECTING,
                serverUrl = serverUrl,
                callsign = callsign,
                errorMessage = null,
            )
        }
        state.appendLog("Connecting to $serverUrl ...")

        val request = Request.Builder().url(serverUrl).build()
        webSocket = client.newWebSocket(request, Listener(callsign))
    }

    fun disconnect() {
        manualDisconnect = true
        reconnectJob?.cancel()
        webSocket?.close(NetworkConstants.CLOSE_NORMAL, "user disconnect")
        webSocket = null
        state.update {
            it.copy(
                status = ConnectionStatus.DISCONNECTED,
                playerId = null,
                sessionToken = null,
            )
        }
        state.appendLog("Disconnected.")
    }

    fun send(json: String) {
        val ws = webSocket
        if (ws == null) {
            Log.w(TAG, "send() called with no active WebSocket")
            return
        }
        Log.d(TAG, "TX: $json")
        ws.send(json)
    }

    fun sendAuthenticate(callsign: String) {
        val msg =
            AuthenticateMsg(
                callsign = callsign,
                playGamesToken = "dev-token", // placeholder for dev/skip_gpgs_verify mode
            )
        send(protocolJson.encodeToString(AuthenticateMsg.serializer(), msg))
    }

    fun requestLobbyList() {
        send(protocolJson.encodeToString(ListLobbiesMsg.serializer(), ListLobbiesMsg()))
    }

    fun createLobby(
        game: String,
        mission: String,
        mode: String,
        maxPlayers: Int,
    ) {
        val msg =
            CreateLobbyMsg(
                game = game,
                mission = mission,
                mode = mode,
                maxPlayers = maxPlayers,
            )
        send(protocolJson.encodeToString(CreateLobbyMsg.serializer(), msg))
    }

    private fun scheduleReconnect() {
        if (manualDisconnect) return
        reconnectJob?.cancel()
        reconnectJob =
            scope.launch {
                val delayMs =
                    min(
                        NetworkConstants.RECONNECT_BASE_DELAY_MS * (1L shl reconnectAttempt),
                        NetworkConstants.RECONNECT_MAX_DELAY_MS,
                    )
                state.update { it.copy(status = ConnectionStatus.RECONNECTING) }
                state.appendLog("Reconnecting in ${delayMs}ms (attempt ${reconnectAttempt + 1})...")
                delay(delayMs)
                reconnectAttempt++
                val s = state.state.value
                connect(s.serverUrl, s.callsign)
            }
    }

    private class Listener(
        private val callsign: String,
    ) : WebSocketListener() {
        override fun onOpen(
            webSocket: WebSocket,
            response: Response,
        ) {
            Log.i(TAG, "WebSocket open")
            reconnectAttempt = 0
            state.update { it.copy(status = ConnectionStatus.AUTHENTICATING) }
            state.appendLog("Connected. Authenticating as '$callsign'...")
            sendAuthenticate(callsign)
        }

        override fun onMessage(
            webSocket: WebSocket,
            text: String,
        ) {
            Log.d(TAG, "RX: $text")
            handleMessage(text)
        }

        override fun onClosing(
            webSocket: WebSocket,
            code: Int,
            reason: String,
        ) {
            Log.i(TAG, "WebSocket closing: $code $reason")
            webSocket.close(NetworkConstants.CLOSE_NORMAL, null)
        }

        override fun onClosed(
            webSocket: WebSocket,
            code: Int,
            reason: String,
        ) {
            Log.i(TAG, "WebSocket closed: $code $reason")
            this@MatchmakingService.webSocket = null
            state.update {
                it.copy(
                    status = ConnectionStatus.DISCONNECTED,
                    playerId = null,
                    sessionToken = null,
                )
            }
            state.appendLog("Connection closed ($code).")
            scheduleReconnect()
        }

        override fun onFailure(
            webSocket: WebSocket,
            t: Throwable,
            response: Response?,
        ) {
            Log.e(TAG, "WebSocket failure: ${t.message}", t)
            this@MatchmakingService.webSocket = null
            state.update {
                it.copy(
                    status = ConnectionStatus.DISCONNECTED,
                    errorMessage = t.message,
                    playerId = null,
                    sessionToken = null,
                )
            }
            state.appendLog("Error: ${t.message}")
            scheduleReconnect()
        }
    }

    private fun handleMessage(text: String) {
        when (val msg = ServerMessage.parse(text)) {
            is ServerMessage.AuthOkMsg -> {
                state.update {
                    it.copy(
                        status = ConnectionStatus.CONNECTED,
                        playerId = msg.data.playerId,
                        sessionToken = msg.data.sessionToken,
                        errorMessage = null,
                    )
                }
                state.appendLog("Authenticated! Player ID: ${msg.data.playerId}")
                // Auto-request lobby list after auth
                requestLobbyList()
            }

            is ServerMessage.AuthFailMsg -> {
                state.update {
                    it.copy(
                        status = ConnectionStatus.DISCONNECTED,
                        errorMessage = "Auth failed: ${msg.data.reason}",
                    )
                }
                state.appendLog("Auth failed: ${msg.data.reason}")
                manualDisconnect = true // don't auto-reconnect on auth failure
            }

            is ServerMessage.PowChallengeMsg -> {
                state.appendLog("PoW challenge received (not yet implemented)")
            }

            is ServerMessage.ErrorMsg -> {
                state.update { it.copy(errorMessage = "${msg.data.code}: ${msg.data.message}") }
                state.appendLog("Server error: ${msg.data.code} - ${msg.data.message}")
            }

            is ServerMessage.MotdMsg -> {
                state.update { it.copy(motd = msg.data.message) }
                state.appendLog("MOTD: ${msg.data.message}")
            }

            is ServerMessage.LobbyListReceived -> {
                state.update { it.copy(lobbies = msg.data.lobbies) }
                state.appendLog("Lobby list: ${msg.data.lobbies.size} lobbies")
            }

            is ServerMessage.LobbyUpdated -> {
                state.appendLog(
                    "Lobby updated: ${msg.data.lobbyId} (${msg.data.players.size} players)",
                )
            }

            is ServerMessage.ServerStatusReceived -> {
                state.update { it.copy(serverStatus = msg.data) }
                state.appendLog(
                    "Server: ${msg.data.onlinePlayers} online, " +
                        "${msg.data.activeGamesCount} games",
                )
            }

            is ServerMessage.GameStarting -> {
                state.appendLog("Game starting: ${msg.data.mission} @ ${msg.data.hostAddr}")
            }

            is ServerMessage.RateLimited -> {
                state.appendLog("Rate limited, retry in ${msg.data.retryAfterMs}ms")
            }

            is ServerMessage.VersionRejected -> {
                state.update {
                    it.copy(errorMessage = "Version rejected: ${msg.data.reason}")
                }
                state.appendLog("Version rejected: ${msg.data.reason}")
                manualDisconnect = true
            }

            is ServerMessage.Unknown -> {
                state.appendLog("Unknown message type: ${msg.type}")
            }
        }
    }
}

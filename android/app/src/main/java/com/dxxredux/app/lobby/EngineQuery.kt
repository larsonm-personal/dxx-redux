package com.dxxredux.app.lobby

import android.os.SystemClock
import com.dxxredux.app.multiplayer.NetLog
import com.dxxredux.app.multiplayer.NetworkConstants
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.SocketTimeoutException

internal object D1EngineQuery {
    init {
        System.loadLibrary("dxx-redux-d1")
    }

    external fun request(): ByteArray

    external fun decode(packet: ByteArray): String?
}

internal object D2EngineQuery {
    init {
        System.loadLibrary("dxx-redux-d2")
    }

    external fun request(): ByteArray

    external fun decode(packet: ByteArray): String?
}

internal data class EngineQueryResult(
    val game: LanLobbyAnnounce? = null,
    val error: String? = null,
)

/** Information requests only: never sends the engine's player admission request */
internal object EngineQuery {
    suspend fun probe(
        host: String,
        port: Int = NetworkConstants.ENGINE_PORT,
        timeoutMs: Long = 1000,
        gameHint: String? = null,
    ): EngineQueryResult =
        coroutineScope {
            val results = Channel<EngineQueryResult>(2)
            val queries =
                (gameHint?.let { listOf(it) } ?: listOf("d1", "d2"))
                    .map { game ->
                        launch(Dispatchers.IO) { results.send(query(host, port, timeoutMs, game)) }
                    }
            var failure = EngineQueryResult()
            repeat(queries.size) {
                val result = results.receive()
                if (result.game != null) {
                    queries.forEach { it.cancel() }
                    return@coroutineScope result
                }
                if (failure.error == null && result.error != null) failure = result
            }
            failure
        }

    private suspend fun query(
        host: String,
        port: Int,
        timeoutMs: Long,
        game: String,
    ): EngineQueryResult =
        withContext(Dispatchers.IO) {
            try {
                val request = if (game == "d1") D1EngineQuery.request() else D2EngineQuery.request()
                DatagramSocket().use { socket ->
                    // Separate connected sockets identify the engine and reject unexpected senders/ports
                    socket.connect(InetSocketAddress(InetAddress.getByName(host), port))
                    socket.soTimeout = 50
                    val deadline = SystemClock.elapsedRealtime() + timeoutMs
                    var nextSend = 0L
                    var sends = 0
                    while (SystemClock.elapsedRealtime() < deadline) {
                        currentCoroutineContext().ensureActive()
                        val now = SystemClock.elapsedRealtime()
                        if (now >= nextSend) {
                            socket.send(DatagramPacket(request, request.size))
                            sends++
                            nextSend = now + 250
                        }
                        val packet = DatagramPacket(ByteArray(8192), 8192)
                        try {
                            socket.receive(packet)
                        } catch (_: SocketTimeoutException) {
                            continue
                        }
                        val bytes = packet.data.copyOf(packet.length)
                        val decoded = if (game == "d1") D1EngineQuery.decode(bytes) else D2EngineQuery.decode(bytes)
                        if (decoded == null) {
                            NetLog.log(
                                "LAN",
                                "Engine probe ignored malformed reply game=$game host=$host:$port bytes=${packet.length}",
                            )
                            continue
                        }
                        val info = JSONObject(decoded)
                        val error =
                            when (info.optString("error")) {
                                "version" -> {
                                    "The game at $host uses an incompatible $game network version. Update both devices."
                                }

                                "unavailable" -> {
                                    "The game at $host is not ready to accept a join. Try again shortly."
                                }

                                else -> {
                                    null
                                }
                            }
                        NetLog.log(
                            "LAN",
                            "Engine probe reply game=$game host=$host:$port sends=$sends result=${error ?: "compatible"}",
                        )
                        if (error != null) return@withContext EngineQueryResult(error = error)
                        return@withContext EngineQueryResult(
                            game =
                                LanLobbyAnnounce(
                                    lobbyId = "engine:$game:$host:$port",
                                    callsign = host,
                                    game = game,
                                    mission = info.getString("mission"),
                                    mode = info.getString("mode"),
                                    difficulty = info.getInt("difficulty"),
                                    levelNum = info.getInt("level"),
                                    playerCount = info.getInt("players"),
                                    maxPlayers = info.getInt("max_players"),
                                    hostAddress = host,
                                    hostPort = port,
                                    status = "in_game",
                                ),
                        )
                    }
                    NetLog.log("LAN", "Engine probe timeout game=$game host=$host:$port sends=$sends")
                }
            } catch (e: kotlinx.coroutines.CancellationException) {
                throw e
            } catch (e: Exception) {
                NetLog.log("LAN", "Engine probe failed game=$game host=$host:$port error=${e.message}")
            } catch (e: LinkageError) {
                NetLog.log("LAN", "Engine probe unavailable game=$game error=${e.message}")
            }
            EngineQueryResult()
        }
}

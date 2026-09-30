package com.dxxredux.app

import android.app.Instrumentation
import com.dxxredux.app.lobby.D1EngineQuery
import com.dxxredux.app.lobby.D2EngineQuery
import com.dxxredux.app.lobby.EngineQuery
import com.dxxredux.app.lobby.LobbyService
import com.dxxredux.app.lobby.buildAnnounce
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.SocketTimeoutException
import java.util.concurrent.atomic.AtomicInteger

/** Replays actual engine replies through real UDP sockets, including loss and wrong senders */
internal class EngineQueryChecks(private val instrumentation: Instrumentation) {
    fun run(host: String, game: String) = runBlocking {
        val request = if (game == "d1") D1EngineQuery.request() else D2EngineQuery.request()
        val captured = DatagramSocket().use { socket ->
            socket.connect(InetSocketAddress(host, 42424))
            socket.soTimeout = 3000
            socket.send(DatagramPacket(request, request.size))
            val packet = DatagramPacket(ByteArray(8192), 8192)
            socket.receive(packet)
            packet.data.copyOf(packet.length)
        }
        val actual = checkNotNull(EngineQuery.probe(host, gameHint = game).game) { "Real engine query failed: $game" }
        check(actual.game == game && actual.mode == "coop" && actual.playerCount == 1)
        fun decode(bytes: ByteArray) = if (game == "d1") D1EngineQuery.decode(bytes) else D2EngineQuery.decode(bytes)
        check(decode(captured) != null)
        for (length in listOf(0, 1, 8, 9, 32)) check(decode(captured.copyOf(length)) == null)
        check(decode(captured.copyOf().apply { this[0] = 10 }) == null) // SYNC is not game information

        val peer = DatagramSocket(null).apply { bind(InetSocketAddress("127.0.0.2", 42424)); soTimeout = 50 }
        val lobby = DatagramSocket(null).apply { reuseAddress = true; bind(InetSocketAddress("127.0.0.2", 42400)); soTimeout = 50 }
        val requests = AtomicInteger()
        val behavior = AtomicInteger(0) // 0=loss/malformed then reply, 1=deny, 2=silence, 3=wrong sender
        val responder = launch(Dispatchers.IO) {
            while (isActive) {
                val p = DatagramPacket(ByteArray(8192), 8192)
                try { peer.receive(p) } catch (_: SocketTimeoutException) { continue }
                if (!p.data.copyOf(p.length).contentEquals(request)) continue
                val number = requests.incrementAndGet()
                val response = when (behavior.get()) {
                    1 -> byteArrayOf(1) + request.copyOfRange(5, 13) // VERSION_DENY mirrors request versions
                    2 -> continue
                    3 -> {
                        DatagramSocket().use { wrong -> wrong.send(DatagramPacket(captured, captured.size, p.socketAddress)) }
                        continue
                    }
                    else -> if (number == 1) continue else if (number == 2) captured.copyOf(8) else captured
                }
                peer.send(DatagramPacket(response, response.size, p.socketAddress))
            }
        }
        try {
            check(EngineQuery.probe("127.0.0.2", timeoutMs = 1500).game?.game == game)
            check(requests.get() >= 3) { "Lost/malformed packet did not trigger retries" }
            behavior.set(3)
            check(EngineQuery.probe("127.0.0.2", timeoutMs = 350).game == null) { "Accepted wrong source port" }
            behavior.set(0)
            LobbyService.startDiscovery(instrumentation.targetContext, "ProbeTest")
            val lobbyResponder = launch(Dispatchers.IO) {
                while (isActive) {
                    val p = DatagramPacket(ByteArray(8192), 8192)
                    try { lobby.receive(p) } catch (_: SocketTimeoutException) { continue }
                    if (!String(p.data, 0, p.length).contains("QUERY")) continue
                    val response = buildAnnounce("preferred", "Host", game, actual.mission, "coop", 1, 4)
                    lobby.send(DatagramPacket(response, response.size, InetAddress.getByName("127.0.0.1"), 42400))
                }
            }
            check(LobbyService.tryJoinLobbyByIp("127.0.0.2", "ProbeTest", probeEngine = true))
            check(LobbyService.lanLaunchEvent.value == null) { "Engine beat the launcher lobby" }
            lobbyResponder.cancelAndJoin()
            LobbyService.stopDiscovery()
            LobbyService.startDiscovery(instrumentation.targetContext, "ProbeTest")
            check(LobbyService.tryJoinLobbyByIp("127.0.0.2", "ProbeTest", probeEngine = true))
            check(LobbyService.lanLaunchEvent.value?.game == game) { "Engine-only host did not emit launch" }
            LobbyService.stopDiscovery()
            LobbyService.startDiscovery(instrumentation.targetContext, "ProbeTest")
            behavior.set(1)
            check(!LobbyService.tryJoinLobbyByIp("127.0.0.2", "ProbeTest", probeEngine = true))
            check(LobbyService.diagnostics.value.contains("incompatible"))
            check(LobbyService.lanLaunchEvent.value == null)
            behavior.set(2)
            check(!LobbyService.tryJoinLobbyByIp("127.0.0.2", "ProbeTest", probeEngine = true))
            check(LobbyService.lanLaunchEvent.value == null)
            val pending = async(Dispatchers.IO) { LobbyService.tryJoinLobbyByIp("127.0.0.2", "ProbeTest", probeEngine = true) }
            delay(100)
            LobbyService.stopDiscovery()
            behavior.set(0)
            check(!pending.await())
            check(LobbyService.lanLaunchEvent.value == null) { "Stopped discovery launched a game" }
        } finally {
            responder.cancelAndJoin()
            peer.close()
            lobby.close()
            LobbyService.stopDiscovery()
        }
    }
}

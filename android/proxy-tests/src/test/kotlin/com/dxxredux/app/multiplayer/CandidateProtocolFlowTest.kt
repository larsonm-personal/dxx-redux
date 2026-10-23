package com.dxxredux.app.multiplayer

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import okhttp3.Request
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import okio.ByteString
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetSocketAddress
import java.net.SocketTimeoutException

class CandidateProtocolFlowTest {
    @Test
    fun onlyExplicitDebugEmulatorConnectionsTranslateProvenHostLoopbackAddresses() {
        assertEquals("10.0.2.2:1234", localDevelopmentEndpoint(true, "10.0.2.2", "127.0.0.1:1234"))
        for (host in listOf("public.example", "127.0.0.1", "10.0.2.2.example", "192.168.1.1", null)) {
            assertEquals("127.0.0.1:1234", localDevelopmentEndpoint(true, host, "127.0.0.1:1234"))
        }
        assertEquals("127.0.0.1:1234", localDevelopmentEndpoint(false, "10.0.2.2", "127.0.0.1:1234"))
        for (address in listOf("8.8.8.8:53", "192.168.1.1:80", "127.0.0.2:1234", "127.0.0.1:0", "127.0.0.1:99999")) {
            assertEquals(address, localDevelopmentEndpoint(true, "10.0.2.2", address))
        }
    }

    private class Control : WebSocket {
        val sent = java.util.concurrent.CopyOnWriteArrayList<String>()

        override fun request() = Request.Builder().url("ws://127.0.0.1:9000/ws").build()

        override fun queueSize() = 0L

        override fun send(text: String): Boolean {
            sent.add(text)
            return true
        }

        override fun send(bytes: ByteString) = error("Unexpected binary control message")

        override fun close(
            code: Int,
            reason: String?,
        ) = true

        override fun cancel() = Unit
    }

    private val service = MatchmakingService

    @Test(timeout = 15000)
    fun renewalOffersAndReadyCallbacksRequireCurrentGamePeerSocketAndGeneration() {
        service.disconnect()
        val owner = SupervisorJob()
        val scope = CoroutineScope(owner + Dispatchers.IO)
        DatagramSocket(InetSocketAddress("127.0.0.1", 0)).use { server ->
            val shared = DatagramSocket(InetSocketAddress("127.0.0.1", 0))
            val proxy = LocalhostProxy(scope, shared)
            try {
                fun key() =
                    ByteArray(
                        32,
                    ).also { java.security.SecureRandom().nextBytes(it) }.joinToString("") { "%02x".format(it) }
                val control = Control()
                field("webSocket").set(service, control)
                field("manualDisconnect").setBoolean(service, false)
                field("localhostProxy").set(service, proxy)
                field("gameEpoch").set(service, "game")
                MatchmakingStateHolder.update {
                    it.copy(
                        playerId = "self",
                        currentLobby =
                            CurrentLobbyState(
                                "room",
                                players = listOf(LobbyPlayerInfo(1, "peer", "Peer", false, "admission")),
                            ),
                    )
                }
                proxy.reconcileRoster(mapOf(1 to "peer"))
                val port = DatagramSocket().use { it.localPort }
                proxy.addPeer(
                    PeerProxyConfig(
                        1,
                        port,
                        InetSocketAddress("127.0.0.1", server.localPort),
                        true,
                        42u,
                        1,
                        0,
                        key(),
                        key(),
                        relayEpoch = "old",
                    ),
                )
                proxy.start()
                val send = key()
                val receive = key()
                val message =
                    org.json
                        .JSONObject()
                        .put(
                            "type",
                            "RELAY_REKEY_OFFER",
                        ).put("lobby_id", "room")
                        .put("game_epoch", "game")
                        .put("old_token", 42)
                        .put("old_epoch", "old")
                        .put("new_token", 43)
                        .put("new_epoch", "new")
                        .put("peer_id", "peer")
                        .put("peer_slot", 1)
                        .put("ready_timeout_ms", 3000)
                        .put(
                            "credentials",
                            org.json
                                .JSONObject()
                                .put(
                                    "own_slot",
                                    0,
                                ).put("send_key", send)
                                .put("receive_key", receive)
                                .put("allocation_epoch", "new"),
                        )
                val listener = listener()
                for ((name, value) in listOf(
                    "lobby_id" to "stale-room",
                    "game_epoch" to "stale-game",
                    "peer_id" to "stale-peer",
                )) {
                    listener.onMessage(
                        control,
                        org.json
                            .JSONObject(message.toString())
                            .put(name, value)
                            .toString(),
                    )
                }
                listener.onMessage(Control(), message.toString())
                listener.onMessage(control, message.toString())
                synchronized(service) {
                    field("networkGeneration").setLong(service, field("networkGeneration").getLong(service) + 1)
                }
                val codec = HostRelayCodec(43u, 1, 0, receive, send)
                server.soTimeout = 3000
                var proved = false
                while (!proved) {
                    val packet = DatagramPacket(ByteArray(RelayTransport.MAX_PACKET), RelayTransport.MAX_PACKET)
                    server.receive(packet)
                    val decoded = codec.decode(packet.data, packet.length) ?: continue
                    val bytes =
                        when (decoded.kind) {
                            RelayTransport.BIND -> {
                                codec.encode(RelayTransport.CHALLENGE, ByteArray(16))
                            }

                            RelayTransport.PROOF -> {
                                proved = true
                                codec.encode(RelayTransport.BOUND, byteArrayOf())
                            }

                            else -> {
                                continue
                            }
                        }
                    server.send(DatagramPacket(bytes, bytes.size, packet.socketAddress))
                }
                Thread.sleep(100)
                assertTrue(control.sent.none { it.contains("RELAY_REKEY_READY") })
            } finally {
                service.disconnect()
                proxy.shutdown()
                shared.close()
                scope.cancel()
                kotlinx.coroutines.runBlocking { kotlinx.coroutines.withTimeout(2000) { owner.join() } }
            }
        }
    }

    private fun field(name: String) = service.javaClass.getDeclaredField(name).apply { isAccessible = true }

    private fun listener(): WebSocketListener =
        service.javaClass.declaredClasses
            .single { it.simpleName == "Listener" }
            .getDeclaredConstructor(
                String::class.java,
            ).apply { isAccessible = true }
            .newInstance("Self") as WebSocketListener

    private fun pending(
        control: WebSocket,
        admission: String,
        request: String,
    ): CompletableDeferred<CandidateChallengeMsg> {
        val reply = CompletableDeferred<CandidateChallengeMsg>()
        val type = service.javaClass.declaredClasses.single { it.simpleName == "PendingCandidateChallenge" }
        val constructor = type.declaredConstructors.single().apply { isAccessible = true }
        val value =
            constructor.newInstance(
                control,
                field("networkGeneration").getLong(service),
                "room",
                admission,
                request,
                reply,
            )
        field("pendingCandidateChallenge").set(service, value)
        return reply
    }

    private fun challenge(
        admission: String = "admission",
        request: String = "request",
        room: String = "room",
    ) = """{"type":"CANDIDATE_CHALLENGE","lobby_id":"$room","admission_id":"$admission",
        "request_id":"$request","attempt_id":"server-attempt","credential":"secret-proof-canary"}"""

    @Test
    fun challengeCallbacksRequireCurrentControlRequestGenerationAndAdmission() {
        service.disconnect()
        try {
            val control = Control()
            field("webSocket").set(service, control)
            field("manualDisconnect").setBoolean(service, false)
            val listener = listener()
            val reply = pending(control, "admission", "request")
            listener.onMessage(control, challenge(admission = "old-admission"))
            listener.onMessage(control, challenge(request = "old-request"))
            listener.onMessage(control, challenge(room = "old-room"))
            listener.onMessage(Control(), challenge())
            assertFalse(reply.isCompleted)
            listener.onMessage(control, challenge())
            assertTrue(reply.isCompleted)
            assertFalse(reply.isCancelled)

            val stale = pending(Control(), "admission", "request")
            listener.onMessage(control, challenge())
            assertFalse(stale.isCompleted)
            val oldGeneration = pending(control, "admission", "request")
            field("networkGeneration").setLong(service, field("networkGeneration").getLong(service) + 1L)
            listener.onMessage(control, challenge())
            assertFalse(oldGeneration.isCompleted)
            stale.cancel()
            oldGeneration.cancel()
        } finally {
            service.disconnect()
        }
    }

    @Test
    fun serverRoundChangeCancelsAnOldChallengeAndRetiresItsSharedSocket() {
        service.disconnect()
        DatagramSocket().use { udp ->
            try {
                val control = Control()
                field("webSocket").set(service, control)
                field("manualDisconnect").setBoolean(service, false)
                field("candidateSocket").set(service, udp)
                MatchmakingStateHolder.update {
                    it.copy(
                        playerId = "self",
                        currentLobby =
                            CurrentLobbyState(
                                "room",
                                players = listOf(LobbyPlayerInfo(0, "self", "Self", false, "old-admission")),
                            ),
                    )
                }
                val reply = pending(control, "old-admission", "request")
                val generation = field("networkGeneration").getLong(service)
                listener().onMessage(
                    control,
                    """{"type":"LOBBY_UPDATE","lobby_id":"room","players":[
                    {"slot":0,"player_id":"self","callsign":"Self","ready":false,"candidate_admission_id":"new-admission"}]}""",
                )
                assertTrue(reply.isCancelled)
                assertTrue(udp.isClosed)
                assertTrue(field("networkGeneration").getLong(service) > generation)
                assertEquals(
                    "new-admission",
                    MatchmakingStateHolder.state.value.currentLobby!!
                        .players
                        .single()
                        .candidateAdmissionId,
                )
            } finally {
                service.disconnect()
            }
        }
    }

    @Test
    fun lateJoinProbeForOldAdmissionCannotEmitUdpButCurrentAdmissionCan() {
        service.disconnect()
        val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
        DatagramSocket(InetSocketAddress("127.0.0.1", 0)).use { target ->
            val shared = DatagramSocket(InetSocketAddress("127.0.0.1", 0))
            val proxy = LocalhostProxy(scope, shared)
            try {
                val control = Control()
                field("webSocket").set(service, control)
                field("manualDisconnect").setBoolean(service, false)
                field("localhostProxy").set(service, proxy)
                proxy.reconcileRoster(mapOf(1 to "joiner"))
                proxy.start()
                MatchmakingStateHolder.update {
                    it.copy(
                        playerId = "self",
                        currentLobby =
                            CurrentLobbyState(
                                "room",
                                players = listOf(LobbyPlayerInfo(1, "joiner", "Joiner", false, "current-admission")),
                            ),
                    )
                }
                val listener = listener()

                fun probe(admission: String) =
                    """{"type":"LATE_JOIN_PROBE","joiner_id":"joiner",
                    "admission_id":"$admission","joiner_callsign":"Joiner","probe_addrs":["127.0.0.1:${target.localPort}"]}"""
                target.soTimeout = 150
                listener.onMessage(control, probe("old-admission"))
                assertTrue(
                    runCatching {
                        target.receive(DatagramPacket(ByteArray(13), 13))
                    }.exceptionOrNull() is SocketTimeoutException,
                )
                target.soTimeout = 2000
                listener.onMessage(control, probe("current-admission"))
                val packet = DatagramPacket(ByteArray(13), 13)
                target.receive(packet)
                assertEquals(12, packet.length)
            } finally {
                service.disconnect()
                proxy.shutdown()
                shared.close()
                scope.cancel()
            }
        }
    }
}

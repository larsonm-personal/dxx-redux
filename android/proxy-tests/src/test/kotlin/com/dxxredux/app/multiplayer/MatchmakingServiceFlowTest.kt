package com.dxxredux.app.multiplayer

import okhttp3.Request
import okhttp3.WebSocket
import okio.ByteString
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** Runs the actual service's completed-join consumer; Android storage/GPGS/IPC are not simulated */
class MatchmakingServiceFlowTest {
    @Test fun acceptedFriendJoinToCodedRoomDoesNotIssueSecondJoin() {
        val sent = mutableListOf<String>()
        val socket =
            object : WebSocket {
                override fun request() = Request.Builder().url("http://127.0.0.1/ws").build()

                override fun queueSize() = 0L

                override fun send(text: String): Boolean {
                    sent.add(text)
                    return true
                }

                override fun send(bytes: ByteString) = error("Unexpected binary message")

                override fun close(
                    code: Int,
                    reason: String?,
                ) = true

                override fun cancel() = Unit
            }
        val service = MatchmakingService
        val type = service.javaClass

        fun field(
            name: String,
            value: Any?,
        ) {
            type.getDeclaredField(name).apply { isAccessible = true }.set(service, value)
        }
        val listenerType = type.declaredClasses.first { it.simpleName == "Listener" }
        val listener =
            listenerType
                .getDeclaredConstructor(String::class.java)
                .apply { isAccessible = true }
                .newInstance("Joiner") as okhttp3.WebSocketListener

        fun receive(body: String) {
            listener.onMessage(socket, body)
        }
        service.disconnect()
        try {
            field("webSocket", socket)
            field("manualDisconnect", false)
            // Keep this consumer test from launching independent STUN/UPnP work
            field("stunCompleted", true)
            MatchmakingStateHolder.update { it.copy(playerId = "joiner", status = ConnectionStatus.CONNECTED) }
            receive(
                """{"type":"LOBBY_LIST","lobbies":[{"lobby_id":"coded-room","host_callsign":"Host",
                "game":"d2","player_count":1,"max_players":3,"joinable":true,"has_code":true}]}""",
            )
            assertTrue(
                MatchmakingStateHolder.state.value.lobbies
                    .single()
                    .hasCode,
            )
            service.joinFriendGame("accepted-friend")
            receive("""{"type":"JOIN_FRIEND_GAME_RESP","success":true,"lobby_id":"coded-room"}""")
            receive(
                """{"type":"LOBBY_UPDATE","lobby_id":"coded-room","players":[
                {"player_id":"accepted-friend","slot":0,"callsign":"Host","ready":false},
                {"player_id":"joiner","slot":2,"callsign":"Joiner","ready":false}]}""",
            )
            assertEquals(
                "coded-room",
                MatchmakingStateHolder.state.value.currentLobby
                    ?.lobbyId,
            )
            assertEquals(
                listOf(0, 2),
                MatchmakingStateHolder.state.value.currentLobby
                    ?.players
                    ?.map { it.slot },
            )
            assertTrue(sent.any { it.contains("JOIN_FRIEND_GAME") })
            assertTrue("Completed friend join sent another code-less join", sent.none { it.contains("JOIN_LOBBY") })
        } finally {
            service.disconnect()
        }
    }
}

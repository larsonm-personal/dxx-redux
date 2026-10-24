package com.dxxredux.app.multiplayer

import okhttp3.Request
import okhttp3.WebSocket
import okio.ByteString
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.concurrent.TimeUnit

class GoogleMatchmakingFlowTest {
    private class Socket : WebSocket {
        val sent = mutableListOf<String>()
        var cancelled = false

        override fun request() = Request.Builder().url("https://fixture.invalid/ws").build()

        override fun queueSize() = 0L

        override fun send(text: String): Boolean {
            sent += text
            return true
        }

        override fun send(bytes: ByteString) = false

        override fun close(
            code: Int,
            reason: String?,
        ) = true

        override fun cancel() {
            cancelled = true
        }
    }

    @Test
    fun reconnectAuthenticationUsesOnlyMatchingCachedCredential() {
        val service = MatchmakingService

        fun field(name: String) = service.javaClass.getDeclaredField(name).apply { isAccessible = true }
        service.disconnect()
        try {
            val socket = Socket()
            field("webSocket").set(service, socket)
            field("manualDisconnect").set(service, false)
            val expiry = System.nanoTime() + TimeUnit.SECONDS.toNanos(60)
            field(
                "googleSession",
            ).set(service, GoogleServerSession("https://fixture.invalid:443", "a".repeat(64), expiry))
            service.sendAuthenticate("Fixture")
            assertEquals(1, socket.sent.size)
            assertTrue(socket.sent.single().contains("\"auth_method\":\"google_session\""))
            assertTrue(socket.sent.single().contains("\"session_credential\":\"${"a".repeat(64)}\""))
            assertFalse(socket.cancelled)
            // Missing credentials stop reconnect instead of invoking the unavailable host provider
            field("googleSession").set(service, null)
            service.sendAuthenticate("Fixture")
            assertEquals(1, socket.sent.size)
            assertTrue(socket.cancelled)
            assertEquals(ConnectionStatus.DISCONNECTED, MatchmakingStateHolder.state.value.status)
        } finally {
            field("googleSession").set(service, null)
            service.disconnect()
        }
    }

    @Test
    fun publicConnectWithoutProviderDoesNotOpenGameplaySocket() {
        val service = MatchmakingService
        service.disconnect()
        service.connect("wss://fixture.invalid/ws", "Fixture")
        assertEquals(ConnectionStatus.DISCONNECTED, MatchmakingStateHolder.state.value.status)
        val socket = service.javaClass.getDeclaredField("webSocket").apply { isAccessible = true }
        assertEquals(null, socket.get(service))
        service.disconnect()
    }
}

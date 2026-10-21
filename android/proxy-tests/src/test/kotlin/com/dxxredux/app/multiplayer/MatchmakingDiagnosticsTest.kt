package com.dxxredux.app.multiplayer

import android.util.Log
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class MatchmakingDiagnosticsTest {
    @Test fun controlCredentialsAndMalformedBodiesStayOutOfDiagnostics() {
        val send = "synthetic-send-canary-do-not-log"
        val receive = "synthetic-receive-canary-do-not-log"
        val token = "987654321"
        val session = "synthetic-session-canary-do-not-log"
        val body = """{"type":"GAME_STARTING","host_addr":"127.0.0.1:9","game":"d2",
            "peers":[{"slot":2,"addr":"127.0.0.1:9","is_relay":true,"relay_token":$token,
            "relay_credentials":{"own_slot":0,"send_key":"$send","receive_key":"$receive"}}]}"""
        assertNotNull(MatchmakingDiagnostics.parse(body))
        // Serialization exceptions can include the complete input and nearby secrets
        assertNull(MatchmakingDiagnostics.parse(body.replace("\"slot\":2", "\"slot\":\"$session\"")))
        assertNull(MatchmakingDiagnostics.parse("{\"type\":\"AUTH_OK\",\"session_token\":\"$session\"}"))
        val diagnostics = Log.entries + NetLog.entries
        assertTrue(diagnostics.any { it.contains("Invalid matchmaking message") })
        for (canary in listOf(send, receive, token, session)) {
            assertTrue("Synthetic credential appeared in diagnostics", diagnostics.none { it.contains(canary) })
        }
    }
}

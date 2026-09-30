package com.dxxredux.app

import android.app.Instrumentation
import com.dxxredux.app.lobby.LobbyService
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.SocketTimeoutException
import java.util.concurrent.atomic.AtomicLong

/** Real UDP replies before and after the diagnostic client's socket replacement */
internal class DiscoveryExperimentChecks(private val instrumentation: Instrumentation) {
    fun run() {
        val context = instrumentation.targetContext
        LobbyService.stopDiscovery()
        val peer = DatagramSocket(null)
        try {
            peer.reuseAddress = true
            peer.bind(InetSocketAddress("127.0.0.2", 42400))
            peer.soTimeout = 300
            LobbyService.startDiscovery(context, "DiscoveryTest")
            val generationField = LobbyService::class.java.getDeclaredField("socketGeneration").apply { isAccessible = true }
            val generation = generationField.get(null) as AtomicLong
            val initialGeneration = generation.get()
            LobbyService.startDiscoveryExperiment("127.0.0.2")
            val phases = mutableSetOf<Int>()
            val ids = mutableSetOf<String>()
            var receivedReplyAfterReopen = false
            val deadline = System.currentTimeMillis() + 90_000
            while (LobbyService.discoveryExperimentActive.value && System.currentTimeMillis() < deadline) {
                if (6 in phases && LobbyService.discoveredLobbies.value.any { it.announce.lobbyId == "diagnostic-peer" }) {
                    receivedReplyAfterReopen = true
                }
                val packet = DatagramPacket(ByteArray(8192), 8192)
                try {
                    peer.receive(packet)
                } catch (_: SocketTimeoutException) {
                    continue
                }
                val query = JSONObject(String(packet.data, 0, packet.length, Charsets.UTF_8))
                if (query.optString("type") != "QUERY" || !query.has("trace_run")) continue
                check(ids.add(query.getString("trace_id"))) { "Duplicate transmission ID" }
                phases.add(query.getInt("trace_phase"))
                val reply = JSONObject()
                    .put("type", "ANNOUNCE").put("lobby_id", "diagnostic-peer")
                    .put("callsign", "DiagnosticPeer").put("game", "d2")
                    .put("status", "lobby").put("query_reply", true)
                    .put("reply_trace_id", query.getString("trace_id"))
                    .toString().toByteArray(Charsets.UTF_8)
                peer.send(DatagramPacket(reply, reply.size, packet.address, packet.port))
            }
            check(!LobbyService.discoveryExperimentActive.value) { "Experiment did not finish" }
            check(LobbyService.discoveryExperiment.value.startsWith("Discovery test complete"))
            check(phases == setOf(2, 6)) { "Missing unicast rounds: $phases" }
            check(ids.size == 10) { "Expected five probes per unicast round, got ${ids.size}" }
            check(generation.get() == initialGeneration + 2) { "Socket replacement phases did not run" }
            check(receivedReplyAfterReopen) { "Replies after socket replacement did not reach discovery" }

            // Exercise the real host QUERY handler and its echoed correlation fields
            LobbyService.hostLobby("DiagnosticHost", "d2", "", "coop", 4)
            val query = JSONObject().put("type", "QUERY").put("trace_id", "host-echo-check")
                .put("trace_ms", 123).put("trace_strategy", "direct-ip")
                .put("trace_run", "test-run").put("trace_phase", 2)
                .toString().toByteArray(Charsets.UTF_8)
            peer.send(DatagramPacket(query, query.size, InetAddress.getByName("127.0.0.1"), 42400))
            peer.soTimeout = 3000
            val response = DatagramPacket(ByteArray(8192), 8192)
            peer.receive(response)
            val echo = JSONObject(String(response.data, 0, response.length, Charsets.UTF_8))
            check(echo.getString("reply_trace_id") == "host-echo-check")
            check(echo.getString("reply_trace_run") == "test-run")
            check(echo.getInt("reply_trace_phase") == 2)
            check(echo.getLong("reply_trace_ms") == 123L)
            LobbyService.stopHosting()
            LobbyService.startDiscoveryExperiment("127.0.0.2")
            LobbyService.notifyAppBackgrounded()
            check(!LobbyService.discoveryExperimentActive.value) { "Backgrounding did not cancel probes" }
        } finally {
            LobbyService.stopDiscovery()
            peer.close()
        }
    }
}

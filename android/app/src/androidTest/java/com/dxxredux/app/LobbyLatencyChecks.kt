package com.dxxredux.app

import android.app.Instrumentation
import com.dxxredux.app.lobby.LanPlayer
import com.dxxredux.app.lobby.LobbyService
import com.dxxredux.app.lobby.MSG_ANNOUNCE
import com.dxxredux.app.lobby.MSG_JOIN_ACK
import com.dxxredux.app.lobby.MSG_PREPARE
import com.dxxredux.app.lobby.MSG_QUERY
import com.dxxredux.app.lobby.MSG_START
import com.dxxredux.app.lobby.MSG_START_CANCEL
import com.dxxredux.app.lobby.buildJoin
import com.dxxredux.app.multiplayer.MissionCompatibilityStatus
import com.dxxredux.app.multiplayer.MissionStatusReport
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import kotlinx.coroutines.flow.MutableStateFlow
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.ConcurrentHashMap

/** Real UDP exercises for the measured slow-save/serialized-start regressions */
internal class LobbyLatencyChecks(private val instrumentation: Instrumentation) {
    private fun field(name: String) = LobbyService::class.java.getDeclaredField(name).apply { isAccessible = true }
    private fun waitUntil(message: String, predicate: () -> Boolean) {
        val deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(5)
        while (!predicate() && System.nanoTime() < deadline) Thread.sleep(10)
        check(predicate()) { message }
    }

    @Suppress("UNCHECKED_CAST")
    fun run() = runBlocking {
        val context = instrumentation.targetContext
        LobbyService.stopDiscovery()
        val peer = DatagramSocket(null)
        val release = CountDownLatch(1)
        fun send(json: JSONObject) {
            val data = json.toString().toByteArray(Charsets.UTF_8)
            peer.send(DatagramPacket(data, data.size, InetAddress.getByName("127.0.0.1"), 42400))
        }
        fun receive(type: String): JSONObject {
            val deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(5)
            while (System.nanoTime() < deadline) {
                val packet = DatagramPacket(ByteArray(8192), 8192)
                try { peer.receive(packet) } catch (_: java.net.SocketTimeoutException) { continue }
                val json = JSONObject(String(packet.data, 0, packet.length, Charsets.UTF_8))
                if (json.optString("type") == type) return json
            }
            error("No $type received")
        }
        try {
            peer.reuseAddress = true
            peer.bind(InetSocketAddress("127.0.0.2", 42400))
            peer.soTimeout = 1000
            val entered = CountDownLatch(1)
            val checks = AtomicInteger()
            LobbyService.hostedSaveCheckForTest = { _, _ ->
                checks.incrementAndGet()
                entered.countDown()
                check(release.await(15, TimeUnit.SECONDS)) { "Test did not release slow save check" }
                null
            }
            LobbyService.startDiscovery(context, "LatencyHost")
            LobbyService.hostLobby("LatencyHost", "d2", "d2", "coop", 4)
            check(entered.await(3, TimeUnit.SECONDS))
            repeat(3) {
                val started = System.nanoTime()
                send(JSONObject().put("type", MSG_QUERY).put("trace_id", "latency-$it"))
                val response = receive(MSG_ANNOUNCE)
                check(response.getString("reply_trace_id") == "latency-$it")
                check(TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - started) < 750) { "QUERY blocked on save scan" }
            }
            val lobbyId = field("hostedLobbyId").get(null) as String
            val join = JSONObject(String(buildJoin(lobbyId, "Peer", "latency-peer"), Charsets.UTF_8))
            send(join)
            receive(MSG_JOIN_ACK)
            check(release.count == 1L && checks.get() == 1) { "Packet path reran save validation" }
            release.countDown()
            waitUntil("Save check did not publish") { !LobbyService.hostedSaveChecking.value }
            // Discard the immediate replies above and stop sending queries. Broadcasts do
            // not reach this loopback-bound peer; periodic unicast must keep discovery alive
            val repeated = receive(MSG_ANNOUNCE)
            check(repeated.getString("trace_strategy") == "discovery-peer") { "Lost query reply had no unicast recovery" }
            check(!repeated.has("reply_trace_id")) { "Periodic announcement reused an old query correlation" }
            val discoveryPeers = field("discoveryPeers").get(null) as ConcurrentHashMap<String, Long>
            discoveryPeers["127.0.0.2"] = android.os.SystemClock.elapsedRealtime() - 30_000
            val announce = LobbyService::class.java.getDeclaredMethod("broadcastAnnounce").apply { isAccessible = true }
            announce.invoke(LobbyService)
            check(discoveryPeers.isEmpty()) { "Expired discovery peer kept receiving announcements" }
            val players = field("_hostedLobbyPlayers").get(null) as MutableStateFlow<List<LanPlayer>>
            players.value = players.value.map { it.copy(ready = true, missionStatus = MissionStatusReport("fixture", MissionCompatibilityStatus.MATCH)) }
            LobbyService.startGame(1, 1)
            val hostLaunch = checkNotNull(LobbyService.lanLaunchEvent.value)
            val prepare = receive(MSG_PREPARE)
            check(prepare.getLong("launch_id") == hostLaunch.lanLaunchId)
            check(!field("gameStarted").getBoolean(null)) { "PREPARE committed the host before Activity launch" }
            LobbyService.failGameLaunch(hostLaunch, "Injected host failure")
            check(receive(MSG_START_CANCEL).getLong("launch_id") == hostLaunch.lanLaunchId)
            waitUntil("Save recheck did not finish") { !LobbyService.hostedSaveChecking.value }
            LobbyService.startGame(1, 1)
            val retry = checkNotNull(LobbyService.lanLaunchEvent.value)
            check(receive(MSG_PREPARE).getLong("launch_id") == retry.lanLaunchId)
            check(retry.lanLaunchId > hostLaunch.lanLaunchId)
            LobbyService.confirmGameLaunch(retry)
            receive(MSG_START)
            // Drain initial retries, then prove a heartbeat recovers a lost commit
            Thread.sleep(650)
            peer.soTimeout = 50
            try { while (true) peer.receive(DatagramPacket(ByteArray(8192), 8192)) } catch (_: java.net.SocketTimeoutException) { }
            peer.soTimeout = 1000
            send(join)
            receive(MSG_JOIN_ACK)
            check(receive(MSG_START).getLong("launch_id") == retry.lanLaunchId)

            LobbyService.stopDiscovery()
            LobbyService.startDiscovery(context, "LatencyClient")
            val joined = field("_joinedLobby").get(null) as MutableStateFlow<LobbyService.JoinedLobbyInfo?>
            joined.value = LobbyService.JoinedLobbyInfo("remote", "127.0.0.2", "d2", "d2", "coop", 4)
            fun launch(type: String, attempt: Long) = JSONObject().put("type", type).put("lobby_id", "remote")
                .put("launch_id", attempt).put("game", "d2").put("mission", "d2")
            send(launch(MSG_PREPARE, 1))
            waitUntil("PREPARE did not emit client launch") { LobbyService.lanLaunchEvent.value != null }
            val client = checkNotNull(LobbyService.lanLaunchEvent.value)
            LobbyService.clearLaunchEvent()
            val approval = async(Dispatchers.IO) { LobbyService.awaitHostLaunch(client) }
            send(launch(MSG_PREPARE, 1))
            delay(100)
            check(!approval.isCompleted && LobbyService.lanLaunchEvent.value == null) { "Client launched early or repeated preparation" }
            send(launch(MSG_START_CANCEL, 1).put("reason", "Injected abort"))
            check(withTimeout(2000) { approval.await() } == "Injected abort")
            send(launch(MSG_START, 1))
            delay(100)
            check(LobbyService.lanLaunchEvent.value == null) { "Late START resurrected aborted attempt" }
            send(launch(MSG_PREPARE, 2))
            waitUntil("New attempt was ignored") { LobbyService.lanLaunchEvent.value?.lanLaunchId == 2L }
            val second = checkNotNull(LobbyService.lanLaunchEvent.value)
            LobbyService.clearLaunchEvent()
            val secondApproval = async(Dispatchers.IO) { LobbyService.awaitHostLaunch(second) }
            DatagramSocket().use { wrongHost ->
                val wrong = launch(MSG_START, 2).toString().toByteArray(Charsets.UTF_8)
                wrongHost.send(DatagramPacket(wrong, wrong.size, InetAddress.getByName("127.0.0.1"), 42400))
            }
            send(launch(MSG_START_CANCEL, 1))
            delay(100)
            check(!secondApproval.isCompleted) { "Old cancellation cancelled the new attempt" }
            send(launch(MSG_START, 2))
            check(withTimeout(2000) { secondApproval.await() } == null)
            check(LobbyService.lanLaunchEvent.value == null) { "Commit duplicated the launch event" }
            send(launch(MSG_START, 3)) // PREPARE lost
            waitUntil("Commit without PREPARE was lost") { LobbyService.lanLaunchEvent.value?.lanLaunchId == 3L }
            check(LobbyService.awaitHostLaunch(checkNotNull(LobbyService.lanLaunchEvent.value)) == null)

            LobbyService.stopDiscovery()
            LobbyService.startDiscovery(context, "ManualClient")
            val probe = async(Dispatchers.IO) { LobbyService.tryJoinLobbyByIp("127.0.0.2", "ManualClient") }
            receive(MSG_QUERY) // Drop the first query
            receive(MSG_QUERY)
            send(JSONObject().put("type", MSG_ANNOUNCE).put("lobby_id", "manual").put("game", "d2").put("status", "lobby"))
            check(withTimeout(2000) { probe.await() }) { "Manual IP did not retry a lost query" }
            LobbyService.stopDiscovery()
            LobbyService.startDiscovery(context, "ManualClient")
            val delayed = async(Dispatchers.IO) { LobbyService.tryJoinLobbyByIp("127.0.0.2", "ManualClient", probeEngine = true) }
            receive(MSG_QUERY)
            delay(1500)
            check(!delayed.isCompleted) { "Manual IP still gives up after one second" }
            send(JSONObject().put("type", MSG_ANNOUNCE).put("lobby_id", "delayed").put("game", "d2").put("status", "lobby"))
            check(withTimeout(2000) { delayed.await() }) { "Delayed launcher reply was not joined" }
            check(LobbyService.lanLaunchEvent.value == null) { "Launcher reply entered the engine before the host started" }
            LobbyService.stopDiscovery()
            check(discoveryPeers.isEmpty()) { "Stopping discovery retained unicast peers" }
            LobbyService.startDiscovery(context, "ManualClient")
            val cancelled = async(Dispatchers.IO) { LobbyService.tryJoinLobbyByIp("127.0.0.2", "ManualClient", probeEngine = true) }
            delay(100)
            cancelled.cancelAndJoin()
            send(JSONObject().put("type", MSG_ANNOUNCE).put("lobby_id", "cancelled").put("game", "d2").put("status", "lobby"))
            delay(300)
            check(LobbyService.joinedLobby.value == null && LobbyService.lanLaunchEvent.value == null) { "Cancelled manual probe joined after a late reply" }
            LobbyService.stopDiscovery()
            LobbyService.startDiscovery(context, "ManualClient")
            check(!LobbyService.tryJoinLobbyByIp("127.0.0.2", "ManualClient", timeoutMs = 350))
            check(LobbyService.diagnostics.value.startsWith("No response from")) { "Silence was reported as incompatibility" }
            check(LobbyService.lanLaunchEvent.value == null) { "Silence launched an engine join" }

            val oldCheckEntered = CountDownLatch(1)
            val oldCheckRelease = CountDownLatch(1)
            val oldCheckFinished = CountDownLatch(1)
            LobbyService.hostedSaveCheckForTest = { _, mission ->
                if (mission == "descent") {
                    oldCheckEntered.countDown()
                    oldCheckRelease.await(5, TimeUnit.SECONDS)
                    oldCheckFinished.countDown()
                    "Obsolete lobby warning"
                } else null
            }
            LobbyService.hostLobby("OldHost", "d2", "descent", "coop", 4)
            check(oldCheckEntered.await(3, TimeUnit.SECONDS))
            LobbyService.hostLobby("NewHost", "d2", "d2", "coop", 4)
            waitUntil("New lobby validation did not complete") { !LobbyService.hostedSaveChecking.value }
            oldCheckRelease.countDown()
            check(oldCheckFinished.await(2, TimeUnit.SECONDS))
            delay(100)
            check(LobbyService.hostedSaveWarning.value == null) { "Old validation polluted the new lobby" }
        } finally {
            release.countDown()
            LobbyService.hostedSaveCheckForTest = null
            LobbyService.stopDiscovery()
            peer.close()
        }
    }
}

package com.dxxredux.app.multiplayer

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.SocketTimeoutException

class RelayTransportTest {
    private fun nativeGameplayTypes(): List<Int> {
        val names = listOf("UPID_PDATA", "UPID_MDATA_PNORM", "UPID_MDATA_PNEEDACK")
        val headers =
            listOf("d1", "d2").map { game ->
                val text = java.io.File(System.getProperty("dxx.repoRoot"), "$game/main/net_udp.h").readText()
                names.map { name ->
                    Regex("(?m)^#define\\s+$name\\s+(\\d+)\\b").find(text)!!.groupValues[1].toInt()
                }
            }
        assertEquals("D1/D2 producer IDs diverged", headers[0], headers[1])
        assertEquals(
            headers[0],
            listOf(LocalhostProxy.UPID_PDATA, LocalhostProxy.UPID_MDATA_PNORM, LocalhostProxy.UPID_MDATA_PNEEDACK),
        )
        return headers[0]
    }

    private fun freshKey() =
        ByteArray(32)
            .also { java.security.SecureRandom().nextBytes(it) }
            .joinToString("") { "%02x".format(it) }

    private val keyA = freshKey()
    private val keyB = freshKey()
    private val loopback = InetAddress.getByName("127.0.0.1")

    @Test fun crossLanguageAesGcmFixtureAndReplay() {
        val keyA = (0..31).joinToString("") { "%02x".format(it) }
        val sender = HostRelayCodec(0x11223344u, 0, 2, keyA, keyB)
        val receiver = RelayTransport(0x11223344u, 2, 0, keyB, keyA)
        val payload = ByteArray(32) { it.toByte() }
        val packet = sender.encode(RelayTransport.DATA, payload)
        assertEquals(
            "4458523344332211000002010000000000000015d7bdff40f1361906275b32e0ab34f8" +
                "058e0e4770a742936b1919c23a2d6d415e459fba6bf2d29a8c42b21b34632496",
            packet
                .joinToString(
                    "",
                ) {
                    "%02x".format(it)
                },
        )
        val corrupt = packet.clone().also { it[19] = (it[19].toInt() xor 1).toByte() }
        assertNull(receiver.decode(corrupt, corrupt.size))
        assertArrayEquals(payload, receiver.decode(packet, packet.size)!!.payload)
        assertNull(receiver.decode(packet, packet.size))
        val second = sender.encode(RelayTransport.DATA, payload)
        val third = sender.encode(RelayTransport.DATA, payload)
        assertNotNull(receiver.decode(third, third.size))
        assertNotNull(receiver.decode(second, second.size))
        assertNull(receiver.decode(third, third.size))
    }

    private fun receive(socket: DatagramSocket): DatagramPacket {
        val packet = DatagramPacket(ByteArray(RelayTransport.MAX_PACKET + 1), RelayTransport.MAX_PACKET + 1)
        socket.receive(packet)
        return packet
    }

    private fun send(
        socket: DatagramSocket,
        address: InetSocketAddress,
        bytes: ByteArray,
    ) {
        socket.send(DatagramPacket(bytes, bytes.size, address))
    }

    private fun noEngineDelivery(engine: DatagramSocket) {
        engine.soTimeout = 150
        try {
            receive(engine)
            fail("Untrusted packet reached native engine")
        } catch (_: SocketTimeoutException) {
        }
    }

    @Test(timeout = 15_000)
    fun realProxyChecksEndpointKeysAndReplayInBothSocketModes() {
        val canaries = mutableListOf<String>()
        for (shared in listOf(false, true)) {
            val keyA = freshKey()
            val keyB = freshKey()
            canaries.addAll(listOf(keyA, keyB))
            val owner = SupervisorJob()
            val scope = CoroutineScope(owner + Dispatchers.IO)
            val network = if (shared) DatagramSocket(0, loopback) else null
            val proxy = LocalhostProxy(scope, network)
            try {
                DatagramSocket(NetworkConstants.ENGINE_PORT, loopback).use { engine ->
                    DatagramSocket(0, loopback).use { server ->
                        DatagramSocket(0, loopback).use { stranger ->
                            server.soTimeout = 2000
                            val port = DatagramSocket(0, loopback).use { it.localPort }
                            val oldEpoch =
                                java.util.UUID
                                    .randomUUID()
                                    .toString()
                            proxy.reconcileRoster(mapOf(2 to "renewal-peer"))
                            proxy.addPeer(
                                PeerProxyConfig(
                                    2,
                                    port,
                                    InetSocketAddress(loopback, server.localPort),
                                    true,
                                    42u,
                                    2,
                                    0,
                                    keyA,
                                    keyB,
                                    relayEpoch = oldEpoch,
                                ),
                            )
                            proxy.start()
                            val remote = HostRelayCodec(42u, 2, 0, keyB, keyA)
                            val request = receive(server)
                            val address = InetSocketAddress(request.address, request.port)
                            assertEquals(RelayTransport.BIND, remote.decode(request.data, request.length)!!.kind)
                            send(server, address, remote.encode(RelayTransport.CHALLENGE, ByteArray(16) { 7 }))
                            val proof = receive(server)
                            assertEquals(RelayTransport.PROOF, remote.decode(proof.data, proof.length)!!.kind)
                            send(server, address, remote.encode(RelayTransport.BOUND, byteArrayOf()))
                            val deadline = System.nanoTime() + 2_000_000_000L
                            while (!proxy.getStats().all { it.relayBound } &&
                                System.nanoTime() < deadline
                            ) {
                                Thread.sleep(5)
                            }
                            assertTrue(proxy.getStats().all { it.relayBound })
                            val pdata = nativeGameplayTypes().first().toByte()
                            val payload = ByteArray(RelayTransport.MAX_PAYLOAD) { pdata }
                            val valid = remote.encode(RelayTransport.DATA, payload)
                            send(stranger, address, valid)
                            noEngineDelivery(engine)
                            val bad = valid.clone().also { it[4] = (it[4].toInt() xor 1).toByte() }
                            send(server, address, bad)
                            noEngineDelivery(engine)
                            send(server, address, valid)
                            engine.soTimeout = 2000
                            val incoming = receive(engine)
                            assertArrayEquals(payload, incoming.data.copyOf(incoming.length))
                            send(server, address, valid)
                            noEngineDelivery(engine)
                            assertEquals("Native PDATA was not counted", 1L, proxy.getStats().single().gameplayReceived)
                            for (upid in nativeGameplayTypes().drop(1) + listOf(0x22, 0x25, 0x26, 19)) {
                                val probe = byteArrayOf(upid.toByte(), 1, 2)
                                send(server, address, remote.encode(RelayTransport.DATA, probe))
                                assertArrayEquals(probe, receive(engine).let { it.data.copyOf(it.length) })
                            }
                            val counted = System.nanoTime() + 2_000_000_000L
                            while (proxy.getStats().single().packetsReceived < 7 &&
                                System.nanoTime() < counted
                            ) {
                                Thread.sleep(5)
                            }
                            assertEquals(7L, proxy.getStats().single().packetsReceived)
                            assertEquals(3L, proxy.getStats().single().gameplayReceived)
                            send(engine, InetSocketAddress(loopback, port), payload)
                            var decoded: RelayTransport.Packet?
                            do {
                                val packet = receive(server)
                                decoded = remote.decode(packet.data, packet.length)
                            }
                            while (decoded?.kind != RelayTransport.DATA)
                            assertArrayEquals(payload, decoded.payload)
                            val nextSend = freshKey()
                            val nextReceive = freshKey()
                            canaries.addAll(listOf(nextSend, nextReceive))
                            val nextEpoch =
                                java.util.UUID
                                    .randomUUID()
                                    .toString()
                            val offer =
                                RelayRekeyMsg(
                                    "room",
                                    "game",
                                    42,
                                    oldEpoch,
                                    43,
                                    nextEpoch,
                                    "renewal-peer",
                                    2,
                                    RelayCredentialsMsg(0, nextSend, nextReceive, nextEpoch),
                                    3000,
                                )
                            val ready =
                                java.util.concurrent.atomic
                                    .AtomicInteger()
                            assertTrue(proxy.stageRelayRekey(offer) { ready.incrementAndGet() })
                            assertTrue(proxy.stageRelayRekey(offer) { fail("Duplicate offer replaced callback") })
                            val replacement = HostRelayCodec(43u, 2, 0, nextReceive, nextSend)
                            while (ready.get() == 0) {
                                val packet = receive(server)
                                assertEquals(address, packet.socketAddress)
                                val token =
                                    java.nio.ByteBuffer
                                        .wrap(
                                            packet.data,
                                        ).order(java.nio.ByteOrder.LITTLE_ENDIAN)
                                        .getInt(4)
                                val codec = if (token == 43) replacement else remote
                                when (codec.decode(packet.data, packet.length)?.kind) {
                                    RelayTransport.BIND -> {
                                        send(
                                            server,
                                            address,
                                            codec.encode(
                                                if (token ==
                                                    43
                                                ) {
                                                    RelayTransport.CHALLENGE
                                                } else {
                                                    RelayTransport.BOUND
                                                },
                                                if (token == 43) ByteArray(16) { 9 } else byteArrayOf(),
                                            ),
                                        )
                                    }

                                    RelayTransport.PROOF -> {
                                        send(server, address, codec.encode(RelayTransport.BOUND, byteArrayOf()))
                                    }
                                }
                                if (token == 43) Thread.sleep(10)
                            }
                            assertEquals(1, ready.get())
                            // Receive the prepared generation before our control-side commit arrives
                            send(server, address, replacement.encode(RelayTransport.DATA, payload))
                            assertArrayEquals(payload, receive(engine).let { it.data.copyOf(it.length) })
                            assertTrue(!proxy.commitRelayRekey(offer.scope().copy(newEpoch = "wrong"), 200))
                            assertTrue(proxy.commitRelayRekey(offer.scope(), 200))
                            assertTrue(proxy.commitRelayRekey(offer.scope(), 3000))
                            proxy.abortRelayRekey(offer.scope())
                            send(engine, InetSocketAddress(loopback, port), payload)
                            do {
                                val packet = receive(server)
                                assertEquals(address, packet.socketAddress)
                                decoded = replacement.decode(packet.data, packet.length)
                            } while (decoded?.kind != RelayTransport.DATA)
                            assertArrayEquals(payload, decoded.payload)
                            Thread.sleep(250)
                            send(server, address, remote.encode(RelayTransport.DATA, payload))
                            noEngineDelivery(engine)
                            assertTrue(!proxy.stageRelayRekey(offer) {})
                            val expiringEpoch =
                                java.util.UUID
                                    .randomUUID()
                                    .toString()
                            val expiring =
                                RelayRekeyMsg(
                                    "room",
                                    "game",
                                    43,
                                    nextEpoch,
                                    44,
                                    expiringEpoch,
                                    "renewal-peer",
                                    2,
                                    RelayCredentialsMsg(0, freshKey(), freshKey(), expiringEpoch),
                                    50,
                                )
                            assertTrue(proxy.stageRelayRekey(expiring) {})
                            Thread.sleep(75)
                            assertTrue(!proxy.commitRelayRekey(expiring.scope(), 0))
                            assertTrue(!proxy.stageRelayRekey(expiring) {})
                            send(server, address, replacement.encode(RelayTransport.DATA, payload))
                            assertArrayEquals(payload, receive(engine).let { it.data.copyOf(it.length) })
                        }
                    }
                }
            } finally {
                proxy.shutdown()
                scope.cancel()
                network?.close()
                runBlocking { withTimeout(2000) { owner.join() } }
            }
        }
        val diagnostics = NetLog.entries + android.util.Log.entries
        assertTrue(
            "Synthetic relay credential appeared in diagnostics",
            diagnostics.none { line -> canaries.any { line.contains(it) } },
        )
    }

    @Test(timeout = 15_000)
    fun relayAcknowledgementLossTerminatesAndReleasesResources() =
        runBlocking {
            val owner = SupervisorJob()
            val scope = CoroutineScope(owner + Dispatchers.IO)
            val network = DatagramSocket(0, loopback)
            val failed = kotlinx.coroutines.CompletableDeferred<String>()
            val proxy = LocalhostProxy(scope, network, onFailure = { failed.complete(it) })
            val port = DatagramSocket(0, loopback).use { it.localPort }
            try {
                DatagramSocket(0, loopback).use { server ->
                    server.soTimeout = 2000
                    proxy.addPeer(
                        PeerProxyConfig(
                            2,
                            port,
                            InetSocketAddress(loopback, server.localPort),
                            true,
                            42u,
                            2,
                            0,
                            keyA,
                            keyB,
                        ),
                    )
                    // Make the timeout worker yield after closing the engine socket,
                    // so its sibling observes closure before coroutineScope selects a cause
                    val peersField = LocalhostProxy::class.java.getDeclaredField("peerProxies")
                    peersField.isAccessible = true
                    val peer = (peersField.get(proxy) as List<*>).single()!!
                    val localField = peer.javaClass.getDeclaredField("localSocket")
                    localField.isAccessible = true
                    (localField.get(peer) as DatagramSocket).close()
                    val delayedClose =
                        object : DatagramSocket(port, loopback) {
                            private val firstClose =
                                java.util.concurrent.atomic
                                    .AtomicBoolean(true)

                            override fun close() {
                                super.close()
                                if (firstClose.compareAndSet(true, false)) Thread.sleep(200)
                            }
                        }
                    localField.set(peer, delayedClose)
                    proxy.start()
                    val remote = HostRelayCodec(42u, 2, 0, keyB, keyA)
                    val request = receive(server)
                    val address = InetSocketAddress(request.address, request.port)
                    assertEquals(RelayTransport.BIND, remote.decode(request.data, request.length)!!.kind)
                    send(server, address, remote.encode(RelayTransport.CHALLENGE, ByteArray(16) { 7 }))
                    val proof = receive(server)
                    assertEquals(RelayTransport.PROOF, remote.decode(proof.data, proof.length)!!.kind)
                    send(server, address, remote.encode(RelayTransport.BOUND, byteArrayOf()))
                    // Simulate a dead/restarted relay by withholding further acknowledgements
                    val message = withTimeout(12_000) { failed.await() }
                    assertTrue("Unexpected proxy failure: $message", message.contains("within 10 seconds"))
                    withTimeout(2000) { owner.children.toList().forEach { it.join() } }
                    assertTrue(network.isClosed)
                    DatagramSocket(port, loopback).use { assertTrue(it.isBound) }
                    assertTrue(proxy.getStats().isEmpty())
                }
            } finally {
                proxy.shutdown()
                scope.cancel()
                network.close()
                withTimeout(2000) { owner.join() }
            }
        }

    @Test fun transportRecreationRequiresFreshDirectionalKeys() {
        val first = RelayTransport(42u, 0, 2, keyA, keyB)
        first.encode(RelayTransport.BIND, ByteArray(16))
        try {
            RelayTransport(42u, 0, 2, keyA, keyB)
            fail("Transport recreation reused its sending key and sequence")
        } catch (_: IllegalStateException) {
        }
        try {
            RelayTransport(42u, 0, 2, freshKey(), keyB)
            fail("Transport recreation reset the receive replay window with an old key")
        } catch (_: IllegalStateException) {
        }
        try {
            val same = freshKey()
            RelayTransport(42u, 0, 2, same, same)
            fail("Both directions accepted the same key")
        } catch (_: IllegalArgumentException) {
        }
        val replacement = RelayTransport(42u, 0, 2, freshKey(), freshKey())
        assertNotNull(replacement.encode(RelayTransport.BIND, ByteArray(16)))
        try {
            RelayTransport(43u, 0, 2, keyB, freshKey())
            fail("A receiving key was reused for sending")
        } catch (_: IllegalStateException) {
        }
        try {
            RelayTransport(43u, 0, 2, freshKey(), keyA)
            fail("A sending key was reused for receiving")
        } catch (_: IllegalStateException) {
        }
    }

    @Test(timeout = 25_000)
    fun middleSlotDeparturePreservesSurvivorAndFreshSlotReuse() =
        runBlocking {
            val owner = SupervisorJob()
            val scope = CoroutineScope(owner + Dispatchers.IO)
            val network = DatagramSocket(0, loopback)
            val failed = kotlinx.coroutines.CompletableDeferred<String>()
            val proxy = LocalhostProxy(scope, network, onFailure = { failed.complete(it) })
            val ports = listOf(1, 2).associateWith { DatagramSocket(0, loopback).use { socket -> socket.localPort } }
            val remotes = java.util.concurrent.ConcurrentHashMap<UInt, HostRelayCodec>()
            val proven =
                java.util.concurrent.ConcurrentHashMap
                    .newKeySet<UInt>()
            val server = DatagramSocket(0, loopback)
            server.soTimeout = 250

            fun add(
                slot: Int,
                token: UInt,
            ) {
                val uplink = freshKey()
                val downlink = freshKey()
                remotes[token] = HostRelayCodec(token, slot, 0, downlink, uplink)
                proxy.addPeer(
                    PeerProxyConfig(
                        slot,
                        ports.getValue(slot),
                        InetSocketAddress(loopback, server.localPort),
                        true,
                        token,
                        slot,
                        0,
                        uplink,
                        downlink,
                    ),
                )
            }
            try {
                scope.launch {
                    while (kotlinx.coroutines.currentCoroutineContext()[kotlinx.coroutines.Job]?.isActive == true) {
                        val packet =
                            try {
                                receive(server)
                            } catch (
                                _: SocketTimeoutException,
                            ) {
                                continue
                            } catch (e: java.net.SocketException) {
                                if (!owner.isActive) break else throw e
                            }
                        val token =
                            java.nio.ByteBuffer
                                .wrap(
                                    packet.data,
                                    4,
                                    4,
                                ).order(java.nio.ByteOrder.LITTLE_ENDIAN)
                                .int
                                .toUInt()
                        val remote = remotes[token] ?: continue
                        val decoded = remote.decode(packet.data, packet.length) ?: continue
                        val address = InetSocketAddress(packet.address, packet.port)
                        when (decoded.kind) {
                            RelayTransport.BIND -> {
                                send(
                                    server,
                                    address,
                                    remote.encode(
                                        if (token in proven) RelayTransport.BOUND else RelayTransport.CHALLENGE,
                                        if (token in proven) byteArrayOf() else ByteArray(16) { 7 },
                                    ),
                                )
                            }

                            RelayTransport.PROOF -> {
                                proven.add(token)
                                send(server, address, remote.encode(RelayTransport.BOUND, byteArrayOf()))
                            }

                            RelayTransport.DATA -> {
                                send(server, address, remote.encode(RelayTransport.DATA, decoded.payload))
                            }
                        }
                    }
                }
                proxy.reconcileRoster(mapOf(1 to "departing", 2 to "survivor"))
                add(1, 101u)
                add(2, 102u)
                proxy.start()

                suspend fun bound(count: Int) {
                    withTimeout(4000) {
                        while (proxy.getStats().size != count ||
                            proxy.getStats().any { !it.relayBound }
                        ) {
                            kotlinx.coroutines.delay(10)
                        }
                    }
                }
                bound(2)
                proxy.reconcileRoster(mapOf(2 to "survivor"))
                remotes.remove(101u)
                withTimeout(2000) {
                    while (true) {
                        try {
                            DatagramSocket(ports.getValue(1), loopback).use { assertTrue(it.isBound) }
                            break
                        } catch (
                            _: java.net.BindException,
                        ) {
                            kotlinx.coroutines.delay(5)
                        }
                    }
                }
                assertEquals(listOf(2), proxy.getStats().map { it.peerSlot })
                DatagramSocket(NetworkConstants.ENGINE_PORT, loopback).use { engine ->
                    engine.soTimeout = 2000

                    fun roundTrip(slot: Int) {
                        val payload = byteArrayOf(nativeGameplayTypes().first().toByte(), slot.toByte(), 0x33)
                        send(engine, InetSocketAddress(loopback, ports.getValue(slot)), payload)
                        val received = receive(engine)
                        assertArrayEquals(payload, received.data.copyOf(received.length))
                    }
                    val deadline = System.nanoTime() + 11_000_000_000L
                    while (System.nanoTime() < deadline) {
                        roundTrip(2)
                        kotlinx.coroutines.delay(200)
                    }
                    assertTrue("Departed peer failed the surviving proxy", !failed.isCompleted)
                    assertTrue(!network.isClosed)
                    proxy.reconcileRoster(mapOf(1 to "replacement", 2 to "survivor"))
                    add(1, 103u)
                    bound(2)
                    roundTrip(1)
                    roundTrip(2)
                    // Identity changes at the same stable slot retire the old route immediately
                    proxy.reconcileRoster(mapOf(1 to "replacement-again", 2 to "survivor"))
                    add(1, 104u)
                    bound(2)
                    roundTrip(1)
                    roundTrip(2)
                    assertTrue(!failed.isCompleted)
                }
            } finally {
                proxy.shutdown()
                scope.cancel()
                server.close()
                network.close()
                withTimeout(2000) { owner.join() }
            }
        }
}

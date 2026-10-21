package com.dxxredux.app.multiplayer

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
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
    private val keyA = (0..31).joinToString("") { "%02x".format(it) }
    private val keyB = (32..63).joinToString("") { "%02x".format(it) }
    private val loopback = InetAddress.getByName("127.0.0.1")

    @Test fun crossLanguageAesGcmFixtureAndReplay() {
        val sender = RelayTransport(0x11223344u, 0, 2, keyA, keyB)
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
        for (shared in listOf(false, true)) {
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
                            proxy.start()
                            val remote = RelayTransport(42u, 2, 0, keyB, keyA)
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
                            val payload = ByteArray(RelayTransport.MAX_PAYLOAD) { 0x22 }
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
                            send(engine, InetSocketAddress(loopback, port), payload)
                            var decoded: RelayTransport.Packet?
                            do {
                                val packet = receive(server)
                                decoded = remote.decode(packet.data, packet.length)
                            }
                            while (decoded?.kind != RelayTransport.DATA)
                            assertArrayEquals(payload, decoded.payload)
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
                    proxy.start()
                    val remote = RelayTransport(42u, 2, 0, keyB, keyA)
                    val request = receive(server)
                    val address = InetSocketAddress(request.address, request.port)
                    assertEquals(RelayTransport.BIND, remote.decode(request.data, request.length)!!.kind)
                    send(server, address, remote.encode(RelayTransport.CHALLENGE, ByteArray(16) { 7 }))
                    val proof = receive(server)
                    assertEquals(RelayTransport.PROOF, remote.decode(proof.data, proof.length)!!.kind)
                    send(server, address, remote.encode(RelayTransport.BOUND, byteArrayOf()))
                    // Simulate a dead/restarted relay by withholding further acknowledgements
                    val message = withTimeout(12_000) { failed.await() }
                    assertTrue(message.contains("within 10 seconds"))
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
}

package com.dxxredux.app.multiplayer

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import java.io.IOException
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.SocketTimeoutException
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

class ProxyFailureTest {
    private val loopback = InetAddress.getByName("127.0.0.1")

    // Fail exactly one keepalive send, while every real loopback route stays usable
    private class KeepaliveFailureSocket : DatagramSocket(InetSocketAddress("127.0.0.1", 0)) {
        val injected = CountDownLatch(1)

        @Volatile var failGameplay = false

        override fun send(packet: DatagramPacket) {
            if (failGameplay && packet.data[packet.offset] == 42.toByte()) {
                failGameplay = false
                throw IOException("injected gameplay send failure")
            }
            if (packet.length == 1 && packet.data[packet.offset] == 0.toByte() && injected.count > 0) {
                injected.countDown()
                throw IOException(
                    "injected keepalive send failure",
                    IOException("sendto failed: ENETUNREACH (injected)"),
                )
            }
            super.send(packet)
        }
    }

    private fun send(
        socket: DatagramSocket,
        destination: Int,
        value: Byte,
    ) {
        socket.send(DatagramPacket(byteArrayOf(value), 1, loopback, destination))
    }

    private fun receive(socket: DatagramSocket): Byte {
        val packet = DatagramPacket(ByteArray(32), 32)
        socket.receive(packet)
        return packet.data[0]
    }

    @Test(timeout = 10_000)
    fun gameplayIOExceptionIsLoggedAndExistingForwardingContinues() {
        NetLog.entries.clear()
        val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
        val socket = KeepaliveFailureSocket().apply { failGameplay = true }
        val failed = CountDownLatch(1)
        val proxy = LocalhostProxy(scope, socket) { failed.countDown() }
        try {
            DatagramSocket(0, loopback).use { engine ->
                DatagramSocket(0, loopback).use { remote ->
                    remote.soTimeout = 2_000
                    val localPort = DatagramSocket(0, loopback).use { it.localPort }
                    proxy.addPeer(PeerProxyConfig(0, localPort, InetSocketAddress(loopback, remote.localPort), false))
                    proxy.start()
                    send(engine, localPort, 42)
                    send(engine, localPort, 43)
                    assertEquals(43.toByte(), receive(remote))
                    assertFalse(socket.isClosed)
                    assertEquals(1L, failed.count)
                    assertTrue(
                        NetLog.entries.any {
                            it.contains("operation=gameplay-send") &&
                                it.contains("injected gameplay send failure")
                        },
                    )
                }
            }
        } finally {
            proxy.shutdown()
            scope.cancel()
            socket.close()
        }
    }

    @Test(timeout = 30_000)
    fun currentKeepaliveIOExceptionClosesProxyAndExportsOriginalOperation() {
        NetLog.entries.clear()
        val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
        val failed = CountDownLatch(1)
        val socket = KeepaliveFailureSocket()
        val proxy = LocalhostProxy(scope, socket) { failed.countDown() }
        try {
            DatagramSocket(NetworkConstants.ENGINE_PORT, loopback).use { engine ->
                DatagramSocket(0, loopback).use { remote ->
                    engine.soTimeout = 2_000
                    remote.soTimeout = 2_000
                    val localPort = DatagramSocket(0, loopback).use { it.localPort }
                    proxy.addPeer(PeerProxyConfig(0, localPort, InetSocketAddress(loopback, remote.localPort), false))
                    proxy.start()
                    send(engine, localPort, 42)
                    assertEquals(42.toByte(), receive(remote))
                    send(remote, socket.localPort, 43)
                    assertEquals(43.toByte(), receive(engine))

                    assertTrue("Keepalive must reach injected send", socket.injected.await(20, TimeUnit.SECONDS))
                    assertTrue("Current proxy must report failure", failed.await(3, TimeUnit.SECONDS))
                    assertTrue("Current behavior closes shared socket", socket.isClosed)
                    val failure = NetLog.entries.first { it.contains("failure operation=keepalive-send") }
                    assertTrue(failure.contains("java.io.IOException: injected keepalive send failure"))
                    assertTrue(failure.contains("ENETUNREACH"))
                    assertTrue(failure.contains("closed=false"))
                    assertTrue(failure.contains("tx_ok=1 rx_ok=1"))
                    assertTrue(failure.contains("last_tx_ms="))
                    assertTrue(failure.contains("last_rx_ms="))
                    assertTrue(failure.contains("remote=/127.0.0.1:${remote.localPort}"))
                    assertTrue(NetLog.entries.any { it.contains("worker keepalive") })
                    assertTrue(NetLog.entries.any { it.contains("shutdown caller") })
                    println(
                        "CURRENT: keepalive IOException closes proxy; exported log identifies original operation and recent successful traffic",
                    )
                }
            }
        } finally {
            proxy.shutdown()
            scope.cancel()
            socket.close()
        }
    }

    @Test(timeout = 30_000)
    fun historicalKeepaliveIOExceptionAlsoBreaksForwardingButLeavesSocketsOpen() {
        assumeTrue("Opt in with -PcompareProxyHistory=true", java.lang.Boolean.getBoolean("dxx.compareProxyHistory"))
        val owner = SupervisorJob()
        val scope = CoroutineScope(owner + Dispatchers.IO)
        val socket = KeepaliveFailureSocket()
        var proxy: AutoCloseable? = null
        try {
            DatagramSocket(NetworkConstants.ENGINE_PORT, loopback).use { engine ->
                DatagramSocket(0, loopback).use { remote ->
                    engine.soTimeout = 2_000
                    remote.soTimeout = 2_000
                    val localPort = DatagramSocket(0, loopback).use { it.localPort }
                    proxy =
                        Class
                            .forName("com.dxxredux.app.multiplayer.historical.HistoricalLocalhostProxyKt")
                            .getMethod(
                                "createHistoricalProxy",
                                CoroutineScope::class.java,
                                DatagramSocket::class.java,
                                Int::class.javaPrimitiveType,
                                Int::class.javaPrimitiveType,
                            ).invoke(null, scope, socket, localPort, remote.localPort) as AutoCloseable
                    send(engine, localPort, 42)
                    assertEquals(42.toByte(), receive(remote))
                    send(remote, socket.localPort, 43)
                    assertEquals(43.toByte(), receive(engine))

                    assertTrue(socket.injected.await(20, TimeUnit.SECONDS))

                    // The outer launch catches failures; its nested coroutineScope is cancelling
                    fun hasCancelledPeer() = owner.children.any { worker -> worker.children.any { it.isCancelled } }
                    val deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(2)
                    while (!hasCancelledPeer() && System.nanoTime() < deadline) Thread.sleep(10)
                    assertTrue("Historical peer scope enters cancellation", hasCancelledPeer())
                    assertFalse("Historical code does not close the socket", socket.isClosed)

                    // The independent shared receiver still forwards inbound traffic
                    send(remote, socket.localPort, 44)
                    assertEquals(44.toByte(), receive(engine))
                    // Cancellation cannot interrupt receive(); one last packet unblocks it
                    send(engine, localPort, 45)
                    assertEquals(45.toByte(), receive(remote))
                    send(engine, localPort, 46)
                    remote.soTimeout = 500
                    try {
                        receive(remote)
                        throw AssertionError("Historical outbound worker unexpectedly recovered")
                    } catch (_: SocketTimeoutException) {
                        // Expected: the old worker has exited despite the route remaining usable
                    }
                    println(
                        "BEFORE SEPTEMBER 28: same IOException cancels outbound forwarding; sockets remain open and inbound still works",
                    )
                }
            }
        } finally {
            proxy?.close()
            scope.cancel()
            socket.close()
        }
    }
}

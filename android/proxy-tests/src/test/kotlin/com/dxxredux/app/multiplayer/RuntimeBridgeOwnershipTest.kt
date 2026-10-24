package com.dxxredux.app.multiplayer

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Bundle
import android.os.HostBinder
import android.os.Message
import org.junit.Assert.*
import org.junit.Test

class RuntimeBridgeOwnershipTest {
    private class Owner : Context() {
        lateinit var connection: ServiceConnection
        val messages = mutableListOf<Message>()
        val binder = HostBinder { messages += it }
        var unbound = 0

        override fun bindService(
            intent: Intent,
            connection: ServiceConnection,
            flags: Int,
        ): Boolean {
            this.connection = connection
            return true
        }

        override fun unbindService(connection: ServiceConnection) {
            unbound++
        }
    }

    @Test fun oldActivityAndBindingCallbacksCannotRetireReplacement() {
        val bridge = RuntimeGameStateBridge
        val old = Owner()
        val replacement = Owner()
        var timeouts = 0
        bridge.claimActivity(old, "old")
        assertTrue(bridge.connect(old, true, "old", { intArrayOf(1, 2, 4, 1, 4) }, {}))
        old.connection.onServiceConnected(ComponentName(), old.binder)
        val oldReply = old.messages.first().replyTo!!
        bridge.claimActivity(replacement, "new")
        assertEquals(1, old.messages.count { it.what == RUNTIME_IPC_GAME_STOPPED })
        assertTrue(bridge.connect(replacement, true, "new", { intArrayOf(1, 2, 4, 1, 4) }, { timeouts++ }))
        replacement.connection.onServiceConnected(ComponentName(), replacement.binder)
        assertFalse(bridge.ownsActivity(old, "old"))
        assertFalse(bridge.connect(old, false, "old", { intArrayOf() }, {}))
        old.connection.onServiceConnected(ComponentName(), old.binder)
        old.connection.onServiceDisconnected(ComponentName())
        old.connection.onBindingDied(ComponentName())
        old.connection.onNullBinding(ComponentName())
        oldReply.send(
            Message(RUNTIME_IPC_BACKGROUND_TIMEOUT).apply {
                data = Bundle().apply { putString(RUNTIME_IPC_KEY_ATTEMPT, "old") }
            },
        )
        assertEquals(0, timeouts)
        assertFalse(bridge.disconnect("old"))
        bridge.noteActivityVisibility(true)
        assertEquals(RUNTIME_IPC_ACTIVITY_VISIBILITY, replacement.messages.last().what)
        assertEquals(
            "new",
            replacement.messages
                .last()
                .data
                .getString(RUNTIME_IPC_KEY_ATTEMPT),
        )
        assertEquals(1, old.messages.count { it.what == RUNTIME_IPC_REGISTER })
        assertEquals(0, replacement.messages.count { it.what == RUNTIME_IPC_GAME_STOPPED })
        assertEquals(0, replacement.unbound)
        replacement.messages.first().replyTo!!.send(
            Message(RUNTIME_IPC_BACKGROUND_TIMEOUT).apply {
                data = Bundle().apply { putString(RUNTIME_IPC_KEY_ATTEMPT, "new") }
            },
        )
        assertEquals(1, timeouts)
        assertTrue(bridge.disconnect("new"))
        assertEquals(1, replacement.messages.count { it.what == RUNTIME_IPC_GAME_STOPPED })
        assertEquals(1, replacement.unbound)
    }

    @Test fun actualProxyAttachesReceiptOnlyToLocalNativeRequest() {
        val scope =
            kotlinx.coroutines.CoroutineScope(kotlinx.coroutines.SupervisorJob() + kotlinx.coroutines.Dispatchers.IO)
        val loopback = java.net.InetAddress.getByName("127.0.0.1")
        val shared = java.net.DatagramSocket(0, loopback)
        val remote = java.net.DatagramSocket(0, loopback)
        val engine = java.net.DatagramSocket(NetworkConstants.ENGINE_PORT, loopback)
        engine.soTimeout = 3000
        remote.soTimeout = 3000
        val proxy = LocalhostProxy(scope, shared)
        try {
            proxy.addPeer(
                PeerProxyConfig(
                    2,
                    42432,
                    java.net.InetSocketAddress(loopback, remote.localPort),
                    false,
                    admissionId = "00010203-0405-0607-0809-0a0b0c0d0e0f",
                ),
            )
            proxy.start()
            val nativeRequest = byteArrayOf(8, 42)
            remote.send(java.net.DatagramPacket(nativeRequest, nativeRequest.size, loopback, shared.localPort))
            val received = java.net.DatagramPacket(ByteArray(100), 100)
            engine.receive(received)
            assertArrayEquals(
                byteArrayOf(68, 88, 79, 65, 2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 8, 42),
                received.data.copyOf(received.length),
            )
            engine.send(java.net.DatagramPacket(nativeRequest, nativeRequest.size, loopback, 42432))
            remote.receive(received)
            assertArrayEquals(nativeRequest, received.data.copyOf(received.length))
            val ordinary = byteArrayOf(7, 42)
            remote.send(java.net.DatagramPacket(ordinary, ordinary.size, loopback, shared.localPort))
            engine.receive(received)
            assertArrayEquals(ordinary, received.data.copyOf(received.length))
        } finally {
            proxy.shutdown()
            shared.close()
            remote.close()
            engine.close()
            scope.coroutineContext[kotlinx.coroutines.Job]!!.cancel()
        }
    }
}

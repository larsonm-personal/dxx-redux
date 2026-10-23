package com.dxxredux.app.multiplayer

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetSocketAddress
import java.util.UUID
import java.util.concurrent.Callable
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

class StunProofClientTest {
    private val transaction = hex("0102030405060708090a0b0c")
    private val credential = UUID.fromString("10213243-5465-7687-98a9-bacbdcedfe0f")
    private val receipt = UUID.fromString("00112233-4455-6677-8899-aabbccddeeff")

    // Identical vectors live in server/src/stun.rs
    private val requestVector = hex("000100142112a4420102030405060708090a0b0cc0010010102132435465768798a9bacbdcedfe0f")
    private val responseVector =
        hex("010100202112a4420102030405060708090a0b0c0020000800013180291aa046c002001000112233445566778899aabbccddeeff")

    @Test
    fun matchesServerWireVectors() {
        assertArrayEquals(requestVector, StunProofClient.buildRequest(transaction, credential))
        val observation = StunProofClient.parseResponse(responseVector, responseVector.size, transaction)!!
        assertEquals(InetSocketAddress("8.8.4.4", 4242), observation.endpoint)
        assertEquals(receipt, observation.receipt)
    }

    @Test
    fun rejectsEveryTruncationOversizeAndMalformedFixedField() {
        for (length in 0 until StunProofClient.RESPONSE_BYTES) {
            assertNull(StunProofClient.parseResponse(responseVector.copyOf(length), length, transaction))
        }
        assertNull(StunProofClient.parseResponse(responseVector, StunProofClient.RESPONSE_BYTES + 1, transaction))
        assertNull(
            StunProofClient.parseResponse(
                responseVector + byteArrayOf(0),
                StunProofClient.RESPONSE_BYTES + 1,
                transaction,
            ),
        )
        assertNull(StunProofClient.parseResponse(responseVector, responseVector.size, byteArrayOf()))
        for (index in listOf(0, 1, 2, 3, 4, 7, 8, 19, 20, 21, 22, 23, 24, 25, 32, 33, 34, 35)) {
            val malformed = responseVector.copyOf().apply { this[index] = (this[index].toInt() xor 0x80).toByte() }
            assertNull(
                "Accepted malformed byte $index",
                StunProofClient.parseResponse(malformed, malformed.size, transaction),
            )
        }
        // Former attribute-parser hazards: huge lengths and an IPv6-shaped mapped address
        for ((index, value) in listOf(22 to 0xFF, 23 to 0xFF, 25 to 2, 34 to 0xFF, 35 to 0xFF)) {
            val malformed = responseVector.copyOf().apply { this[index] = value.toByte() }
            assertNull(StunProofClient.parseResponse(malformed, malformed.size, transaction))
        }
    }

    @Test
    fun liveSocketRejectsWrongSourceTransactionAndSizeBeforeAcceptingReceipt() {
        DatagramSocket(InetSocketAddress("127.0.0.1", 0)).use { server ->
            DatagramSocket(InetSocketAddress("127.0.0.1", 0)).use { stranger ->
                DatagramSocket(InetSocketAddress("127.0.0.1", 0)).use { client ->
                    client.soTimeout = 237
                    server.soTimeout = 3000
                    val executor = Executors.newSingleThreadExecutor()
                    try {
                        val worker =
                            executor.submit(
                                Callable {
                                    val request =
                                        DatagramPacket(
                                            ByteArray(StunProofClient.REQUEST_BYTES + 1),
                                            StunProofClient.REQUEST_BYTES + 1,
                                        )
                                    server.receive(request)
                                    assertEquals(StunProofClient.REQUEST_BYTES, request.length)
                                    assertArrayEquals(
                                        requestVector.copyOfRange(20, 40),
                                        request.data.copyOfRange(20, 40),
                                    )
                                    val response =
                                        responseVector.copyOf().apply {
                                            request.data.copyInto(
                                                this,
                                                8,
                                                8,
                                                20,
                                            )
                                        }
                                    val forged = response.copyOf().apply { this[36] = 0x7F }
                                    stranger.send(
                                        DatagramPacket(forged, forged.size, request.socketAddress),
                                    )
                                    val wrongTransaction =
                                        response.copyOf().apply {
                                            this[8] = (this[8].toInt() xor 1).toByte()
                                        }
                                    server.send(
                                        DatagramPacket(wrongTransaction, wrongTransaction.size, request.socketAddress),
                                    )
                                    val oversized = forged + byteArrayOf(0)
                                    server.send(
                                        DatagramPacket(oversized, oversized.size, request.socketAddress),
                                    )
                                    server.send(
                                        DatagramPacket(response, response.size, request.socketAddress),
                                    )
                                },
                            )
                        val observations =
                            StunProofClient.discover(
                                listOf(server.localSocketAddress as InetSocketAddress),
                                credential,
                                client,
                            )
                        worker.get(5, TimeUnit.SECONDS)
                        assertEquals(
                            listOf(StunProofObservation(InetSocketAddress("8.8.4.4", 4242), receipt)),
                            observations,
                        )
                        assertEquals(237, client.soTimeout)
                        assertFalse(client.isClosed)
                    } finally {
                        executor.shutdownNow()
                        assertTrue(executor.awaitTermination(5, TimeUnit.SECONDS))
                    }
                }
            }
        }
    }

    @Test(timeout = 7000)
    fun receiveFloodCannotIncreaseRequestCountOrAdvanceRetryDeadline() {
        val sends = mutableListOf<Long>()
        var receives = 0
        val server = InetSocketAddress("127.0.0.1", 44444)
        val socket =
            object : DatagramSocket(0, InetSocketAddress("127.0.0.1", 0).address) {
                override fun send(packet: DatagramPacket) {
                    sends.add(System.nanoTime())
                }

                override fun receive(packet: DatagramPacket) {
                    receives++
                    packet.data.fill(0)
                    packet.length = StunProofClient.RESPONSE_BYTES
                    packet.socketAddress = server
                }
            }
        socket.use {
            socket.soTimeout = 123
            assertTrue(StunProofClient.discover(listOf(server), credential, socket).isEmpty())
            assertEquals(2, sends.size)
            assertEquals(32, receives)
            assertTrue(TimeUnit.NANOSECONDS.toMillis(sends[1] - sends[0]) >= 1900L)
            assertEquals(123, socket.soTimeout)
        }
    }

    @Test
    fun missingServersDoNoWorkAndCancellationRestoresSharedSocketTimeout() {
        DatagramSocket().use { socket ->
            socket.soTimeout = 321
            assertTrue(StunProofClient.discover(emptyList(), credential, socket).isEmpty())
            val cancellation = IllegalStateException("cancelled")
            val result =
                runCatching {
                    StunProofClient.discover(
                        listOf(InetSocketAddress("127.0.0.1", 44444)),
                        credential,
                        socket,
                    ) { throw cancellation }
                }
            assertEquals(cancellation, result.exceptionOrNull())
            assertEquals(321, socket.soTimeout)
            assertFalse(socket.isClosed)
        }
    }

    @Test
    fun invalidServerSetsAreRejectedBeforeAnySocketWork() {
        var sends = 0
        val server = InetSocketAddress("127.0.0.1", 44444)
        val invalid =
            listOf(
                List(3) { InetSocketAddress("127.0.0.1", 44444 + it) },
                listOf(server, server),
                listOf(InetSocketAddress.createUnresolved("unused.invalid", 3478)),
                listOf(InetSocketAddress("127.0.0.1", 0)),
            )
        val socket =
            object : DatagramSocket(0, server.address) {
                override fun send(packet: DatagramPacket) {
                    sends++
                }
            }
        socket.use {
            socket.soTimeout = 456
            for (servers in invalid) {
                val error = runCatching { StunProofClient.discover(servers, credential, socket) }.exceptionOrNull()
                assertTrue(error is IllegalArgumentException)
                assertEquals(456, socket.soTimeout)
            }
            assertEquals(0, sends)
            assertFalse(socket.isClosed)
        }
    }

    private fun hex(value: String): ByteArray = value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
}

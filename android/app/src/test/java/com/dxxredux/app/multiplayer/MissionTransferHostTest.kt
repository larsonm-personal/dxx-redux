package com.dxxredux.app.multiplayer

import com.dxxredux.app.FileSetManager
import com.dxxredux.app.ModManager
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.Socket
import java.security.MessageDigest
import java.util.Random
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class MissionTransferHostTest {
    @get:Rule val temporaryFolder = TemporaryFolder()
    private val clients = mutableListOf<Socket>()
    private lateinit var requirement: MissionRequirement
    private lateinit var filesDir: File

    @After
    fun closeHost() {
        MissionTransferService.stopHost()
        clients.forEach { runCatching { it.close() } }
    }

    @Test(timeout = 15_000)
    fun slowHeadersDoNotOccupyAuthenticatedTransferSlots() {
        startHost()
        val grant = grant("player")
        repeat(7) { connect(grant) }
        await { count("pending") == 7 }
        assertEquals(0, count("active"))
        transfer(grant)
        assertEquals(7, count("pending"))
        assertEquals(0, count("grants"))
        await { count("pending") == 0 }
    }

    @Test(timeout = 15_000)
    fun absoluteHeaderDeadlineCannotBeExtendedByDrippingBytes() {
        startHost()
        val grant = grant("player")
        val socket = connect(grant)
        await { count("pending") == 1 }
        val started = System.nanoTime()
        while (System.nanoTime() - started < 3_000_000_000L) {
            runCatching { socket.getOutputStream().write(' '.code) }
            Thread.sleep(100)
            if (count("pending") == 0) break
        }
        assertEquals(0, count("pending"))
        assertTrue(TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - started) < 2800)
        assertEquals(1, count("grants"))
        transfer(grant)
    }

    @Test(timeout = 15_000)
    fun wrongSourceAndMalformedRequestsCannotBurnGrantOrKillHost() {
        startHost()
        val grant = grant("player")
        val wrongSource = connect(grant, "127.0.0.2")
        assertClosed(wrongSource)
        assertEquals(0, count("source_budgets"))
        assertEquals(1, count("grants"))
        val otherGrant = requireNotNull(MissionTransferService.authorize("other", "127.0.0.2", requirement.revision))
        val wrongToken = connect(otherGrant, "127.0.0.2")
        sendRequest(wrongToken, grant)
        assertClosed(wrongToken)
        assertEquals(2, count("grants"))
        transfer(otherGrant, "127.0.0.2")
        val invalid =
            listOf(
                "not-json",
                "{token:'${grant.token}',offset:0}",
                "{\"token\":\"${grant.token}\",\"offset\":\"0\"}",
                "{\"token\":\"${grant.token}\",\"offset\":0.5}",
                "{\"token\":\"${grant.token}\",\"offset\":-1}",
                "{\"token\":\"${grant.token}\",\"offset\":1}",
                "{\"token\":\"${grant.token}\",\"offset\":0,\"extra\":true}",
                "{\"token\":\"invalid\",\"offset\":0}",
                "\r",
                "\u0000",
                "[".repeat(4096),
            )
        invalid.forEach { request ->
            val client = connect(grant)
            runCatching { client.getOutputStream().write("$request\n".toByteArray()) }
            assertClosed(client)
            await { count("pending") == 0 }
            assertEquals(1, count("grants"))
        }
        transfer(grant)
        val replay = connect(grant)
        assertClosed(replay)
    }

    @Test(timeout = 20_000)
    fun sevenPlayersSharingAnAddressCanTransferConcurrently() {
        startHost(8 * 1024 * 1024)
        val grants = (1..7).map { grant("player-$it") }
        val sockets =
            grants.map { grant ->
                connect(grant).also { sendRequest(it, grant) }
            }
        await { count("active") == 7 }
        assertNull(MissionTransferService.authorize("player-1", "127.0.0.1", requirement.revision))
        assertEquals(0, count("grants"))
        val pool = Executors.newFixedThreadPool(7)
        try {
            val results =
                sockets.map { socket ->
                    pool.submit<Boolean> {
                        receive(socket)
                        true
                    }
                }
            results.forEach { assertTrue(it.get(12, TimeUnit.SECONDS)) }
        } finally {
            pool.shutdownNow()
        }
        await { count("active") == 0 }
    }

    @Test(timeout = 15_000)
    fun grantsAndPendingSocketsAreBoundedAndRetriesAreIdempotent() {
        startHost()
        val grants = (1..7).map { grant("player-$it") }
        repeat(100) {
            assertNull(MissionTransferService.authorize("extra-$it", "127.0.0.1", requirement.revision))
            assertEquals(grants[0], grant("player-1"))
        }
        assertNull(MissionTransferService.authorize("x".repeat(129), "127.0.0.1", requirement.revision))
        assertNull(MissionTransferService.authorize("bad", "localhost", requirement.revision))
        assertEquals(7, count("grants"))
        repeat(8) { connect(grants[0]) }
        await { count("pending") == 8 }
        assertClosed(connect(grants[0]))
        assertEquals(0, count("active"))
        MissionTransferService.maintainHost(System.nanoTime() + TimeUnit.SECONDS.toNanos(3))
        await { count("pending") == 0 }
        transfer(grants[0])
    }

    @Test(timeout = 15_000)
    fun globalPendingLimitAndSourceRateBudgetRecoverWithoutLosingGrants() {
        startHost()
        val first = grant("one")
        val second = requireNotNull(MissionTransferService.authorize("two", "127.0.0.2", requirement.revision))
        repeat(7) { connect(first) }
        repeat(7) { connect(second, "127.0.0.2") }
        await { count("pending") == 14 }
        assertClosed(connect(first))
        assertEquals(0, count("active"))
        MissionTransferService.maintainHost(System.nanoTime() + TimeUnit.SECONDS.toNanos(3))
        await { count("pending") == 0 }
        // Seven accepted above plus 25 below exhaust this source's 32-request window
        repeat(25) {
            val client = connect(first)
            client.getOutputStream().write("{}\n".toByteArray())
            assertClosed(client)
            await { count("pending") == 0 }
        }
        assertClosed(connect(first))
        assertEquals(2, count("grants"))
        MissionTransferService.maintainHost(System.nanoTime() + TimeUnit.SECONDS.toNanos(11))
        transfer(first)
        transfer(second, "127.0.0.2")
    }

    @Test(timeout = 15_000)
    fun sourceBudgetTableIsBoundedAcrossGrantAddressChurn() {
        startHost()
        for (index in 1..64) {
            val address = "127.0.0.$index"
            val grant = requireNotNull(MissionTransferService.authorize("moving", address, requirement.revision))
            val client = connect(grant, address)
            client.getOutputStream().write("{}\n".toByteArray())
            assertClosed(client)
        }
        assertEquals(64, count("source_budgets"))
        assertEquals(1, count("grants"))
        val next = requireNotNull(MissionTransferService.authorize("moving", "127.0.0.65", requirement.revision))
        assertClosed(connect(next, "127.0.0.65"))
        assertEquals(64, count("source_budgets"))
        MissionTransferService.maintainHost(System.nanoTime() + TimeUnit.SECONDS.toNanos(11))
        transfer(next, "127.0.0.65")
    }

    @Test(timeout = 15_000)
    fun simultaneousTokenReplayCannotStartASecondTransfer() {
        startHost(8 * 1024 * 1024)
        val grant = grant("player")
        val first = connect(grant)
        val replay = connect(grant)
        await { count("pending") == 2 }
        sendRequest(first, grant)
        await { count("active") == 1 }
        sendRequest(replay, grant)
        assertClosed(replay)
        assertEquals(1, count("active"))
        assertEquals(0, count("grants"))
        receive(first)
        await { count("active") == 0 }
    }

    @Test(timeout = 15_000)
    fun realHostSupportsAlignedResumeAndCompletedOffsets() {
        startHost(2 * 1024 * 1024)
        val grant = grant("resume")
        val socket = connect(grant)
        sendRequest(socket, grant, 1024 * 1024L)
        receive(socket, 1024 * 1024L)
        await { count("active") == 0 }
        val completedGrant = grant("resume")
        val completed = connect(completedGrant)
        sendRequest(completed, completedGrant, requireNotNull(requirement.sizeBytes))
        receive(completed, requireNotNull(requirement.sizeBytes))
    }

    @Test(timeout = 15_000)
    fun actualClientReceivesAndRevalidatesArchiveFromActualHost() {
        startHost(2 * 1024 * 1024)
        val clientFiles = temporaryFolder.newFolder()
        repeat(2) {
            val grant = grant("actual-client")
            MissionTransferService.receiveArchive(
                clientFiles,
                "127.0.0.1",
                grant,
                requirement,
                it + 1,
                Socket(),
                {},
                {},
            )
            assertEquals(requirement.sizeBytes, MissionTransferService.resumeOffset(clientFiles, requirement))
            await { count("active") == 0 }
        }
    }

    @Test(timeout = 15_000)
    fun monotonicGrantExpiryAndStalledTransferCleanupReleaseResources() {
        startHost(8 * 1024 * 1024)
        val expired = grant("expired")
        assertEquals(expired, grant("expired"))
        MissionTransferService.maintainHost(System.nanoTime() + TimeUnit.SECONDS.toNanos(31))
        assertEquals(0, count("grants"))
        assertClosed(connect(expired))
        val stalled = grant("stalled")
        val socket = connect(stalled)
        sendRequest(socket, stalled)
        await { count("active") == 1 }
        MissionTransferService.maintainHost(System.nanoTime() + TimeUnit.SECONDS.toNanos(16))
        await { count("active") == 0 }
        assertNotNull(MissionTransferService.authorize("stalled", "127.0.0.1", requirement.revision))
        transfer(grant("healthy"))
    }

    @Test(timeout = 25_000)
    fun realWatchdogClosesAClientThatStopsReading() {
        startHost(8 * 1024 * 1024)
        val stalled = grant("stalled")
        sendRequest(connect(stalled), stalled)
        await { count("active") == 1 }
        await(timeoutMs = 18_000) { count("active") == 0 }
        transfer(grant("healthy"))
    }

    @Test(timeout = 15_000)
    fun stopAndReplacementClosePendingAndActiveSocketsAndRevokeOldGrants() {
        startHost(8 * 1024 * 1024)
        val pendingGrant = grant("pending")
        val pending = connect(pendingGrant)
        val activeGrant = grant("active")
        val active = connect(activeGrant)
        sendRequest(active, activeGrant)
        await { count("pending") == 1 && count("active") == 1 }
        assertTrue(MissionTransferService.startHost(filesDir, requirement))
        assertClosed(pending)
        runCatching { active.getInputStream().readBytes() }.onFailure {
            assertFalse(it is java.net.SocketTimeoutException)
        }
        assertEquals(0, count("pending"))
        assertEquals(0, count("active"))
        val replacement = grant("replacement")
        val stale = connect(replacement)
        sendRequest(stale, pendingGrant)
        assertClosed(stale)
        transfer(replacement)
        MissionTransferService.stopHost()
        assertTrue(MissionTransferService.hostDiagnostics().isEmpty())
        assertNull(MissionTransferService.authorize("late", "127.0.0.1", requirement.revision))
    }

    private fun startHost(payloadBytes: Int = 128 * 1024) {
        filesDir = temporaryFolder.newFolder()
        val sets = FileSetManager(filesDir)
        val setDir = sets.getSetDir(sets.getActive())
        val archive = temporaryFolder.newFile()
        val payload = ByteArray(payloadBytes).also { Random(42).nextBytes(it) }
        ZipOutputStream(archive.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("secure.mn2"))
            zip.write("name = Secure\ntype = normal\nnum_levels = 1\nsecure.rl2\n".toByteArray())
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("secure.hog"))
            zip.write("DHF".toByteArray())
            zip.write("secure.rl2".toByteArray().copyOf(13))
            repeat(4) { zip.write((payload.size ushr (it * 8)) and 255) }
            zip.write(payload)
            zip.closeEntry()
        }
        val manager = ModManager(filesDir, setDir = setDir)
        assertNotNull(manager.importMissionZipFile(archive, "secure.zip"))
        val mission = MissionScanner.scan(filesDir, setDir, "d2", "coop").single { it.filename == "secure" }
        requirement = MissionScanner.requirement("d2", mission, true)
        assertTrue(requirement.offerAvailable)
        assertTrue(MissionTransferService.startHost(filesDir, requirement))
    }

    private fun grant(player: String): MissionTransferGrant =
        requireNotNull(MissionTransferService.authorize(player, "127.0.0.1", requirement.revision))

    private fun connect(
        grant: MissionTransferGrant,
        source: String = "127.0.0.1",
    ): Socket =
        Socket().also { socket ->
            clients.add(socket)
            socket.receiveBufferSize = 1024
            socket.soTimeout = 4000
            socket.bind(InetSocketAddress(InetAddress.getByName(source), 0))
            socket.connect(InetSocketAddress("127.0.0.1", grant.port), 2000)
        }

    private fun sendRequest(
        socket: Socket,
        grant: MissionTransferGrant,
        offset: Long = 0L,
    ) {
        socket.getOutputStream().write("{\"token\":\"${grant.token}\",\"offset\":$offset}\n".toByteArray())
    }

    private fun transfer(
        grant: MissionTransferGrant,
        source: String = "127.0.0.1",
    ) {
        val socket = connect(grant, source)
        sendRequest(socket, grant)
        receive(socket)
    }

    private fun receive(
        socket: Socket,
        offset: Long = 0L,
    ) {
        val input = socket.getInputStream().buffered()
        val header = StringBuilder()
        while (true) {
            val value = input.read()
            check(value >= 0 && header.length < MissionTransferService.MAX_HEADER_BYTES)
            if (value == '\n'.code) break
            header.append(value.toChar())
        }
        assertEquals(requirement.revision, JSONObject(header.toString()).getString("revision"))
        val digest = MessageDigest.getInstance("SHA-256")
        val buffer = ByteArray(64 * 1024)
        var received = 0L
        while (true) {
            val count = input.read(buffer)
            if (count < 0) break
            digest.update(buffer, 0, count)
            received += count
        }
        assertEquals(requireNotNull(requirement.sizeBytes) - offset, received)
        val expected =
            if (offset == 0L) {
                requirement.sha256
            } else {
                val wrapper = ModManager.forActiveSet(filesDir).modFile(requireNotNull(requirement.wrapperFilename))
                val expectedDigest = MessageDigest.getInstance("SHA-256")
                wrapper.inputStream().use { archive ->
                    var remaining = offset
                    while (remaining > 0) remaining -= archive.skip(remaining).also { check(it > 0) }
                    while (true) {
                        val count = archive.read(buffer)
                        if (count < 0) break
                        expectedDigest.update(buffer, 0, count)
                    }
                }
                expectedDigest.digest().joinToString("") { "%02x".format(it) }
            }
        assertEquals(expected, digest.digest().joinToString("") { "%02x".format(it) })
    }

    private fun assertClosed(socket: Socket) {
        val result = runCatching { socket.getInputStream().read() }
        if (result.isSuccess) {
            assertEquals(-1, result.getOrThrow())
        } else {
            assertFalse(result.exceptionOrNull() is java.net.SocketTimeoutException)
        }
    }

    private fun count(name: String): Int = MissionTransferService.hostDiagnostics()[name] ?: 0

    private fun await(
        timeoutMs: Long = 5000L,
        condition: () -> Boolean,
    ) {
        val deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(timeoutMs)
        while (!condition()) {
            check(System.nanoTime() < deadline) { "Timed out: ${MissionTransferService.hostDiagnostics()}" }
            Thread.sleep(10)
        }
    }
}

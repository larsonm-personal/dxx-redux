package com.dxxredux.app.multiplayer

import com.dxxredux.app.MissionContentIdentity
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket
import java.net.SocketException
import java.util.concurrent.Callable
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

class MissionTransferSecurityTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    private val chunkSize = MissionContentIdentity.DEFAULT_CHUNK_SIZE_BYTES

    @Test
    fun hostileNetworkMetadataNeverPersistsOrMutatesPartialContent() {
        val source = temporaryFolder.newFile("source.zip").apply { writeBytes(ByteArray(137) { it.toByte() }) }
        val identity = MissionContentIdentity.compute(source)
        val requirement = requirement(identity)
        val filesDir = temporaryFolder.newFolder("network")
        val partial =
            partial(filesDir, requirement).apply {
                parentFile!!.mkdirs()
                writeBytes(byteArrayOf(42))
            }
        for ((name, mutate) in invalidMetadata()) {
            val header = metadata(requirement, identity).apply(mutate)
            val (result, request) = exchange(filesDir, requirement, header.toString())
            assertTrue("Accepted $name", result.isFailure)
            assertTrue("Fatal error for $name", result.exceptionOrNull() is Exception)
            assertEquals(0L, request.getLong("offset"))
            assertArrayEquals("Changed partial for $name", byteArrayOf(42), partial.readBytes())
            assertFalse("Persisted $name", sidecar(filesDir, requirement).exists())
        }
    }

    @Test
    fun hostileSavedManifestsFailClosedBeforeAllocationDivisionOrFileMutation() {
        val source = temporaryFolder.newFile("saved-source.zip").apply { writeBytes(ByteArray(151) { it.toByte() }) }
        val identity = MissionContentIdentity.compute(source)
        val requirement = requirement(identity)
        val filesDir = temporaryFolder.newFolder("saved")
        val partial =
            partial(filesDir, requirement).apply {
                parentFile!!.mkdirs()
                writeBytes(source.readBytes())
            }
        val invalid =
            invalidMetadata() +
                listOf<Pair<String, JSONObject.() -> Unit>>(
                    "missing schema" to { remove("schema") },
                    "wrong schema" to { put("schema", 2) },
                    "coerced schema" to { put("schema", "1") },
                    "negative verified bytes" to { put("verified_bytes", -1L) },
                    "unaligned verified bytes" to { put("verified_bytes", 1L) },
                    "overflow verified bytes" to { put("verified_bytes", Long.MAX_VALUE) },
                    "coerced verified bytes" to { put("verified_bytes", identity.sizeBytes.toString()) },
                    "fractional verified bytes" to { put("verified_bytes", identity.sizeBytes + 0.5) },
                )
        for ((name, mutate) in invalid) {
            val json =
                metadata(
                    requirement,
                    identity,
                ).put("schema", 1).put("verified_bytes", identity.sizeBytes).apply(mutate)
            sidecar(filesDir, requirement).writeText(json.toString())
            assertEquals("Resumed $name", 0L, MissionTransferService.resumeOffset(filesDir, requirement))
            assertArrayEquals("Changed partial for $name", source.readBytes(), partial.readBytes())
            assertEquals("Rewrote $name", json.toString(), sidecar(filesDir, requirement).readText())
        }
    }

    @Test
    fun boundedHeaderAndManifestReadsRejectOversizeWithoutChangingPartial() {
        val source = temporaryFolder.newFile("bounded-source.zip").apply { writeBytes(byteArrayOf(1, 2, 3)) }
        val identity = MissionContentIdentity.compute(source)
        val requirement = requirement(identity)
        val filesDir = temporaryFolder.newFolder("bounded")
        val partial =
            partial(filesDir, requirement).apply {
                parentFile!!.mkdirs()
                writeBytes(source.readBytes())
            }
        val oversized =
            metadata(requirement, identity)
                .put("schema", 1)
                .put("verified_bytes", identity.sizeBytes)
                .put("padding", "x".repeat(MissionTransferService.MAX_HEADER_BYTES))
                .toString()
        assertTrue(exchange(filesDir, requirement, oversized).first.isFailure)
        assertFalse(sidecar(filesDir, requirement).exists())
        sidecar(filesDir, requirement).writeText(oversized)
        assertEquals(0L, MissionTransferService.resumeOffset(filesDir, requirement))
        assertArrayEquals(source.readBytes(), partial.readBytes())
    }

    @Test
    fun nestedHeadersAreRejectedBeforeRecursiveJsonParsing() {
        val source = temporaryFolder.newFile("nested-source.zip").apply { writeBytes(byteArrayOf(1)) }
        val identity = MissionContentIdentity.compute(source)
        val requirement = requirement(identity)
        val filesDir = temporaryFolder.newFolder("nested")
        for (prefix in listOf("{\"chunk_sha256\":", "{'padding':'\"', 'chunk_sha256':", "{/* \" */ 'chunk_sha256':")) {
            val nested = prefix + "[".repeat(8000) + "0" + "]".repeat(8000) + "}"
            val result = exchange(filesDir, requirement, nested).first
            assertTrue(result.exceptionOrNull() is IllegalArgumentException)
            assertFalse(sidecar(filesDir, requirement).exists())
        }
        val nested = "{\"chunk_sha256\":" + "[".repeat(8000) + "0" + "]".repeat(8000) + "}"
        assertFalse(partial(filesDir, requirement).exists())
        partial(filesDir, requirement).apply {
            parentFile!!.mkdirs()
            writeBytes(byteArrayOf(1))
        }
        sidecar(filesDir, requirement).writeText(nested)
        assertEquals(0L, MissionTransferService.resumeOffset(filesDir, requirement))
        assertArrayEquals(byteArrayOf(1), partial(filesDir, requirement).readBytes())
    }

    @Test
    fun maximumSizeMetadataFitsTheBoundAndAdmitsExactly256Hashes() {
        val identity =
            MissionContentIdentity(
                MISSION_TRANSFER_MAX_BYTES,
                "0".repeat(64),
                chunkSize,
                List(256) { "0".repeat(64) },
            )
        val requirement = requirement(identity)
        val filesDir = temporaryFolder.newFolder("maximum-size")
        val header = metadata(requirement, identity).toString()
        assertTrue(header.toByteArray().size < MissionTransferService.MAX_HEADER_BYTES)
        // Empty payload interrupts the transfer, but the accepted metadata is persisted for resume
        assertTrue(exchange(filesDir, requirement, header).first.isFailure)
        val saved = JSONObject(sidecar(filesDir, requirement).readText())
        assertEquals(256, saved.getJSONArray("chunk_sha256").length())
        assertEquals(0L, saved.getLong("verified_bytes"))
        assertEquals(0L, partial(filesDir, requirement).length())
    }

    @Test
    fun interruptedTcpTransferResumesAtVerifiedChunkAndCompletesUnalignedTail() {
        val bytes = ByteArray(chunkSize * 2 + 37) { (it * 31).toByte() }
        val source = temporaryFolder.newFile("resume-source.zip").apply { writeBytes(bytes) }
        val identity = MissionContentIdentity.compute(source)
        val requirement = requirement(identity)
        val filesDir = temporaryFolder.newFolder("resume")
        val header = metadata(requirement, identity).toString()
        val statuses = mutableListOf<MissionStatusReport>()
        val (interrupted, firstRequest) =
            exchange(
                filesDir,
                requirement,
                header,
                bytes.copyOf(chunkSize + 173),
                statuses,
            )
        assertTrue(interrupted.isFailure)
        assertEquals(0L, firstRequest.getLong("offset"))
        assertEquals(
            chunkSize.toLong(),
            sidecar(filesDir, requirement).readText().let(::JSONObject).getLong("verified_bytes"),
        )
        assertEquals(chunkSize.toLong(), MissionTransferService.resumeOffset(filesDir, requirement))
        assertEquals(chunkSize.toLong(), partial(filesDir, requirement).length())

        val (completed, secondRequest) =
            exchange(
                filesDir,
                requirement,
                header,
                bytes.copyOfRange(chunkSize, bytes.size),
                statuses,
            )
        completed.getOrThrow()
        assertEquals(chunkSize.toLong(), secondRequest.getLong("offset"))
        assertArrayEquals(bytes, partial(filesDir, requirement).readBytes())
        assertEquals(identity, MissionContentIdentity.compute(partial(filesDir, requirement)))
        assertEquals(identity.sizeBytes, MissionTransferService.resumeOffset(filesDir, requirement))
        assertEquals(identity.sizeBytes, statuses.last().verifiedBytes)
        assertTrue(statuses.all { it.validFor(requirement) })

        // A completed unaligned tail can be reverified without requesting payload again
        val (reverified, finalRequest) = exchange(filesDir, requirement, header)
        reverified.getOrThrow()
        assertEquals(identity.sizeBytes, finalRequest.getLong("offset"))
        assertArrayEquals(bytes, partial(filesDir, requirement).readBytes())
    }

    @Test
    fun invalidRequirementCannotChooseFilesystemPathsOrConnect() {
        val filesDir = temporaryFolder.newFolder("invalid-requirement")
        val base = requirement(MissionContentIdentity(1L, "0".repeat(64), chunkSize, listOf("0".repeat(64))))
        for (requirement in listOf(
            base.copy(sha256 = "../../outside"),
            base.copy(sizeBytes = 0L),
            base.copy(sizeBytes = MISSION_TRANSFER_MAX_BYTES + 1L),
            base.copy(offerAvailable = false),
            base.copy(kind = MissionRequirement.KIND_BUILTIN, wrapperFilename = null, offerAvailable = false),
        )) {
            Socket().use { socket ->
                val failure =
                    runCatching {
                        MissionTransferService.receiveArchive(
                            filesDir,
                            "127.0.0.1",
                            MissionTransferGrant("token", 1, base.revision),
                            requirement,
                            1,
                            socket,
                            {},
                            {},
                        )
                    }.exceptionOrNull()
                assertTrue(failure is IllegalArgumentException)
                assertFalse(socket.isConnected)
            }
            assertEquals(0L, MissionTransferService.resumeOffset(filesDir, requirement))
        }
        assertTrue(filesDir.listFiles()!!.isEmpty())
    }

    @Test
    fun directResumeValidationRejectsHostileChunkSizesAndCounts() {
        val bytes = byteArrayOf(1, 2, 3)
        val partial = temporaryFolder.newFile("direct.partial").apply { writeBytes(bytes) }
        val identity = MissionContentIdentity.compute(partial)
        for (size in listOf(Int.MIN_VALUE, -1, 0, 1, 1024, chunkSize * 2, Int.MAX_VALUE)) {
            assertEquals(0L, MissionTransferService.validatePartial(partial, 3L, 3L, size, identity.chunkSha256))
            assertArrayEquals(bytes, partial.readBytes())
        }
        for (hashes in listOf(emptyList(), listOf("f"), identity.chunkSha256 + identity.chunkSha256)) {
            assertEquals(0L, MissionTransferService.validatePartial(partial, 3L, 3L, chunkSize, hashes))
        }
        for (size in listOf(Long.MIN_VALUE, -1L, 0L, MISSION_TRANSFER_MAX_BYTES + 1L, Long.MAX_VALUE)) {
            assertEquals(0L, MissionTransferService.validatePartial(partial, 3L, size, chunkSize, identity.chunkSha256))
        }
        assertArrayEquals(bytes, partial.readBytes())
    }

    private fun invalidMetadata(): List<Pair<String, JSONObject.() -> Unit>> =
        listOf(
            "zero chunk" to { put("chunk_size", 0) },
            "negative chunk" to { put("chunk_size", -1) },
            "huge chunk" to { put("chunk_size", Int.MAX_VALUE) },
            "wrapped chunk" to { put("chunk_size", 4_296_015_872L) },
            "overflow chunk" to { put("chunk_size", Long.MAX_VALUE) },
            "fractional chunk" to { put("chunk_size", chunkSize + 0.5) },
            "coerced chunk" to { put("chunk_size", chunkSize.toString()) },
            "tiny chunk" to { put("chunk_size", 1) },
            "missing chunk" to { remove("chunk_size") },
            "null chunk" to { put("chunk_size", JSONObject.NULL) },
            "zero size" to { put("size_bytes", 0) },
            "negative size" to { put("size_bytes", -1) },
            "huge size" to { put("size_bytes", Long.MAX_VALUE) },
            "coerced size" to { put("size_bytes", getLong("size_bytes").toString()) },
            "fractional size" to { put("size_bytes", getLong("size_bytes") + 0.5) },
            "wrong revision" to { put("revision", "old-round") },
            "wrong whole hash" to { put("sha256", "f".repeat(64)) },
            "missing hashes" to { remove("chunk_sha256") },
            "empty hashes" to { put("chunk_sha256", JSONArray()) },
            "extra hashes" to { getJSONArray("chunk_sha256").put("0".repeat(64)) },
            "too many hashes" to { put("chunk_sha256", JSONArray(List(257) { "0".repeat(64) })) },
            "non-string hash" to { put("chunk_sha256", JSONArray(listOf(1))) },
            "short hash" to { put("chunk_sha256", JSONArray(listOf("0".repeat(63)))) },
            "long hash" to { put("chunk_sha256", JSONArray(listOf("0".repeat(65)))) },
            "uppercase hash" to { put("chunk_sha256", JSONArray(listOf("A".repeat(64)))) },
            "non-hex hash" to { put("chunk_sha256", JSONArray(listOf("g".repeat(64)))) },
        )

    private fun requirement(identity: MissionContentIdentity) =
        MissionRequirement(
            revision = "security-test",
            game = "d2",
            missionKey = "custom",
            displayName = "Custom",
            kind = MissionRequirement.KIND_WRAPPER,
            wrapperFilename = "custom.zip",
            sizeBytes = identity.sizeBytes,
            sha256 = identity.sha256,
            offerAvailable = true,
        )

    private fun metadata(
        requirement: MissionRequirement,
        identity: MissionContentIdentity,
    ) = JSONObject()
        .put("revision", requirement.revision)
        .put("size_bytes", identity.sizeBytes)
        .put("sha256", identity.sha256)
        .put("chunk_size", identity.chunkSizeBytes)
        .put("chunk_sha256", JSONArray(identity.chunkSha256))

    private fun partial(
        filesDir: File,
        requirement: MissionRequirement,
    ) = File(filesDir, ".mission_transfers/${requirement.sha256}.partial")

    private fun sidecar(
        filesDir: File,
        requirement: MissionRequirement,
    ) = File(filesDir, ".mission_transfers/${requirement.sha256}.json")

    private fun exchange(
        filesDir: File,
        requirement: MissionRequirement,
        header: String,
        payload: ByteArray = byteArrayOf(),
        statuses: MutableList<MissionStatusReport> = mutableListOf(),
    ): Pair<Result<Unit>, JSONObject> =
        ServerSocket(0, 1, InetAddress.getLoopbackAddress()).use { listener ->
            listener.soTimeout = 5000
            val executor = Executors.newSingleThreadExecutor()
            try {
                val request =
                    executor.submit(
                        Callable {
                            listener.accept().use { peer ->
                                peer.soTimeout = 5000
                                val request = JSONObject(peer.getInputStream().bufferedReader().readLine())
                                try {
                                    peer.getOutputStream().apply {
                                        write(header.toByteArray(Charsets.UTF_8))
                                        write('\n'.code)
                                        write(payload)
                                        flush()
                                    }
                                } catch (_: SocketException) {
                                    // Rejecting a malicious header may close the socket before the peer finishes writing
                                }
                                request
                            }
                        },
                    )
                val result =
                    runCatching {
                        Socket().use { socket ->
                            MissionTransferService.receiveArchive(
                                filesDir,
                                listener.inetAddress.hostAddress!!,
                                MissionTransferGrant("test-token", listener.localPort, requirement.revision),
                                requirement,
                                1,
                                socket,
                                { statuses.add(it) },
                                {},
                            )
                        }
                    }
                result to request.get(10, TimeUnit.SECONDS)
            } finally {
                executor.shutdownNow()
                assertTrue(executor.awaitTermination(5, TimeUnit.SECONDS))
            }
        }
}

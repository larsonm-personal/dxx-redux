package com.dxxredux.app.multiplayer

import android.content.Context
import com.dxxredux.app.AtomicFilePublication
import com.dxxredux.app.FileSetManager
import com.dxxredux.app.MissionContentIdentity
import com.dxxredux.app.ModManager
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.InputStream
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket
import java.security.MessageDigest
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.Semaphore
import java.util.concurrent.locks.LockSupport

internal data class MissionTransferGrant(
    val token: String,
    val port: Int,
    val revision: String,
)

internal object MissionTransferService {
    private const val SOCKET_TIMEOUT_MS = 15_000
    private const val TOKEN_LIFETIME_MS = 30_000L
    private const val MAX_REQUEST_BYTES = 4096
    private const val MAX_HEADER_BYTES = 256 * 1024
    private const val BUFFER_BYTES = 64 * 1024
    private const val MAX_CONCURRENT_TRANSFERS = 7
    private const val PER_CLIENT_BYTES_PER_SECOND = 2L * 1024L * 1024L

    private data class Authorization(
        val playerId: String,
        val address: String,
        val expiresAtMs: Long,
    )

    private data class HostSession(
        val requirement: MissionRequirement,
        val wrapper: File,
        val identity: MissionContentIdentity,
        val socket: ServerSocket,
        val scope: CoroutineScope,
        val authorizations: ConcurrentHashMap<String, Authorization>,
        val slots: Semaphore,
    )

    @Volatile private var host: HostSession? = null
    private var clientJob: Job? = null

    @Synchronized
    fun startHost(
        context: Context,
        requirement: MissionRequirement,
    ): Boolean {
        stopHost()
        if (!requirement.isWrapper || !requirement.offerAvailable || !requirement.isValid) return false
        val sets = FileSetManager(context.filesDir)
        val setDir = sets.getSetDir(sets.getActive())
        val manager = ModManager(context.filesDir, context, setDir)
        val filename = requirement.wrapperFilename ?: return false
        val wrapper = manager.modFile(filename).canonicalFile
        val root = manager.importDirectory().canonicalFile
        if (!wrapper.isFile || !wrapper.toPath().startsWith(root.toPath())) return false
        val mod = manager.listMods().firstOrNull { it.filename == filename } ?: return false
        val identity = manager.ensureMissionContentIdentity(mod.filename) ?: return false
        if (identity.sizeBytes != requirement.sizeBytes || identity.sha256 != requirement.sha256) return false
        val socket = ServerSocket(0)
        socket.reuseAddress = true
        val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
        val session =
            HostSession(
                requirement,
                wrapper,
                identity,
                socket,
                scope,
                ConcurrentHashMap(),
                Semaphore(MAX_CONCURRENT_TRANSFERS, true),
            )
        host = session
        scope.launch { acceptLoop(session) }
        return true
    }

    @Synchronized
    fun stopHost() {
        val session = host ?: return
        host = null
        runCatching { session.socket.close() }
        session.scope.cancel()
        session.authorizations.clear()
    }

    fun authorize(
        playerId: String,
        address: String,
        revision: String,
    ): MissionTransferGrant? {
        val session = host ?: return null
        if (revision != session.requirement.revision || !session.requirement.offerAvailable) return null
        val token = UUID.randomUUID().toString()
        session.authorizations[token] =
            Authorization(playerId.take(128), address, System.currentTimeMillis() + TOKEN_LIFETIME_MS)
        return MissionTransferGrant(token, session.socket.localPort, revision)
    }

    @Synchronized
    fun download(
        context: Context,
        hostAddress: String,
        grant: MissionTransferGrant,
        requirement: MissionRequirement,
        attempt: Int,
        onStatus: (MissionStatusReport) -> Unit,
        onFinished: (Boolean) -> Unit,
    ) {
        clientJob?.cancel()
        clientJob =
            CoroutineScope(SupervisorJob() + Dispatchers.IO).launch {
                try {
                    receive(context, hostAddress, grant, requirement, attempt, onStatus)
                    onFinished(true)
                } catch (_: CancellationException) {
                    onFinished(false)
                } catch (_: Exception) {
                    val offset = resumeOffset(context, requirement)
                    onStatus(
                        transferReport(
                            requirement,
                            MissionCompatibilityStatus.FAILED_RESUMABLE,
                            offset,
                            grant.token,
                            attempt,
                            "transfer_interrupted",
                        ),
                    )
                    onFinished(false)
                }
            }
    }

    @Synchronized
    fun cancelClient() {
        clientJob?.cancel()
        clientJob = null
    }

    private suspend fun acceptLoop(session: HostSession) {
        while (session.scope.isActive) {
            val client =
                try {
                    session.socket.accept()
                } catch (_: Exception) {
                    break
                }
            session.scope.launch {
                if (!session.slots.tryAcquire()) {
                    runCatching { client.close() }
                    return@launch
                }
                try {
                    serve(session, client)
                } finally {
                    session.slots.release()
                    runCatching { client.close() }
                }
            }
        }
    }

    private fun serve(
        session: HostSession,
        client: Socket,
    ) {
        client.soTimeout = SOCKET_TIMEOUT_MS
        val request = JSONObject(readLineBounded(client.getInputStream(), MAX_REQUEST_BYTES))
        val token = request.optString("token")
        val authorization = session.authorizations.remove(token) ?: return
        if (authorization.expiresAtMs < System.currentTimeMillis()) return
        val remote = client.inetAddress.hostAddress ?: return
        if (!sameAddress(authorization.address, remote)) return
        val offset = request.optLong("offset", -1L)
        val chunkSize = session.identity.chunkSizeBytes
        if (
            offset < 0L ||
            offset > session.identity.sizeBytes ||
            (offset != session.identity.sizeBytes && offset % chunkSize != 0L)
        ) {
            return
        }
        val header =
            JSONObject()
                .put("revision", session.requirement.revision)
                .put("size_bytes", session.identity.sizeBytes)
                .put("sha256", session.identity.sha256)
                .put("chunk_size", chunkSize)
                .put("chunk_sha256", JSONArray(session.identity.chunkSha256))
        val output = client.getOutputStream()
        output.write(header.toString().toByteArray(Charsets.UTF_8))
        output.write('\n'.code)
        session.wrapper.inputStream().buffered(BUFFER_BYTES).use { input ->
            var remaining = offset
            while (remaining > 0L) {
                val skipped = input.skip(remaining)
                if (skipped <= 0L) return
                remaining -= skipped
            }
            val buffer = ByteArray(BUFFER_BYTES)
            val startedAtNs = System.nanoTime()
            var sentBytes = 0L
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                output.write(buffer, 0, count)
                sentBytes += count
                throttleTransfer(startedAtNs, sentBytes)
            }
            output.flush()
        }
    }

    private fun throttleTransfer(
        startedAtNs: Long,
        sentBytes: Long,
    ) {
        val targetElapsedNs = sentBytes * 1_000_000_000L / PER_CLIENT_BYTES_PER_SECOND
        val remainingNs = targetElapsedNs - (System.nanoTime() - startedAtNs)
        if (remainingNs > 0L) LockSupport.parkNanos(remainingNs)
    }

    private fun receive(
        context: Context,
        hostAddress: String,
        grant: MissionTransferGrant,
        requirement: MissionRequirement,
        attempt: Int,
        onStatus: (MissionStatusReport) -> Unit,
    ) {
        require(grant.revision == requirement.revision)
        val files = partialFiles(context, requirement)
        files.root.mkdirs()
        var offset = resumeOffset(context, requirement)
        Socket(InetAddress.getByName(hostAddress), grant.port).use { socket ->
            socket.soTimeout = SOCKET_TIMEOUT_MS
            val request = JSONObject().put("token", grant.token).put("offset", offset)
            socket.getOutputStream().apply {
                write(request.toString().toByteArray(Charsets.UTF_8))
                write('\n'.code)
                flush()
            }
            val header = JSONObject(readLineBounded(socket.getInputStream(), MAX_HEADER_BYTES))
            val size = header.getLong("size_bytes")
            val sha256 = header.getString("sha256")
            val chunkSize = header.getInt("chunk_size")
            val chunks = header.getJSONArray("chunk_sha256").toStringList()
            require(size == requirement.sizeBytes && sha256 == requirement.sha256)
            require(chunkSize > 0 && chunks.isNotEmpty())
            offset = validatePartial(files.partial, offset, size, chunkSize, chunks)
            require(offset == request.getLong("offset"))
            writePartialManifest(files.sidecar, requirement, chunkSize, chunks, offset)
            onStatus(transferReport(requirement, MissionCompatibilityStatus.DOWNLOADING, offset, grant.token, attempt))
            appendAndVerify(
                socket.getInputStream(),
                files.partial,
                requirement,
                chunkSize,
                chunks,
                offset,
            ) { verified ->
                writePartialManifest(files.sidecar, requirement, chunkSize, chunks, verified)
                onStatus(
                    transferReport(
                        requirement,
                        MissionCompatibilityStatus.DOWNLOADING,
                        verified,
                        grant.token,
                        attempt,
                    ),
                )
            }
        }
        onStatus(
            transferReport(
                requirement,
                MissionCompatibilityStatus.VERIFYING,
                requirement.sizeBytes ?: 0L,
                grant.token,
                attempt,
            ),
        )
        val identity = MissionContentIdentity.compute(files.partial)
        require(identity.sizeBytes == requirement.sizeBytes && identity.sha256 == requirement.sha256)
        val sets = FileSetManager(context.filesDir)
        val setDir = sets.getSetDir(sets.getActive())
        val manager = ModManager(context.filesDir, context, setDir)
        val imported = manager.importMissionZipFileMovingSource(files.partial, uniqueDownloadName(manager, requirement))
        require(imported != null)
        promoteImportedMission(manager, imported.filename)
        files.sidecar.delete()
        val resolved = MissionCompatibilityResolver.resolve(context, requirement, "coop")
        require(resolved.status == MissionCompatibilityStatus.MATCH)
        onStatus(resolved.copy(verifiedBytes = requirement.sizeBytes))
    }

    private fun appendAndVerify(
        input: InputStream,
        partial: File,
        requirement: MissionRequirement,
        chunkSize: Int,
        chunks: List<String>,
        offset: Long,
        onVerified: (Long) -> Unit,
    ) {
        val expectedSize = requirement.sizeBytes ?: error("Missing transfer size")
        partial.parentFile?.mkdirs()
        java.io.RandomAccessFile(partial, "rw").use { output ->
            output.setLength(offset)
            output.seek(offset)
            val buffer = ByteArray(BUFFER_BYTES)
            var written = offset
            var chunkIndex = (offset / chunkSize).toInt()
            var chunkBytes = 0
            var digest = MessageDigest.getInstance("SHA-256")
            while (written < expectedSize) {
                val count = input.read(buffer, 0, minOf(buffer.size.toLong(), expectedSize - written).toInt())
                if (count < 0) error("Mission transfer ended early")
                output.write(buffer, 0, count)
                var sourceOffset = 0
                while (sourceOffset < count) {
                    val used = minOf(count - sourceOffset, chunkSize - chunkBytes)
                    digest.update(buffer, sourceOffset, used)
                    sourceOffset += used
                    chunkBytes += used
                    written += used
                    if (chunkBytes == chunkSize || written == expectedSize) {
                        require(digest.digest().toHex() == chunks[chunkIndex])
                        output.fd.sync()
                        onVerified(written)
                        chunkIndex++
                        chunkBytes = 0
                        digest = MessageDigest.getInstance("SHA-256")
                    }
                }
            }
            require(input.read() < 0) { "Mission transfer exceeds advertised size" }
        }
    }

    private fun resumeOffset(
        context: Context,
        requirement: MissionRequirement,
    ): Long {
        val files = partialFiles(context, requirement)
        if (!files.partial.isFile || !files.sidecar.isFile) return 0L
        return runCatching {
            val sidecar = JSONObject(files.sidecar.readText())
            if (sidecar.getString("revision") != requirement.revision) return@runCatching 0L
            val chunkSize = sidecar.getInt("chunk_size")
            val chunks = sidecar.getJSONArray("chunk_sha256").toStringList()
            validatePartial(
                files.partial,
                sidecar.getLong("verified_bytes"),
                requirement.sizeBytes ?: return@runCatching 0L,
                chunkSize,
                chunks,
            )
        }.getOrDefault(0L)
    }

    internal fun validatePartial(
        partial: File,
        claimedBytes: Long,
        expectedSize: Long,
        chunkSize: Int,
        chunks: List<String>,
    ): Long {
        if (
            !partial.isFile ||
            claimedBytes <= 0L ||
            claimedBytes > expectedSize ||
            (claimedBytes != expectedSize && claimedBytes % chunkSize != 0L)
        ) {
            return 0L
        }
        val limit = minOf(claimedBytes, partial.length(), expectedSize)
        var verified = 0L
        partial.inputStream().buffered(BUFFER_BYTES).use { input ->
            val buffer = ByteArray(chunkSize)
            var index = 0
            while (verified < limit && index < chunks.size) {
                val expectedChunkBytes = minOf(chunkSize.toLong(), expectedSize - verified).toInt()
                if (verified + expectedChunkBytes > limit) break
                var used = 0
                while (used < expectedChunkBytes) {
                    val count = input.read(buffer, used, expectedChunkBytes - used)
                    if (count < 0) break
                    used += count
                }
                val digest =
                    MessageDigest
                        .getInstance("SHA-256")
                        .apply { update(buffer, 0, used) }
                        .digest()
                        .toHex()
                if (used != expectedChunkBytes || digest != chunks[index]) {
                    break
                }
                verified += used
                index++
            }
        }
        java.io.RandomAccessFile(partial, "rw").use { it.setLength(verified) }
        return verified
    }

    private data class PartialFiles(
        val root: File,
        val partial: File,
        val sidecar: File,
    )

    private fun partialFiles(
        context: Context,
        requirement: MissionRequirement,
    ): PartialFiles {
        val root = File(context.filesDir, ".mission_transfers")
        val key =
            requirement.sha256 ?: requirement.revision
                .hashCode()
                .toUInt()
                .toString(16)
        return PartialFiles(root, File(root, "$key.partial"), File(root, "$key.json"))
    }

    private fun writePartialManifest(
        file: File,
        requirement: MissionRequirement,
        chunkSize: Int,
        chunks: List<String>,
        verifiedBytes: Long,
    ) {
        val json =
            JSONObject()
                .put("schema", 1)
                .put("revision", requirement.revision)
                .put("size_bytes", requirement.sizeBytes)
                .put("sha256", requirement.sha256)
                .put("chunk_size", chunkSize)
                .put("chunk_sha256", JSONArray(chunks))
                .put("verified_bytes", verifiedBytes)
        AtomicFilePublication.writeUtf8(file, json.toString())
    }

    private fun uniqueDownloadName(
        manager: ModManager,
        requirement: MissionRequirement,
    ): String {
        val original = requirement.wrapperFilename ?: "mission.zip"
        val existing = manager.modFile(original)
        if (!existing.exists()) return original
        val dot = original.lastIndexOf('.')
        val stem = if (dot > 0) original.substring(0, dot) else original
        val suffix = if (dot > 0) original.substring(dot) else ".zip"
        return "${stem}_${requirement.sha256?.take(8)}$suffix"
    }

    private fun promoteImportedMission(
        manager: ModManager,
        filename: String,
    ) {
        while (true) {
            val ordered = manager.listMods().sortedBy { it.order }
            val index = ordered.indexOfFirst { it.filename == filename }
            if (index <= 0) return
            manager.moveUp(index)
        }
    }

    private fun transferReport(
        requirement: MissionRequirement,
        status: MissionCompatibilityStatus,
        verifiedBytes: Long,
        transferId: String,
        attempt: Int,
        failureCode: String? = null,
    ): MissionStatusReport =
        MissionStatusReport(
            revision = requirement.revision,
            status = status,
            verifiedBytes = verifiedBytes,
            totalBytes = requirement.sizeBytes ?: 0L,
            transferId = transferId.take(64),
            attempt = attempt,
            failureCode = failureCode,
        )

    private fun readLineBounded(
        input: InputStream,
        maxBytes: Int,
    ): String {
        val bytes = ByteArray(maxBytes)
        var used = 0
        while (used < bytes.size) {
            val value = input.read()
            if (value < 0) error("Unexpected end of transfer header")
            if (value == '\n'.code) return String(bytes, 0, used, Charsets.UTF_8)
            if (value == '\r'.code || value < 0x20) error("Invalid transfer header")
            bytes[used++] = value.toByte()
        }
        error("Transfer header exceeds $maxBytes bytes")
    }

    private fun JSONArray.toStringList(): List<String> = (0 until length()).map { index -> getString(index) }

    private fun sameAddress(
        expected: String,
        actual: String,
    ): Boolean = runCatching { InetAddress.getByName(expected) == InetAddress.getByName(actual) }.getOrDefault(false)

    private fun ByteArray.toHex(): String = joinToString("") { "%02x".format(it) }
}

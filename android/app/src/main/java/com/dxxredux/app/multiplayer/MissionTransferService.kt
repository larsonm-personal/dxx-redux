package com.dxxredux.app.multiplayer

import android.content.Context
import android.util.Log
import com.dxxredux.app.AtomicFilePublication
import com.dxxredux.app.FileSetManager
import com.dxxredux.app.MissionContentIdentity
import com.dxxredux.app.MissionDistributionPolicy
import com.dxxredux.app.MissionDownloadPolicy
import com.dxxredux.app.ModManager
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.serialization.json.Json
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.InputStream
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.security.MessageDigest
import java.util.UUID
import java.util.concurrent.locks.LockSupport

internal data class MissionTransferGrant(
    val token: String,
    val port: Int,
    val revision: String,
)

internal object MissionTransferService {
    private const val TAG = "MissionTransfer"
    private const val SOCKET_TIMEOUT_MS = 15_000
    private const val TOKEN_LIFETIME_MS = 30_000L
    private const val REQUEST_TIMEOUT_MS = 2_000L
    private const val MAX_TRANSFER_LIFETIME_MS = 30L * 60L * 1000L
    private const val MAX_PENDING_REQUESTS = 14
    private const val MAX_PENDING_PER_SOURCE = 8
    private const val ACCEPT_WINDOW_MS = 10_000L
    private const val MAX_ACCEPTS_PER_WINDOW = 32
    private const val MAX_SOURCE_BUDGETS = 64
    private const val MAX_REQUEST_BYTES = 4096

    // 256 one-MiB chunks fit below 18 KiB including their hashes
    internal const val MAX_HEADER_BYTES = 32 * 1024
    private const val BUFFER_BYTES = 64 * 1024
    private const val STATUS_REPORT_INTERVAL_NS = 250L * 1000L * 1000L
    private const val MAX_CONCURRENT_TRANSFERS = 7
    private const val PER_CLIENT_BYTES_PER_SECOND = 32L * 1024L * 1024L

    private data class Authorization(
        val playerId: String,
        val address: InetAddress,
        val issuedAtNs: Long,
    )

    private class HostConnection(
        val address: InetAddress,
        val acceptedAtNs: Long,
    ) {
        var playerId: String? = null
        var lastProgressNs = acceptedAtNs
    }

    private class AcceptBudget(
        val startedAtNs: Long,
        var count: Int = 0,
    )

    private class MissionFinalizationException(
        cause: Throwable,
    ) : Exception(cause)

    private data class HostSession(
        val requirement: MissionRequirement,
        val wrapper: File,
        val identity: MissionContentIdentity,
        val socket: ServerSocket,
        val scope: CoroutineScope,
        val authorizations: MutableMap<String, Authorization> = mutableMapOf(),
        val clients: MutableMap<Socket, HostConnection> = mutableMapOf(),
        val acceptBudgets: MutableMap<InetAddress, AcceptBudget> = mutableMapOf(),
    )

    @Volatile private var host: HostSession? = null

    private class ClientTransfer(
        val socket: Socket = Socket(),
    ) {
        lateinit var job: Job
    }

    private val clientScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val clientWriter = Mutex()
    private var clientTransfer: ClientTransfer? = null

    @Synchronized
    fun startHost(
        context: Context,
        requirement: MissionRequirement,
    ): Boolean = startHost(context.filesDir, requirement, context)

    @Synchronized
    internal fun startHost(
        filesDir: File,
        requirement: MissionRequirement,
        context: Context? = null,
    ): Boolean {
        stopHost()
        if (!requirement.isWrapper || !requirement.offerAvailable || !requirement.isValid) return false
        val sets = FileSetManager(filesDir)
        val setDir = sets.getSetDir(sets.getActive())
        val manager = ModManager(filesDir, context, setDir)
        val filename = requirement.wrapperFilename ?: return false
        val wrapper = manager.modFile(filename).canonicalFile
        val root = manager.importDirectory().canonicalFile
        if (!wrapper.isFile || !wrapper.toPath().startsWith(root.toPath())) return false
        // Reinspect local bytes; a peer's requirement and the picker are not authorization
        if (!hostArchiveAllowed(requirement, wrapper)) return false
        val mod = manager.listMods().firstOrNull { it.filename == filename } ?: return false
        val identity = manager.ensureMissionContentIdentity(mod.filename) ?: return false
        if (identity.sizeBytes != requirement.sizeBytes || identity.sha256 != requirement.sha256) return false
        if (!validChunkMetadata(identity.sizeBytes, identity.chunkSizeBytes, identity.chunkSha256)) return false
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
            )
        host = session
        scope.launch { acceptLoop(session) }
        scope.launch {
            while (isActive) {
                delay(250L)
                expireHostConnections(session, System.nanoTime())
            }
        }
        return true
    }

    internal fun hostArchiveAllowed(
        requirement: MissionRequirement,
        wrapper: File,
    ): Boolean =
        requirement.isValid && requirement.isWrapper && requirement.offerAvailable &&
            MissionDistributionPolicy.missionPolicy(requirement.missionKey) == MissionDownloadPolicy.USER_SUPPLIED &&
            MissionDistributionPolicy.archivePolicy(wrapper) == MissionDownloadPolicy.USER_SUPPLIED

    @Synchronized
    fun stopHost() {
        val session = host ?: return
        host = null
        synchronized(session) {
            runCatching { session.socket.close() }
            session.scope.cancel()
            session.clients.keys.forEach { runCatching { it.close() } }
            session.clients.clear()
            session.authorizations.clear()
            session.acceptBudgets.clear()
        }
    }

    fun authorize(
        playerId: String,
        address: String,
        revision: String,
    ): MissionTransferGrant? {
        val session = host ?: return null
        if (revision != session.requirement.revision || !session.requirement.offerAvailable) return null
        if (playerId.isBlank() || playerId.length > 128) return null
        val numericAddress = numericAddress(address) ?: return null
        synchronized(session) {
            if (host !== session || !session.scope.isActive) return null
            val now = System.nanoTime()
            pruneGrants(session, now)
            if (session.clients.values.any { it.playerId == playerId }) return null
            val existing = session.authorizations.entries.firstOrNull { it.value.playerId == playerId }
            if (existing != null) {
                if (existing.value.address == numericAddress) {
                    return MissionTransferGrant(existing.key, session.socket.localPort, revision)
                }
                session.authorizations.remove(existing.key)
            }
            if (session.authorizations.size >= MAX_CONCURRENT_TRANSFERS) return null
            val token = UUID.randomUUID().toString()
            session.authorizations[token] = Authorization(playerId, numericAddress, now)
            return MissionTransferGrant(token, session.socket.localPort, revision)
        }
    }

    private fun pruneGrants(
        session: HostSession,
        nowNs: Long,
    ) {
        session.authorizations.entries.removeAll { nowNs - it.value.issuedAtNs >= TOKEN_LIFETIME_MS * 1_000_000L }
    }

    // One timer closes blocking reads/writes; SO_TIMEOUT alone cannot bound writes
    private fun expireHostConnections(
        session: HostSession,
        nowNs: Long,
    ) {
        synchronized(session) {
            pruneGrants(session, nowNs)
            session.acceptBudgets.entries.removeAll { nowNs - it.value.startedAtNs >= ACCEPT_WINDOW_MS * 1_000_000L }
            session.clients.forEach { (socket, connection) ->
                val age = nowNs - connection.acceptedAtNs
                val expired =
                    if (connection.playerId == null) {
                        age >= REQUEST_TIMEOUT_MS * 1_000_000L
                    } else {
                        nowNs - connection.lastProgressNs >= SOCKET_TIMEOUT_MS * 1_000_000L ||
                            age >= MAX_TRANSFER_LIFETIME_MS * 1_000_000L
                    }
                if (expired) runCatching { socket.close() }
            }
        }
    }

    internal fun hostDiagnostics(): Map<String, Int> {
        val session = host ?: return emptyMap()
        return synchronized(session) {
            mapOf(
                "pending" to session.clients.values.count { it.playerId == null },
                "active" to session.clients.values.count { it.playerId != null },
                "grants" to session.authorizations.size,
                "source_budgets" to session.acceptBudgets.size,
            )
        }
    }

    internal fun maintainHost(nowNs: Long = System.nanoTime()) {
        host?.let { expireHostConnections(it, nowNs) }
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
        cancelClient()
        val transfer = ClientTransfer()
        clientTransfer = transfer
        val report: (MissionStatusReport) -> Unit = { status ->
            transfer.job.ensureActive()
            publishClientResult(transfer) { onStatus(status) }
            transfer.job.ensureActive()
        }
        transfer.job =
            clientScope.launch(start = CoroutineStart.LAZY) {
                // Cancellation does not release file ownership until blocking work has actually returned
                clientWriter.withLock {
                    try {
                        transfer.job.ensureActive()
                        receive(context, hostAddress, grant, requirement, attempt, transfer.socket, report) {
                            transfer.job.ensureActive()
                        }
                        publishClientResult(transfer) { onFinished(true) }
                    } catch (e: CancellationException) {
                        throw e
                    } catch (e: MissionFinalizationException) {
                        transfer.job.ensureActive()
                        Log.e(TAG, "Mission finalization failed", e.cause)
                        report(
                            transferReport(
                                requirement,
                                MissionCompatibilityStatus.ERROR,
                                requirement.sizeBytes ?: 0L,
                                grant.token,
                                attempt,
                                "mission_finalization_failed",
                            ),
                        )
                        publishClientResult(transfer) { onFinished(false) }
                    } catch (e: Exception) {
                        transfer.job.ensureActive()
                        Log.e(TAG, "Mission transfer or finalization failed", e)
                        val offset = resumeOffset(context, requirement)
                        report(
                            transferReport(
                                requirement,
                                MissionCompatibilityStatus.FAILED_RESUMABLE,
                                offset,
                                grant.token,
                                attempt,
                                "transfer_interrupted",
                            ),
                        )
                        publishClientResult(transfer) { onFinished(false) }
                    } finally {
                        runCatching { transfer.socket.close() }
                    }
                }
            }
        transfer.job.start()
    }

    @Synchronized
    fun cancelClient() {
        val previous = clientTransfer ?: return
        clientTransfer = null
        previous.job.cancel()
        runCatching { previous.socket.close() }
    }

    @Synchronized
    private fun publishClientResult(
        transfer: ClientTransfer,
        publish: () -> Unit,
    ) {
        if (clientTransfer === transfer && transfer.job.isActive) publish()
    }

    private suspend fun acceptLoop(session: HostSession) {
        while (session.scope.isActive) {
            val client =
                try {
                    session.socket.accept()
                } catch (_: Exception) {
                    break
                }
            val accepted =
                synchronized(session) {
                    admitRequest(session, client, System.nanoTime())
                }
            if (!accepted) {
                runCatching { client.close() }
                continue
            }
            session.scope.launch {
                try {
                    serve(session, client)
                } catch (e: CancellationException) {
                    throw e
                } catch (_: Exception) {
                    // Malformed requests and disconnected peers are not coroutine failures
                } finally {
                    runCatching { client.close() }
                    synchronized(session) { session.clients.remove(client) }
                }
            }
        }
    }

    // Called under the session lock before allocating a coroutine or request buffer
    private fun admitRequest(
        session: HostSession,
        client: Socket,
        nowNs: Long,
    ): Boolean {
        if (host !== session || !session.scope.isActive) return false
        pruneGrants(session, nowNs)
        val address = client.inetAddress
        if (session.authorizations.values.none { it.address == address }) return false
        val pending = session.clients.values.filter { it.playerId == null }
        if (pending.size >= MAX_PENDING_REQUESTS ||
            pending.count { it.address == address } >= MAX_PENDING_PER_SOURCE
        ) {
            return false
        }
        session.acceptBudgets.entries.removeAll { nowNs - it.value.startedAtNs >= ACCEPT_WINDOW_MS * 1_000_000L }
        val budget =
            session.acceptBudgets[address] ?: run {
                if (session.acceptBudgets.size >= MAX_SOURCE_BUDGETS) return false
                AcceptBudget(nowNs).also { session.acceptBudgets[address] = it }
            }
        if (budget.count >= MAX_ACCEPTS_PER_WINDOW) return false
        budget.count++
        session.clients[client] = HostConnection(address, nowNs)
        return true
    }

    private fun serve(
        session: HostSession,
        client: Socket,
    ) {
        val connection = synchronized(session) { session.clients[client] } ?: return
        val request = parseTransferJson(readRequest(client, connection.acceptedAtNs))
        if (request.keys().asSequence().toSet() != setOf("token", "offset")) return
        val token = request.opt("token") as? String ?: return
        val offset = exactLong(request, "offset")
        val chunkSize = session.identity.chunkSizeBytes
        if (
            offset < 0L ||
            offset > session.identity.sizeBytes ||
            (offset != session.identity.sizeBytes && offset % chunkSize != 0L)
        ) {
            return
        }
        synchronized(session) {
            val now = System.nanoTime()
            pruneGrants(session, now)
            if (host !== session || !session.scope.isActive || client.isClosed ||
                now - connection.acceptedAtNs >= REQUEST_TIMEOUT_MS * 1_000_000L
            ) {
                return
            }
            val authorization = session.authorizations[token] ?: return
            if (authorization.address != connection.address) return
            val active = session.clients.values.filter { it.playerId != null }
            if (active.size >= MAX_CONCURRENT_TRANSFERS || active.any { it.playerId == authorization.playerId }) return
            session.authorizations.remove(token)
            connection.playerId = authorization.playerId
            connection.lastProgressNs = now
        }
        if (!hostArchiveAllowed(session.requirement, session.wrapper)) return
        val header =
            JSONObject()
                .put("revision", session.requirement.revision)
                .put("size_bytes", session.identity.sizeBytes)
                .put("sha256", session.identity.sha256)
                .put("chunk_size", chunkSize)
                .put("chunk_sha256", JSONArray(session.identity.chunkSha256))
        val headerBytes = header.toString().toByteArray(Charsets.UTF_8)
        require(headerBytes.size < MAX_HEADER_BYTES)
        val output = client.getOutputStream()
        output.write(headerBytes)
        output.write('\n'.code)
        synchronized(session) { connection.lastProgressNs = System.nanoTime() }
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
            val bytesToSend = session.identity.sizeBytes - offset
            while (sentBytes < bytesToSend) {
                val count = input.read(buffer, 0, minOf(buffer.size.toLong(), bytesToSend - sentBytes).toInt())
                if (count < 0) error("Mission archive was truncated")
                output.write(buffer, 0, count)
                synchronized(session) { connection.lastProgressNs = System.nanoTime() }
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

    internal fun averageBytesPerSecond(
        completedBytes: Long,
        elapsedNanos: Long,
    ): Long {
        if (completedBytes <= 0L || elapsedNanos <= 0L) return 0L
        return (completedBytes.toDouble() * 1_000_000_000.0 / elapsedNanos.toDouble())
            .toLong()
            .coerceIn(1L, MAX_REPORTED_TRANSFER_BYTES_PER_SECOND)
    }

    private fun receive(
        context: Context,
        hostAddress: String,
        grant: MissionTransferGrant,
        requirement: MissionRequirement,
        attempt: Int,
        socket: Socket,
        onStatus: (MissionStatusReport) -> Unit,
        checkActive: () -> Unit,
    ) {
        receiveArchive(context.filesDir, hostAddress, grant, requirement, attempt, socket, onStatus, checkActive)
        val files = partialFiles(context.filesDir, requirement)
        val verificationStartedAtNs = System.nanoTime()
        onStatus(
            transferReport(
                requirement,
                MissionCompatibilityStatus.VERIFYING,
                0L,
                grant.token,
                attempt,
            ),
        )
        val identity =
            MissionContentIdentity.compute(files.partial) { verified ->
                checkActive()
                val bytesPerSecond = averageBytesPerSecond(verified, System.nanoTime() - verificationStartedAtNs)
                onStatus(
                    transferReport(
                        requirement,
                        MissionCompatibilityStatus.VERIFYING,
                        verified,
                        grant.token,
                        attempt,
                        bytesPerSecond = bytesPerSecond,
                    ),
                )
            }
        require(identity.sizeBytes == requirement.sizeBytes && identity.sha256 == requirement.sha256)
        checkActive()
        try {
            finalizeMission(context, requirement, grant, attempt, files, identity, onStatus, checkActive)
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            throw MissionFinalizationException(e)
        }
    }

    // The actual TCP/resume path, separate from Android import finalization for host-JVM coverage
    internal fun receiveArchive(
        filesDir: File,
        hostAddress: String,
        grant: MissionTransferGrant,
        requirement: MissionRequirement,
        attempt: Int,
        socket: Socket,
        onStatus: (MissionStatusReport) -> Unit,
        checkActive: () -> Unit,
    ) {
        requireTransferRequirement(requirement)
        require(grant.revision == requirement.revision)
        require(grant.port in 1..65535)
        val files = partialFiles(filesDir, requirement)
        var offset = resumeOffset(filesDir, requirement)
        checkActive()
        socket.use {
            socket.connect(InetSocketAddress(hostAddress, grant.port), SOCKET_TIMEOUT_MS)
            socket.soTimeout = SOCKET_TIMEOUT_MS
            val request = JSONObject().put("token", grant.token).put("offset", offset)
            socket.getOutputStream().apply {
                write(request.toString().toByteArray(Charsets.UTF_8))
                write('\n'.code)
                flush()
            }
            val header = parseTransferJson(readLineBounded(socket.getInputStream(), MAX_HEADER_BYTES))
            val metadata = parseMetadata(header, requirement)
            val chunkSize = metadata.chunkSizeBytes
            val chunks = metadata.chunkSha256
            offset = validatePartial(files.partial, offset, metadata.sizeBytes, chunkSize, chunks)
            require(offset == request.getLong("offset"))
            writePartialManifest(files.sidecar, requirement, metadata, offset)
            onStatus(transferReport(requirement, MissionCompatibilityStatus.DOWNLOADING, offset, grant.token, attempt))
            val downloadStartedAtNs = System.nanoTime()
            val downloadStartedBytes = offset
            appendAndVerify(
                socket.getInputStream(),
                files.partial,
                metadata,
                offset,
            ) { verified ->
                checkActive()
                writePartialManifest(files.sidecar, requirement, metadata, verified)
                val bytesPerSecond =
                    averageBytesPerSecond(
                        verified - downloadStartedBytes,
                        System.nanoTime() - downloadStartedAtNs,
                    )
                onStatus(
                    transferReport(
                        requirement,
                        MissionCompatibilityStatus.DOWNLOADING,
                        verified,
                        grant.token,
                        attempt,
                        bytesPerSecond = bytesPerSecond,
                    ),
                )
            }
        }
    }

    private fun finalizeMission(
        context: Context,
        requirement: MissionRequirement,
        grant: MissionTransferGrant,
        attempt: Int,
        files: PartialFiles,
        identity: MissionContentIdentity,
        onStatus: (MissionStatusReport) -> Unit,
        checkActive: () -> Unit,
    ) {
        val wrapperSize = identity.sizeBytes
        val importProgressLimit = wrapperSize * 95L / 100L
        var finalizingBytes = 0L
        var lastFinalizingReportNs = 0L

        fun reportFinalizing(
            progressBytes: Long = finalizingBytes,
            force: Boolean = false,
        ) {
            checkActive()
            finalizingBytes = progressBytes.coerceIn(finalizingBytes, wrapperSize)
            val now = System.nanoTime()
            if (!force && now - lastFinalizingReportNs < STATUS_REPORT_INTERVAL_NS) return
            lastFinalizingReportNs = now
            onStatus(
                transferReport(
                    requirement,
                    MissionCompatibilityStatus.FINALIZING,
                    finalizingBytes,
                    grant.token,
                    attempt,
                ),
            )
        }
        reportFinalizing(force = true)
        val sets = FileSetManager(context.filesDir)
        val setDir = sets.getSetDir(sets.getActive())
        val manager = ModManager(context.filesDir, context, setDir)
        val imported =
            manager.importMissionZipFileMovingSource(
                files.partial,
                uniqueDownloadName(manager, requirement),
                acceptedIdentity = identity,
                onProgress = { progress ->
                    val mappedBytes =
                        if (progress.bytesTotal > 0L) {
                            (importProgressLimit * progress.bytesDone.toDouble() / progress.bytesTotal.toDouble())
                                .toLong()
                                .coerceIn(0L, importProgressLimit)
                        } else {
                            finalizingBytes
                        }
                    reportFinalizing(mappedBytes, force = mappedBytes == importProgressLimit)
                },
            )
        require(imported != null)
        reportFinalizing(wrapperSize, force = true)
        promoteImportedMission(manager, imported.filename)
        files.sidecar.delete()
        val resolved = MissionCompatibilityResolver.resolve(context, requirement, "coop")
        require(resolved.status == MissionCompatibilityStatus.MATCH)
        onStatus(
            transferReport(
                requirement,
                MissionCompatibilityStatus.MATCH,
                wrapperSize,
                grant.token,
                attempt,
            ),
        )
    }

    private fun appendAndVerify(
        input: InputStream,
        partial: File,
        metadata: MissionContentIdentity,
        offset: Long,
        onVerified: (Long) -> Unit,
    ) {
        val expectedSize = metadata.sizeBytes
        val chunkSize = metadata.chunkSizeBytes
        val chunks = metadata.chunkSha256
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
    ): Long = resumeOffset(context.filesDir, requirement)

    internal fun resumeOffset(
        filesDir: File,
        requirement: MissionRequirement,
    ): Long =
        runCatching {
            requireTransferRequirement(requirement)
            val files = partialFiles(filesDir, requirement)
            if (!files.partial.isFile || !files.sidecar.isFile) return@runCatching 0L
            val sidecar =
                files.sidecar.inputStream().use { input ->
                    // A length check alone races replacement/growth and does not bound the read
                    val bytes = ByteArray(MAX_HEADER_BYTES + 1)
                    var used = 0
                    while (used < bytes.size) {
                        val count = input.read(bytes, used, bytes.size - used)
                        if (count < 0) break
                        used += count
                    }
                    require(used <= MAX_HEADER_BYTES)
                    parseTransferJson(String(bytes, 0, used, Charsets.UTF_8))
                }
            require(exactLong(sidecar, "schema") == 1L)
            val metadata = parseMetadata(sidecar, requirement)
            validatePartial(
                files.partial,
                exactLong(sidecar, "verified_bytes"),
                metadata.sizeBytes,
                metadata.chunkSizeBytes,
                metadata.chunkSha256,
            )
        }.getOrDefault(0L)

    private fun requireTransferRequirement(requirement: MissionRequirement) {
        require(requirement.isValid && requirement.isWrapper && requirement.offerAvailable)
        require(requirement.sizeBytes in 1L..MISSION_TRANSFER_MAX_BYTES)
    }

    private fun parseTransferJson(text: String): JSONObject {
        // These flat objects contain only a hash array; bound recursion before org.json parses them
        var depth = 0
        var quoted = false
        var escaped = false
        for (char in text) {
            if (quoted) {
                if (escaped) {
                    escaped = false
                } else if (char == '\\') {
                    escaped = true
                } else if (char == '"') {
                    quoted = false
                }
            } else {
                when (char) {
                    '"' -> {
                        quoted = true
                    }

                    '{', '[' -> {
                        depth++
                        require(depth <= 2) { "Nested transfer metadata" }
                    }

                    '}', ']' -> {
                        depth--
                    }
                }
            }
        }
        // org.json also accepts single quotes/comments, whose quote rules differ from the depth scan
        // Strict JSON rejects such prefixes before it can recurse into any scan-hidden structure
        return JSONObject(Json.parseToJsonElement(text).toString())
    }

    // org.json getters coerce strings/fractions and can truncate overflowing integers
    private fun exactLong(
        json: JSONObject,
        name: String,
    ): Long =
        when (val value = json.get(name)) {
            is Int -> value.toLong()
            is Long -> value
            else -> error("Invalid integer field: $name")
        }

    private fun parseMetadata(
        json: JSONObject,
        requirement: MissionRequirement,
    ): MissionContentIdentity {
        requireTransferRequirement(requirement)
        require(json.get("revision") == requirement.revision)
        val size = exactLong(json, "size_bytes")
        require(size == requirement.sizeBytes)
        val sha256 = json.get("sha256") as? String ?: error("Invalid content hash")
        require(sha256 == requirement.sha256)
        require(exactLong(json, "chunk_size") == MissionContentIdentity.DEFAULT_CHUNK_SIZE_BYTES.toLong())
        val array = json.getJSONArray("chunk_sha256")
        // Bound array traversal/list materialization before inspecting individual hashes
        require(array.length().toLong() == (size - 1L) / MissionContentIdentity.DEFAULT_CHUNK_SIZE_BYTES + 1L)
        val chunks =
            (0 until array.length()).map { index ->
                val hash = array.get(index) as? String ?: error("Invalid chunk hash")
                require(MissionContentIdentity.isValidSha256(hash))
                hash
            }
        return MissionContentIdentity(size, sha256, MissionContentIdentity.DEFAULT_CHUNK_SIZE_BYTES, chunks)
    }

    private fun validChunkMetadata(
        size: Long,
        chunkSize: Int,
        chunks: List<String>,
    ): Boolean =
        size in 1L..MISSION_TRANSFER_MAX_BYTES &&
            chunkSize == MissionContentIdentity.DEFAULT_CHUNK_SIZE_BYTES &&
            chunks.size.toLong() == (size - 1L) / chunkSize + 1L &&
            chunks.all(MissionContentIdentity::isValidSha256)

    internal fun validatePartial(
        partial: File,
        claimedBytes: Long,
        expectedSize: Long,
        chunkSize: Int,
        chunks: List<String>,
    ): Long {
        if (
            !validChunkMetadata(expectedSize, chunkSize, chunks) ||
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
            val buffer = ByteArray(BUFFER_BYTES)
            var index = 0
            while (verified < limit && index < chunks.size) {
                val expectedChunkBytes = minOf(chunkSize.toLong(), expectedSize - verified).toInt()
                if (verified + expectedChunkBytes > limit) break
                var used = 0
                val digest = MessageDigest.getInstance("SHA-256")
                while (used < expectedChunkBytes) {
                    val count = input.read(buffer, 0, minOf(buffer.size, expectedChunkBytes - used))
                    if (count < 0) break
                    digest.update(buffer, 0, count)
                    used += count
                }
                if (used != expectedChunkBytes || digest.digest().toHex() != chunks[index]) {
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
        val partial: File,
        val sidecar: File,
    )

    private fun partialFiles(
        filesDir: File,
        requirement: MissionRequirement,
    ): PartialFiles {
        requireTransferRequirement(requirement)
        val root = File(filesDir, ".mission_transfers")
        val key = requireNotNull(requirement.sha256)
        return PartialFiles(File(root, "$key.partial"), File(root, "$key.json"))
    }

    private fun writePartialManifest(
        file: File,
        requirement: MissionRequirement,
        metadata: MissionContentIdentity,
        verifiedBytes: Long,
    ) {
        val json =
            JSONObject()
                .put("schema", 1)
                .put("revision", requirement.revision)
                .put("size_bytes", metadata.sizeBytes)
                .put("sha256", metadata.sha256)
                .put("chunk_size", metadata.chunkSizeBytes)
                .put("chunk_sha256", JSONArray(metadata.chunkSha256))
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

    internal fun transferReport(
        requirement: MissionRequirement,
        status: MissionCompatibilityStatus,
        verifiedBytes: Long,
        transferId: String,
        attempt: Int,
        failureCode: String? = null,
        bytesPerSecond: Long = 0L,
    ): MissionStatusReport =
        MissionStatusReport(
            revision = requirement.revision,
            status = status,
            verifiedBytes = verifiedBytes,
            totalBytes = requirement.sizeBytes ?: 0L,
            transferId = transferId.take(64),
            attempt = attempt,
            failureCode = failureCode,
            bytesPerSecond = bytesPerSecond,
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

    private fun readRequest(
        socket: Socket,
        acceptedAtNs: Long,
    ): String {
        val input = socket.getInputStream()
        val bytes = ByteArray(MAX_REQUEST_BYTES)
        for (index in bytes.indices) {
            val remainingNs = REQUEST_TIMEOUT_MS * 1_000_000L - (System.nanoTime() - acceptedAtNs)
            check(remainingNs > 0L) { "Transfer request deadline exceeded" }
            socket.soTimeout = ((remainingNs + 999_999L) / 1_000_000L).toInt()
            val value = input.read()
            check(value >= 0) { "Unexpected end of transfer request" }
            if (value == '\n'.code) return String(bytes, 0, index, Charsets.UTF_8)
            check(value >= 0x20) { "Invalid transfer request" }
            bytes[index] = value.toByte()
        }
        error("Transfer request exceeds $MAX_REQUEST_BYTES bytes")
    }

    // Addresses originate in the lobby transport, never resolve a caller's hostname
    private fun numericAddress(address: String): InetAddress? =
        runCatching {
            if (address.contains(':')) {
                val parts = address.split('%')
                require(parts.size <= 2 && parts[0].all { it in "0123456789abcdefABCDEF:." })
                require(
                    parts.size == 1 ||
                        (parts[1].length in 1..64 && parts[1].all { it.isLetterOrDigit() || it in "_.-" }),
                )
                InetAddress.getByName(address)
            } else {
                val parts = address.split('.')
                require(parts.size == 4)
                val bytes =
                    parts
                        .map {
                            require(it.isNotEmpty() && it.length <= 3 && it.all { digit -> digit in '0'..'9' })
                            it.toInt().also { value -> require(value in 0..255) }.toByte()
                        }.toByteArray()
                InetAddress.getByAddress(bytes)
            }
        }.getOrNull()

    private fun ByteArray.toHex(): String = joinToString("") { "%02x".format(it) }
}

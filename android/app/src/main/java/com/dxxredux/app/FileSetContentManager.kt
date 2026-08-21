package com.dxxredux.app

import android.system.Os
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.security.MessageDigest
import java.util.Locale
import java.util.UUID

internal data class FileSetContentReconcileResult(
    val entries: List<FileSetContentEntry>,
    val adoptedIds: List<String>,
    val removedDuplicatePaths: List<String>,
    val conflicts: List<String>,
)

/** Durable ownership, state, and launch projections for one file set. */
internal class FileSetContentManager(
    private val setDir: File,
) {
    private data class OwnedFile(
        val path: String,
        val sizeBytes: Long,
        val sha256: String,
    )

    private data class StoredEntry(
        val id: String,
        val displayName: String,
        val game: String,
        val kind: String,
        val versionName: String?,
        val sourceUri: String?,
        val problem: String?,
        val files: List<OwnedFile>,
    )

    private data class EntryState(
        val enabled: Boolean,
        val order: Int,
    )

    private val contentDir = File(setDir, ".content")
    private val entriesDir = File(contentDir, "entries")
    private val stagingDir = File(contentDir, ".staging")
    private val trashDir = File(contentDir, ".trash")
    private val stateFile = File(setDir, "content_state.json")
    private val projectionDir = File(setDir, ".content_projection")

    fun reconcile(): FileSetContentReconcileResult =
        synchronized(CONTENT_LOCK) {
            if (!setDir.isDirectory) {
                return@synchronized FileSetContentReconcileResult(emptyList(), emptyList(), emptyList(), emptyList())
            }
            entriesDir.mkdirs()
            cleanupTransactionDirectory(stagingDir)
            cleanupTransactionDirectory(trashDir)

            val conflicts = mutableListOf<String>()
            val stored = readStoredEntries(conflicts).toMutableMap()
            val removedDuplicates = removePublishedSourceDuplicates(stored.values, conflicts).toMutableList()
            val discovered = FileSetContentCatalog.scan(setDir)
            val adopted = mutableListOf<String>()
            for (entry in discovered) {
                val existing = stored[entry.id]
                if (existing != null) {
                    conflicts += "${entry.displayName}: content paths collide with managed entry ${entry.id}"
                    continue
                }
                val published = publishEntry(entry)
                stored[published.id] = published
                adopted += published.id
                removedDuplicates += removeAdoptedSources(entry, published, conflicts)
            }

            val state = repairState(stored.keys)
            if (adopted.isNotEmpty()) {
                removedDuplicates += removePublishedSourceDuplicates(stored.values, conflicts)
            }
            val entries = materialize(stored.values, state)
            FileSetContentReconcileResult(entries, adopted, removedDuplicates, conflicts.distinct())
        }

    fun listEntries(): List<FileSetContentEntry> =
        synchronized(CONTENT_LOCK) {
            val conflicts = mutableListOf<String>()
            val stored = readStoredEntries(conflicts)
            materialize(stored.values, repairState(stored.keys))
        }

    fun setEnabled(
        id: String,
        enabled: Boolean,
    ) = synchronized(CONTENT_LOCK) {
        val stored = readStoredEntries(mutableListOf())
        require(id in stored) { "Unknown content entry $id" }
        val state = repairState(stored.keys).toMutableMap()
        val previous = state.getValue(id)
        state[id] = previous.copy(enabled = enabled)
        writeState(state)
    }

    fun move(
        id: String,
        newIndex: Int,
    ) = synchronized(CONTENT_LOCK) {
        val stored = readStoredEntries(mutableListOf())
        require(id in stored) { "Unknown content entry $id" }
        val state = repairState(stored.keys)
        val ids =
            state.entries
                .sortedBy { it.value.order }
                .map { it.key }
                .toMutableList()
        ids.remove(id)
        ids.add(newIndex.coerceIn(0, ids.size), id)
        writeState(ids.mapIndexed { index, entryId -> entryId to state.getValue(entryId).copy(order = index) }.toMap())
    }

    fun deleteEntry(id: String): Boolean =
        synchronized(CONTENT_LOCK) {
            val entryDir = entryDirectory(id)
            if (!entryDir.isDirectory) return@synchronized false
            trashDir.mkdirs()
            val trash = File(trashDir, "$id-${UUID.randomUUID()}")
            check(entryDir.renameTo(trash)) { "Could not retire content entry $id" }
            val stored = readStoredEntries(mutableListOf())
            val state = repairState(stored.keys)
            writeState(state)
            trash.deleteRecursively()
            for (game in listOf(GameFileFormats.GAME_D1, GameFileFormats.GAME_D2)) buildProjection(game)
            NativeTextureLookupCache.clear()
            true
        }

    fun buildProjection(
        game: String,
        includeD1ForD2: Boolean = true,
    ): File =
        synchronized(CONTENT_LOCK) {
            require(game == GameFileFormats.GAME_D1 || game == GameFileFormats.GAME_D2)
            val entries =
                listEntries().filter { entry -> entry.enabled && entry.appliesToGame(game, includeD1ForD2) }
            projectionDir.mkdirs()
            val target = File(projectionDir, game)
            val temporary = AtomicFilePublication.uniqueSibling(target, "tmp")
            check(temporary.mkdirs()) { "Could not create content projection" }
            try {
                for (entry in entries.sortedBy { it.order }) {
                    for ((index, source) in entry.files.withIndex()) {
                        val relative = entry.virtualPaths[index]
                        val destination = containedFile(temporary, relative)
                        if (destination.exists()) continue
                        destination.parentFile?.mkdirs()
                        linkOrCopy(source, destination)
                        check(
                            source.length() == destination.length(),
                        ) { "Projected content size mismatch for $relative" }
                    }
                }
                AtomicFilePublication.publishDirectory(temporary, target)
            } finally {
                if (temporary.exists()) temporary.deleteRecursively()
            }
            NativeTextureLookupCache.clear()
            target
        }

    fun projectionHasFiles(game: String): Boolean =
        synchronized(CONTENT_LOCK) {
            File(projectionDir, game).walkTopDown().any { it.isFile }
        }

    fun resolveFile(
        path: String,
        enabledOnly: Boolean = true,
    ): File? =
        synchronized(CONTENT_LOCK) {
            val identity = normalizePath(path).lowercase(Locale.US)
            val leaf = portableGameFilenameIdentity(GameFileFormats.leafName(path))
            val candidates =
                listEntries().filter { !enabledOnly || it.enabled }.flatMap { entry ->
                    entry.virtualPaths.mapIndexedNotNull { index, virtualPath ->
                        val virtualIdentity = normalizePath(virtualPath).lowercase(Locale.US)
                        when {
                            virtualIdentity == identity -> 0 to entry.files[index]
                            portableGameFilenameIdentity(File(virtualPath).name) == leaf -> 1 to entry.files[index]
                            else -> null
                        }
                    }
                }
            val bestRank = candidates.minOfOrNull { it.first } ?: return@synchronized null
            candidates
                .filter { it.first == bestRank }
                .map { it.second }
                .distinctBy { it.absolutePath }
                .singleOrNull()
        }

    fun buildLaunchPaths(
        game: String,
        includeD1ForD2: Boolean = true,
    ): List<String> =
        synchronized(CONTENT_LOCK) {
            val entries =
                listEntries().filter { entry -> entry.enabled && entry.appliesToGame(game, includeD1ForD2) }
            val projection = buildProjection(game, includeD1ForD2)
            buildList {
                entries.sortedBy { it.order }.forEach { entry ->
                    entry.files.filter { GameFileFormats.isDxa(it.name) }.forEach { add(it.absolutePath) }
                }
                if (projection.walkTopDown().any { it.isFile }) add(projection.absolutePath)
            }
        }

    private fun FileSetContentEntry.appliesToGame(
        game: String,
        includeD1ForD2: Boolean,
    ): Boolean =
        this.game == game || this.game == GameFileFormats.GAME_BOTH ||
            (includeD1ForD2 && game == GameFileFormats.GAME_D2 && this.game == GameFileFormats.GAME_D1)

    private fun publishEntry(entry: FileSetContentEntry): StoredEntry {
        val target = entryDirectory(entry.id)
        if (target.exists()) error("Content entry ${entry.id} already exists")
        stagingDir.mkdirs()
        val temporary = File(stagingDir, "${entry.id}-${UUID.randomUUID()}")
        val payload = File(temporary, "payload")
        check(payload.mkdirs()) { "Could not stage content entry ${entry.id}" }
        val ownedFiles = mutableListOf<OwnedFile>()
        try {
            for ((index, source) in entry.files.withIndex()) {
                val relative = entry.virtualPaths.getOrElse(index) { source.name }
                val destination = containedFile(payload, relative)
                destination.parentFile?.mkdirs()
                linkOrCopy(source, destination)
                val sha256 = sha256(destination)
                check(source.length() == destination.length() && sha256 == sha256(source)) {
                    "Could not verify staged content $relative"
                }
                ownedFiles += OwnedFile(normalizePath(relative), destination.length(), sha256)
            }
            val stored =
                StoredEntry(
                    entry.id,
                    entry.displayName,
                    entry.game,
                    entry.kind,
                    entry.versionName,
                    entry.sourceUri,
                    entry.problem,
                    ownedFiles,
                )
            AtomicFilePublication.writeUtf8(File(temporary, ENTRY_FILE), entryJson(stored).toString(2) + "\n")
            AtomicFilePublication.publishDirectory(temporary, target)
            return stored
        } finally {
            if (temporary.exists()) temporary.deleteRecursively()
        }
    }

    private fun readStoredEntries(conflicts: MutableList<String>): Map<String, StoredEntry> {
        val result = linkedMapOf<String, StoredEntry>()
        val directories =
            entriesDir
                .listFiles()
                ?.filter { it.isDirectory }
                ?.sortedBy { it.name }
                .orEmpty()
        for (directory in directories) {
            val stored =
                runCatching { parseEntry(directory) }.getOrElse { failure ->
                    recoverStoredEntry(directory, failure, conflicts)
                } ?: continue
            result[stored.id] = stored
        }
        return result
    }

    private fun recoverStoredEntry(
        originalDirectory: File,
        failure: Throwable,
        conflicts: MutableList<String>,
    ): StoredEntry? {
        val payload = File(originalDirectory, "payload")
        val files =
            payload
                .takeIf(File::isDirectory)
                ?.walkTopDown()
                ?.filter(File::isFile)
                ?.sortedBy { it.relativeTo(payload).invariantSeparatorsPath.lowercase(Locale.US) }
                ?.toList()
                .orEmpty()
        val reason = failure.message ?: "invalid content manifest"
        if (files.isEmpty()) {
            conflicts += "${originalDirectory.name}: $reason; no payload files were found"
            return null
        }
        var directory = originalDirectory
        var id = directory.name
        if (!ID_PATTERN.matches(id)) {
            val paths = files.map { it.relativeTo(payload).invariantSeparatorsPath }
            var salt = 0
            var recoveredDirectory: File
            do {
                id = recoveryId(directory.name, paths, salt++)
                recoveredDirectory = entryDirectory(id)
            } while (recoveredDirectory.exists())
            check(directory.renameTo(recoveredDirectory)) {
                "Could not recover content owner ${directory.name}"
            }
            directory = recoveredDirectory
        }
        val recoveredPayload = File(directory, "payload")
        val ownedFiles =
            recoveredPayload
                .walkTopDown()
                .filter(File::isFile)
                .map { file ->
                    val path = normalizePath(file.relativeTo(recoveredPayload).invariantSeparatorsPath)
                    OwnedFile(path, file.length(), sha256(file))
                }.sortedBy { it.path.lowercase(Locale.US) }
                .toList()
        val stored =
            StoredEntry(
                id = id,
                displayName = "Recovered content ${id.take(8)}",
                game = GameFileFormats.GAME_BOTH,
                kind = FileSetContentCatalog.KIND_OTHER,
                versionName = null,
                sourceUri = null,
                problem = "Recovered after invalid content manifest: $reason",
                files = ownedFiles,
            )
        AtomicFilePublication.writeUtf8(File(directory, ENTRY_FILE), entryJson(stored).toString(2) + "\n")
        conflicts += "${stored.displayName}: ${stored.problem}"
        return stored
    }

    private fun parseEntry(directory: File): StoredEntry {
        val json = JSONObject(File(directory, ENTRY_FILE).readText())
        val id = json.getString("id")
        require(id == directory.name && ID_PATTERN.matches(id)) { "Content ID does not match its directory" }
        val filesJson = json.getJSONArray("files")
        val files =
            (0 until filesJson.length()).map { index ->
                val value = filesJson.getJSONObject(index)
                val owned =
                    OwnedFile(
                        normalizePath(value.getString("path")),
                        value.getLong("size_bytes"),
                        value.getString("sha256").lowercase(Locale.US),
                    )
                val payload = containedFile(File(directory, "payload"), owned.path)
                require(
                    payload.isFile && payload.length() == owned.sizeBytes,
                ) { "Owned file is missing: ${owned.path}" }
                owned
            }
        require(files.isNotEmpty() && files.map { it.path }.distinct().size == files.size) {
            "Content entry has no files or duplicate paths"
        }
        return StoredEntry(
            id,
            json.getString("display_name"),
            json.optString("game", GameFileFormats.GAME_BOTH),
            json.optString("kind", FileSetContentCatalog.KIND_OTHER),
            json.optString("version_name").takeIf { it.isNotBlank() },
            json.optString("source_uri").takeIf { it.isNotBlank() },
            json.optString("problem").takeIf { it.isNotBlank() },
            files,
        )
    }

    private fun materialize(
        stored: Collection<StoredEntry>,
        state: Map<String, EntryState>,
    ): List<FileSetContentEntry> =
        stored
            .map { entry ->
                val entryState = state.getValue(entry.id)
                val payloadRoot = File(entryDirectory(entry.id), "payload")
                FileSetContentEntry(
                    id = entry.id,
                    displayName = entry.displayName,
                    game = entry.game,
                    kind = entry.kind,
                    files = entry.files.map { containedFile(payloadRoot, it.path) },
                    versionName = entry.versionName,
                    sourceUri = entry.sourceUri,
                    problem = entry.problem,
                    enabled = entryState.enabled,
                    order = entryState.order,
                    virtualPaths = entry.files.map { it.path },
                )
            }.sortedBy { it.order }

    private fun repairState(ids: Set<String>): Map<String, EntryState> {
        val loaded = readState()
        val orderedIds =
            loaded.entries
                .filter { it.key in ids }
                .sortedBy { it.value.order }
                .map { it.key }
                .toMutableList()
        orderedIds += ids.filter { it !in loaded }.sorted()
        val repaired =
            orderedIds
                .mapIndexed { index, id ->
                    id to EntryState(loaded[id]?.enabled ?: true, index)
                }.toMap()
        if (repaired != loaded) writeState(repaired)
        return repaired
    }

    private fun readState(): Map<String, EntryState> =
        runCatching {
            if (!stateFile.isFile) return@runCatching emptyMap()
            val array = JSONObject(stateFile.readText()).getJSONArray("entries")
            (0 until array.length()).associate { index ->
                val value = array.getJSONObject(index)
                value.getString("id") to EntryState(value.optBoolean("enabled", true), value.optInt("order", index))
            }
        }.getOrDefault(emptyMap())

    private fun writeState(state: Map<String, EntryState>) {
        val array = JSONArray()
        state.entries.sortedBy { it.value.order }.forEach { (id, value) ->
            array.put(JSONObject().put("id", id).put("enabled", value.enabled).put("order", value.order))
        }
        AtomicFilePublication.writeUtf8(stateFile, JSONObject().put("entries", array).toString(2) + "\n")
    }

    private fun removePublishedSourceDuplicates(
        stored: Collection<StoredEntry>,
        conflicts: MutableList<String>,
    ): List<String> {
        val removed = mutableListOf<String>()
        for (entry in stored) {
            val payloadRoot = File(entryDirectory(entry.id), "payload")
            for (owned in entry.files) {
                val source = containedFile(setDir, owned.path)
                if (!source.isFile) continue
                val payload = containedFile(payloadRoot, owned.path)
                if (source.length() == owned.sizeBytes && payload.isFile && sha256(source) == owned.sha256) {
                    check(source.delete()) { "Could not remove published source ${owned.path}" }
                    if (source.parentFile?.canonicalFile == setDir.canonicalFile) {
                        AssetManifest(setDir).remove(source.name)
                    }
                    removeEmptyParents(source.parentFile)
                    removed += owned.path
                } else {
                    conflicts += "${entry.displayName}: ${owned.path} differs from its managed payload"
                }
            }
        }
        return removed
    }

    private fun removeAdoptedSources(
        discovered: FileSetContentEntry,
        published: StoredEntry,
        conflicts: MutableList<String>,
    ): List<String> {
        val removed = mutableListOf<String>()
        val payloadRoot = File(entryDirectory(published.id), "payload")
        for ((index, source) in discovered.files.withIndex()) {
            val owned = published.files[index]
            val payload = containedFile(payloadRoot, owned.path)
            val sourcePath = normalizePath(source.relativeTo(setDir).invariantSeparatorsPath)
            if (source.isFile && source.length() == owned.sizeBytes && payload.isFile &&
                sha256(source) == owned.sha256
            ) {
                check(source.delete()) { "Could not remove adopted source $sourcePath" }
                if (source.parentFile?.canonicalFile == setDir.canonicalFile) {
                    AssetManifest(setDir).remove(source.name)
                }
                removeEmptyParents(source.parentFile)
                removed += sourcePath
            } else {
                conflicts += "${discovered.displayName}: adopted source $sourcePath differs from its managed payload"
            }
        }
        return removed
    }

    private fun removeEmptyParents(start: File?) {
        val root = setDir.canonicalFile
        var current = start?.canonicalFile
        while (current != null && current != root && current.toPath().startsWith(root.toPath())) {
            if (current.listFiles()?.isNotEmpty() != false || !current.delete()) break
            current = current.parentFile?.canonicalFile
        }
    }

    private fun cleanupTransactionDirectory(directory: File) {
        directory.listFiles()?.forEach { it.deleteRecursively() }
    }

    private fun entryJson(entry: StoredEntry): JSONObject =
        JSONObject()
            .put("id", entry.id)
            .put("display_name", entry.displayName)
            .put("game", entry.game)
            .put("kind", entry.kind)
            .put("version_name", entry.versionName)
            .put("source_uri", entry.sourceUri)
            .put("problem", entry.problem)
            .put(
                "files",
                JSONArray(
                    entry.files.map { file ->
                        JSONObject()
                            .put("path", file.path)
                            .put("size_bytes", file.sizeBytes)
                            .put("sha256", file.sha256)
                    },
                ),
            )

    private fun linkOrCopy(
        source: File,
        destination: File,
    ) {
        try {
            Os.link(source.absolutePath, destination.absolutePath)
            return
        } catch (_: Throwable) {
            // JVM tests use Android stubs and some imported volumes cannot hard-link.
        }
        FileInputStream(source).use { input ->
            FileOutputStream(destination).use { output ->
                input.copyTo(output)
                output.fd.sync()
            }
        }
    }

    private fun containedFile(
        root: File,
        relativePath: String,
    ): File {
        val canonicalRoot = root.canonicalFile
        val target = File(canonicalRoot, normalizePath(relativePath)).canonicalFile
        require(target.toPath().startsWith(canonicalRoot.toPath()) && target != canonicalRoot) {
            "Content path escapes its owner: $relativePath"
        }
        return target
    }

    private fun entryDirectory(id: String): File {
        require(ID_PATTERN.matches(id)) { "Invalid content ID" }
        return File(entriesDir, id)
    }

    private fun sha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        FileInputStream(file).buffered().use { input ->
            val buffer = ByteArray(64 * 1024)
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                if (count > 0) digest.update(buffer, 0, count)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it.toInt() and 0xff) }
    }

    private fun recoveryId(
        directoryName: String,
        paths: List<String>,
        salt: Int,
    ): String {
        val digest = MessageDigest.getInstance("SHA-256")
        digest.update("recovered\n$salt\n$directoryName\n${paths.joinToString("\n")}".toByteArray())
        return digest.digest().take(12).joinToString("") { "%02x".format(it.toInt() and 0xff) }
    }

    private fun normalizePath(path: String): String = path.replace('\\', '/').trim('/')

    companion object {
        private val CONTENT_LOCK = Any()
        private const val ENTRY_FILE = "entry.json"
        private val ID_PATTERN = Regex("^[0-9a-f]{24}$")
    }
}

package com.dxxredux.app

import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.security.MessageDigest
import java.util.Locale

class MissionZipMusicStageManager private constructor(
    cacheDir: File,
    private val beforePublish: (File, File) -> Unit,
) {
    constructor(cacheDir: File) : this(cacheDir, { _, _ -> })

    internal constructor(
        cacheDir: File,
        beforePublish: (File, File) -> Unit,
        testOnly: Unit = Unit,
    ) : this(cacheDir, beforePublish)

    private val root = File(cacheDir, "mission_zip_music")

    fun stageCompressedAudioTrack(
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
    ): File? =
        AtomicFilePublication.transaction {
            if (!track.playable || track.kind != MissionZipMusic.KIND_COMPRESSED_AUDIO) return@transaction null
            if (track.sizeBytes > ExtractionLimits.MAX_ENTRY_BYTES) return@transaction null
            val archive = track.sourceFilePath?.let(::File) ?: File(catalog.archivePath)
            if (!archive.isFile && !archive.isDirectory) return@transaction null
            val cacheIdentity = stagedIdentity(catalog, track)
            val output = stagedFile(cacheIdentity, track)
            if (isCompleteGeneration(output, cacheIdentity, track.sizeBytes)) return@transaction output
            val targetDirectory = checkNotNull(output.parentFile)
            root.mkdirs()
            cleanupOldFiles(requiredBytes = track.sizeBytes.coerceAtLeast(0L))
            ImportStorageGuard.requireFreeSpace(
                root,
                track.sizeBytes.takeIf { it > 0L } ?: archive.length(),
                "stage ${track.displayName}",
            )
            val temporaryDirectory = AtomicFilePublication.uniqueSibling(targetDirectory, "tmp")
            check(temporaryDirectory.mkdir()) { "Could not create temporary music cache generation" }
            val temporaryOutput = File(temporaryDirectory, output.name)
            val budget = ExtractionBudget()
            try {
                val ok =
                    runCatching {
                        if (track.sourceFilePath != null && archive.isFile) {
                            extractFromSourceFile(archive, track, temporaryOutput, budget)
                        } else if (archive.isDirectory) {
                            extractFromDirectory(archive, track, temporaryOutput, budget)
                        } else {
                            ArchiveFiles.open(archive).use { archiveFile ->
                                when {
                                    track.hogEntryName != null && track.nestedEntryPath != null -> {
                                        archiveFile.openEntryStream(track.archiveEntryPath)?.use { dxa ->
                                            extractFromDxaHog(
                                                dxa,
                                                track.nestedEntryPath,
                                                track.hogEntryName,
                                                temporaryOutput,
                                                budget,
                                            )
                                        } ?: false
                                    }

                                    track.hogEntryName != null -> {
                                        archiveFile.openEntryStream(track.archiveEntryPath)?.use { hog ->
                                            extractFromHog(hog, track.hogEntryName, temporaryOutput, budget)
                                        } ?: false
                                    }

                                    track.nestedEntryPath != null -> {
                                        archiveFile.openEntryStream(track.archiveEntryPath)?.use { dxa ->
                                            extractFromDxa(dxa, track.nestedEntryPath, temporaryOutput, budget)
                                        } ?: false
                                    }

                                    else -> {
                                        archiveFile.openEntryStream(track.archiveEntryPath)?.use { input ->
                                            copyTrack(
                                                input,
                                                temporaryOutput,
                                                track.sizeBytes,
                                                track.archiveEntryPath,
                                                budget,
                                            )
                                            true
                                        } ?: false
                                    }
                                }
                            }
                        }
                    }.getOrDefault(false)
                if (!ok || !validSize(temporaryOutput, track.sizeBytes)) return@transaction null
                FileOutputStream(temporaryOutput, true).use { it.fd.sync() }
                val marker = identityFile(temporaryOutput)
                FileOutputStream(marker).use { stream ->
                    stream.write("$cacheIdentity\n${temporaryOutput.length()}".toByteArray(Charsets.US_ASCII))
                    stream.fd.sync()
                }
                AtomicFilePublication.publishDirectory(temporaryDirectory, targetDirectory, beforePublish)
                stagedFile(cacheIdentity, track).takeIf {
                    isCompleteGeneration(it, cacheIdentity, track.sizeBytes)
                }
            } finally {
                temporaryDirectory.deleteRecursively()
            }
        }

    fun readMidiTrackBytes(
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
    ): ByteArray? {
        if (!track.playable || track.kind != MissionZipMusic.KIND_MIDI) return null
        if (track.sizeBytes < 0 || track.sizeBytes > MAX_MIDI_BYTES) return null
        val archive = track.sourceFilePath?.let(::File) ?: File(catalog.archivePath)
        if (!archive.isFile && !archive.isDirectory) return null
        return runCatching {
            val budget = ExtractionBudget(maxEntryBytes = MAX_MIDI_BYTES, maxTotalBytes = MAX_MIDI_BYTES)
            if (track.sourceFilePath != null && archive.isFile) {
                readFromSourceFile(archive, track, budget)
            } else if (archive.isDirectory) {
                readFromDirectory(archive, track, budget)
            } else {
                ArchiveFiles.open(archive).use { archiveFile ->
                    when {
                        track.hogEntryName != null && track.nestedEntryPath != null -> {
                            archiveFile.openEntryStream(track.archiveEntryPath)?.use { dxa ->
                                readFromDxaHog(dxa, track.nestedEntryPath, track.hogEntryName, budget)
                            }
                        }

                        track.hogEntryName != null -> {
                            archiveFile.openEntryStream(track.archiveEntryPath)?.use { hog ->
                                readFromHog(hog, track.hogEntryName, budget)
                            }
                        }

                        track.nestedEntryPath != null -> {
                            archiveFile.openEntryStream(track.archiveEntryPath)?.use { dxa ->
                                readFromDxa(dxa, track.nestedEntryPath, budget)
                            }
                        }

                        else -> {
                            archiveFile.openEntryStream(track.archiveEntryPath)?.use {
                                readTrack(it, track.sizeBytes, track.archiveEntryPath, budget)
                            }
                        }
                    }
                }
            }
        }.getOrNull()
    }

    fun cleanupOldFiles(
        maxAgeMs: Long = 24L * 60L * 60L * 1000L,
        maxBytes: Long = MAX_CACHE_BYTES,
        requiredBytes: Long = 0L,
    ) {
        require(maxAgeMs >= 0L && maxBytes >= 0L && requiredBytes >= 0L)
        val cutoff = System.currentTimeMillis() - maxAgeMs
        val children = root.listFiles().orEmpty()
        children.forEach { child ->
            if (child.lastModified() < cutoff) child.deleteRecursively()
        }
        val retained = root.listFiles().orEmpty().sortedWith(compareBy<File> { it.lastModified() }.thenBy { it.name })
        var total = retained.sumOf(::fileTreeBytes)
        val target = (maxBytes - requiredBytes.coerceAtMost(maxBytes)).coerceAtLeast(0L)
        for (child in retained) {
            if (total <= target) break
            val bytes = fileTreeBytes(child)
            if (child.deleteRecursively()) total = (total - bytes).coerceAtLeast(0L)
        }
    }

    private fun fileTreeBytes(file: File): Long =
        if (file.isFile) file.length() else file.listFiles().orEmpty().sumOf(::fileTreeBytes)

    private fun stagedFile(
        identity: String,
        track: MissionZipMusicTrack,
    ): File {
        val safeName = track.displayName.replace(Regex("[^a-zA-Z0-9._-]"), "_").ifBlank { "track.${track.extension}" }
        return File(File(root, identity), safeName)
    }

    private fun stagedIdentity(
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
    ): String =
        sha256(
            listOf(
                "dxx-mission-music-stage-v1",
                catalog.sourceIdentity,
                track.id,
                track.archiveEntryPath,
                track.nestedEntryPath.orEmpty(),
                track.hogEntryName.orEmpty(),
            ).joinToString("\u0000"),
        )

    private fun identityFile(output: File): File = File(output.parentFile, "${output.name}.identity")

    private fun isCompleteGeneration(
        output: File,
        identity: String,
        declaredSize: Long,
    ): Boolean =
        runCatching {
            if (!validSize(output, declaredSize)) return@runCatching false
            val marker = identityFile(output).readLines(Charsets.US_ASCII)
            marker.size == 2 && marker[0] == identity && marker[1].toLongOrNull() == output.length()
        }.getOrDefault(false)

    private fun validSize(
        output: File,
        declaredSize: Long,
    ): Boolean =
        output.isFile &&
            if (declaredSize > 0L) output.length() == declaredSize else output.length() > 0L

    private fun sourceFile(
        rootDir: File,
        relativePath: String,
    ): File? {
        val rootCanonical = rootDir.canonicalFile
        val file = File(rootCanonical, normalizePath(relativePath).replace('/', File.separatorChar)).canonicalFile
        if (!file.path.startsWith(rootCanonical.path + File.separator)) return null
        return file.takeIf { it.isFile }
    }

    private fun extractFromDirectory(
        rootDir: File,
        track: MissionZipMusicTrack,
        output: File,
        budget: ExtractionBudget,
    ): Boolean {
        val file = sourceFile(rootDir, track.archiveEntryPath) ?: return false
        return when {
            track.hogEntryName != null && track.nestedEntryPath != null -> {
                file.inputStream().use {
                    extractFromDxaHog(
                        it,
                        track.nestedEntryPath,
                        track.hogEntryName,
                        output,
                        budget,
                    )
                }
            }

            track.hogEntryName != null -> {
                file.inputStream().use { extractFromHog(it, track.hogEntryName, output, budget) }
            }

            track.nestedEntryPath != null -> {
                file.inputStream().use { extractFromDxa(it, track.nestedEntryPath, output, budget) }
            }

            else -> {
                file.inputStream().use { input -> copyTrack(input, output, file.length(), file.name, budget) }
                true
            }
        }
    }

    private fun readFromDirectory(
        rootDir: File,
        track: MissionZipMusicTrack,
        budget: ExtractionBudget,
    ): ByteArray? {
        val file = sourceFile(rootDir, track.archiveEntryPath) ?: return null
        return when {
            track.hogEntryName != null && track.nestedEntryPath != null -> {
                file.inputStream().use { readFromDxaHog(it, track.nestedEntryPath, track.hogEntryName, budget) }
            }

            track.hogEntryName != null -> {
                file.inputStream().use { readFromHog(it, track.hogEntryName, budget) }
            }

            track.nestedEntryPath != null -> {
                file.inputStream().use { readFromDxa(it, track.nestedEntryPath, budget) }
            }

            else -> {
                file.inputStream().use { readTrack(it, file.length(), file.name, budget) }
            }
        }
    }

    private fun extractFromSourceFile(
        file: File,
        track: MissionZipMusicTrack,
        output: File,
        budget: ExtractionBudget,
    ): Boolean =
        when {
            track.hogEntryName != null && track.nestedEntryPath != null -> {
                file.inputStream().use {
                    extractFromDxaHog(
                        it,
                        track.nestedEntryPath,
                        track.hogEntryName,
                        output,
                        budget,
                    )
                }
            }

            track.hogEntryName != null -> {
                file.inputStream().use { extractFromHog(it, track.hogEntryName, output, budget) }
            }

            track.nestedEntryPath != null -> {
                file.inputStream().use { extractFromDxa(it, track.nestedEntryPath, output, budget) }
            }

            else -> {
                file.inputStream().use { input -> copyTrack(input, output, file.length(), file.name, budget) }
                true
            }
        }

    private fun readFromSourceFile(
        file: File,
        track: MissionZipMusicTrack,
        budget: ExtractionBudget,
    ): ByteArray? =
        when {
            track.hogEntryName != null && track.nestedEntryPath != null -> {
                file.inputStream().use { readFromDxaHog(it, track.nestedEntryPath, track.hogEntryName, budget) }
            }

            track.hogEntryName != null -> {
                file.inputStream().use { readFromHog(it, track.hogEntryName, budget) }
            }

            track.nestedEntryPath != null -> {
                file.inputStream().use { readFromDxa(it, track.nestedEntryPath, budget) }
            }

            else -> {
                file.inputStream().use { readTrack(it, file.length(), file.name, budget) }
            }
        }

    private fun extractFromDxa(
        dxa: InputStream,
        nestedEntryPath: String,
        output: File,
        budget: ExtractionBudget,
    ): Boolean {
        openZipInputStreamSkippingPreamble(dxa).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory && normalizePath(entry.name).equals(nestedEntryPath, ignoreCase = true)) {
                    val expanded =
                        FileOutputStream(output).use {
                            zip.copyToBounded(it, budget, entry.compressedSize, entry.name)
                        }
                    zip.closeEntry()
                    if (entry.size < 0 || entry.size != expanded) return false
                    budget.validateExpansion(expanded, entry.compressedSize, entry.name)
                    return true
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return false
    }

    private fun readFromDxa(
        dxa: InputStream,
        nestedEntryPath: String,
        budget: ExtractionBudget,
    ): ByteArray? {
        openZipInputStreamSkippingPreamble(dxa).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory && normalizePath(entry.name).equals(nestedEntryPath, ignoreCase = true)) {
                    val result =
                        zip.readBytesBounded(
                            ExtractionLimits.MAX_ENTRY_BYTES,
                            entry.name,
                            budget,
                            entry.compressedSize,
                            expectedSizeBytes = entry.size,
                        )
                    zip.closeEntry()
                    if (entry.size < 0 || entry.size != result.size.toLong()) return null
                    budget.validateExpansion(result.size.toLong(), entry.compressedSize, entry.name)
                    return result
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return null
    }

    private fun readFromDxaHog(
        dxa: InputStream,
        nestedEntryPath: String,
        hogEntryName: String,
        budget: ExtractionBudget,
    ): ByteArray? {
        openZipInputStreamSkippingPreamble(dxa).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory && normalizePath(entry.name).equals(nestedEntryPath, ignoreCase = true)) {
                    val expanded = BudgetedEntryInputStream(zip, budget, entry.name, entry.compressedSize)
                    val result = readFromHog(expanded, hogEntryName, budget)
                    expanded.drain()
                    zip.closeEntry()
                    budget.validateExpansion(expanded.entryBytes, entry.compressedSize, entry.name)
                    return result
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return null
    }

    private fun readFromHog(
        hog: InputStream,
        hogEntryName: String,
        budget: ExtractionBudget,
    ): ByteArray? {
        if (hog.readNBytesCompat(3).toString(Charsets.US_ASCII) != "DHF") return null
        while (true) {
            val nameBytes = hog.readNBytesCompat(13)
            if (nameBytes.isEmpty() || nameBytes.size != 13) return null
            val lenBytes = hog.readNBytesCompat(4)
            if (lenBytes.size != 4) return null
            val name = hogEntryName(nameBytes)
            val size = leInt(lenBytes).toLong() and 0xffff_ffffL
            budget.registerEntry(size, size, name)
            if (name.equals(hogEntryName, ignoreCase = true)) {
                if (size > ExtractionLimits.MAX_ENTRY_BYTES || size > Int.MAX_VALUE) return null
                val bytes = hog.readNBytesCompat(size.toInt())
                if (bytes.size.toLong() != size) return null
                return bytes
            }
            hog.skipFullyBounded(size, budget, name)
        }
    }

    private fun extractFromDxaHog(
        dxa: InputStream,
        nestedEntryPath: String,
        hogEntryName: String,
        output: File,
        budget: ExtractionBudget,
    ): Boolean {
        openZipInputStreamSkippingPreamble(dxa).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory && normalizePath(entry.name).equals(nestedEntryPath, ignoreCase = true)) {
                    val expanded = BudgetedEntryInputStream(zip, budget, entry.name, entry.compressedSize)
                    val result = extractFromHog(expanded, hogEntryName, output, budget)
                    expanded.drain()
                    zip.closeEntry()
                    budget.validateExpansion(expanded.entryBytes, entry.compressedSize, entry.name)
                    return result
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return false
    }

    private fun extractFromHog(
        hog: InputStream,
        hogEntryName: String,
        output: File,
        budget: ExtractionBudget,
    ): Boolean {
        if (hog.readNBytesCompat(3).toString(Charsets.US_ASCII) != "DHF") return false
        while (true) {
            val nameBytes = hog.readNBytesCompat(13)
            if (nameBytes.isEmpty() || nameBytes.size != 13) return false
            val lenBytes = hog.readNBytesCompat(4)
            if (lenBytes.size != 4) return false
            val name = hogEntryName(nameBytes)
            val size = leInt(lenBytes).toLong() and 0xffff_ffffL
            budget.registerEntry(size, size, name)
            if (name.equals(hogEntryName, ignoreCase = true)) {
                FileOutputStream(output).use { out -> hog.copyLimitedTo(out, size, budget, name) }
                return true
            }
            hog.skipFullyBounded(size, budget, name)
        }
    }

    private fun ReadableArchiveFile.openEntryStream(path: String): InputStream? {
        val normalized = normalizePath(path)
        val entry =
            findEntry(normalized) ?: entries.firstOrNull {
                !it.isDirectory && normalizePath(it.path).equals(normalized, ignoreCase = true)
            }
        return entry?.let { openInputStream(it) }
    }

    private class BudgetedEntryInputStream(
        private val input: InputStream,
        private val budget: ExtractionBudget,
        private val label: String,
        private val compressedSize: Long,
    ) : InputStream() {
        var entryBytes = 0L
            private set

        override fun read(): Int {
            val value = input.read()
            if (value >= 0) account(1)
            return value
        }

        override fun read(
            bytes: ByteArray,
            offset: Int,
            length: Int,
        ): Int {
            val count = input.read(bytes, offset, length)
            if (count > 0) account(count)
            return count
        }

        fun drain() {
            val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
            while (read(buffer) >= 0) {
                // Keep draining so the descriptor resolves exact entry sizes
            }
        }

        private fun account(count: Int) {
            entryBytes += count
            budget.accountActual(count, entryBytes, compressedSize, label)
        }
    }

    private fun InputStream.copyLimitedTo(
        output: FileOutputStream,
        byteCount: Long,
        budget: ExtractionBudget,
        label: String,
    ) {
        val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
        var remaining = byteCount
        while (remaining > 0) {
            val read = read(buffer, 0, minOf(buffer.size.toLong(), remaining).toInt())
            if (read < 0) throw IOException("Unexpected end of $label")
            budget.accountActual(read, byteCount - remaining + read, byteCount, label)
            output.write(buffer, 0, read)
            remaining -= read.toLong()
        }
    }

    private fun copyTrack(
        input: InputStream,
        output: File,
        declaredSize: Long,
        label: String,
        budget: ExtractionBudget,
    ) {
        budget.registerEntry(declaredSize, declaredSize, label)
        FileOutputStream(output).use { input.copyToBounded(it, budget, declaredSize, label) }
    }

    private fun readTrack(
        input: InputStream,
        declaredSize: Long,
        label: String,
        budget: ExtractionBudget,
    ): ByteArray {
        budget.registerEntry(declaredSize, declaredSize, label)
        return input.readBytesBounded(
            ExtractionLimits.MAX_ENTRY_BYTES,
            label,
            budget,
            declaredSize,
            expectedSizeBytes = declaredSize,
        )
    }

    private fun InputStream.readNBytesCompat(count: Int): ByteArray {
        val out = ByteArray(count)
        var total = 0
        while (total < count) {
            val read = read(out, total, count - total)
            if (read < 0) break
            total += read
        }
        return if (total == count) out else out.copyOf(total)
    }

    private fun InputStream.skipFullyBounded(
        count: Long,
        budget: ExtractionBudget,
        label: String,
    ) {
        val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
        var total = 0L
        while (total < count) {
            val read = read(buffer, 0, minOf(buffer.size.toLong(), count - total).toInt())
            if (read < 0) throw IOException("Unexpected end of $label")
            total += read
            budget.accountActual(read, total, count, label)
        }
    }

    private fun hogEntryName(bytes: ByteArray): String =
        bytes
            .takeWhile { it.toInt() != 0 }
            .toByteArray()
            .toString(Charsets.US_ASCII)
            .trim()

    private fun leInt(bytes: ByteArray): Int =
        (bytes[0].toInt() and 0xff) or
            ((bytes[1].toInt() and 0xff) shl 8) or
            ((bytes[2].toInt() and 0xff) shl 16) or
            ((bytes[3].toInt() and 0xff) shl 24)

    private fun sha256(text: String): String =
        MessageDigest
            .getInstance("SHA-256")
            .digest(text.toByteArray(Charsets.UTF_8))
            .joinToString("") { "%02x".format(it) }

    private fun normalizePath(path: String): String = path.replace('\\', '/').trim('/').lowercase(Locale.US)

    private companion object {
        const val MAX_CACHE_BYTES = 256L * 1024L * 1024L
        const val MAX_MIDI_BYTES = 64L * 1024L * 1024L
    }
}

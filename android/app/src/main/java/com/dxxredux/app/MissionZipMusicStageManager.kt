package com.dxxredux.app

import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.security.MessageDigest
import java.util.Locale

class MissionZipMusicStageManager(
    cacheDir: File,
) {
    private val root = File(cacheDir, "mission_zip_music")

    fun stageCompressedAudioTrack(
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
    ): File? {
        if (!track.playable || track.kind != MissionZipMusic.KIND_COMPRESSED_AUDIO) return null
        if (track.sizeBytes > ExtractionLimits.MAX_ENTRY_BYTES) return null
        val archive = track.sourceFilePath?.let(::File) ?: File(catalog.archivePath)
        if (!archive.isFile && !archive.isDirectory) return null
        val cacheIdentity = stagedIdentity(catalog, track)
        val output = stagedFile(cacheIdentity, track)
        val identityFile = identityFile(output)
        if (output.isFile &&
            runCatching {
                identityFile.isFile && identityFile.readText(Charsets.US_ASCII) == cacheIdentity
            }.getOrDefault(false) &&
            (
                (track.sizeBytes > 0L && output.length() == track.sizeBytes) ||
                    (track.sizeBytes <= 0L && output.length() > 0L)
            )
        ) {
            return output
        }
        output.parentFile?.mkdirs()
        ImportStorageGuard.requireFreeSpace(
            output.parentFile ?: output,
            track.sizeBytes.takeIf { it > 0L } ?: archive.length(),
            "stage ${track.displayName}",
        )
        val temp = File(output.parentFile, "${output.name}.tmp")
        temp.delete()
        val ok =
            runCatching {
                if (track.sourceFilePath != null && archive.isFile) {
                    extractFromSourceFile(archive, track, temp)
                } else if (archive.isDirectory) {
                    extractFromDirectory(archive, track, temp)
                } else {
                    ArchiveFiles.open(archive).use { archiveFile ->
                        when {
                            track.hogEntryName != null && track.nestedEntryPath != null -> {
                                archiveFile.openEntryStream(track.archiveEntryPath)?.use { dxa ->
                                    extractFromDxaHog(dxa, track.nestedEntryPath, track.hogEntryName, temp)
                                } ?: false
                            }

                            track.hogEntryName != null -> {
                                archiveFile.openEntryStream(track.archiveEntryPath)?.use { hog ->
                                    extractFromHog(hog, track.hogEntryName, temp)
                                } ?: false
                            }

                            track.nestedEntryPath != null -> {
                                archiveFile.openEntryStream(track.archiveEntryPath)?.use { dxa ->
                                    extractFromDxa(dxa, track.nestedEntryPath, temp)
                                } ?: false
                            }

                            else -> {
                                archiveFile.openEntryStream(track.archiveEntryPath)?.use { input ->
                                    copyTrack(input, temp, track.sizeBytes, track.archiveEntryPath)
                                }
                                true
                            }
                        }
                    }
                }
            }.getOrDefault(false)
        if (!ok || !temp.isFile) {
            temp.delete()
            return null
        }
        output.delete()
        if (!temp.renameTo(output)) {
            temp.copyTo(output, overwrite = true)
            temp.delete()
        }
        identityFile.writeText(cacheIdentity, Charsets.US_ASCII)
        return output.takeIf { it.isFile }
    }

    fun readMidiTrackBytes(
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
    ): ByteArray? {
        if (!track.playable || track.kind != MissionZipMusic.KIND_MIDI) return null
        if (track.sizeBytes > ExtractionLimits.MAX_ENTRY_BYTES) return null
        val archive = track.sourceFilePath?.let(::File) ?: File(catalog.archivePath)
        if (!archive.isFile && !archive.isDirectory) return null
        return runCatching {
            if (track.sourceFilePath != null && archive.isFile) {
                readFromSourceFile(archive, track)
            } else if (archive.isDirectory) {
                readFromDirectory(archive, track)
            } else {
                ArchiveFiles.open(archive).use { archiveFile ->
                    when {
                        track.hogEntryName != null && track.nestedEntryPath != null -> {
                            archiveFile.openEntryStream(track.archiveEntryPath)?.use { dxa ->
                                readFromDxaHog(dxa, track.nestedEntryPath, track.hogEntryName)
                            }
                        }

                        track.hogEntryName != null -> {
                            archiveFile.openEntryStream(track.archiveEntryPath)?.use { hog ->
                                readFromHog(hog, track.hogEntryName)
                            }
                        }

                        track.nestedEntryPath != null -> {
                            archiveFile.openEntryStream(track.archiveEntryPath)?.use { dxa ->
                                readFromDxa(dxa, track.nestedEntryPath)
                            }
                        }

                        else -> {
                            archiveFile.openEntryStream(track.archiveEntryPath)?.use {
                                readTrack(it, track.sizeBytes, track.archiveEntryPath)
                            }
                        }
                    }
                }
            }
        }.getOrNull()
    }

    fun cleanupOldFiles(maxAgeMs: Long = 24L * 60L * 60L * 1000L) {
        val cutoff = System.currentTimeMillis() - maxAgeMs
        root.listFiles()?.forEach { child ->
            if (child.lastModified() < cutoff) child.deleteRecursively()
        }
    }

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
    ): Boolean {
        val file = sourceFile(rootDir, track.archiveEntryPath) ?: return false
        return when {
            track.hogEntryName != null && track.nestedEntryPath != null -> {
                file.inputStream().use { extractFromDxaHog(it, track.nestedEntryPath, track.hogEntryName, output) }
            }

            track.hogEntryName != null -> {
                file.inputStream().use { extractFromHog(it, track.hogEntryName, output) }
            }

            track.nestedEntryPath != null -> {
                file.inputStream().use { extractFromDxa(it, track.nestedEntryPath, output) }
            }

            else -> {
                file.inputStream().use { input -> copyTrack(input, output, file.length(), file.name) }
                true
            }
        }
    }

    private fun readFromDirectory(
        rootDir: File,
        track: MissionZipMusicTrack,
    ): ByteArray? {
        val file = sourceFile(rootDir, track.archiveEntryPath) ?: return null
        return when {
            track.hogEntryName != null && track.nestedEntryPath != null -> {
                file.inputStream().use { readFromDxaHog(it, track.nestedEntryPath, track.hogEntryName) }
            }

            track.hogEntryName != null -> {
                file.inputStream().use { readFromHog(it, track.hogEntryName) }
            }

            track.nestedEntryPath != null -> {
                file.inputStream().use { readFromDxa(it, track.nestedEntryPath) }
            }

            else -> {
                file.inputStream().use { readTrack(it, file.length(), file.name) }
            }
        }
    }

    private fun extractFromSourceFile(
        file: File,
        track: MissionZipMusicTrack,
        output: File,
    ): Boolean =
        when {
            track.hogEntryName != null && track.nestedEntryPath != null -> {
                file.inputStream().use { extractFromDxaHog(it, track.nestedEntryPath, track.hogEntryName, output) }
            }

            track.hogEntryName != null -> {
                file.inputStream().use { extractFromHog(it, track.hogEntryName, output) }
            }

            track.nestedEntryPath != null -> {
                file.inputStream().use { extractFromDxa(it, track.nestedEntryPath, output) }
            }

            else -> {
                file.inputStream().use { input -> copyTrack(input, output, file.length(), file.name) }
                true
            }
        }

    private fun readFromSourceFile(
        file: File,
        track: MissionZipMusicTrack,
    ): ByteArray? =
        when {
            track.hogEntryName != null && track.nestedEntryPath != null -> {
                file.inputStream().use { readFromDxaHog(it, track.nestedEntryPath, track.hogEntryName) }
            }

            track.hogEntryName != null -> {
                file.inputStream().use { readFromHog(it, track.hogEntryName) }
            }

            track.nestedEntryPath != null -> {
                file.inputStream().use { readFromDxa(it, track.nestedEntryPath) }
            }

            else -> {
                file.inputStream().use { readTrack(it, file.length(), file.name) }
            }
        }

    private fun extractFromDxa(
        dxa: InputStream,
        nestedEntryPath: String,
        output: File,
    ): Boolean {
        openZipInputStreamSkippingPreamble(dxa).use { zip ->
            val budget = ExtractionBudget()
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory && normalizePath(entry.name).equals(nestedEntryPath, ignoreCase = true)) {
                    FileOutputStream(output).use {
                        zip.copyToBounded(it, budget, entry.compressedSize, entry.name)
                    }
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
    ): ByteArray? {
        openZipInputStreamSkippingPreamble(dxa).use { zip ->
            val budget = ExtractionBudget()
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory && normalizePath(entry.name).equals(nestedEntryPath, ignoreCase = true)) {
                    return zip.readBytesBounded(
                        ExtractionLimits.MAX_ENTRY_BYTES,
                        entry.name,
                        budget,
                        entry.compressedSize,
                    )
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
    ): ByteArray? {
        openZipInputStreamSkippingPreamble(dxa).use { zip ->
            val budget = ExtractionBudget()
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory && normalizePath(entry.name).equals(nestedEntryPath, ignoreCase = true)) {
                    return readFromHog(zip, hogEntryName)
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
    ): ByteArray? {
        if (hog.readNBytesCompat(3).toString(Charsets.US_ASCII) != "DHF") return null
        val budget = ExtractionBudget()
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
            hog.skipFullyCompat(size)
        }
    }

    private fun extractFromDxaHog(
        dxa: InputStream,
        nestedEntryPath: String,
        hogEntryName: String,
        output: File,
    ): Boolean {
        openZipInputStreamSkippingPreamble(dxa).use { zip ->
            val budget = ExtractionBudget()
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory && normalizePath(entry.name).equals(nestedEntryPath, ignoreCase = true)) {
                    return extractFromHog(zip, hogEntryName, output)
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
    ): Boolean {
        if (hog.readNBytesCompat(3).toString(Charsets.US_ASCII) != "DHF") return false
        val budget = ExtractionBudget()
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
            hog.skipFullyCompat(size)
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
    ) {
        val budget = ExtractionBudget()
        budget.registerEntry(declaredSize, declaredSize, label)
        FileOutputStream(output).use { input.copyToBounded(it, budget, declaredSize, label) }
    }

    private fun readTrack(
        input: InputStream,
        declaredSize: Long,
        label: String,
    ): ByteArray {
        val budget = ExtractionBudget()
        budget.registerEntry(declaredSize, declaredSize, label)
        return input.readBytesBounded(ExtractionLimits.MAX_ENTRY_BYTES, label, budget, declaredSize)
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

    private fun InputStream.skipFullyCompat(count: Long) {
        var remaining = count
        while (remaining > 0) {
            val skipped = skip(remaining)
            if (skipped > 0) {
                remaining -= skipped
            } else if (read() >= 0) {
                remaining--
            } else {
                return
            }
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
}

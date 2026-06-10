package com.dxxredux.app

import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.security.MessageDigest
import java.util.Locale
import java.util.zip.ZipFile

class MissionZipMusicStageManager(
    cacheDir: File,
) {
    private val root = File(cacheDir, "mission_zip_music")

    fun stageCompressedAudioTrack(
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
    ): File? {
        if (!track.playable || track.kind != MissionZipMusic.KIND_COMPRESSED_AUDIO) return null
        val archive = File(catalog.archivePath)
        if (!archive.isFile) return null
        val output = stagedFile(archive, track)
        if (output.isFile && (track.sizeBytes <= 0 || output.length() == track.sizeBytes)) return output
        output.parentFile?.mkdirs()
        val temp = File(output.parentFile, "${output.name}.tmp")
        temp.delete()
        val ok =
            runCatching {
                ZipFile(archive).use { zip ->
                    when {
                        track.hogEntryName != null && track.nestedEntryPath != null -> {
                            zip.openEntryStream(track.archiveEntryPath)?.use { dxa ->
                                extractFromDxaHog(dxa, track.nestedEntryPath, track.hogEntryName, temp)
                            } ?: false
                        }

                        track.hogEntryName != null -> {
                            zip.openEntryStream(track.archiveEntryPath)?.use { hog ->
                                extractFromHog(hog, track.hogEntryName, temp)
                            } ?: false
                        }

                        track.nestedEntryPath != null -> {
                            zip.openEntryStream(track.archiveEntryPath)?.use { dxa ->
                                extractFromDxa(dxa, track.nestedEntryPath, temp)
                            } ?: false
                        }

                        else -> {
                            zip.openEntryStream(track.archiveEntryPath)?.use { input ->
                                FileOutputStream(temp).use { outputStream -> input.copyTo(outputStream) }
                            }
                            true
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
        return output.takeIf { it.isFile }
    }

    fun readMidiTrackBytes(
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
    ): ByteArray? {
        if (!track.playable || track.kind != MissionZipMusic.KIND_MIDI) return null
        val archive = File(catalog.archivePath)
        if (!archive.isFile) return null
        return runCatching {
            ZipFile(archive).use { zip ->
                when {
                    track.hogEntryName != null && track.nestedEntryPath != null -> {
                        zip.openEntryStream(track.archiveEntryPath)?.use { dxa ->
                            readFromDxaHog(dxa, track.nestedEntryPath, track.hogEntryName)
                        }
                    }

                    track.hogEntryName != null -> {
                        zip.openEntryStream(track.archiveEntryPath)?.use { hog ->
                            readFromHog(hog, track.hogEntryName)
                        }
                    }

                    track.nestedEntryPath != null -> {
                        zip.openEntryStream(track.archiveEntryPath)?.use { dxa ->
                            readFromDxa(dxa, track.nestedEntryPath)
                        }
                    }

                    else -> {
                        zip.openEntryStream(track.archiveEntryPath)?.use { it.readBytes() }
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
        archive: File,
        track: MissionZipMusicTrack,
    ): File {
        val key = sha256("${archive.name}:${archive.length()}:${archive.lastModified()}:${track.id}")
        val safeName = track.displayName.replace(Regex("[^a-zA-Z0-9._-]"), "_").ifBlank { "track.${track.extension}" }
        return File(File(root, key.take(16)), safeName)
    }

    private fun extractFromDxa(
        dxa: InputStream,
        nestedEntryPath: String,
        output: File,
    ): Boolean {
        openZipInputStreamSkippingPreamble(dxa).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                if (!entry.isDirectory && normalizePath(entry.name).equals(nestedEntryPath, ignoreCase = true)) {
                    FileOutputStream(output).use { zip.copyTo(it) }
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
            var entry = zip.nextEntry
            while (entry != null) {
                if (!entry.isDirectory && normalizePath(entry.name).equals(nestedEntryPath, ignoreCase = true)) {
                    return zip.readBytes()
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
            var entry = zip.nextEntry
            while (entry != null) {
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
        while (true) {
            val nameBytes = hog.readNBytesCompat(13)
            if (nameBytes.isEmpty() || nameBytes.size != 13) return null
            val lenBytes = hog.readNBytesCompat(4)
            if (lenBytes.size != 4) return null
            val name = hogEntryName(nameBytes)
            val size = leInt(lenBytes).toLong() and 0xffff_ffffL
            if (name.equals(hogEntryName, ignoreCase = true)) {
                return hog.readNBytesCompat(size.coerceAtMost(Int.MAX_VALUE.toLong()).toInt())
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
            var entry = zip.nextEntry
            while (entry != null) {
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
        while (true) {
            val nameBytes = hog.readNBytesCompat(13)
            if (nameBytes.isEmpty() || nameBytes.size != 13) return false
            val lenBytes = hog.readNBytesCompat(4)
            if (lenBytes.size != 4) return false
            val name = hogEntryName(nameBytes)
            val size = leInt(lenBytes).toLong() and 0xffff_ffffL
            if (name.equals(hogEntryName, ignoreCase = true)) {
                FileOutputStream(output).use { out -> hog.copyLimitedTo(out, size) }
                return true
            }
            hog.skipFullyCompat(size)
        }
    }

    private fun ZipFile.openEntryStream(path: String): InputStream? {
        val normalized = normalizePath(path)
        val direct = getEntry(normalized)
        if (direct != null) return getInputStream(direct)
        val entries = entries()
        while (entries.hasMoreElements()) {
            val entry = entries.nextElement()
            if (!entry.isDirectory && normalizePath(entry.name).equals(normalized, ignoreCase = true)) {
                return getInputStream(entry)
            }
        }
        return null
    }

    private fun InputStream.copyLimitedTo(
        output: FileOutputStream,
        byteCount: Long,
    ) {
        val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
        var remaining = byteCount
        while (remaining > 0) {
            val read = read(buffer, 0, minOf(buffer.size.toLong(), remaining).toInt())
            if (read < 0) return
            output.write(buffer, 0, read)
            remaining -= read.toLong()
        }
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

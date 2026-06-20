package com.dxxredux.app

import org.apache.commons.compress.archivers.sevenz.SevenZArchiveEntry
import org.apache.commons.compress.archivers.sevenz.SevenZFile
import java.io.Closeable
import java.io.File
import java.io.InputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipFile

internal data class ArchiveFileEntry(
    val path: String,
    val isDirectory: Boolean,
    val sizeBytes: Long,
    val compressedSizeBytes: Long,
    internal val handle: Any,
) {
    val name: String get() = path.substringAfterLast('/').substringAfterLast('\\')
}

internal interface ReadableArchiveFile : Closeable {
    val format: String
    val entries: List<ArchiveFileEntry>

    fun openInputStream(entry: ArchiveFileEntry): InputStream

    fun findEntry(path: String): ArchiveFileEntry? {
        val normalized = normalizeArchivePath(path)
        entries.firstOrNull { normalizeArchivePath(it.path) == normalized }?.let { return it }
        return entries.firstOrNull { normalizeArchivePath(it.path).equals(normalized, ignoreCase = true) }
    }
}

internal object ArchiveFiles {
    fun isSupportedArchiveName(name: String): Boolean = GameFileFormats.extensionOf(name) in setOf("zip", "7z")

    fun open(file: File): ReadableArchiveFile {
        val ext = GameFileFormats.extensionOf(file.name)
        if (ext == "7z") return SevenZReadableArchive(file)
        return try {
            ZipReadableArchive(file)
        } catch (zipError: Exception) {
            try {
                SevenZReadableArchive(file)
            } catch (_: Exception) {
                throw zipError
            }
        }
    }
}

internal fun normalizeArchivePath(path: String): String = path.replace('\\', '/').trim('/')

private class ZipReadableArchive(
    file: File,
) : ReadableArchiveFile {
    private val zip = ZipFile(file)
    override val format: String = "zip"
    override val entries: List<ArchiveFileEntry> =
        zip
            .entries()
            .asSequence()
            .map { entry ->
                ArchiveFileEntry(
                    path = entry.name,
                    isDirectory = entry.isDirectory,
                    sizeBytes = entry.size.coerceAtLeast(0),
                    compressedSizeBytes = entry.compressedSize.coerceAtLeast(0),
                    handle = entry,
                )
            }.toList()

    override fun openInputStream(entry: ArchiveFileEntry): InputStream = zip.getInputStream(entry.handle as ZipEntry)

    override fun close() = zip.close()
}

private class SevenZReadableArchive(
    file: File,
) : ReadableArchiveFile {
    private val sevenZ = SevenZFile.builder().setFile(file).get()
    override val format: String = "7z"
    override val entries: List<ArchiveFileEntry> =
        sevenZ.entries.map { entry ->
            ArchiveFileEntry(
                path = entry.name.orEmpty(),
                isDirectory = entry.isDirectory,
                sizeBytes = entry.size.coerceAtLeast(0),
                compressedSizeBytes = entry.size.coerceAtLeast(0),
                handle = entry,
            )
        }

    override fun openInputStream(entry: ArchiveFileEntry): InputStream =
        sevenZ.getInputStream(entry.handle as SevenZArchiveEntry)

    override fun close() = sevenZ.close()
}

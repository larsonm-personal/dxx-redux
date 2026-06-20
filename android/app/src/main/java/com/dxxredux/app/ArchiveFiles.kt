package com.dxxredux.app

import net.sf.sevenzipjbinding.ExtractOperationResult
import net.sf.sevenzipjbinding.ISequentialOutStream
import net.sf.sevenzipjbinding.PropID
import net.sf.sevenzipjbinding.SevenZip
import net.sf.sevenzipjbinding.impl.RandomAccessFileInStream
import org.apache.commons.compress.archivers.sevenz.SevenZArchiveEntry
import org.apache.commons.compress.archivers.sevenz.SevenZFile
import java.io.Closeable
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.io.RandomAccessFile
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
    fun isSupportedArchiveName(name: String): Boolean = GameFileFormats.extensionOf(name) in setOf("zip", "7z", "rar")

    fun open(file: File): ReadableArchiveFile {
        val ext = GameFileFormats.extensionOf(file.name)
        if (ext == "7z") return SevenZReadableArchive(file)
        if (ext == "rar") return ExtractedReadableArchive(file, "rar")
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

private class ExtractedReadableArchive(
    file: File,
    override val format: String,
) : ReadableArchiveFile {
    private val root =
        File
            .createTempFile("dxx_archive_", "_$format")
            .also {
                it.delete()
                it.mkdirs()
            }.canonicalFile
    override val entries: List<ArchiveFileEntry>

    init {
        extractRarArchiveToDirectory(file, root)
        entries =
            root
                .walkTopDown()
                .drop(1)
                .map { child ->
                    val relative =
                        root
                            .toPath()
                            .relativize(child.toPath())
                            .toString()
                            .replace('\\', '/')
                    ArchiveFileEntry(
                        path = relative,
                        isDirectory = child.isDirectory,
                        sizeBytes = if (child.isFile) child.length() else 0L,
                        compressedSizeBytes = if (child.isFile) child.length() else 0L,
                        handle = child,
                    )
                }.toList()
    }

    override fun openInputStream(entry: ArchiveFileEntry): InputStream = (entry.handle as File).inputStream()

    override fun close() {
        root.deleteRecursively()
    }
}

internal fun extractRarArchiveToDirectory(
    archive: File,
    targetRoot: File,
) {
    targetRoot.mkdirs()
    runCatching {
        extractRarWithSevenZipBinding(archive, targetRoot)
    }.getOrElse { sevenZipError ->
        runCatching {
            extractRarWithHostTar(archive, targetRoot)
        }.getOrElse { tarError ->
            throw IllegalArgumentException(
                "RAR extraction failed: ${sevenZipError.message ?: sevenZipError.javaClass.simpleName}; " +
                    tarError.message,
                tarError,
            )
        }
    }
}

private fun extractRarWithSevenZipBinding(
    archive: File,
    targetRoot: File,
) {
    val canonicalRoot = targetRoot.canonicalFile
    RandomAccessFile(archive, "r").use { randomAccessFile ->
        val input = RandomAccessFileInStream(randomAccessFile)
        try {
            val inArchive = SevenZip.openInArchive(null, input)
            try {
                for (index in 0 until inArchive.getNumberOfItems()) {
                    val isDirectory = inArchive.getProperty(index, PropID.IS_FOLDER) as? Boolean ?: false
                    val path = inArchive.getStringProperty(index, PropID.PATH)?.replace('\\', '/')?.trim('/')
                    if (path.isNullOrBlank()) continue
                    val output = File(canonicalRoot, path.replace('/', File.separatorChar)).canonicalFile
                    if (!output.path.startsWith(canonicalRoot.path + File.separator)) continue
                    if (isDirectory) {
                        output.mkdirs()
                        continue
                    }
                    output.parentFile?.mkdirs()
                    FileOutputStream(output).use { fileOutput ->
                        val result =
                            inArchive.extractSlow(
                                index,
                                ISequentialOutStream { data ->
                                    fileOutput.write(data)
                                    data.size
                                },
                            )
                        if (result != ExtractOperationResult.OK) {
                            throw IllegalArgumentException("7-Zip extraction returned $result for $path")
                        }
                    }
                }
            } finally {
                inArchive.close()
            }
        } finally {
            input.close()
        }
    }
}

private fun extractRarWithHostTar(
    archive: File,
    targetRoot: File,
) {
    val process =
        ProcessBuilder("tar", "-xf", archive.absolutePath, "-C", targetRoot.absolutePath)
            .redirectErrorStream(true)
            .start()
    val output = process.inputStream.bufferedReader().use { it.readText() }
    val exitCode = process.waitFor()
    if (exitCode != 0) {
        throw IllegalArgumentException("host tar failed with exit code $exitCode: $output")
    }
}

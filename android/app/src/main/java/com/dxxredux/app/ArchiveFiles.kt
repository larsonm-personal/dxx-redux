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
import java.io.IOException
import java.io.InputStream
import java.io.RandomAccessFile
import java.text.Normalizer
import java.util.Locale
import java.util.concurrent.TimeUnit
import java.util.zip.ZipEntry
import java.util.zip.ZipFile

internal data class ArchiveFileEntry(
    val path: String,
    val isDirectory: Boolean,
    val sizeBytes: Long,
    val compressedSizeBytes: Long?,
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

internal data class ArchiveOutputProjection(
    val sourcePath: String,
    val relativePath: String,
    val isDirectory: Boolean,
)

internal fun validateArchiveOutputProjections(
    projections: List<ArchiveOutputProjection>,
    label: String,
): List<ArchiveOutputProjection> {
    val normalized =
        projections.map { projection ->
            val relativePath = canonicalArchiveRelativePath(projection.relativePath)
            require(!relativePath.isNullOrBlank()) {
                "$label has an invalid output path: ${projection.sourcePath}"
            }
            projection.copy(relativePath = relativePath)
        }
    val filesByKey = normalized.filterNot { it.isDirectory }.groupBy { archiveOutputKey(it.relativePath) }
    filesByKey
        .filterValues { it.size > 1 }
        .toSortedMap()
        .values
        .firstOrNull()
        ?.let { collision ->
            val sources = collision.map { it.sourcePath }.sorted().joinToString(", ")
            throw IllegalArgumentException("$label has colliding file outputs: $sources")
        }
    val directoryKeys = mutableSetOf<String>()
    for (projection in normalized) {
        if (projection.isDirectory) directoryKeys += archiveOutputKey(projection.relativePath)
        val components = projection.relativePath.split('/')
        for (end in 1 until components.size) {
            directoryKeys += archiveOutputKey(components.take(end).joinToString("/"))
        }
    }
    normalized
        .filterNot { it.isDirectory }
        .sortedBy { archiveOutputKey(it.relativePath) }
        .firstOrNull { archiveOutputKey(it.relativePath) in directoryKeys }
        ?.let { collision ->
            throw IllegalArgumentException(
                "$label has a file and directory output collision: ${collision.sourcePath}",
            )
        }
    return normalized
}

private fun canonicalArchiveRelativePath(path: String): String? {
    val components = mutableListOf<String>()
    for (component in path.replace('\\', '/').split('/')) {
        when (component) {
            "", "." -> Unit
            ".." -> if (components.isEmpty()) return null else components.removeAt(components.lastIndex)
            else -> components += component
        }
    }
    return components.joinToString("/")
}

// Collision policy is Unicode NFC plus locale-independent case folding
private fun archiveOutputKey(path: String): String =
    Normalizer.normalize(path, Normalizer.Form.NFC).lowercase(Locale.ROOT)

private class ZipReadableArchive(
    file: File,
) : ReadableArchiveFile {
    private val zip = ZipFile(file)
    override val format: String = "zip"
    override val entries: List<ArchiveFileEntry> =
        try {
            ExtractionBudget().let { budget ->
                zip
                    .entries()
                    .asSequence()
                    .map { entry ->
                        ArchiveFileEntry(
                            path = entry.name,
                            isDirectory = entry.isDirectory,
                            sizeBytes = entry.size.coerceAtLeast(0),
                            compressedSizeBytes = entry.compressedSize.takeIf { it >= 0 },
                            handle = entry,
                        ).also {
                            budget.registerEntry(
                                if (it.isDirectory) 0 else it.sizeBytes,
                                if (it.isDirectory) 0 else it.compressedSizeBytes ?: 0,
                                it.path,
                            )
                        }
                    }.toList()
            }
        } catch (error: Exception) {
            zip.close()
            throw error
        }

    override fun openInputStream(entry: ArchiveFileEntry): InputStream = zip.getInputStream(entry.handle as ZipEntry)

    override fun close() = zip.close()
}

private class SevenZReadableArchive(
    file: File,
) : ReadableArchiveFile {
    private val sevenZ =
        SevenZFile
            .builder()
            .setFile(file)
            .setMaxMemoryLimitKiB(ExtractionLimits.MAX_SEVEN_Z_DECODER_MEMORY_KIB)
            .get()
    override val format: String = "7z"
    override val entries: List<ArchiveFileEntry> =
        try {
            ExtractionBudget().let { budget ->
                sevenZ.entries.map { entry ->
                    ArchiveFileEntry(
                        path = entry.name.orEmpty(),
                        isDirectory = entry.isDirectory,
                        sizeBytes = entry.size.coerceAtLeast(0),
                        // Commons Compress does not expose the per-entry compressed size here.
                        // Actual-byte limits still apply when the entry is materialized.
                        compressedSizeBytes = null,
                        handle = entry,
                    ).also {
                        budget.registerEntry(
                            if (it.isDirectory) 0 else it.sizeBytes,
                            if (it.isDirectory) 0 else it.compressedSizeBytes ?: 0,
                            it.path,
                        )
                    }
                }
            }
        } catch (error: Exception) {
            sevenZ.close()
            throw error
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
        try {
            extractRarArchiveToDirectory(file, root)
            val budget = ExtractionBudget()
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
                            compressedSizeBytes = null,
                            handle = child,
                        ).also {
                            budget.registerEntry(
                                if (it.isDirectory) 0 else it.sizeBytes,
                                if (it.isDirectory) 0 else it.compressedSizeBytes ?: 0,
                                it.path,
                            )
                        }
                    }.toList()
        } catch (error: Exception) {
            root.deleteRecursively()
            throw error
        }
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
        targetRoot.deleteRecursively()
        targetRoot.mkdirs()
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
                val budget = ExtractionBudget()
                val items =
                    (0 until inArchive.getNumberOfItems()).mapNotNull { index ->
                        val path = inArchive.getStringProperty(index, PropID.PATH)?.replace('\\', '/')?.trim('/')
                        if (path.isNullOrBlank()) return@mapNotNull null
                        RarExtractionItem(
                            index = index,
                            path = path,
                            isDirectory = inArchive.getProperty(index, PropID.IS_FOLDER) as? Boolean ?: false,
                            size = (inArchive.getProperty(index, PropID.SIZE) as? Number)?.toLong() ?: -1L,
                            packedSize =
                                (inArchive.getProperty(index, PropID.PACKED_SIZE) as? Number)?.toLong() ?: -1L,
                        )
                    }
                val outputs =
                    validateArchiveOutputProjections(
                        items.map { ArchiveOutputProjection(it.path, it.path, it.isDirectory) },
                        "RAR archive",
                    )
                for ((item, outputPath) in items.zip(outputs)) {
                    val index = item.index
                    val path = outputPath.relativePath
                    val isDirectory = item.isDirectory
                    val size = item.size
                    val packedSize = item.packedSize
                    budget.registerEntry(if (isDirectory) 0 else size, if (isDirectory) 0 else packedSize, path)
                    val output = File(canonicalRoot, path.replace('/', File.separatorChar)).canonicalFile
                    if (!output.path.startsWith(canonicalRoot.path + File.separator)) continue
                    if (isDirectory) {
                        output.mkdirs()
                        continue
                    }
                    output.parentFile?.mkdirs()
                    var entryBytes = 0L
                    FileOutputStream(output).use { fileOutput ->
                        val result =
                            inArchive.extractSlow(
                                index,
                                ISequentialOutStream { data ->
                                    entryBytes += data.size
                                    budget.accountActual(data.size, entryBytes, packedSize, path)
                                    if (entryBytes % (8L * 1024L * 1024L) < data.size) {
                                        ImportStorageGuard.requireFreeSpace(canonicalRoot, 0, "extract $path")
                                    }
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

private data class RarExtractionItem(
    val index: Int,
    val path: String,
    val isDirectory: Boolean,
    val size: Long,
    val packedSize: Long,
)

private fun extractRarWithHostTar(
    archive: File,
    targetRoot: File,
) {
    validateHostTarArchiveOutputs(archive, targetRoot)
    val diagnosticFile = File(targetRoot.parentFile, "${targetRoot.name}.tar.log")
    diagnosticFile.delete()
    val process =
        ProcessBuilder("tar", "-xf", archive.absolutePath, "-C", targetRoot.absolutePath)
            .redirectErrorStream(true)
            .redirectOutput(diagnosticFile)
            .start()
    val deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(300)
    try {
        while (!process.waitFor(100, TimeUnit.MILLISECONDS)) {
            if (System.nanoTime() >= deadline) throw IOException("host tar exceeded 300 seconds")
            if (diagnosticFile.length() > ExtractionLimits.MAX_DESCRIPTOR_BYTES) {
                throw IOException("host tar diagnostics exceed ${ExtractionLimits.MAX_DESCRIPTOR_BYTES} bytes")
            }
            validateExtractedTree(targetRoot)
        }
    } catch (error: Exception) {
        process.destroyForcibly()
        process.waitFor(5, TimeUnit.SECONDS)
        diagnosticFile.delete()
        throw error
    }
    validateExtractedTree(targetRoot)
    val output =
        try {
            diagnosticFile.inputStream().use {
                it
                    .readBytesBounded(ExtractionLimits.MAX_DESCRIPTOR_BYTES, "host tar diagnostics")
                    .toString(Charsets.UTF_8)
            }
        } finally {
            diagnosticFile.delete()
        }
    val exitCode = process.exitValue()
    if (exitCode != 0) {
        throw IllegalArgumentException("host tar failed with exit code $exitCode: $output")
    }
}

private fun validateHostTarArchiveOutputs(
    archive: File,
    targetRoot: File,
) {
    val listingFile = File(targetRoot.parentFile, "${targetRoot.name}.tar.list")
    val errorFile = File(targetRoot.parentFile, "${targetRoot.name}.tar.list.log")
    listingFile.delete()
    errorFile.delete()
    val process =
        ProcessBuilder("tar", "-tf", archive.absolutePath)
            .redirectOutput(listingFile)
            .redirectError(errorFile)
            .start()
    val deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(300)
    try {
        while (!process.waitFor(100, TimeUnit.MILLISECONDS)) {
            if (System.nanoTime() >= deadline) throw IOException("host tar listing exceeded 300 seconds")
            if (listingFile.length() + errorFile.length() > ExtractionLimits.MAX_DESCRIPTOR_BYTES) {
                throw IOException("host tar listing exceeds ${ExtractionLimits.MAX_DESCRIPTOR_BYTES} bytes")
            }
        }
        val diagnostics =
            errorFile.inputStream().use {
                it
                    .readBytesBounded(ExtractionLimits.MAX_DESCRIPTOR_BYTES, "host tar listing diagnostics")
                    .toString(Charsets.UTF_8)
            }
        if (process.exitValue() != 0) {
            throw IllegalArgumentException(
                "host tar listing failed with exit code ${process.exitValue()}: $diagnostics",
            )
        }
        val listing =
            listingFile.inputStream().use {
                it
                    .readBytesBounded(ExtractionLimits.MAX_DESCRIPTOR_BYTES, "host tar listing")
                    .toString(Charsets.UTF_8)
            }
        validateArchiveOutputProjections(
            listing
                .lineSequence()
                .filter { it.isNotBlank() }
                .map { path -> ArchiveOutputProjection(path, path.trimEnd('/'), path.endsWith('/')) }
                .toList(),
            "RAR archive",
        )
    } catch (error: Exception) {
        process.destroyForcibly()
        process.waitFor(5, TimeUnit.SECONDS)
        throw error
    } finally {
        listingFile.delete()
        errorFile.delete()
    }
}

private fun validateExtractedTree(targetRoot: File) {
    val budget = ExtractionBudget()
    targetRoot.walkTopDown().drop(1).forEach { child ->
        val relative = targetRoot.toPath().relativize(child.toPath()).toString()
        budget.registerEntry(if (child.isFile) child.length() else 0, 0, relative)
    }
    ImportStorageGuard.requireFreeSpace(targetRoot, 0, "extract ${targetRoot.name}")
}

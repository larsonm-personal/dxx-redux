package com.dxxredux.app

import net.sf.sevenzipjbinding.ExtractAskMode
import net.sf.sevenzipjbinding.ExtractOperationResult
import net.sf.sevenzipjbinding.IArchiveExtractCallback
import net.sf.sevenzipjbinding.IInArchive
import net.sf.sevenzipjbinding.ISequentialOutStream
import net.sf.sevenzipjbinding.PropID
import net.sf.sevenzipjbinding.SevenZip
import net.sf.sevenzipjbinding.SevenZipNativeInitializationException
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
import java.util.zip.ZipEntry
import java.util.zip.ZipFile

internal data class ArchiveFileEntry(
    val path: String,
    val isDirectory: Boolean,
    val sizeBytes: Long,
    val compressedSizeBytes: Long?,
    internal val handle: Any,
    val modifiedDate: String? = null,
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
    // Bounded format dispatch only; extraction still validates the complete archive
    fun isNonZipDistribution(file: File): Boolean =
        RandomAccessFile(file, "r").use { archive ->
            val header = ByteArray(minOf(8L, archive.length()).toInt())
            archive.readFully(header)
            val sevenZip = byteArrayOf(0x37, 0x7a, 0xbc.toByte(), 0xaf.toByte(), 0x27, 0x1c)
            val rar4 = byteArrayOf(0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00)
            val rar5 = byteArrayOf(0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x01, 0x00)
            listOf(sevenZip, rar4, rar5).any { signature ->
                header.size >= signature.size && signature.indices.all { header[it] == signature[it] }
            }
        }

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

internal class ArchiveOutputValidationException(
    message: String,
) : IllegalArgumentException(message)

internal sealed class RarNativeCapabilityException(
    message: String,
    cause: Throwable? = null,
) : IOException(message, cause)

internal class RarNativeBackendUnavailableException(
    message: String,
    cause: Throwable? = null,
) : RarNativeCapabilityException(message, cause)

internal class RarNativeFormatUnsupportedException(
    message: String,
    cause: Throwable? = null,
) : RarNativeCapabilityException(message, cause)

internal fun validateArchiveOutputProjections(
    projections: List<ArchiveOutputProjection>,
    label: String,
): List<ArchiveOutputProjection> {
    val normalized =
        projections.map { projection ->
            val relativePath = canonicalArchiveRelativePath(projection.relativePath)
            if (relativePath.isNullOrBlank()) {
                throw ArchiveOutputValidationException("$label has an invalid output path: ${projection.sourcePath}")
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
            throw ArchiveOutputValidationException("$label has colliding file outputs: $sources")
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
            throw ArchiveOutputValidationException(
                "$label has a file and directory output collision: ${collision.sourcePath}",
            )
        }
    return normalized
}

internal fun canonicalArchiveRelativePath(path: String): String? {
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
                            modifiedDate = ArchiveEntryDates.zip(entry),
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
    private var nativeArchive: ReadableArchiveFile? = null
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
                        modifiedDate = ArchiveEntryDates.sevenZ(entry),
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

    private val sourceFile = file

    override fun openInputStream(entry: ArchiveFileEntry): InputStream {
        nativeArchive?.let { return nativeInput(it, entry) }
        return try {
            sevenZ.getInputStream(entry.handle as SevenZArchiveEntry)
        } catch (error: IOException) {
            // Commons can catalog BCJ2 folders but cannot construct their decoder
            if (!isSevenZMultiStreamUnsupported(error)) throw error
            val extracted = ExtractedReadableArchive(sourceFile, "7z")
            nativeArchive = extracted
            nativeInput(extracted, entry)
        }
    }

    private fun nativeInput(
        archive: ReadableArchiveFile,
        entry: ArchiveFileEntry,
    ): InputStream {
        val selected =
            archive.entries.singleOrNull {
                it.path == normalizeArchivePath(entry.path) &&
                    it.sizeBytes == entry.sizeBytes
            }
                ?: throw IOException("Native 7z catalog differs for ${entry.path}")
        return archive.openInputStream(selected)
    }

    override fun close() {
        try {
            nativeArchive?.close()
        } finally {
            sevenZ.close()
        }
    }
}

internal fun isSevenZMultiStreamUnsupported(error: IOException): Boolean =
    error.message == "Multi input/output stream coders are not yet supported"

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
            val originalDates = readRarModificationDates(file)
            if (format == "7z") {
                extractRarWithSevenZipBinding(file, root, solid = true)
            } else {
                extractRarArchiveToDirectory(file, root)
            }
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
                            modifiedDate = originalDates[relative],
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

// Read the catalog before extraction; fallback extractors do not guarantee stored timestamps
private fun readRarModificationDates(file: File): Map<String, String> =
    runCatching {
        RandomAccessFile(file, "r").use { input ->
            val stream = RandomAccessFileInStream(input)
            val archive = SevenZip.openInArchive(null, stream)
            try {
                buildMap {
                    for (index in 0 until archive.getNumberOfItems()) {
                        val name = archive.getStringProperty(index, PropID.PATH) ?: continue
                        val timestamp = archive.getProperty(index, PropID.LAST_MODIFICATION_TIME)
                        val date = timestamp as? java.util.Date ?: continue
                        put(normalizeArchivePath(name), ArchiveEntryDates.utc(date))
                    }
                }
            } finally {
                archive.close()
            }
        }
    }.getOrDefault(emptyMap())

internal fun extractRarArchiveToDirectory(
    archive: File,
    targetRoot: File,
    nativeExtractor: (File, File) -> Unit = { archive, root -> extractRarWithSevenZipBinding(archive, root) },
) {
    targetRoot.mkdirs()
    try {
        nativeExtractor(archive, targetRoot)
    } catch (nativeError: Throwable) {
        val capabilityError = nativeRarCapabilityFailure(nativeError) ?: throw nativeError
        targetRoot.deleteRecursively()
        throw capabilityError
    }
}

private fun nativeRarCapabilityFailure(error: Throwable): RarNativeCapabilityException? =
    when (error) {
        is RarNativeCapabilityException -> {
            error
        }

        is SevenZipNativeInitializationException -> {
            RarNativeBackendUnavailableException(
                "native RAR backend is unavailable: ${error.message ?: error.javaClass.simpleName}",
                error,
            )
        }

        is LinkageError -> {
            RarNativeBackendUnavailableException(
                "native RAR backend is unavailable: ${error.message ?: error.javaClass.simpleName}",
                error,
            )
        }

        is ExceptionInInitializerError -> {
            (error.cause as? LinkageError)?.let { cause ->
                RarNativeBackendUnavailableException(
                    "native RAR backend is unavailable: ${cause.message ?: cause.javaClass.simpleName}",
                    error,
                )
            }
        }

        else -> {
            error.cause
                ?.takeIf { it !== error }
                ?.let(::nativeRarCapabilityFailure)
        }
    }

private fun extractRarWithSevenZipBinding(
    archive: File,
    targetRoot: File,
    solid: Boolean = false,
) {
    val canonicalRoot = targetRoot.canonicalFile
    RandomAccessFile(archive, "r").use { randomAccessFile ->
        val input = RandomAccessFileInStream(randomAccessFile)
        try {
            val inArchive = SevenZip.openInArchive(null, input)
            try {
                val source =
                    object : RarCatalogSource {
                        override val itemCount = inArchive.getNumberOfItems()

                        override fun path(index: Int): String? = inArchive.getStringProperty(index, PropID.PATH)

                        override fun isDirectory(index: Int): Boolean =
                            inArchive.getProperty(index, PropID.IS_FOLDER) as? Boolean ?: false

                        override fun size(index: Int): Long =
                            (inArchive.getProperty(index, PropID.SIZE) as? Number)?.toLong() ?: -1L

                        override fun packedSize(index: Int): Long =
                            (inArchive.getProperty(index, PropID.PACKED_SIZE) as? Number)?.toLong() ?: -1L
                    }
                enumerateRarCatalog(source).use catalogUse@{ catalog ->
                    if (solid) {
                        extractSolidCatalog(inArchive, catalog, canonicalRoot)
                        return@catalogUse
                    }
                    val budget = catalog.extractionBudget
                    for (item in catalog.items) {
                        val index = item.index
                        val path = item.path
                        val isDirectory = item.isDirectory
                        val size = item.size
                        val packedSize = item.packedSize
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
                }
            } finally {
                inArchive.close()
            }
        } finally {
            input.close()
        }
    }
}

// Decode each solid block once while retaining the admitted catalog and byte budget
internal fun extractSolidCatalog(
    archive: IInArchive,
    catalog: RarCatalog,
    root: File,
) {
    val items = catalog.items.associateBy { it.index }
    val completed = mutableSetOf<Int>()
    var current: RarExtractionItem? = null
    var output: FileOutputStream? = null
    var entryBytes = 0L
    val callback =
        object : IArchiveExtractCallback {
            override fun setTotal(total: Long) = Unit

            override fun setCompleted(complete: Long) {
                if (Thread.currentThread().isInterrupted) throw IOException("Archive extraction interrupted")
            }

            override fun prepareOperation(mode: ExtractAskMode) = Unit

            override fun getStream(
                index: Int,
                mode: ExtractAskMode,
            ): ISequentialOutStream? {
                check(current == null && output == null) { "Native archive did not finish the previous entry" }
                if (mode != ExtractAskMode.EXTRACT) return null
                val item = items[index] ?: throw IOException("Native archive requested an unadmitted entry")
                check(index !in completed) { "Native archive repeated an entry" }
                current = item
                entryBytes = 0
                val destination = File(root, item.path.replace('/', File.separatorChar)).canonicalFile
                if (!destination.path.startsWith(
                        root.path + File.separator,
                    )
                ) {
                    throw IOException("Archive output escaped its root")
                }
                if (item.isDirectory) {
                    if (!destination.isDirectory &&
                        !destination.mkdirs()
                    ) {
                        throw IOException("Cannot create archive directory")
                    }
                    return null
                }
                destination.parentFile?.mkdirs()
                val stream = FileOutputStream(destination)
                output = stream
                return ISequentialOutStream { data ->
                    if (Thread.currentThread().isInterrupted) throw IOException("Archive extraction interrupted")
                    entryBytes += data.size
                    catalog.extractionBudget.accountActual(data.size, entryBytes, item.packedSize, item.path)
                    if (entryBytes >
                        item.size
                    ) {
                        throw IOException("Archive entry exceeds its declared size: ${item.path}")
                    }
                    if (entryBytes % (8L * 1024L * 1024L) < data.size) {
                        ImportStorageGuard.requireFreeSpace(root, 0, "extract ${item.path}")
                    }
                    stream.write(data)
                    data.size
                }
            }

            override fun setOperationResult(result: ExtractOperationResult) {
                output?.close()
                output = null
                val item = current
                current = null
                if (result !=
                    ExtractOperationResult.OK
                ) {
                    throw IOException("7-Zip extraction returned $result for ${item?.path}")
                }
                if (item != null) {
                    if (!item.isDirectory &&
                        entryBytes != item.size
                    ) {
                        throw IOException("Incomplete archive entry: ${item.path}")
                    }
                    completed.add(item.index)
                }
            }
        }
    try {
        archive.extract(
            catalog.items
                .map { it.index }
                .sorted()
                .toIntArray(),
            false,
            callback,
        )
        if (current != null ||
            completed.size != items.size
        ) {
            throw IOException("Native archive extraction did not complete its catalog")
        }
    } finally {
        output?.close()
    }
}

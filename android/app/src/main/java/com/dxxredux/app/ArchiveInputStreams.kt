package com.dxxredux.app

import java.io.File
import java.io.FileInputStream
import java.io.InputStream
import java.io.RandomAccessFile
import java.util.zip.ZipException
import java.util.zip.ZipInputStream

private const val ZIP_LOCAL_FILE_HEADER = 0x04034b50L
private const val ZIP_CENTRAL_FILE_HEADER = 0x02014b50L
private const val ZIP_END_OF_CENTRAL_DIRECTORY = 0x06054b50L
private const val ZIP_LOCAL_HEADER_BYTES = 30L
private const val ZIP_CENTRAL_HEADER_BYTES = 46L
private const val ZIP_EOCD_BYTES = 22
private const val ZIP_MAX_COMMENT_BYTES = 0xffff
private const val ZIP64_SENTINEL_16 = 0xffff
private const val ZIP64_SENTINEL_32 = 0xffffffffL
private const val ZIP_SUPPORTED_FLAGS = 0x080e

internal fun openZipInputStreamSkippingPreamble(
    input: InputStream,
    stagingDirectory: File? = null,
    maxSourceBytes: Long = ExtractionLimits.MAX_ZIP_PREAMBLE_BYTES,
): ZipInputStream {
    val staged = File.createTempFile("dxx-zip-", ".tmp", stagingDirectory)
    try {
        stageBoundedZip(input, staged, maxSourceBytes)
        val firstLocalHeader = validateZipAndFindFirstLocalHeader(staged)
        val stagedInput = FileInputStream(staged)
        return try {
            stagedInput.channel.position(firstLocalHeader)
            StagedZipInputStream(stagedInput, staged)
        } catch (e: Exception) {
            stagedInput.close()
            throw e
        }
    } catch (e: Exception) {
        staged.delete()
        throw e
    }
}

private fun stageBoundedZip(
    input: InputStream,
    staged: File,
    maxSourceBytes: Long,
) {
    // A one-shot stream cannot prove its trailing central directory before it is staged
    require(maxSourceBytes > 0L) { "ZIP source limit must be positive" }
    val buffer = ByteArray(64 * 1024)
    var total = 0L
    input.use { source ->
        staged.outputStream().buffered().use { output ->
            while (true) {
                val remaining = maxSourceBytes - total
                val count = source.read(buffer, 0, minOf(buffer.size.toLong(), remaining + 1).toInt())
                if (count < 0) break
                if (count == 0) continue
                if (count.toLong() > remaining) throw ZipException("ZIP source exceeds $maxSourceBytes bytes")
                output.write(buffer, 0, count)
                total += count
            }
        }
    }
}

private fun validateZipAndFindFirstLocalHeader(staged: File): Long =
    RandomAccessFile(staged, "r").use { archive ->
        val eocdOffset = findEocd(archive)
        archive.seek(eocdOffset + 4)
        val diskNumber = archive.readU16()
        val centralDisk = archive.readU16()
        val diskEntries = archive.readU16()
        val totalEntries = archive.readU16()
        val centralSize = archive.readU32()
        val centralOffset = archive.readU32()
        val commentLength = archive.readU16()
        if (eocdOffset + ZIP_EOCD_BYTES + commentLength != archive.length()) {
            throw ZipException("ZIP end record has an invalid comment length")
        }
        if (diskNumber != 0 || centralDisk != 0 || diskEntries != totalEntries) {
            throw ZipException("Multi-disk ZIP archives are not supported")
        }
        if (totalEntries == 0 || totalEntries == ZIP64_SENTINEL_16 ||
            centralSize == ZIP64_SENTINEL_32 || centralOffset == ZIP64_SENTINEL_32
        ) {
            throw ZipException("Empty and ZIP64 archives are not supported")
        }
        val centralPhysical = eocdOffset - centralSize
        val archiveBase = centralPhysical - centralOffset
        if (archiveBase < 0 || centralPhysical < 0 || centralPhysical > eocdOffset) {
            throw ZipException("ZIP central directory has invalid bounds")
        }

        var cursor = centralPhysical
        var firstLocalHeader = Long.MAX_VALUE
        repeat(totalEntries) {
            archive.seek(cursor)
            if (archive.readU32() != ZIP_CENTRAL_FILE_HEADER) throw ZipException("Invalid ZIP central directory")
            archive.skipBytes(4)
            val flags = archive.readU16()
            val method = archive.readU16()
            archive.skipBytes(8)
            val compressedSize = archive.readU32()
            archive.skipBytes(4)
            val nameLength = archive.readU16()
            val extraLength = archive.readU16()
            val entryCommentLength = archive.readU16()
            val entryDisk = archive.readU16()
            archive.skipBytes(6)
            val localOffset = archive.readU32()
            validateZipEntryFields(flags, method, nameLength, entryDisk, localOffset, compressedSize)
            val next = Math.addExact(cursor, ZIP_CENTRAL_HEADER_BYTES + nameLength + extraLength + entryCommentLength)
            if (next > eocdOffset) throw ZipException("ZIP central entry exceeds its directory")
            val centralName = ByteArray(nameLength)
            archive.seek(cursor + ZIP_CENTRAL_HEADER_BYTES)
            archive.readFully(centralName)

            val localPhysical = Math.addExact(archiveBase, localOffset)
            validateLocalHeader(
                archive,
                localPhysical,
                centralPhysical,
                flags,
                method,
                compressedSize,
                centralName,
            )
            firstLocalHeader = minOf(firstLocalHeader, localPhysical)
            cursor = next
        }
        if (cursor != eocdOffset) throw ZipException("ZIP central directory size does not match its entries")
        if (firstLocalHeader > ExtractionLimits.MAX_ZIP_PREAMBLE_BYTES) {
            throw ZipException("ZIP preamble exceeds ${ExtractionLimits.MAX_ZIP_PREAMBLE_BYTES} bytes")
        }
        firstLocalHeader
    }

private fun findEocd(archive: RandomAccessFile): Long {
    val length = archive.length()
    val firstCandidate = maxOf(0L, length - ZIP_EOCD_BYTES - ZIP_MAX_COMMENT_BYTES)
    var offset = length - ZIP_EOCD_BYTES
    while (offset >= firstCandidate) {
        archive.seek(offset)
        if (archive.readU32() == ZIP_END_OF_CENTRAL_DIRECTORY) {
            archive.seek(offset + 20)
            if (offset + ZIP_EOCD_BYTES + archive.readU16() == length) return offset
        }
        offset--
    }
    throw ZipException("ZIP end record not found")
}

private fun validateZipEntryFields(
    flags: Int,
    method: Int,
    nameLength: Int,
    diskNumber: Int,
    localOffset: Long,
    compressedSize: Long,
) {
    if (flags and ZIP_SUPPORTED_FLAGS.inv() != 0 || method !in setOf(0, 8)) {
        throw ZipException("ZIP entry uses unsupported flags or compression")
    }
    if (nameLength == 0 || diskNumber != 0 || localOffset == ZIP64_SENTINEL_32 ||
        compressedSize == ZIP64_SENTINEL_32
    ) {
        throw ZipException("ZIP entry has invalid metadata")
    }
}

private fun validateLocalHeader(
    archive: RandomAccessFile,
    offset: Long,
    centralPhysical: Long,
    expectedFlags: Int,
    expectedMethod: Int,
    compressedSize: Long,
    expectedName: ByteArray,
) {
    if (offset < 0 || offset > centralPhysical - ZIP_LOCAL_HEADER_BYTES) {
        throw ZipException("ZIP local header is outside the archive")
    }
    archive.seek(offset)
    if (archive.readU32() != ZIP_LOCAL_FILE_HEADER) throw ZipException("ZIP local header is invalid")
    archive.skipBytes(2)
    val flags = archive.readU16()
    val method = archive.readU16()
    archive.skipBytes(16)
    val nameLength = archive.readU16()
    val extraLength = archive.readU16()
    if (flags != expectedFlags || method != expectedMethod || nameLength != expectedName.size) {
        throw ZipException("ZIP local and central headers disagree")
    }
    val dataOffset = Math.addExact(offset, ZIP_LOCAL_HEADER_BYTES + nameLength + extraLength)
    if (dataOffset > centralPhysical || compressedSize > centralPhysical - dataOffset) {
        throw ZipException("ZIP local entry exceeds the archive")
    }
    val localName = ByteArray(nameLength)
    archive.readFully(localName)
    if (!localName.contentEquals(expectedName)) throw ZipException("ZIP local and central names disagree")
}

private fun RandomAccessFile.readU16(): Int {
    val low = read()
    val high = read()
    if (low < 0 || high < 0) throw ZipException("Truncated ZIP metadata")
    return low or (high shl 8)
}

private fun RandomAccessFile.readU32(): Long = readU16().toLong() or (readU16().toLong() shl 16)

private class StagedZipInputStream(
    input: FileInputStream,
    private val staged: File,
) : ZipInputStream(input) {
    override fun close() {
        try {
            super.close()
        } finally {
            staged.delete()
        }
    }
}

package com.dxxredux.app

import java.io.ByteArrayOutputStream
import java.io.File
import java.io.InputStream
import java.util.Locale
import java.util.zip.ZipFile
import java.util.zip.ZipInputStream

internal object GameFileMetadata {
    private const val MAX_ENTRY_BYTES = 64L * 1024L * 1024L
    private const val MAX_PIG_BYTES = 64L * 1024L * 1024L
    private const val MAX_POG_BYTES = 64L * 1024L * 1024L
    private const val MAX_EXAMPLES = 8
    private const val PIG_ID = "PPIG"
    private const val PIG_VERSION = 2
    private const val POG_ID = "DPOG"
    private const val POG_VERSION = 1
    private const val D1_BITMAP_HEADER_SIZE = 17
    private const val D2_BITMAP_HEADER_SIZE = 18
    private const val SOUND_HEADER_SIZE = 20
    private const val DBM_FLAG_ABM = 64
    private const val DBM_NUM_FRAMES = 63

    data class EntrySummary(
        val name: String,
        val sizeBytes: Long,
        val role: String,
    )

    data class CategorySummary(
        val label: String,
        val count: Int,
        val sizeBytes: Long,
    )

    data class Summary(
        val format: String,
        val scope: String,
        val game: String,
        val detailRows: List<Pair<String, String>>,
        val categories: List<CategorySummary>,
        val examples: List<EntrySummary>,
        val notes: List<String> = emptyList(),
        val problems: List<String> = emptyList(),
    )

    fun summarizeLocalFile(file: File): Summary? {
        if (!file.isFile) return null
        return when (GameFileFormats.extensionOf(file.name)) {
            "hog" -> file.inputStream().use { summarizeHog(file.name, file.length(), it) }
            "dxa" -> summarizeZipFile(file, "DXA", "Mod archive")
            "pig" -> summarizePig(file.name, file.length()) { file.inputStream() }
            "pog" -> summarizePog(file.name, file.length()) { file.inputStream() }
            else -> null
        }
    }

    fun summarizeZipConstituent(
        zipFile: File,
        path: String,
        displayName: String,
    ): Summary? {
        if (!zipFile.isFile) return null
        ZipFile(zipFile).use { zip ->
            val entry = zip.getEntry(path) ?: return null
            val size = entry.size.coerceAtLeast(0)
            return zip.getInputStream(entry).use { input ->
                when (GameFileFormats.extensionOf(displayName)) {
                    "hog" -> summarizeHog(displayName, size, input)
                    "dxa" -> summarizeZipStream(displayName, input, "DXA", "Mod archive")
                    "pig" -> summarizePig(displayName, size) { zip.getInputStream(entry) }
                    "pog" -> summarizePog(displayName, size) { zip.getInputStream(entry) }
                    "mn2", "msn" -> summarizeMissionDescriptor(displayName, input)
                    else -> null
                }
            }
        }
    }

    private fun summarizeMissionDescriptor(
        name: String,
        input: InputStream,
    ): Summary {
        val mission = GameFileFormats.parseMissionDescriptor(name, input.readBytes().toString(Charsets.UTF_8))
        val rows =
            buildList {
                add("Title" to mission.displayName)
                mission.type?.let { add("Mission type" to it) }
                mission.author?.let { add("Author" to it) }
                mission.editor?.let { add("Editor" to it) }
                add("Levels" to mission.levelNames.size.toString())
                if (mission.levelNames.isNotEmpty()) add("Level names" to mission.levelNames.joinToString(", "))
                if (mission.secretLevelNames.isNotEmpty()) {
                    add("Secret levels" to mission.secretLevelNames.size.toString())
                    add("Secret level names" to mission.secretLevelNames.joinToString(", "))
                }
                if (mission.assetReferences.isNotEmpty()) {
                    add(
                        "Referenced assets" to
                            mission.assetReferences.entries.joinToString(", ") { "${it.key}: ${it.value}" },
                    )
                }
            }
        return Summary(
            format = "Mission descriptor",
            scope = "Mission metadata",
            game = gameLabel(mission.game),
            detailRows = rows,
            categories = emptyList(),
            examples = emptyList(),
        )
    }

    private fun summarizeHog(
        name: String,
        sizeBytes: Long,
        input: InputStream,
    ): Summary {
        val entries = mutableListOf<EntrySummary>()
        val problems = mutableListOf<String>()
        var bytesRead = 0L
        val magic = input.readExact(3)
        bytesRead += magic.size
        if (magic.size != 3 || magic.toString(Charsets.US_ASCII) != "DHF") {
            return problemSummary("HOG", "Mission or game archive", "Unknown", "Invalid HOG magic")
        }
        while (sizeBytes <= 0 || bytesRead < sizeBytes) {
            val nameBytes = input.readExact(13)
            if (nameBytes.isEmpty()) break
            if (nameBytes.size != 13) {
                problems += "Truncated entry name"
                break
            }
            val lenBytes = input.readExact(4)
            if (lenBytes.size != 4) {
                problems += "Truncated entry size"
                break
            }
            bytesRead += 17
            val entrySize = leInt(lenBytes, 0).toLong() and 0xffff_ffffL
            val entryName = asciiName(nameBytes)
            if (entrySize > MAX_ENTRY_BYTES || (sizeBytes > 0 && bytesRead + entrySize > sizeBytes)) {
                problems += "Invalid size for $entryName"
                break
            }
            entries += EntrySummary(entryName, entrySize, roleForEntry(entryName))
            input.skipFully(entrySize)
            bytesRead += entrySize
        }
        val game = detectGame(name, entries)
        val rows =
            listOf(
                "Entries" to entries.size.toString(),
                "Embedded data" to entries.sumOf { it.sizeBytes }.toString(),
                "Contents" to roleRollup(entries),
            )
        return Summary(
            format = "HOG",
            scope =
                if (entries.any { GameFileFormats.isLevelFile(it.name) }) {
                    "Mission archive"
                } else {
                    "Game archive"
                },
            game = gameLabel(game),
            detailRows = rows,
            categories = categorySummaries(entries),
            examples = entries.take(MAX_EXAMPLES),
            notes = inferEntryNotes(entries) + cappedNote(entries.size, "entries"),
            problems = problems,
        )
    }

    private fun summarizeZipFile(
        file: File,
        format: String,
        scope: String,
    ): Summary =
        ZipFile(file).use { zip ->
            val entries =
                zip
                    .entries()
                    .asSequence()
                    .filter { !it.isDirectory }
                    .map {
                        val name = it.name.substringAfterLast('/')
                        EntrySummary(name, it.size.coerceAtLeast(0), roleForEntry(name))
                    }.toList()
            zipSummary(format, scope, file.name, entries)
        }

    private fun summarizeZipStream(
        name: String,
        input: InputStream,
        format: String,
        scope: String,
    ): Summary {
        val entries = mutableListOf<EntrySummary>()
        ZipInputStream(input).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                if (!entry.isDirectory) {
                    val entryName = entry.name.substringAfterLast('/')
                    entries += EntrySummary(entryName, entry.size.coerceAtLeast(0), roleForEntry(entryName))
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return zipSummary(format, scope, name, entries)
    }

    private fun zipSummary(
        format: String,
        scope: String,
        name: String,
        entries: List<EntrySummary>,
    ): Summary =
        Summary(
            format = format,
            scope = scope,
            game = gameLabel(detectGame(name, entries)),
            detailRows =
                listOf(
                    "Entries" to entries.size.toString(),
                    "Contents" to roleRollup(entries),
                ),
            categories = categorySummaries(entries),
            examples = entries.take(MAX_EXAMPLES),
            notes = inferEntryNotes(entries) + cappedNote(entries.size, "entries"),
        )

    private fun summarizePig(
        name: String,
        sizeBytes: Long,
        openInput: () -> InputStream,
    ): Summary? {
        if (sizeBytes > MAX_PIG_BYTES) {
            return problemSummary("PIG", "Texture and sound data", "Unknown", "PIG is too large to summarize")
        }
        val bytes = openInput().use { it.readBytesCapped(MAX_PIG_BYTES.toInt()) }
        return if (bytes.size >= 12 && bytes.copyOfRange(0, 4).toString(Charsets.US_ASCII) == PIG_ID) {
            summarizeD2Pig(bytes)
        } else {
            summarizeD1Pig(bytes)
        }?.copy(format = "PIG", scope = "Texture and sound data")
    }

    private fun summarizePog(
        name: String,
        sizeBytes: Long,
        openInput: () -> InputStream,
    ): Summary? {
        if (sizeBytes > MAX_POG_BYTES) {
            return problemSummary("POG", "Texture override pack", "D2", "POG is too large to summarize")
        }
        val bytes = openInput().use { it.readBytesCapped(MAX_POG_BYTES.toInt()) }
        if (bytes.size < 12 || bytes.copyOfRange(0, 4).toString(Charsets.US_ASCII) != POG_ID) {
            return problemSummary("POG", "Texture override pack", "D2", "Invalid POG magic")
        }
        val version = leInt(bytes, 4)
        if (version != POG_VERSION) {
            return problemSummary("POG", "Texture override pack", "D2", "Unsupported POG version $version")
        }
        val bitmapCount = leInt(bytes, 8)
        if (!validCount(bitmapCount)) {
            return problemSummary("POG", "Texture override pack", "D2", "Invalid bitmap count")
        }
        val indexStart = 12
        val headerStart = indexStart + bitmapCount * 2
        val dataStart = headerStart + bitmapCount * D2_BITMAP_HEADER_SIZE
        if (dataStart > bytes.size) {
            return problemSummary("POG", "Texture override pack", "D2", "Truncated POG headers")
        }
        val indices = (0 until bitmapCount).map { leShort(bytes, indexStart + it * 2) }
        val bitmaps = readBitmapHeaders(bytes, headerStart, bitmapCount, D2_BITMAP_HEADER_SIZE, true)
        val examples =
            bitmaps.take(MAX_EXAMPLES).mapIndexed { index, bitmap ->
                EntrySummary(
                    bitmap.name,
                    0,
                    "override ${indices[index]} (${bitmap.width}x${bitmap.height})",
                )
            }
        val rows =
            buildList {
                add("Overrides" to bitmapCount.toString())
                add("Texture data" to (bytes.size - dataStart).coerceAtLeast(0).toString())
                if (indices.isNotEmpty()) add("Override range" to "${indices.min()}-${indices.max()}")
            }
        return Summary(
            format = "POG",
            scope = "Texture override pack",
            game = "D2",
            detailRows = rows,
            categories = listOf(CategorySummary("Texture override", bitmapCount, 0)),
            examples = examples,
            notes = cappedNote(bitmapCount, "overrides"),
        )
    }

    private fun summarizeD2Pig(bytes: ByteArray): Summary? {
        val version = leInt(bytes, 4)
        if (version != PIG_VERSION) {
            return problemSummary("PIG", "Texture and sound data", "D2", "Unsupported D2 PIG version $version")
        }
        val bitmapCount = leInt(bytes, 8)
        if (!validCount(bitmapCount)) {
            return problemSummary("PIG", "Texture and sound data", "D2", "Invalid bitmap count")
        }
        val headerStart = 12
        val dataStart = headerStart + bitmapCount * D2_BITMAP_HEADER_SIZE
        if (dataStart > bytes.size) {
            return problemSummary("PIG", "Texture and sound data", "D2", "Truncated bitmap headers")
        }
        val bitmaps = readBitmapHeaders(bytes, headerStart, bitmapCount, D2_BITMAP_HEADER_SIZE, true)
        return pigSummary(
            "D2",
            bitmapCount,
            0,
            (bytes.size - dataStart).coerceAtLeast(0).toLong(),
            bitmaps,
            listOf("Version $version"),
        )
    }

    private fun summarizeD1Pig(bytes: ByteArray): Summary? {
        val offset = leInt(bytes, 0).takeIf { it >= 0 && it + 8 <= bytes.size && plausibleD1Counts(bytes, it) } ?: 0
        if (!plausibleD1Counts(bytes, offset)) {
            return problemSummary("PIG", "Texture and sound data", "D1", "Unsupported or truncated D1 PIG")
        }
        val bitmapCount = leInt(bytes, offset)
        val soundCount = leInt(bytes, offset + 4)
        val headerStart = offset + 8
        val bitmapHeaderBytes = bitmapCount * D1_BITMAP_HEADER_SIZE
        val soundHeaderBytes = soundCount * SOUND_HEADER_SIZE
        if (headerStart + bitmapHeaderBytes + soundHeaderBytes > bytes.size) {
            return problemSummary("PIG", "Texture and sound data", "D1", "Truncated D1 PIG headers")
        }
        val bitmaps = readBitmapHeaders(bytes, headerStart, bitmapCount, D1_BITMAP_HEADER_SIZE, false)
        val notes = if (offset > 0) listOf("Pig data starts at $offset") else emptyList()
        val dataStart = headerStart + bitmapHeaderBytes + soundHeaderBytes
        return pigSummary(
            "D1",
            bitmapCount,
            soundCount,
            (bytes.size - dataStart).coerceAtLeast(0).toLong(),
            bitmaps,
            notes,
        )
    }

    private data class BitmapHeader(
        val name: String,
        val width: Int,
        val height: Int,
        val flags: Int,
        val animatedFrames: Int,
    )

    private fun readBitmapHeaders(
        bytes: ByteArray,
        start: Int,
        count: Int,
        size: Int,
        d2: Boolean,
    ): List<BitmapHeader> =
        (0 until count).map { index ->
            val p = start + index * size
            val name = asciiName(bytes.copyOfRange(p, p + 8))
            val dflags = bytes[p + 8].toInt() and 0xff
            val widthBase = bytes[p + 9].toInt() and 0xff
            val heightBase = bytes[p + 10].toInt() and 0xff
            val whExtra = if (d2) bytes[p + 11].toInt() and 0xff else 0
            val flags = bytes[p + if (d2) 12 else 11].toInt() and 0xff
            val width =
                if (d2) {
                    widthBase + ((whExtra and 0x0f) shl 8)
                } else {
                    widthBase +
                        if (dflags and 128 != 0) 256 else 0
                }
            val height = if (d2) heightBase + ((whExtra and 0xf0) shl 4) else heightBase
            BitmapHeader(
                name = name,
                width = width,
                height = height,
                flags = flags,
                animatedFrames = if (dflags and DBM_FLAG_ABM != 0) dflags and DBM_NUM_FRAMES else 0,
            )
        }

    private fun pigSummary(
        game: String,
        bitmapCount: Int,
        soundCount: Int,
        dataBytes: Long,
        bitmaps: List<BitmapHeader>,
        notes: List<String>,
    ): Summary {
        val rle = bitmaps.count { it.flags and 8 != 0 }
        val transparent = bitmaps.count { it.flags and 1 != 0 }
        val superTransparent = bitmaps.count { it.flags and 2 != 0 }
        val noLighting = bitmaps.count { it.flags and 4 != 0 }
        val animated = bitmaps.count { it.animatedFrames > 0 }
        val rows =
            buildList {
                add("Bitmaps" to bitmapCount.toString())
                if (soundCount > 0) add("Sounds" to soundCount.toString())
                add("Data bytes" to dataBytes.toString())
                add("RLE bitmaps" to rle.toString())
                if (animated > 0) add("Animated groups" to animated.toString())
                if (transparent > 0) add("Transparent" to transparent.toString())
                if (superTransparent > 0) add("Super transparent" to superTransparent.toString())
                if (noLighting > 0) add("No lighting" to noLighting.toString())
            }
        val examples =
            bitmaps.take(MAX_EXAMPLES).map {
                EntrySummary(it.name, 0, "${it.width}x${it.height} bitmap")
            }
        return Summary(
            format = "PIG",
            scope = "Texture and sound data",
            game = game,
            detailRows = rows,
            categories =
                listOfNotNull(
                    CategorySummary("Bitmap", bitmapCount, 0),
                    if (soundCount > 0) CategorySummary("Sound", soundCount, 0) else null,
                ),
            examples = examples,
            notes = notes + cappedNote(bitmaps.size, "bitmaps"),
        )
    }

    private fun plausibleD1Counts(
        bytes: ByteArray,
        offset: Int,
    ): Boolean {
        if (offset < 0 || offset + 8 > bytes.size) return false
        val bitmapCount = leInt(bytes, offset)
        val soundCount = leInt(bytes, offset + 4)
        if (!validCount(bitmapCount) || !validCount(soundCount)) return false
        return offset + 8 + bitmapCount * D1_BITMAP_HEADER_SIZE + soundCount * SOUND_HEADER_SIZE <= bytes.size
    }

    private fun validCount(count: Int): Boolean = count in 0..20_000

    private fun categorySummaries(entries: List<EntrySummary>): List<CategorySummary> =
        entries
            .groupBy { it.role }
            .map { (role, group) -> CategorySummary(role, group.size, group.sumOf { it.sizeBytes }) }
            .sortedByDescending { it.count }

    private fun roleRollup(entries: List<EntrySummary>): String {
        if (entries.isEmpty()) return "No entries"
        return categorySummaries(entries)
            .take(5)
            .joinToString(", ") { "${it.count} ${it.label.lowercase(Locale.US)}" }
    }

    private fun roleForEntry(name: String): String = GameFileFormats.roleLabel(name)

    private fun detectGame(
        name: String,
        entries: List<EntrySummary>,
    ): String = GameFileFormats.gameHint(name, entries.map { it.name }, fallback = GameFileFormats.GAME_UNKNOWN)

    private fun gameLabel(game: String): String =
        when (game) {
            "d1" -> "D1"
            "d2" -> "D2"
            "both" -> "D1/D2"
            else -> "Unknown"
        }

    private fun inferEntryNotes(entries: List<EntrySummary>): List<String> =
        buildList {
            if (entries.any { GameFileFormats.extensionOf(it.name) == "ied" }) add("Includes Inferno editor data")
            if (entries.any {
                    GameFileFormats.extensionOf(
                        it.name,
                    ) == "txb"
                }
            ) {
                add("Includes encoded briefing or text data")
            }
        }

    private fun cappedNote(
        count: Int,
        label: String,
    ): List<String> =
        if (count > MAX_EXAMPLES) {
            listOf("Showing first $MAX_EXAMPLES of $count $label")
        } else {
            emptyList()
        }

    private fun problemSummary(
        format: String,
        scope: String,
        game: String,
        problem: String,
    ): Summary =
        Summary(
            format = format,
            scope = scope,
            game = game,
            detailRows = emptyList(),
            categories = emptyList(),
            examples = emptyList(),
            problems = listOf(problem),
        )

    private fun InputStream.readExact(count: Int): ByteArray {
        val out = ByteArray(count)
        var pos = 0
        while (pos < count) {
            val read = read(out, pos, count - pos)
            if (read <= 0) break
            pos += read
        }
        return if (pos == count) out else out.copyOf(pos)
    }

    private fun InputStream.skipFully(count: Long) {
        var remaining = count
        val buffer = ByteArray(8192)
        while (remaining > 0) {
            val read = read(buffer, 0, minOf(buffer.size.toLong(), remaining).toInt())
            if (read <= 0) break
            remaining -= read
        }
    }

    private fun InputStream.readBytesCapped(maxBytes: Int): ByteArray {
        val out = ByteArrayOutputStream()
        val buffer = ByteArray(8192)
        var total = 0
        while (true) {
            val read = read(buffer)
            if (read <= 0) break
            total += read
            if (total > maxBytes) break
            out.write(buffer, 0, read)
        }
        return out.toByteArray()
    }

    private fun asciiName(bytes: ByteArray): String {
        val end = bytes.indexOf(0).takeIf { it >= 0 } ?: bytes.size
        return bytes.copyOfRange(0, end).toString(Charsets.US_ASCII).trim()
    }

    private fun leInt(
        bytes: ByteArray,
        offset: Int,
    ): Int =
        (bytes[offset].toInt() and 0xff) or
            ((bytes[offset + 1].toInt() and 0xff) shl 8) or
            ((bytes[offset + 2].toInt() and 0xff) shl 16) or
            ((bytes[offset + 3].toInt() and 0xff) shl 24)

    private fun leShort(
        bytes: ByteArray,
        offset: Int,
    ): Int =
        (bytes[offset].toInt() and 0xff) or
            ((bytes[offset + 1].toInt() and 0xff) shl 8)
}

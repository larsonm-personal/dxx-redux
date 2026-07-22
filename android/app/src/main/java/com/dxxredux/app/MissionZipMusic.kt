package com.dxxredux.app

import java.io.ByteArrayInputStream
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.util.Locale
import java.util.zip.ZipInputStream

data class MissionZipMusicCatalog(
    val archivePath: String,
    val sources: List<MissionZipMusicSource>,
) {
    val hasListableTracks: Boolean get() = sources.any { it.tracks.isNotEmpty() }
}

data class MissionZipMusicSource(
    val id: String,
    val label: String,
    val containerPath: String,
    val tracks: List<MissionZipMusicTrack>,
)

data class MissionZipMusicTrack(
    val id: String,
    val displayName: String,
    val archiveEntryPath: String,
    val nestedEntryPath: String? = null,
    val hogEntryName: String? = null,
    val kind: String,
    val extension: String,
    val sizeBytes: Long,
    val playable: Boolean,
    val sourceFilePath: String? = null,
)

object MissionZipMusic {
    const val KIND_SONG_REFERENCE = "song_reference"
    const val KIND_MIDI = "midi"
    const val KIND_COMPRESSED_AUDIO = "compressed_audio"

    private val SONG_LIST_FILES = setOf("descent.sng", "dxx-r.sng")
    private val MIDI_EXTENSIONS = setOf("hmp", "hmq", "mid", "midi")
    private val COMPRESSED_AUDIO_EXTENSIONS = setOf("flac", "mp3", "ogg", "wav")
    private val PLAYABLE_EXTENSIONS = MIDI_EXTENSIONS + COMPRESSED_AUDIO_EXTENSIONS

    fun inspect(file: java.io.File): MissionZipMusicCatalog? {
        if (!file.isFile) return null
        val sources =
            runCatching {
                ArchiveFiles.open(file).use { archive ->
                    val metadataBudget = musicMetadataBudget()
                    buildList {
                        val archiveBuilder =
                            SourceBuilder(
                                id = "archive",
                                label = "Archive music",
                                containerPath = "",
                            )
                        for (entry in archive.entries) {
                            if (entry.isDirectory) continue
                            val normalized = normalizePath(entry.path)
                            when (extensionOf(normalized)) {
                                "sng" -> {
                                    archive.openInputStream(entry).use { input ->
                                        archiveBuilder.addSongList(
                                            normalized,
                                            input
                                                .readBytesBounded(
                                                    ExtractionLimits.MAX_DESCRIPTOR_BYTES,
                                                    normalized,
                                                    metadataBudget,
                                                    entry.compressedSizeBytes,
                                                ).toString(Charsets.UTF_8),
                                        )
                                    }
                                }

                                in PLAYABLE_EXTENSIONS -> {
                                    archiveBuilder.addPlayable(
                                        archiveEntryPath = normalized,
                                        nestedEntryPath = null,
                                        hogEntryName = null,
                                        name = leafName(normalized),
                                        sizeBytes = entry.sizeBytes,
                                    )
                                }

                                "dxa" -> {
                                    archive.openInputStream(entry).use { input ->
                                        scanDxa(normalized, input)?.let { add(it) }
                                    }
                                }

                                "hog" -> {
                                    archive.openInputStream(entry).use { input ->
                                        scanHog(normalized, input)?.let { add(it) }
                                    }
                                }
                            }
                        }
                        archiveBuilder.build()?.let { add(0, it) }
                    }
                }
            }.getOrNull()
                ?: return null
        return MissionZipMusicCatalog(file.absolutePath, sources).takeIf { it.hasListableTracks }
    }

    internal fun inspectExtracted(record: MissionZipExtractionRecord): MissionZipMusicCatalog? {
        if (!record.rootDir.isDirectory) return null
        val sources =
            buildList {
                val entryBudget = ExtractionBudget()
                val metadataBudget = musicMetadataBudget()
                val archiveBuilder =
                    SourceBuilder(
                        id = "archive",
                        label = "Archive music",
                        containerPath = "",
                    )
                for (file in record.files) {
                    if (file.entryPath.isBlank()) continue
                    val relativePath = normalizePath(file.relativePath)
                    val diskFile = File(record.rootDir, relativePath.replace('/', File.separatorChar))
                    if (!diskFile.isFile) continue
                    entryBudget.registerEntry(file.sizeBytes, file.sizeBytes, relativePath)
                    when (extensionOf(relativePath)) {
                        "sng" -> {
                            archiveBuilder.addSongList(
                                relativePath,
                                diskFile.inputStream().use {
                                    it
                                        .readBytesBounded(
                                            ExtractionLimits.MAX_DESCRIPTOR_BYTES,
                                            relativePath,
                                            metadataBudget,
                                            file.sizeBytes,
                                        ).toString(Charsets.UTF_8)
                                },
                            )
                        }

                        in PLAYABLE_EXTENSIONS -> {
                            archiveBuilder.addPlayable(
                                archiveEntryPath = relativePath,
                                nestedEntryPath = null,
                                hogEntryName = null,
                                name = leafName(relativePath),
                                sizeBytes = file.sizeBytes,
                                sourceFilePath = diskFile.absolutePath,
                            )
                        }

                        "dxa" -> {
                            diskFile.inputStream().use { input ->
                                scanDxa(relativePath, input, sourceFilePath = diskFile.absolutePath)?.let { add(it) }
                            }
                        }

                        "hog" -> {
                            diskFile.inputStream().use { input ->
                                scanHog(relativePath, input, sourceFilePath = diskFile.absolutePath)?.let { add(it) }
                            }
                        }
                    }
                }
                archiveBuilder.build()?.let { add(0, it) }
            }
        return MissionZipMusicCatalog(record.rootDir.absolutePath, sources).takeIf { it.hasListableTracks }
    }

    private fun scanDxa(
        archiveEntryPath: String,
        input: InputStream,
        sourceFilePath: String? = null,
    ): MissionZipMusicSource? {
        val entryBudget = ExtractionBudget()
        val metadataBudget = musicMetadataBudget()
        val builder =
            SourceBuilder(
                id = "dxa:$archiveEntryPath",
                label = "${leafName(archiveEntryPath)} music",
                containerPath = archiveEntryPath,
            )
        openZipInputStreamSkippingPreamble(input).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                entryBudget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory) {
                    val nestedPath = normalizePath(entry.name)
                    when (extensionOf(nestedPath)) {
                        "sng" -> {
                            builder.addSongList(
                                archiveEntryPath,
                                zip
                                    .readBytesBounded(
                                        ExtractionLimits.MAX_DESCRIPTOR_BYTES,
                                        nestedPath,
                                        metadataBudget,
                                        entry.compressedSize,
                                    ).toString(Charsets.UTF_8),
                            )
                        }

                        in PLAYABLE_EXTENSIONS -> {
                            builder.addPlayable(
                                archiveEntryPath = archiveEntryPath,
                                nestedEntryPath = nestedPath,
                                hogEntryName = null,
                                name = leafName(nestedPath),
                                sizeBytes = entry.size.coerceAtLeast(0),
                                sourceFilePath = sourceFilePath,
                            )
                        }

                        "hog" -> {
                            val nestedHogBytes =
                                zip.readBytesBounded(
                                    ExtractionLimits.MAX_ENTRY_BYTES,
                                    nestedPath,
                                    entryBudget,
                                    entry.compressedSize,
                                )
                            scanHog(
                                archiveEntryPath = archiveEntryPath,
                                input = ByteArrayInputStream(nestedHogBytes),
                                nestedEntryPath = nestedPath,
                                sourceFilePath = sourceFilePath,
                            )?.tracks?.forEach(builder::addTrack)
                        }
                    }
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return builder.build()
    }

    private fun scanHog(
        archiveEntryPath: String,
        input: InputStream,
        nestedEntryPath: String? = null,
        sourceFilePath: String? = null,
    ): MissionZipMusicSource? {
        val containerPath = nestedEntryPath ?: archiveEntryPath
        val builder =
            SourceBuilder(
                id = "hog:$archiveEntryPath:${nestedEntryPath.orEmpty()}",
                label = "${leafName(containerPath)} music",
                containerPath = containerPath,
            )
        if (input.readNBytesCompat(3).toString(Charsets.US_ASCII) != "DHF") return null
        val entryBudget = ExtractionBudget()
        val metadataBudget = musicMetadataBudget()
        while (true) {
            val nameBytes = input.readNBytesCompat(13)
            if (nameBytes.isEmpty() || nameBytes.size != 13) break
            val lenBytes = input.readNBytesCompat(4)
            if (lenBytes.size != 4) break
            val entryName = hogEntryName(nameBytes)
            val size = leInt(lenBytes).toLong() and 0xffff_ffffL
            entryBudget.registerEntry(size, size, entryName)
            val ext = extensionOf(entryName)
            if (leafName(entryName).lowercase(Locale.US) in SONG_LIST_FILES) {
                if (size > ExtractionLimits.MAX_DESCRIPTOR_BYTES) {
                    throw IOException("$entryName exceeds ${ExtractionLimits.MAX_DESCRIPTOR_BYTES} bytes")
                }
                val bytes =
                    input.readBytesBoundedExact(
                        size,
                        entryName,
                        metadataBudget,
                    )
                builder.addSongList(archiveEntryPath, bytes.toString(Charsets.UTF_8))
            } else {
                if (ext in PLAYABLE_EXTENSIONS) {
                    builder.addPlayable(
                        archiveEntryPath = archiveEntryPath,
                        nestedEntryPath = nestedEntryPath,
                        hogEntryName = entryName,
                        name = entryName,
                        sizeBytes = size,
                        sourceFilePath = sourceFilePath,
                    )
                }
                input.skipFullyCompat(size)
            }
        }
        return builder.build()
    }

    private class SourceBuilder(
        private val id: String,
        private val label: String,
        private val containerPath: String,
    ) {
        private data class SongReference(
            val name: String,
            val archiveEntryPath: String,
            val index: Int,
        )

        private val songReferences = mutableListOf<SongReference>()
        private val playableTracks = mutableListOf<MissionZipMusicTrack>()

        fun addSongList(
            archiveEntryPath: String,
            text: String,
        ) {
            parseSongListReferences(text).forEachIndexed { index, name ->
                if (songReferences.size >= ExtractionLimits.MAX_ENTRIES) {
                    throw IOException("Song lists exceed ${ExtractionLimits.MAX_ENTRIES} references")
                }
                songReferences += SongReference(name, archiveEntryPath, index)
            }
        }

        fun addPlayable(
            archiveEntryPath: String,
            nestedEntryPath: String?,
            hogEntryName: String?,
            name: String,
            sizeBytes: Long,
            sourceFilePath: String? = null,
        ) {
            val ext = extensionOf(name)
            playableTracks +=
                MissionZipMusicTrack(
                    id =
                        listOf(id, archiveEntryPath, nestedEntryPath.orEmpty(), hogEntryName.orEmpty(), name)
                            .joinToString(":"),
                    displayName = name,
                    archiveEntryPath = archiveEntryPath,
                    nestedEntryPath = nestedEntryPath,
                    hogEntryName = hogEntryName,
                    kind = if (ext in MIDI_EXTENSIONS) KIND_MIDI else KIND_COMPRESSED_AUDIO,
                    extension = ext,
                    sizeBytes = sizeBytes,
                    playable = true,
                    sourceFilePath = sourceFilePath,
                )
        }

        fun addTrack(track: MissionZipMusicTrack) {
            if (track.kind == KIND_SONG_REFERENCE) {
                songReferences += SongReference(track.displayName, track.archiveEntryPath, songReferences.size)
            } else {
                playableTracks += track
            }
        }

        fun build(): MissionZipMusicSource? =
            buildDisplayTracks().takeIf { it.isNotEmpty() }?.let {
                MissionZipMusicSource(id, label, containerPath, it)
            }

        private fun buildDisplayTracks(): List<MissionZipMusicTrack> {
            if (songReferences.isEmpty()) return playableTracks
            val byName =
                playableTracks
                    .groupBy { it.displayName.lowercase(Locale.US) }
                    .mapValues { (_, tracks) -> ArrayDeque(tracks) }
            val byLeafName =
                playableTracks
                    .groupBy { leafName(it.displayName).lowercase(Locale.US) }
                    .mapValues { (_, tracks) -> ArrayDeque(tracks) }
            val usedTrackIds = mutableSetOf<String>()
            val usedNames = mutableSetOf<String>()
            val displayTracks = mutableListOf<MissionZipMusicTrack>()

            songReferences.forEach { reference ->
                val nameKey = reference.name.lowercase(Locale.US)
                if (!usedNames.add(nameKey)) return@forEach
                val playable =
                    byName[nameKey].removeFirstUnused(usedTrackIds)
                        ?: byLeafName[leafName(reference.name).lowercase(Locale.US)].removeFirstUnused(usedTrackIds)
                if (playable != null) {
                    displayTracks += playable
                    usedTrackIds += playable.id
                } else {
                    displayTracks +=
                        MissionZipMusicTrack(
                            id = "$id:sng:${reference.index}:$nameKey",
                            displayName = reference.name,
                            archiveEntryPath = reference.archiveEntryPath,
                            kind = KIND_SONG_REFERENCE,
                            extension = extensionOf(reference.name),
                            sizeBytes = 0,
                            playable = false,
                        )
                }
            }

            playableTracks.forEach { track ->
                if (track.id !in usedTrackIds && usedNames.add(track.displayName.lowercase(Locale.US))) {
                    displayTracks += track
                }
            }
            return displayTracks
        }

        private fun ArrayDeque<MissionZipMusicTrack>?.removeFirstUnused(
            usedTrackIds: Set<String>,
        ): MissionZipMusicTrack? {
            if (this == null) return null
            while (isNotEmpty()) {
                val track = removeFirst()
                if (track.id !in usedTrackIds) return track
            }
            return null
        }
    }

    private fun parseSongListReferences(text: String): List<String> =
        text
            .lineSequence()
            .map {
                it
                    .trim()
                    .substringBefore(' ')
                    .substringBefore('\t')
                    .trim()
            }.filter { it.isNotBlank() && !it.startsWith(";") && !it.startsWith("#") }
            .toList()

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

    private fun InputStream.readBytesBoundedExact(
        count: Long,
        label: String,
        budget: ExtractionBudget,
    ): ByteArray {
        if (count > Int.MAX_VALUE) throw IOException("$label exceeds the in-memory read limit")
        val bytes = readNBytesCompat(count.toInt())
        if (bytes.size.toLong() != count) throw IOException("Unexpected end of $label")
        budget.accountActual(bytes.size, bytes.size.toLong(), count, label)
        return bytes
    }

    private fun musicMetadataBudget() =
        ExtractionBudget(
            maxEntryBytes = ExtractionLimits.MAX_DESCRIPTOR_BYTES,
            maxTotalBytes = ExtractionLimits.MAX_METADATA_BYTES,
        )

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

    private fun extensionOf(path: String): String = path.substringAfterLast('.', "").lowercase(Locale.US)

    private fun leafName(path: String): String = path.substringAfterLast('/').substringAfterLast('\\')

    private fun normalizePath(path: String): String = path.replace('\\', '/').trim('/')
}

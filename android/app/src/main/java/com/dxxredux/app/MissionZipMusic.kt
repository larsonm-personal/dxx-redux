package com.dxxredux.app

import java.io.ByteArrayInputStream
import java.io.InputStream
import java.util.Locale
import java.util.zip.ZipFile
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
                ZipFile(file).use { zip ->
                    buildList {
                        val archiveBuilder =
                            SourceBuilder(
                                id = "archive",
                                label = "Archive music",
                                containerPath = "",
                            )
                        val entries = zip.entries()
                        while (entries.hasMoreElements()) {
                            val entry = entries.nextElement()
                            if (entry.isDirectory) continue
                            val normalized = normalizePath(entry.name)
                            when (extensionOf(normalized)) {
                                "sng" -> {
                                    zip.getInputStream(entry).use { input ->
                                        archiveBuilder.addSongList(
                                            normalized,
                                            input.readBytes().toString(Charsets.UTF_8),
                                        )
                                    }
                                }

                                in PLAYABLE_EXTENSIONS -> {
                                    archiveBuilder.addPlayable(
                                        archiveEntryPath = normalized,
                                        nestedEntryPath = null,
                                        hogEntryName = null,
                                        name = leafName(normalized),
                                        sizeBytes = entry.size.coerceAtLeast(0),
                                    )
                                }

                                "dxa" -> {
                                    zip.getInputStream(entry).use { input ->
                                        scanDxa(normalized, input)?.let { add(it) }
                                    }
                                }

                                "hog" -> {
                                    zip.getInputStream(entry).use { input ->
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

    private fun scanDxa(
        archiveEntryPath: String,
        input: InputStream,
    ): MissionZipMusicSource? {
        val builder =
            SourceBuilder(
                id = "dxa:$archiveEntryPath",
                label = "${leafName(archiveEntryPath)} music",
                containerPath = archiveEntryPath,
            )
        openZipInputStreamSkippingPreamble(input).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                if (!entry.isDirectory) {
                    val nestedPath = normalizePath(entry.name)
                    when (extensionOf(nestedPath)) {
                        "sng" -> {
                            builder.addSongList(archiveEntryPath, zip.readBytes().toString(Charsets.UTF_8))
                        }

                        in PLAYABLE_EXTENSIONS -> {
                            builder.addPlayable(
                                archiveEntryPath = archiveEntryPath,
                                nestedEntryPath = nestedPath,
                                hogEntryName = null,
                                name = leafName(nestedPath),
                                sizeBytes = entry.size.coerceAtLeast(0),
                            )
                        }

                        "hog" -> {
                            val nestedHogBytes = zip.readBytes()
                            scanHog(
                                archiveEntryPath = archiveEntryPath,
                                input = ByteArrayInputStream(nestedHogBytes),
                                nestedEntryPath = nestedPath,
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
    ): MissionZipMusicSource? {
        val containerPath = nestedEntryPath ?: archiveEntryPath
        val builder =
            SourceBuilder(
                id = "hog:$archiveEntryPath:${nestedEntryPath.orEmpty()}",
                label = "${leafName(containerPath)} music",
                containerPath = containerPath,
            )
        if (input.readNBytesCompat(3).toString(Charsets.US_ASCII) != "DHF") return null
        while (true) {
            val nameBytes = input.readNBytesCompat(13)
            if (nameBytes.isEmpty() || nameBytes.size != 13) break
            val lenBytes = input.readNBytesCompat(4)
            if (lenBytes.size != 4) break
            val entryName = hogEntryName(nameBytes)
            val size = leInt(lenBytes).toLong() and 0xffff_ffffL
            val ext = extensionOf(entryName)
            if (leafName(entryName).lowercase(Locale.US) in SONG_LIST_FILES) {
                val bytes = input.readNBytesCompat(size.coerceAtMost(Int.MAX_VALUE.toLong()).toInt())
                builder.addSongList(archiveEntryPath, bytes.toString(Charsets.UTF_8))
            } else {
                if (ext in PLAYABLE_EXTENSIONS) {
                    builder.addPlayable(
                        archiveEntryPath = archiveEntryPath,
                        nestedEntryPath = nestedEntryPath,
                        hogEntryName = entryName,
                        name = entryName,
                        sizeBytes = size,
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
        private val tracks = mutableListOf<MissionZipMusicTrack>()

        fun addSongList(
            archiveEntryPath: String,
            text: String,
        ) {
            parseSongListReferences(text).forEachIndexed { index, name ->
                tracks +=
                    MissionZipMusicTrack(
                        id = "$id:sng:$index:${name.lowercase(Locale.US)}",
                        displayName = name,
                        archiveEntryPath = archiveEntryPath,
                        kind = KIND_SONG_REFERENCE,
                        extension = extensionOf(name),
                        sizeBytes = 0,
                        playable = false,
                    )
            }
        }

        fun addPlayable(
            archiveEntryPath: String,
            nestedEntryPath: String?,
            hogEntryName: String?,
            name: String,
            sizeBytes: Long,
        ) {
            val ext = extensionOf(name)
            tracks +=
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
                )
        }

        fun addTrack(track: MissionZipMusicTrack) {
            tracks += track
        }

        fun build(): MissionZipMusicSource? =
            tracks.takeIf { it.isNotEmpty() }?.let {
                MissionZipMusicSource(id, label, containerPath, it)
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

package com.dxxredux.app

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import java.io.File

@Serializable
data class AudioTagProperty(
    val key: String = "",
    val values: List<String> = emptyList(),
)

@Serializable
data class AudioTagMetadata(
    val parse_status: String = "invalid",
    val format: String = "",
    val title: String = "",
    val composer: String = "",
    val artist: String = "",
    val album_artist: String = "",
    val album: String = "",
    val date: String = "",
    val genre: String = "",
    val comment: String = "",
    val copyright: String = "",
    val track_number: String = "",
    val disc_number: String = "",
    val display_name: String = "",
    val metadata_truncated: Boolean = false,
    val properties: List<AudioTagProperty> = emptyList(),
)

object AudioTagMetadataBridge {
    private val json = Json { ignoreUnknownKeys = true }

    init {
        System.loadLibrary("dxx-redux-d2")
    }

    fun parsePath(
        file: File,
        extension: String,
    ): AudioTagMetadata? =
        runCatching {
            json.decodeFromString<AudioTagMetadata>(nativeParsePath(file.absolutePath, extension))
        }.getOrNull()

    @JvmStatic
    private external fun nativeParsePath(
        path: String,
        extension: String,
    ): String
}

internal fun audioTagMetadataPrintout(metadata: AudioTagMetadata): List<String> =
    buildList {
        if (metadata.title.isNotBlank()) add("Title: ${metadata.title}")
        if (metadata.composer.isNotBlank()) add("Composer: ${metadata.composer}")
        if (metadata.artist.isNotBlank()) add("Artist: ${metadata.artist}")
        if (metadata.album_artist.isNotBlank()) add("Album artist: ${metadata.album_artist}")
        if (metadata.album.isNotBlank()) add("Album: ${metadata.album}")
        if (metadata.track_number.isNotBlank()) add("Track: ${metadata.track_number}")
        if (metadata.disc_number.isNotBlank()) add("Disc: ${metadata.disc_number}")
        if (metadata.date.isNotBlank()) add("Date: ${metadata.date}")
        if (metadata.genre.isNotBlank()) add("Genre: ${metadata.genre}")
        if (metadata.copyright.isNotBlank()) add("Copyright: ${metadata.copyright}")
        if (metadata.comment.isNotBlank()) add("Comment: ${metadata.comment}")
        if (metadata.properties.isNotEmpty()) {
            add("Raw tags:")
            metadata.properties.forEach { property ->
                add("${property.key}: ${property.values.joinToString("; ")}")
            }
        }
        if (metadata.metadata_truncated) add("Metadata output was truncated.")
        if (metadata.parse_status == "no_tags") add("No embedded textual metadata.")
    }

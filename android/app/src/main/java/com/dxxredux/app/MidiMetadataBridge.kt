package com.dxxredux.app

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json

@Serializable
data class MidiTextEvent(
    val track_index: Int = 0,
    val type: String = "",
    val text: String = "",
)

@Serializable
data class MidiMetadata(
    val parse_status: String = "invalid",
    val smf_format: Int = 0,
    val track_count: Int = 0,
    val time_division: Int = 0,
    val title: String = "",
    val composer: String = "",
    val display_name: String = "",
    val metadata_source_filename: String = "",
    val inherited_from_midi: Boolean = false,
    val metadata_truncated: Boolean = false,
    val text_events: List<MidiTextEvent> = emptyList(),
)

object MidiMetadataBridge {
    private val json = Json { ignoreUnknownKeys = true }

    init {
        System.loadLibrary("dxx-redux-d2")
    }

    fun parse(
        bytes: ByteArray,
        isHmp: Boolean,
        sourceFilename: String,
        inheritedFromMidi: Boolean = false,
    ): MidiMetadata? =
        synchronized(MidiEnumerationBridge.nativeDataLock) {
            runCatching {
                json.decodeFromString<MidiMetadata>(
                    nativeParse(bytes, isHmp, sourceFilename, inheritedFromMidi),
                )
            }.getOrNull()
        }

    @JvmStatic
    private external fun nativeParse(
        bytes: ByteArray,
        isHmp: Boolean,
        sourceFilename: String,
        inheritedFromMidi: Boolean,
    ): String
}

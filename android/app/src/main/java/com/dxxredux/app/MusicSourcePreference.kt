package com.dxxredux.app

internal const val PREF_MIDI_EDITOR_SOURCE = "midi_editor_source"
internal const val DEFAULT_MIDI_EDITOR_SOURCE = "d1-builtin"

// Persist the native source ID, not its list position or device-specific HOG path
internal fun preferredMidiEditorSource(
    sources: List<MidiEnumerationBridge.SourceInfo>,
    preferredId: String?,
): MidiEnumerationBridge.SourceInfo? =
    sources.firstOrNull { it.id == preferredId }
        ?: sources.firstOrNull { it.id == DEFAULT_MIDI_EDITOR_SOURCE }
        ?: sources.firstOrNull()

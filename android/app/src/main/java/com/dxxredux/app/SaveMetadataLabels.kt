package com.dxxredux.app

// Keep these values in sync with MUSIC_TYPE_* in d1/main/digi.h and d2/main/digi.h
internal const val MUSIC_TYPE_NONE = 0
internal const val MUSIC_TYPE_BUILTIN = 1
internal const val MUSIC_TYPE_REDBOOK = 2
internal const val MUSIC_TYPE_CUSTOM = 3

internal fun saveMetadataMusicTypeLabel(musicType: Int): String =
    when (musicType) {
        MUSIC_TYPE_NONE -> "None"
        MUSIC_TYPE_BUILTIN -> "Base game MIDI"
        MUSIC_TYPE_REDBOOK -> "CD"
        MUSIC_TYPE_CUSTOM -> "Files"
        else -> "Unknown"
    }

// Keep these names in sync with save_kind_name in jni_resume_save.cpp
internal fun saveMetadataKindLabel(saveKind: String): String =
    when (saveKind) {
        "manual" -> "Manual save"
        "auto_minimize" -> "Auto-save on minimize"
        "auto_exit" -> "Auto-save on exit"
        "auto_progress" -> "Highest progress save"
        "auto_abort" -> "Abort save"
        "auto_periodic" -> "Periodic auto-save"
        else -> "Unknown save kind"
    }

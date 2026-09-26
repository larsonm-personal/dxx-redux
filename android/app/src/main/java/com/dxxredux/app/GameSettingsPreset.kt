package com.dxxredux.app

internal data class PresetSettingPreview(
    val label: String,
    val enabled: Boolean? = null,
    val value: String = "",
)

// Presets apply ordinary editable values, never enforce a persistent mode
internal enum class GameSettingsPreset(
    val title: String,
    val helpersEnabled: Boolean,
) {
    ORIGINAL("Original Descent", false),
    DEFAULTS("Restore Defaults", true),
    ;

    val description: String
        get() =
            when (this) {
                ORIGINAL -> "turns off most android port enhancements to get closer to the original game"
                DEFAULTS -> "android port defaults. includes most enhancements"
            }

    val originalHoming: Boolean get() = true

    val guidebotRoutingMode: Int
        get() =
            when (this) {
                ORIGINAL -> GuidebotRoutingMode.ORIGINAL
                DEFAULTS -> GuidebotRoutingMode.ENHANCED
            }

    val mainViewFov: Int get() = 0 // Native base projection (90 degrees)

    val skipIntroMovie: Boolean get() = false

    val textureFilter: Int get() = 0
    val hudFiltering: Boolean get() = helpersEnabled
    val rewindEnabled: Boolean get() = helpersEnabled
    val serverCoopQol: Boolean get() = helpersEnabled

    fun resetMidiPreferences(
        store: SoundfontStore,
        activate: (String, Boolean) -> Boolean,
    ) {
        store.resetPreferences(activate)
    }

    val settings: List<PresetSettingPreview>
        get() =
            buildList {
                add(PresetSettingPreview("Robot / hostage / secret counts", helpersEnabled))
                add(PresetSettingPreview("Map cheat buttons", helpersEnabled))
                add(PresetSettingPreview("Boss health bar", helpersEnabled))
                add(PresetSettingPreview("In-game FOV", value = "90 deg (Base)"))
                add(PresetSettingPreview("Guidebot helper line", helpersEnabled))
                add(PresetSettingPreview("Persist guidebot goal message", helpersEnabled))
                add(
                    PresetSettingPreview(
                        "Guidebot routing (new games)",
                        value = if (guidebotRoutingMode == GuidebotRoutingMode.ORIGINAL) "Original" else "Enhanced",
                    ),
                )
                add(PresetSettingPreview("New-server Coop QoL\n(teammate arrows, Guidebot, warp)", serverCoopQol))
                add(PresetSettingPreview("Rewind support and overlay controls", rewindEnabled))
                add(PresetSettingPreview("Texture filtering", value = "Nearest"))
                add(PresetSettingPreview("HUD filtering", hudFiltering))
                add(PresetSettingPreview("Skip intro movie on launch", skipIntroMovie))
                add(PresetSettingPreview("Autoselect Only Once", false))
                add(PresetSettingPreview("Original homing (Single/Coop)", originalHoming))
                add(PresetSettingPreview("MIDI renderer", value = "AdLib (OPL3) FM"))
                add(PresetSettingPreview("MIDI soundfont fallback", value = "Bundled"))
                add(PresetSettingPreview("MIDI reverb and chorus", true))
                if (this@GameSettingsPreset == DEFAULTS) {
                    add(PresetSettingPreview("HUD size", value = "Cockpit"))
                    add(PresetSettingPreview("Auto-level", true))
                }
            }
}

package com.dxxredux.app

// Presets stage ordinary editable values, never enforce a persistent mode
internal enum class GameSettingsPreset(
    val title: String,
    val helpersEnabled: Boolean,
) {
    ORIGINAL("Original Descent", false),
    DEFAULTS("Restore Defaults", true),
    ;

    val mainViewFov: Int get() = 0 // Native base projection (90 degrees)

    val skipIntroMovie: Boolean get() = false

    val textureFilter: Int get() = 0
    val hudFiltering: Boolean get() = helpersEnabled
    val rewindEnabled: Boolean get() = helpersEnabled
    val serverCoopQol: Boolean get() = helpersEnabled

    val confirmationLines: List<String>
        get() {
            val state = if (helpersEnabled) "On" else "Off"
            return buildList {
                add("Robot / hostage / secret counts: $state")
                add("Map cheat buttons: $state")
                add("Boss health bar: $state")
                add("In-game FOV: 90 degrees (Base)")
                add("Guidebot helper line: $state")
                add("New-server Coop QoL (teammate arrows, Guidebot, warp): $state")
                add("Rewind support and overlay controls: $state")
                add("Texture filtering: None (nearest)")
                add("HUD filtering: $state")
                add("Skip intro movie on launch: Off")
                if (this@GameSettingsPreset == DEFAULTS) {
                    add("HUD size: Cockpit")
                    add("Auto-level: On")
                    add("Original homing (Single/Coop): Off")
                }
            }
        }
}

package com.dxxredux.app

import java.util.Locale

/** Navigation identities are independent of labels and persisted preference names */
enum class LauncherDestination {
    HOME,
    SETTINGS,
    CONTROLLER,
    CONTROLLER_MAPPING,
    TOUCH_CONTROLS,
    TOUCH_EDITOR,
    GAME_PREFERENCES,
    AUTOSELECT,
    GRAPHICS,
    MUSIC,
    FILES_CONTENT,
    ADVANCED,
    ABOUT,
    MULTIPLAYER,
}

object SettingsSections {
    const val CONTROLLER_SLOTS = "controller.slots"
    const val CONTROLLER_PRESETS = "controller.presets"
    const val CONTROLLER_RAW = "controller.raw"
    const val CONTROLLER_RESPONSE = "controller.response"
    const val CONTROLLER_EXPORT = "controller.export"
    const val CONTROLLER_IMPORT = "controller.import"
    const val TOUCH_SLOTS = "touch.slots"
    const val TOUCH_PRESETS = "touch.presets"
    const val TOUCH_SETTINGS = "touch.settings"
    const val TOUCH_GYRO = "touch.gyro"
    const val TOUCH_ADD = "touch.add"
    const val TOUCH_EXPORT = "touch.export"
    const val TOUCH_IMPORT = "touch.import"
    const val GAME_PRESETS = "game.presets"
    const val GAME_LAUNCHER = "game.launcher"
    const val GAME_PILOT = "game.pilot"
    const val GAME_INTRO = "game.intro"
    const val GAME_GAMEPLAY = "game.gameplay"
    const val GAME_SCREENSAVER = "game.screensaver"
    const val GAME_GUIDEBOT = "game.guidebot"
    const val GAME_HELPERS = "game.helpers"

    const val FILES_SETS = "files.sets"
    const val FILES_INVENTORY = "files.inventory"
    const val FILES_MODS = "files.mods"
    const val FILES_CLEANUP = "files.cleanup"
    const val TOUCH_ENABLED = "touch.enabled"
    const val GRAPHICS_CHOOSER = "graphics.chooser"
    const val GRAPHICS_SELECTIVE_FILTER = "graphics.selective_filter"
    const val GRAPHICS_ORIENTATION = "graphics.orientation"
    const val GRAPHICS_FOV = "graphics.fov"
    const val GRAPHICS_RESOLUTION = "graphics.resolution"
    const val GRAPHICS_FILTERING = "graphics.filtering"
    const val GRAPHICS_COLOR_DEPTH = "graphics.color_depth"
    const val GRAPHICS_MSAA = "graphics.msaa"
    const val GRAPHICS_AF = "graphics.af"
    const val GRAPHICS_CORNERS = "graphics.corners"
    const val GRAPHICS_ORIGINAL = "graphics.original"
    const val GRAPHICS_DEBUG = "graphics.debug"
    const val MUSIC_MIDI = "music.midi"
    const val MUSIC_CD = "music.cd"
    const val MUSIC_FILES = "music.files"
    const val ADVANCED_CONFIG = "advanced.config"
    const val ADVANCED_RESET = "advanced.reset"
    const val ADVANCED_METADATA = "advanced.metadata"
    const val ADVANCED_ROUTES = "advanced.routes"
    const val ADVANCED_CHROMAPRINT = "advanced.chromaprint"
    const val ADVANCED_LOGGING = "advanced.logging"
    const val ADVANCED_CRASHES = "advanced.crashes"
    const val ADVANCED_DEMOS = "advanced.demos"
    const val ADVANCED_STORAGE = "advanced.storage"
    const val ADVANCED_LOCATION = "advanced.location"
}

/** A result describes an entry point only; it never contains a setting mutation or action */
data class SettingsSearchEntry(
    val id: String,
    val title: String,
    val path: String,
    val aliases: List<String>,
    val destination: LauncherDestination,
    val section: String? = null,
)

object SettingsCatalog {
    val categories =
        listOf(
            category(LauncherDestination.CONTROLLER, "Controller", "gamepad", "joystick"),
            category(LauncherDestination.TOUCH_CONTROLS, "Touch controls", "overlay", "touchscreen"),
            category(LauncherDestination.GAME_PREFERENCES, "Game preferences", "gameplay", "pilot"),
            category(LauncherDestination.AUTOSELECT, "Weapon autoselect", "weapon order", "auto select"),
            category(LauncherDestination.GRAPHICS, "Graphics & display", "video", "resolution"),
            category(LauncherDestination.MUSIC, "Music", "sound", "audio"),
            category(LauncherDestination.FILES_CONTENT, "Files & content", "mods", "levels", "missions"),
            category(LauncherDestination.ADVANCED, "Advanced", "debug", "configuration"),
            category(LauncherDestination.ABOUT, "About", "version", "build", "updates"),
        )

    private fun category(
        destination: LauncherDestination,
        title: String,
        vararg aliases: String,
    ) = SettingsSearchEntry(destination.name.lowercase(Locale.ROOT), title, "Settings", aliases.toList(), destination)

    private fun setting(
        destination: LauncherDestination,
        section: String?,
        id: String,
        title: String,
        vararg aliases: String,
    ): SettingsSearchEntry {
        val categoryDestination =
            when (destination) {
                LauncherDestination.CONTROLLER_MAPPING -> LauncherDestination.CONTROLLER
                LauncherDestination.TOUCH_EDITOR -> LauncherDestination.TOUCH_CONTROLS
                else -> destination
            }
        val path = categories.first { it.destination == categoryDestination }.title
        return SettingsSearchEntry(id, title, path, aliases.toList(), destination, section)
    }

    val entries: List<SettingsSearchEntry> =
        categories +
            buildList {
                fun add(
                    destination: LauncherDestination,
                    section: String?,
                    id: String,
                    title: String,
                    vararg aliases: String,
                ) {
                    add(setting(destination, section, id, title, *aliases))
                }
                with(LauncherDestination.CONTROLLER_MAPPING) {
                    add(
                        this,
                        null,
                        "controller.mapping",
                        "Controller mapping",
                        "bindings",
                        "gamepad mapping",
                        "assign buttons",
                    )
                    add(this, SettingsSections.CONTROLLER_SLOTS, "controller.slots", "Controller slots", "profiles")
                    add(this, SettingsSections.CONTROLLER_PRESETS, "controller.presets", "Controller presets")
                    add(
                        this,
                        SettingsSections.CONTROLLER_RAW,
                        "controller.raw",
                        "Raw controller inputs",
                        "test gamepad",
                        "raw inputs",
                    )
                    add(
                        this,
                        SettingsSections.CONTROLLER_RESPONSE,
                        "controller.response",
                        "Controller response",
                        "sensitivity",
                        "deadzone",
                        "axis",
                    )
                    add(this, SettingsSections.CONTROLLER_EXPORT, "controller.export", "Export controller mapping")
                    add(this, SettingsSections.CONTROLLER_IMPORT, "controller.import", "Import controller mapping")
                }
                add(
                    LauncherDestination.TOUCH_CONTROLS,
                    null,
                    "touch.enabled",
                    "Enable touch controls",
                    "overlay",
                    "touchscreen",
                )
                with(LauncherDestination.TOUCH_EDITOR) {
                    add(this, null, "touch.editor", "Edit touch layout")
                    add(this, SettingsSections.TOUCH_SLOTS, "touch.slots", "Touch slots", "profiles")
                    add(this, SettingsSections.TOUCH_PRESETS, "touch.presets", "Touch presets")
                    add(
                        this,
                        SettingsSections.TOUCH_SETTINGS,
                        "touch.settings",
                        "Touch global settings",
                        "hide assigned controls",
                    )
                    add(
                        this,
                        SettingsSections.TOUCH_GYRO,
                        "touch.gyro",
                        "Gyro settings",
                        "tilt",
                        "gyroscope",
                        "activation mode",
                        "response mode",
                        "axis bindings",
                    )
                    add(
                        this,
                        SettingsSections.TOUCH_ADD,
                        "touch.add",
                        "Add touch control",
                        "button",
                        "analog stick",
                        "slider",
                        "multi selector",
                        "axis region",
                    )
                    add(this, SettingsSections.TOUCH_EXPORT, "touch.export", "Export touch layout")
                    add(this, SettingsSections.TOUCH_IMPORT, "touch.import", "Import touch layout")
                }
                with(LauncherDestination.GAME_PREFERENCES) {
                    add(
                        this,
                        SettingsSections.GAME_PRESETS,
                        "game.presets",
                        "Game presets",
                        "original Descent",
                        "restore defaults",
                    )
                    add(
                        this,
                        SettingsSections.GAME_LAUNCHER,
                        "game.demo_offer",
                        "Show D1/D2 demo installer offer on launch",
                    )
                    add(this, SettingsSections.GAME_PILOT, "game.hud", "HUD size")
                    add(this, SettingsSections.GAME_PILOT, "game.autolevel", "Auto-level")
                    add(this, SettingsSections.GAME_PILOT, "game.homing", "Original homing (Single/Coop)")
                    add(this, SettingsSections.GAME_PILOT, "game.counts", "Robot / hostage / secret counts")
                    add(this, SettingsSections.GAME_PILOT, "game.boss", "Boss health bar")
                    add(this, SettingsSections.GAME_PILOT, "game.map", "Map cheats accessible")
                    add(this, SettingsSections.GAME_PILOT, "game.coop", "New-server Coop QoL", "cooperative defaults")
                    add(this, SettingsSections.GAME_INTRO, "game.intro", "Skip intro movie on launch")
                    add(this, SettingsSections.GAME_INTRO, "game.progress", "Show progress bars", "loading")
                    add(this, SettingsSections.GAME_GAMEPLAY, "game.headlight", "Headlight off by default")
                    add(this, SettingsSections.GAME_GAMEPLAY, "game.rewind", "Enable rewind support")
                    add(this, SettingsSections.GAME_GAMEPLAY, "game.rewind_amount", "Rewind amount", "rewind seconds")
                    add(this, SettingsSections.GAME_SCREENSAVER, "game.screensaver", "Screen saver after inactivity")
                    add(this, SettingsSections.GAME_GUIDEBOT, "game.guidebot", "Guidebot routing")
                    add(this, SettingsSections.GAME_GUIDEBOT, "game.guidebot_message", "Persist guidebot goal message")
                    add(this, SettingsSections.GAME_HELPERS, "game.guidebot_line", "Guidebot helper line")
                    add(this, SettingsSections.GAME_HELPERS, "game.player_line", "Nearest-player line")
                }
                with(LauncherDestination.GRAPHICS) {
                    add(
                        this,
                        SettingsSections.GRAPHICS_ORIENTATION,
                        "graphics.orientation",
                        "In-game orientation",
                        "landscape",
                        "portrait",
                    )
                    add(this, SettingsSections.GRAPHICS_FOV, "graphics.fov", "In-game FOV", "field of view")
                    add(this, SettingsSections.GRAPHICS_RESOLUTION, "graphics.resolution", "Render resolution")
                    add(
                        this,
                        SettingsSections.GRAPHICS_FILTERING,
                        "graphics.filtering",
                        "Texture filtering",
                        "texture filters",
                    )
                    add(this, SettingsSections.GRAPHICS_COLOR_DEPTH, "graphics.color_depth", "Color depth")
                    add(
                        this,
                        SettingsSections.GRAPHICS_MSAA,
                        "graphics.msaa",
                        "Anti-aliasing (MSAA)",
                        "antialiasing",
                        "multisample",
                    )
                    add(this, SettingsSections.GRAPHICS_AF, "graphics.af", "Anisotropic filtering (AF)")
                    add(
                        this,
                        SettingsSections.GRAPHICS_CHOOSER,
                        "graphics.first_boot",
                        "Show first boot graphics chooser",
                    )
                    add(
                        this,
                        SettingsSections.GRAPHICS_CORNERS,
                        "graphics.corners",
                        "Display corners",
                        "rounded corners",
                        "text inset",
                    )
                    add(this, SettingsSections.GRAPHICS_ORIGINAL, "graphics.depth", "Classic depth ordering")
                    add(this, SettingsSections.GRAPHICS_ORIGINAL, "graphics.movie", "Movie filter")
                    add(this, SettingsSections.GRAPHICS_ORIGINAL, "graphics.transparency", "Transparency effects")
                    add(this, SettingsSections.GRAPHICS_ORIGINAL, "graphics.light", "Colored dynamic light")
                    add(
                        this,
                        SettingsSections.GRAPHICS_DEBUG,
                        "graphics.debug",
                        "Graphics debug options",
                        "merged wall logging",
                        "video info",
                    )
                }
                add(
                    LauncherDestination.GRAPHICS,
                    SettingsSections.GRAPHICS_SELECTIVE_FILTER,
                    "graphics.menu_filters",
                    "Menus / briefings / videos / text / reticle filters",
                    "reticle",
                    "menu filters",
                )
                add(
                    LauncherDestination.GRAPHICS,
                    SettingsSections.GRAPHICS_SELECTIVE_FILTER,
                    "graphics.hud_filters",
                    "Ship HUD / gauges filters",
                    "gauges",
                    "HUD filters",
                )
                add(
                    LauncherDestination.MUSIC,
                    SettingsSections.MUSIC_MIDI,
                    "music.midi",
                    "MIDI music",
                    "soundfont",
                    "MIDI preview",
                )
                add(
                    LauncherDestination.MUSIC,
                    SettingsSections.MUSIC_CD,
                    "music.cd",
                    "CD Audio",
                    "redbook",
                    "disc",
                    "cue",
                    "bin",
                )
                add(
                    LauncherDestination.MUSIC,
                    SettingsSections.MUSIC_FILES,
                    "music.files",
                    "Audio Files",
                    "audio sources",
                    "music sets",
                    "MP3",
                    "OGG",
                    "FLAC",
                )
                with(LauncherDestination.FILES_CONTENT) {
                    add(this, SettingsSections.FILES_SETS, "files.sets", "Active file set", "file set management")
                    add(
                        this,
                        "files.inventory",
                        "files.inventory",
                        "Game file inventories",
                        "required files",
                        "optional downloads",
                        "Descent 1",
                        "Descent 2",
                    )
                    add(
                        this,
                        "files.mods",
                        "files.mods",
                        "Mods and levels",
                        "missions",
                        "recommended mods",
                        "demos",
                        "package documents",
                        "metadata",
                        "previews",
                    )
                    add(
                        this,
                        SettingsSections.FILES_CLEANUP,
                        "files.cleanup",
                        "File cleanup",
                        "stale files",
                        "import status",
                    )
                }
                with(LauncherDestination.ADVANCED) {
                    add(
                        this,
                        SettingsSections.ADVANCED_CONFIG,
                        "advanced.export",
                        "Export all configs",
                        "config export",
                        "backup",
                    )
                    add(this, SettingsSections.ADVANCED_CONFIG, "advanced.import", "Import config")
                    add(
                        this,
                        SettingsSections.ADVANCED_METADATA,
                        "advanced.metadata",
                        "Online metadata",
                        "AcoustID web lookups",
                        "permission",
                    )
                    add(
                        this,
                        SettingsSections.ADVANCED_ROUTES,
                        "advanced.routes",
                        "Route analysis",
                        "route metadata",
                        "foreground compute",
                        "clear route analysis cache",
                    )
                    add(this, SettingsSections.ADVANCED_CHROMAPRINT, "advanced.chromaprint", "Chromaprint hashing")
                    add(
                        this,
                        SettingsSections.ADVANCED_LOGGING,
                        "advanced.logging",
                        "Debug logging categories",
                        "log files",
                        "tap feedback",
                        "automatic slowdown capture",
                    )
                    add(this, SettingsSections.ADVANCED_CRASHES, "advanced.crashes", "Crash reports")
                    add(
                        this,
                        SettingsSections.ADVANCED_DEMOS,
                        "advanced.demos",
                        "Newly-recorded demos",
                        "record a demo for each level",
                        "record per frame state",
                    )
                    add(
                        this,
                        SettingsSections.ADVANCED_STORAGE,
                        "advanced.storage",
                        "Storage inspector",
                        "app storage files",
                        "SAF links",
                    )
                    add(this, SettingsSections.ADVANCED_LOCATION, "advanced.location", "Imported files location")
                    add(this, SettingsSections.ADVANCED_RESET, "advanced.reset_controls", "Reset all controls")
                    add(this, SettingsSections.ADVANCED_RESET, "advanced.clear_data", "Clear all game data")
                    add(
                        this,
                        SettingsSections.ADVANCED_RESET,
                        "advanced.delete_players",
                        "Delete all player files",
                        "pilots",
                    )
                    add(this, SettingsSections.ADVANCED_RESET, "advanced.restart", "Restart app")
                }
            }

    fun search(query: String): List<SettingsSearchEntry> {
        val normalized = normalize(query)
        if (normalized.isEmpty()) return categories
        val tokens = normalized.split(' ')
        return entries
            .mapIndexedNotNull { index, entry ->
                val title = normalize(entry.title)
                val aliases = entry.aliases.map(::normalize)
                val corpus = "$title ${aliases.joinToString(" ")} ${normalize(entry.path)}"
                val rank =
                    when {
                        title == normalized -> 0
                        title.startsWith(normalized) -> 1
                        tokens.all { token -> title.split(' ').any { it.startsWith(token) } } -> 2
                        aliases.any { it == normalized } -> 3
                        tokens.all { it in corpus } -> 4
                        else -> return@mapIndexedNotNull null
                    }
                Triple(rank, index, entry)
            }.sortedWith(compareBy({ it.first }, { it.second }))
            .map { it.third }
    }

    private fun normalize(value: String) = value.lowercase(Locale.ROOT).replace(Regex("[^\\p{L}\\p{N}]+"), " ").trim()
}

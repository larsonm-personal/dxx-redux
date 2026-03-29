package com.dxxredux.app

/**
 * Shared constants for touch control bindings.
 * Button indices mirror kc_joystick[] in d2/main/kconfig.c — keep in sync.
 * Axis indices mirror the JNI nativeJoystickAxis() mapping in android_input.c.
 */
object TouchBindings {
    // --- Joystick button indices (kc_joystick[] positions) ---
    const val BTN_FIRE_PRIMARY = 0
    const val BTN_FIRE_SECONDARY = 1
    const val BTN_ACCELERATE = 2
    const val BTN_REVERSE = 3
    const val BTN_FIRE_FLARE = 4
    const val BTN_SLIDE_ON = 5
    const val BTN_SLIDE_LEFT = 6
    const val BTN_SLIDE_RIGHT = 7
    const val BTN_SLIDE_UP = 8
    const val BTN_SLIDE_DOWN = 9
    const val BTN_BANK_ON = 10
    const val BTN_BANK_LEFT = 11
    const val BTN_BANK_RIGHT = 12
    const val BTN_REAR_VIEW = 25
    const val BTN_DROP_BOMB = 26
    const val BTN_AFTERBURNER = 27
    const val BTN_CYCLE_PRIMARY = 28
    const val BTN_CYCLE_SECONDARY = 29
    const val BTN_HEADLIGHT = 30
    const val BTN_AUTOMAP = 50
    const val BTN_ENERGY_SHIELD = 52
    const val BTN_TOGGLE_BOMB = 54

    /** Buttons that only exist in D2 (D1 has no afterburner, headlight, etc.). */
    val D2_ONLY_BUTTONS = setOf(BTN_AFTERBURNER, BTN_HEADLIGHT, BTN_ENERGY_SHIELD, BTN_TOGGLE_BOMB)

    /** String labels for D2-only standard button functions (controller config uses labels). */
    val D2_ONLY_BUTTON_LABELS = setOf("Afterburner", "Headlight", "Energy\u2192Shield", "Toggle Bomb")

    /** String labels for D2-only meta actions (controller config uses labels). */
    val D2_ONLY_META_LABELS: Set<String> by lazy {
        D2_ONLY_META_ACTIONS.mapNotNull { META_BUTTON_LABELS[it] }.toSet()
    }

    /**
     * Base offset for mixer-dispatched button IDs sent to JNI.
     * Mixer sends button = MIXER_BTN_BASE + kc_joystick_index.
     * Avoids collisions with C-generated axis-button SDL indices (10-21)
     * and D-pad button indices (22-25).  Pilot file stores matching values
     * so the C kconfig handler matches: kc_joystick[i].value == btn.
     */
    const val MIXER_BTN_BASE = 100

    // --- Virtual (overlay-only) bindings -- not sent as joystick buttons ---
    const val BTN_CHEATS_MENU = 100
    const val BTN_GYRO_RECENTER = 101

    /** All button bindings with readable labels, for UI pickers. */
    val BUTTON_LABELS =
        mapOf(
            BTN_FIRE_PRIMARY to "Fire Primary",
            BTN_FIRE_SECONDARY to "Fire Secondary",
            BTN_ACCELERATE to "Accelerate",
            BTN_REVERSE to "Reverse",
            BTN_FIRE_FLARE to "Fire Flare",
            BTN_SLIDE_ON to "Slide On",
            BTN_SLIDE_LEFT to "Slide Left",
            BTN_SLIDE_RIGHT to "Slide Right",
            BTN_SLIDE_UP to "Slide Up",
            BTN_SLIDE_DOWN to "Slide Down",
            BTN_BANK_ON to "Bank On",
            BTN_BANK_LEFT to "Bank Left",
            BTN_BANK_RIGHT to "Bank Right",
            BTN_REAR_VIEW to "Rear View",
            BTN_DROP_BOMB to "Drop Bomb",
            BTN_AFTERBURNER to "Afterburner",
            BTN_CYCLE_PRIMARY to "Cycle Primary",
            BTN_CYCLE_SECONDARY to "Cycle Secondary",
            BTN_HEADLIGHT to "Headlight",
            BTN_AUTOMAP to "Automap",
            BTN_ENERGY_SHIELD to "Energy->Shield",
            BTN_TOGGLE_BOMB to "Toggle Bomb",
            BTN_CHEATS_MENU to "Cheats Menu",
            BTN_GYRO_RECENTER to "Gyro Recenter",
        )

    // --- Meta actions (extra controls) ---
    // These are dispatched via C-side key injection (android_meta_actions.c),
    // not through kc_joystick[]. IDs start at META_ACTION_OFFSET to avoid
    // collisions.  Keep in sync with android_meta_actions.h.
    const val META_ACTION_OFFSET = 1000

    const val META_QUICK_SAVE = 1000
    const val META_QUICK_LOAD = 1001
    const val META_GAME_MENU = 1002
    const val META_GUIDE_BOT_MENU = 1003
    const val META_GUIDE_FIND_ENERGY = 1004
    const val META_GUIDE_FIND_REACTOR = 1005
    const val META_GUIDE_FIND_SHIELD = 1006
    const val META_GUIDE_FIND_POWERUP = 1007
    const val META_GUIDE_FIND_ROBOT = 1008
    const val META_GUIDE_FIND_HOSTAGE = 1009
    const val META_GUIDE_SCRAM = 1010
    const val META_GUIDE_FIND_ITEMS = 1011
    const val META_GUIDE_FIND_EXIT = 1012
    const val META_GUIDE_CLEAR_GOAL = 1013
    const val META_MULTIPLAYER_HUD = 1014
    const val META_DROP_FLAG = 1015
    const val META_DROP_MARKER = 1016
    const val META_WEAPON_1 = 1020
    const val META_WEAPON_2 = 1021
    const val META_WEAPON_3 = 1022
    const val META_WEAPON_4 = 1023
    const val META_WEAPON_5 = 1024
    const val META_WEAPON_6 = 1025
    const val META_WEAPON_7 = 1026
    const val META_WEAPON_8 = 1027
    const val META_WEAPON_9 = 1028
    const val META_WEAPON_10 = 1029
    const val META_PAUSE = 1030
    const val META_RETURN_TO_LAUNCHER = 1031

    /** Labels for meta actions, used in Extra button pickers. */
    val META_BUTTON_LABELS: Map<Int, String> =
        linkedMapOf(
            META_QUICK_SAVE to "Quick Save",
            META_QUICK_LOAD to "Quick Load",
            META_GAME_MENU to "Game Menu (ESC)",
            META_GUIDE_BOT_MENU to "Guide Bot Menu",
            META_GUIDE_FIND_ENERGY to "GB: Find Energy",
            META_GUIDE_FIND_REACTOR to "GB: Find Reactor",
            META_GUIDE_FIND_SHIELD to "GB: Find Shield",
            META_GUIDE_FIND_POWERUP to "GB: Find Powerup",
            META_GUIDE_FIND_ROBOT to "GB: Find Robot",
            META_GUIDE_FIND_HOSTAGE to "GB: Find Hostage",
            META_GUIDE_SCRAM to "GB: Scram",
            META_GUIDE_FIND_ITEMS to "GB: Find Items",
            META_GUIDE_FIND_EXIT to "GB: Find Exit",
            META_GUIDE_CLEAR_GOAL to "GB: Clear Goal",
            META_MULTIPLAYER_HUD to "Multiplayer HUD",
            META_DROP_FLAG to "Drop Flag",
            META_DROP_MARKER to "Drop Marker",
            META_WEAPON_1 to "Weapon 1",
            META_WEAPON_2 to "Weapon 2",
            META_WEAPON_3 to "Weapon 3",
            META_WEAPON_4 to "Weapon 4",
            META_WEAPON_5 to "Weapon 5",
            META_WEAPON_6 to "Weapon 6",
            META_WEAPON_7 to "Weapon 7",
            META_WEAPON_8 to "Weapon 8",
            META_WEAPON_9 to "Weapon 9",
            META_WEAPON_10 to "Weapon 10",
            META_PAUSE to "Pause",
            META_RETURN_TO_LAUNCHER to "Exit to Launcher",
        )

    /** Meta actions that only exist in D2 (guide bot, markers, CTF flag). */
    val D2_ONLY_META_ACTIONS =
        setOf(
            META_GUIDE_BOT_MENU,
            META_GUIDE_FIND_ENERGY,
            META_GUIDE_FIND_REACTOR,
            META_GUIDE_FIND_SHIELD,
            META_GUIDE_FIND_POWERUP,
            META_GUIDE_FIND_ROBOT,
            META_GUIDE_FIND_HOSTAGE,
            META_GUIDE_SCRAM,
            META_GUIDE_FIND_ITEMS,
            META_GUIDE_FIND_EXIT,
            META_GUIDE_CLEAR_GOAL,
            META_DROP_FLAG,
            META_DROP_MARKER,
        )

    // --- Radial menu preset IDs and segment templates ---
    val RADIAL_PRESET_IDS = listOf("PriWpn", "SecWpn", "Guide")

    /** Segment templates for creating preset radial menus. Keyed by preset ID. */
    val RADIAL_PRESET_SEGMENTS: Map<String, List<RadialSegment>> =
        mapOf(
            "PriWpn" to
                listOf(
                    RadialSegment("Laser", android.view.KeyEvent.KEYCODE_1, weaponIndex = 0),
                    RadialSegment("Vulcan", android.view.KeyEvent.KEYCODE_2, weaponIndex = 1),
                    RadialSegment("Spread", android.view.KeyEvent.KEYCODE_3, weaponIndex = 2),
                    RadialSegment("Plasma", android.view.KeyEvent.KEYCODE_4, weaponIndex = 3),
                    RadialSegment("Fusion", android.view.KeyEvent.KEYCODE_5, weaponIndex = 4),
                ),
            "SecWpn" to
                listOf(
                    RadialSegment("Concsn", android.view.KeyEvent.KEYCODE_6, weaponIndex = 0),
                    RadialSegment("Homing", android.view.KeyEvent.KEYCODE_7, weaponIndex = 1),
                    RadialSegment("Proxim", android.view.KeyEvent.KEYCODE_8, weaponIndex = 2),
                    RadialSegment("Smart", android.view.KeyEvent.KEYCODE_9, weaponIndex = 3),
                    RadialSegment("Mega", android.view.KeyEvent.KEYCODE_0, weaponIndex = 4),
                ),
            "Guide" to
                listOf(
                    RadialSegment("Energy", META_GUIDE_FIND_ENERGY),
                    RadialSegment("Enrg Ctr", META_GUIDE_FIND_REACTOR),
                    RadialSegment("Shield", META_GUIDE_FIND_SHIELD),
                    RadialSegment("Powerup", META_GUIDE_FIND_POWERUP),
                    RadialSegment("Robot", META_GUIDE_FIND_ROBOT),
                    RadialSegment("Hostage", META_GUIDE_FIND_HOSTAGE),
                    RadialSegment("Scram!", META_GUIDE_SCRAM),
                    RadialSegment("Items", META_GUIDE_FIND_ITEMS),
                    RadialSegment("Exit", META_GUIDE_FIND_EXIT),
                ),
        )

    /** Center binding for preset radial menus (-1 = none). */
    val RADIAL_PRESET_CENTER: Map<String, Pair<String, Int>> =
        mapOf(
            "Guide" to ("Clear" to META_GUIDE_CLEAR_GOAL),
        )

    /** Human-readable labels for preset IDs, for the editor UI. */
    val RADIAL_PRESET_LABELS: Map<String, String> =
        mapOf(
            "PriWpn" to "Primary Weapons",
            "SecWpn" to "Secondary Weapons",
            "Guide" to "Guide Bot",
        )

    /** Combined labels map for pickers that show all bindings (standard + extra). */
    val ALL_BUTTON_LABELS: Map<Int, String> = BUTTON_LABELS + META_BUTTON_LABELS

    /** Check if a binding ID is a meta action (dispatched via NativeMetaActions). */
    fun isMetaAction(binding: Int): Boolean = binding >= META_ACTION_OFFSET

    /** Reverse lookup: label string to meta action ID, or -1. */
    fun metaActionIdForLabel(label: String): Int =
        META_BUTTON_LABELS.entries.firstOrNull { it.value == label }?.key ?: -1

    // --- JNI axis indices (nativeJoystickAxis axis parameter) ---
    const val AXIS_LEFT_X = 0 // Left stick horizontal
    const val AXIS_LEFT_Y = 1 // Left stick vertical
    const val AXIS_RIGHT_X = 2 // Right stick horizontal
    const val AXIS_RIGHT_Y = 3 // Right stick vertical
    const val AXIS_LTRIGGER = 4
    const val AXIS_RTRIGGER = 5
    const val AXIS_BANK = 6 // Virtual axis: Bank L/R (gyro roll mode)
    const val AXIS_SLIDE_UD = 7 // Virtual axis: Slide U/D (gyro roll mode)

    /** Axis labels for UI pickers — show game function (default mapping). */
    val AXIS_LABELS =
        mapOf(
            AXIS_LEFT_X to "Slide L/R",
            AXIS_LEFT_Y to "Fwd/Back",
            AXIS_RIGHT_X to "Turn L/R",
            AXIS_RIGHT_Y to "Pitch U/D",
            AXIS_LTRIGGER to "L Trigger",
            AXIS_RTRIGGER to "R Trigger",
            AXIS_BANK to "Bank L/R",
            AXIS_SLIDE_UD to "Slide U/D",
        )

    // --- Human-readable axis names for config export/import ---
    // These are stable identifiers (not the game-function labels which change
    // per mapping). Used in exported/bundled JSON files.
    val AXIS_NAMES =
        mapOf(
            AXIS_LEFT_X to "Left Stick X",
            AXIS_LEFT_Y to "Left Stick Y",
            AXIS_RIGHT_X to "Right Stick X",
            AXIS_RIGHT_Y to "Right Stick Y",
            AXIS_LTRIGGER to "L Trigger",
            AXIS_RTRIGGER to "R Trigger",
            AXIS_BANK to "Bank",
            AXIS_SLIDE_UD to "Slide UD",
        )

    private val AXIS_NAMES_REVERSE: Map<String, Int> by lazy {
        AXIS_NAMES.entries.associate { (k, v) -> v to k }
    }

    private val BUTTON_LABELS_REVERSE: Map<String, Int> by lazy {
        BUTTON_LABELS.entries.associate { (k, v) -> v to k }
    }

    private val META_LABELS_REVERSE: Map<String, Int> by lazy {
        META_BUTTON_LABELS.entries.associate { (k, v) -> v to k }
    }

    /** Convert a binding integer to a human-readable name.
     *  Covers standard buttons, meta actions, and Android keycodes. */
    fun bindingToName(id: Int): String {
        BUTTON_LABELS[id]?.let { return it }
        META_BUTTON_LABELS[id]?.let { return "Meta: $it" }
        // Android KeyEvent keycodes (used in radial menus)
        return keycodeToName(id) ?: "binding_$id"
    }

    /** Convert a human-readable binding name back to its integer ID.
     *  Returns null if the name is not recognized. */
    fun nameToBinding(name: String): Int? {
        BUTTON_LABELS_REVERSE[name]?.let { return it }
        if (name.startsWith("Meta: ")) {
            META_LABELS_REVERSE[name.removePrefix("Meta: ")]?.let { return it }
        }
        // Also accept meta labels without the prefix
        META_LABELS_REVERSE[name]?.let { return it }
        nameToKeycode(name)?.let { return it }
        // Fallback: binding_N format
        if (name.startsWith("binding_")) {
            name.removePrefix("binding_").toIntOrNull()?.let { return it }
        }
        return null
    }

    /** Convert an axis integer to a stable human-readable name. */
    fun axisToName(id: Int): String = AXIS_NAMES[id] ?: "axis_$id"

    /** Convert a human-readable axis name back to its integer ID.
     *  Returns null if the name is not recognized. */
    fun nameToAxis(name: String): Int? {
        AXIS_NAMES_REVERSE[name]?.let { return it }
        if (name.startsWith("axis_")) {
            name.removePrefix("axis_").toIntOrNull()?.let { return it }
        }
        return null
    }

    /** Convert an Android KeyEvent keycode to "KEYCODE_*" string. */
    private fun keycodeToName(keycode: Int): String? {
        // Only map the keycodes actually used in radial menus (0-9 keys)
        val name = android.view.KeyEvent.keyCodeToString(keycode)
        // keyCodeToString returns "KEYCODE_0" etc, or "KEYCODE_UNKNOWN" for bad values
        return if (name != "KEYCODE_UNKNOWN") name else null
    }

    /** Convert a "KEYCODE_*" string to its Android KeyEvent integer value. */
    private fun nameToKeycode(name: String): Int? {
        if (!name.startsWith("KEYCODE_")) return null
        val code = android.view.KeyEvent.keyCodeFromString(name)
        return if (code != android.view.KeyEvent.KEYCODE_UNKNOWN) code else null
    }

    // --- Layout constraints ---
    const val MIN_SIZE = 0.5f
    const val MAX_SIZE = 2.0f
    const val MIN_OPACITY = 0.2f
    const val MAX_OPACITY = 1.0f
    const val MIN_SENSITIVITY = 0.2f
    const val MAX_SENSITIVITY = 3.0f
    const val MIN_DEADZONE = 0
    const val MAX_DEADZONE = 50
    const val MIN_EXPONENT = 1.0f
    const val MAX_EXPONENT = 4.0f
    const val DEFAULT_EXPONENT = 2.0f
    const val MIN_SWIPE_THRESHOLD = 0.1f
    const val MAX_SWIPE_THRESHOLD = 0.9f
    const val DEFAULT_SWIPE_THRESHOLD = 0.3f
    const val MIN_GLOBAL_OPACITY = 0.2f
    const val MAX_GLOBAL_OPACITY = 1.0f
    const val DEFAULT_GLOBAL_OPACITY = 0.7f

    // --- Cheat code definitions (from gamecntl.c cheat_codes[]) ---
    // Keep in sync with cheat_codes[] in d2/main/gamecntl.c and d1/main/gamecntl.c
    data class CheatDef(
        val code: String,
        val label: String,
    )

    val CHEATS_D2 =
        listOf(
            CheatDef("gabbagabbahey", "Cheater!"),
            CheatDef("honestbob", "All Weapons"),
            CheatDef("algroove", "All Keys"),
            CheatDef("alifalafel", "Accessories"),
            CheatDef("almighty", "Invulnerable"),
            CheatDef("blueorb", "Shield Boost"),
            CheatDef("delshiftb", "Destroy Reactor"),
            CheatDef("flash", "Exit Path"),
            CheatDef("freespace", "Level Warp"),
            CheatDef("rockrgrl", "Full Automap"),
            CheatDef("astral", "Ghost Physics"),
            CheatDef("wildfire", "Rapid Fire"),
            CheatDef("duddaboo", "Bouncy Fire"),
            CheatDef("buggin", "Turbo"),
            CheatDef("imagespace", "Robots Stop"),
            CheatDef("spaniard", "Kill All Robots"),
            CheatDef("silkwing", "Robots Kill Robots"),
            CheatDef("godzilla", "Monster Damage"),
            CheatDef("helpvishnu", "Buddy Clone"),
            CheatDef("gowingnut", "Buddy Angry"),
            CheatDef("bittersweet", "Acid"),
        )

    val CHEATS_D1 =
        listOf(
            CheatDef("gabbagabbahey", "Cheater!"),
            CheatDef("scourge", "All Weapons"),
            CheatDef("bigred", "Super Weapons"),
            CheatDef("mitzi", "All Keys"),
            CheatDef("racerx", "Invulnerable"),
            CheatDef("guile", "Cloak"),
            CheatDef("twilight", "Shield Boost"),
            CheatDef("poboys", "Destroy Reactor"),
            CheatDef("flash", "Exit Path"),
            CheatDef("farmerjoe", "Level Warp"),
            CheatDef("bruin", "Extra Life"),
            CheatDef("astral", "Ghost Physics"),
            CheatDef("porgys", "Rapid Fire"),
            CheatDef("buggin", "Turbo"),
            CheatDef("ahimsa", "Robots Stop"),
            CheatDef("bittersweet", "Acid"),
        )
}

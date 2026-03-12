package com.dxxredux.app

/**
 * Shared constants for touch control bindings.
 * Button indices mirror kc_joystick[] in d2/main/kconfig.c — keep in sync.
 * Axis indices mirror the JNI nativeJoystickAxis() mapping in android_input.c.
 */
object TouchBindings {
    // --- Joystick button indices (kc_joystick[] positions) ---
    const val BTN_FIRE_PRIMARY    = 0
    const val BTN_FIRE_SECONDARY  = 1
    const val BTN_ACCELERATE      = 2
    const val BTN_REVERSE         = 3
    const val BTN_FIRE_FLARE      = 4
    const val BTN_SLIDE_ON        = 5
    const val BTN_SLIDE_LEFT      = 6
    const val BTN_SLIDE_RIGHT     = 7
    const val BTN_SLIDE_UP        = 8
    const val BTN_SLIDE_DOWN      = 9
    const val BTN_BANK_ON         = 10
    const val BTN_BANK_LEFT       = 11
    const val BTN_BANK_RIGHT      = 12
    const val BTN_REAR_VIEW       = 25
    const val BTN_DROP_BOMB       = 26
    const val BTN_AFTERBURNER     = 27
    const val BTN_CYCLE_PRIMARY   = 28
    const val BTN_CYCLE_SECONDARY = 29
    const val BTN_HEADLIGHT       = 30
    const val BTN_AUTOMAP         = 50
    const val BTN_ENERGY_SHIELD   = 52
    const val BTN_TOGGLE_BOMB     = 54

    /**
     * Offset added to kc_joystick[] indices when sending touch button events
     * via nativeJoystickButton, to avoid collisions with physical controller
     * button numbers.  Shared constant with C (kconfig.c TOUCH_BTN_OFFSET).
     */
    const val TOUCH_BTN_OFFSET    = 128

    // --- Virtual (overlay-only) bindings – not sent as joystick buttons ---
    const val BTN_CHEATS_MENU     = 100

    /** All button bindings with readable labels, for UI pickers. */
    val BUTTON_LABELS = mapOf(
        BTN_FIRE_PRIMARY    to "Fire Primary",
        BTN_FIRE_SECONDARY  to "Fire Secondary",
        BTN_ACCELERATE      to "Accelerate",
        BTN_REVERSE         to "Reverse",
        BTN_FIRE_FLARE      to "Fire Flare",
        BTN_SLIDE_ON        to "Slide On",
        BTN_SLIDE_LEFT      to "Slide Left",
        BTN_SLIDE_RIGHT     to "Slide Right",
        BTN_SLIDE_UP        to "Slide Up",
        BTN_SLIDE_DOWN      to "Slide Down",
        BTN_BANK_ON         to "Bank On",
        BTN_BANK_LEFT       to "Bank Left",
        BTN_BANK_RIGHT      to "Bank Right",
        BTN_REAR_VIEW       to "Rear View",
        BTN_DROP_BOMB       to "Drop Bomb",
        BTN_AFTERBURNER     to "Afterburner",
        BTN_CYCLE_PRIMARY   to "Cycle Primary",
        BTN_CYCLE_SECONDARY to "Cycle Secondary",
        BTN_HEADLIGHT       to "Headlight",
        BTN_AUTOMAP         to "Automap",
        BTN_ENERGY_SHIELD   to "Energy→Shield",
        BTN_TOGGLE_BOMB     to "Toggle Bomb",
        BTN_CHEATS_MENU     to "Cheats Menu"
    )

    // --- JNI axis indices (nativeJoystickAxis axis parameter) ---
    const val AXIS_LEFT_X   = 0  // Left stick horizontal
    const val AXIS_LEFT_Y   = 1  // Left stick vertical
    const val AXIS_RIGHT_X  = 2  // Right stick horizontal
    const val AXIS_RIGHT_Y  = 3  // Right stick vertical
    const val AXIS_LTRIGGER = 4
    const val AXIS_RTRIGGER = 5

    /** Axis labels for UI pickers — show game function (default mapping). */
    val AXIS_LABELS = mapOf(
        AXIS_LEFT_X   to "Slide L/R",
        AXIS_LEFT_Y   to "Fwd/Back",
        AXIS_RIGHT_X  to "Turn L/R",
        AXIS_RIGHT_Y  to "Pitch U/D",
        AXIS_LTRIGGER to "L Trigger",
        AXIS_RTRIGGER to "R Trigger"
    )

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
    data class CheatDef(val code: String, val label: String)

    val CHEATS_D2 = listOf(
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

    val CHEATS_D1 = listOf(
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

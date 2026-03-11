package com.dxxredux.app

import android.content.Context
import android.view.KeyEvent
import org.json.JSONObject
import java.io.File

/**
 * Loads, saves, and provides preset TouchLayouts.
 * Storage: context.filesDir / "touch_layout.json"
 */
object TouchLayoutRepository {
    private const val FILENAME = "touch_layout.json"

    fun load(context: Context): TouchLayout {
        val file = File(context.filesDir, FILENAME)
        if (!file.exists()) return defaultLayout()
        return try {
            TouchLayout.fromJson(JSONObject(file.readText()))
        } catch (_: Exception) {
            defaultLayout()
        }
    }

    fun save(context: Context, layout: TouchLayout) {
        File(context.filesDir, FILENAME).writeText(layout.toJson().toString(2))
    }

    fun defaultLayout(): TouchLayout = presetSimple()

    // ── Presets ──

    /** Minimal: one move stick + two fire buttons. Matches existing overlay positions. */
    fun presetSimple() = TouchLayout(
        name = "Simple",
        sticks = listOf(
            AnalogStickControl(
                id = "move", xPct = 18f, yPct = 75f,
                axisX = TouchBindings.AXIS_LEFT_X, axisY = TouchBindings.AXIS_LEFT_Y
            )
        ),
        buttons = listOf(
            ButtonControl(id = "fire1", xPct = 87f, yPct = 90f,
                binding = TouchBindings.BTN_FIRE_PRIMARY, label = "A"),
            ButtonControl(id = "fire2", xPct = 92f, yPct = 82f,
                binding = TouchBindings.BTN_FIRE_SECONDARY, label = "B"),
            ButtonControl(id = "map", xPct = 93f, yPct = 8f,
                binding = TouchBindings.BTN_AUTOMAP, label = "MAP", sizeMult = 0.7f)
        )
    )

    /** Dual-stick + common action buttons. */
    fun presetAdvanced() = TouchLayout(
        name = "Advanced",
        sticks = listOf(
            AnalogStickControl(
                id = "move", xPct = 15f, yPct = 80f,
                axisX = TouchBindings.AXIS_LEFT_X, axisY = TouchBindings.AXIS_LEFT_Y
            ),
            AnalogStickControl(
                id = "look", xPct = 85f, yPct = 80f,
                axisX = TouchBindings.AXIS_RIGHT_X, axisY = TouchBindings.AXIS_RIGHT_Y,
                responseCurve = ResponseCurve.EXPONENTIAL
            )
        ),
        buttons = listOf(
            ButtonControl(id = "fire1", xPct = 95f, yPct = 55f,
                binding = TouchBindings.BTN_FIRE_PRIMARY, label = "1"),
            ButtonControl(id = "fire2", xPct = 95f, yPct = 45f,
                binding = TouchBindings.BTN_FIRE_SECONDARY, label = "2"),
            ButtonControl(id = "flare", xPct = 95f, yPct = 35f,
                binding = TouchBindings.BTN_FIRE_FLARE, label = "F", sizeMult = 0.7f),
            ButtonControl(id = "bomb", xPct = 95f, yPct = 65f,
                binding = TouchBindings.BTN_DROP_BOMB, label = "B", sizeMult = 0.7f),
            ButtonControl(id = "map", xPct = 92f, yPct = 8f,
                binding = TouchBindings.BTN_AUTOMAP, label = "MAP", sizeMult = 0.7f),
            ButtonControl(id = "rear", xPct = 85f, yPct = 8f,
                binding = TouchBindings.BTN_REAR_VIEW, label = "R", sizeMult = 0.7f)
        ),
        sliders = listOf(
            SliderControl(id = "throttle", xPct = 5f, yPct = 40f,
                axis = TouchBindings.AXIS_LTRIGGER, sizeMult = 1.2f)
        ),
        radialMenus = listOf(guidebotWheel(10f, 20f), weaponWheel(50f, 8f))
    )

    /** Claw grip: sticks in lower corners, fire buttons in upper corners. */
    fun presetClaw() = TouchLayout(
        name = "Claw",
        sticks = listOf(
            AnalogStickControl(
                id = "move", xPct = 15f, yPct = 80f,
                axisX = TouchBindings.AXIS_LEFT_X, axisY = TouchBindings.AXIS_LEFT_Y,
                floating = true,
                floatingZone = FloatingZone(0f, 40f, 40f, 100f)
            ),
            AnalogStickControl(
                id = "look", xPct = 85f, yPct = 80f,
                axisX = TouchBindings.AXIS_RIGHT_X, axisY = TouchBindings.AXIS_RIGHT_Y,
                floating = true,
                floatingZone = FloatingZone(60f, 40f, 100f, 100f),
                responseCurve = ResponseCurve.EXPONENTIAL
            )
        ),
        buttons = listOf(
            ButtonControl(id = "fire1", xPct = 90f, yPct = 12f,
                binding = TouchBindings.BTN_FIRE_PRIMARY, label = "1"),
            ButtonControl(id = "fire2", xPct = 80f, yPct = 12f,
                binding = TouchBindings.BTN_FIRE_SECONDARY, label = "2"),
            ButtonControl(id = "slide", xPct = 10f, yPct = 12f,
                binding = TouchBindings.BTN_SLIDE_ON, label = "SL", toggle = true),
            ButtonControl(id = "afterburner", xPct = 20f, yPct = 12f,
                binding = TouchBindings.BTN_AFTERBURNER, label = "AB"),
            ButtonControl(id = "map", xPct = 50f, yPct = 5f,
                binding = TouchBindings.BTN_AUTOMAP, label = "MAP", sizeMult = 0.7f)
        ),
        radialMenus = listOf(guidebotWheel(40f, 5f), weaponWheel(60f, 5f)),
        gyro = GyroConfig(enabled = true, activation = GyroActivation.TOUCH_STICK)
    )

    // ── Radial menu factories ──

    /** Guidebot command wheel: 9 commands + center "Clear" (KEY_0 = clear goal). */
    private fun guidebotWheel(xPct: Float, yPct: Float) = RadialMenuControl(
        id = "Guide", xPct = xPct, yPct = yPct, sizeMult = 1f,
        centerLabel = "Clear", centerBinding = KeyEvent.KEYCODE_0,
        segments = listOf(
            RadialSegment("Energy",   KeyEvent.KEYCODE_1),
            RadialSegment("Enrg Ctr", KeyEvent.KEYCODE_2),
            RadialSegment("Shield",   KeyEvent.KEYCODE_3),
            RadialSegment("Powerup",  KeyEvent.KEYCODE_4),
            RadialSegment("Robot",    KeyEvent.KEYCODE_5),
            RadialSegment("Hostage",  KeyEvent.KEYCODE_6),
            RadialSegment("Scram!",   KeyEvent.KEYCODE_7),
            RadialSegment("Spew",     KeyEvent.KEYCODE_8),
            RadialSegment("Exit",     KeyEvent.KEYCODE_9)
        )
    )

    /** Weapon select wheel: 5 primary (keys 1-5) + 5 secondary (keys 6-0). */
    private fun weaponWheel(xPct: Float, yPct: Float) = RadialMenuControl(
        id = "Wpns", xPct = xPct, yPct = yPct, sizeMult = 1f,
        segments = listOf(
            RadialSegment("Laser",   KeyEvent.KEYCODE_1),
            RadialSegment("Vulcan",  KeyEvent.KEYCODE_2),
            RadialSegment("Spread",  KeyEvent.KEYCODE_3),
            RadialSegment("Plasma",  KeyEvent.KEYCODE_4),
            RadialSegment("Fusion",  KeyEvent.KEYCODE_5),
            RadialSegment("Concsn",  KeyEvent.KEYCODE_6),
            RadialSegment("Homing",  KeyEvent.KEYCODE_7),
            RadialSegment("Proxim",  KeyEvent.KEYCODE_8),
            RadialSegment("Smart",   KeyEvent.KEYCODE_9),
            RadialSegment("Mega",    KeyEvent.KEYCODE_0)
        )
    )

    /** All available presets for the preset picker. */
    fun allPresets() = listOf(presetSimple(), presetAdvanced(), presetClaw())
}

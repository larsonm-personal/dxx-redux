package com.dxxredux.app

import android.content.Context
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
                id = "move", xPct = 15f, yPct = 80f,
                axisX = TouchBindings.AXIS_LEFT_X, axisY = TouchBindings.AXIS_LEFT_Y
            )
        ),
        buttons = listOf(
            ButtonControl(id = "fire1", xPct = 85f, yPct = 90f,
                binding = TouchBindings.BTN_FIRE_PRIMARY, label = "1"),
            ButtonControl(id = "fire2", xPct = 90f, yPct = 80f,
                binding = TouchBindings.BTN_FIRE_SECONDARY, label = "2"),
            ButtonControl(id = "map", xPct = 92f, yPct = 8f,
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
        )
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
        gyro = GyroConfig(enabled = true, activation = GyroActivation.TOUCH_STICK)
    )

    /** All available presets for the preset picker. */
    fun allPresets() = listOf(presetSimple(), presetAdvanced(), presetClaw())
}

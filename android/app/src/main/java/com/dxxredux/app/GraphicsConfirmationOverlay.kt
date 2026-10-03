package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.Gravity
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import org.json.JSONObject
import kotlin.math.ceil

/** Uses Android's compositor and clock, independently of the game's EGL/event loop */
internal class GraphicsConfirmationOverlay(
    context: Context,
    private val arm: (Long) -> Boolean,
    private val decide: (Long, Boolean, String) -> Int,
    private val readState: () -> String,
    private val acquireInput: () -> Unit,
    private val releaseInput: () -> Unit,
    private val recoverProcess: (String) -> Unit,
) : FrameLayout(context) {
    private companion object {
        // EGL and shader recreation can exceed one second even when recovery succeeds
        const val RESTORE_TIMEOUT_MS = 3000L
    }

    private val handler = Handler(Looper.getMainLooper())
    private val title = TextView(context)
    private val details = TextView(context)
    private val ok = Button(context)
    private val cancel = Button(context)
    private var trialId = 0L
    private var deadline = 0L
    private var preparing = false
    private var restoring = false
    private var restoreStarted = 0L
    private var selectedOk = false
    private var axesReady = false
    private var axisX = 0
    private var axisY = 0
    private var armPosted = false
    private var tickScheduled = false
    private var firstDrawCancelText = ""

    val active: Boolean get() = visibility == View.VISIBLE

    fun controllerNavigationState(): Map<String, Any> =
        mapOf(
            "graphics_open" to active,
            "graphics_selected_ok" to selectedOk,
            "graphics_axes_ready" to axesReady,
            "graphics_restoring" to restoring,
            "graphics_cancel_text" to cancel.text.toString(),
            "graphics_first_draw_cancel_text" to firstDrawCancelText,
        )

    fun automationTouchPoint(target: String): Pair<Float, Float> {
        check(active) { "Graphics confirmation is not active" }
        val view =
            when (target) {
                "ok" -> ok
                "cancel" -> cancel
                "outside" -> this
                else -> error("Unknown graphics touch target: $target")
            }
        check(view.width > 0 && view.height > 0) { "Graphics touch target is not laid out" }
        val location = IntArray(2)
        view.getLocationInWindow(location)
        return if (target == "outside") {
            (location[0] + dp(8)).toFloat() to (location[1] + dp(8)).toFloat()
        } else {
            location[0] + view.width / 2f to location[1] + view.height / 2f
        }
    }

    init {
        visibility = View.GONE
        isClickable = true
        isFocusable = true
        setWillNotDraw(false)
        setBackgroundColor(0x77000000)
        val panel =
            LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER_HORIZONTAL
                setPadding(dp(24), dp(20), dp(24), dp(20))
                setBackgroundColor(Color.rgb(24, 24, 24))
            }
        title.apply {
            text = "Keep these graphics settings?"
            textSize = 22f
            setTextColor(Color.WHITE)
            gravity = Gravity.CENTER
        }
        details.apply {
            textSize = 16f
            setTextColor(Color.LTGRAY)
            gravity = Gravity.CENTER
            setPadding(0, dp(12), 0, dp(12))
        }
        panel.addView(title)
        panel.addView(details)
        val buttons = LinearLayout(context).apply { orientation = LinearLayout.HORIZONTAL }
        ok.text = "OK"
        cancel.text = "Cancel (5)"
        ok.setOnClickListener { choose(true, "ok") }
        cancel.setOnClickListener { choose(false, "cancel") }
        buttons.addView(ok, LinearLayout.LayoutParams(dp(130), dp(56)))
        buttons.addView(cancel, LinearLayout.LayoutParams(dp(150), dp(56)))
        panel.addView(buttons)
        addView(panel, LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT, Gravity.CENTER))
        updateSelection()
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    fun update(stateText: String) {
        val state = JSONObject(stateText)
        val phase = state.optString("phase")
        if (phase == "failed" || (phase == "disabled" && active)) {
            recoverProcess("Could not save or recover graphics settings. Return to the launcher to retry recovery")
            return
        }
        if (phase !in setOf("preparing", "challenge", "restoring")) {
            dismiss()
            return
        }
        val id = state.getLong("trial_id")
        if (!active || id != trialId) {
            trialId = id
            selectedOk = false
            axesReady = false
            axisX = 0
            axisY = 0
            armPosted = false
            restoreStarted = 0L
            cancel.text = "Cancel (5)"
            firstDrawCancelText = ""
            visibility = View.VISIBLE
            bringToFront()
            acquireInput()
            details.text = changedSettings(state)
            updateSelection()
        }
        preparing = phase == "preparing"
        restoring = phase == "restoring"
        deadline = state.optLong("deadline_ms")
        ok.isEnabled = !preparing && !restoring
        cancel.isEnabled = !restoring
        title.text = if (restoring) "Restoring graphics settings..." else "Keep these graphics settings?"
        if (restoring && restoreStarted == 0L) restoreStarted = SystemClock.elapsedRealtime()
        // Frequent renderer notifications must not postpone either monotonic deadline
        if (!tickScheduled) {
            tickScheduled = true
            handler.postDelayed(tick, 50L)
        }
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (firstDrawCancelText.isEmpty()) firstDrawCancelText = cancel.text.toString()
        if (preparing && !armPosted) {
            armPosted = true
            // Run after this Android UI draw, before allowing the candidate game draw
            post {
                if (active && preparing) {
                    if (!arm(trialId)) choose(false, "overlay_arm_failed")
                    update(readState())
                }
            }
        }
    }

    private val tick =
        object : Runnable {
            override fun run() {
                tickScheduled = false
                if (!active) return
                // Lifecycle transitions can delay this tick after the engine has already restored
                update(readState())
                if (!active) return
                val now = SystemClock.elapsedRealtime()
                if (!restoring && deadline > 0L) {
                    val remaining = deadline - now
                    cancel.text = "Cancel (${ceil(remaining.coerceAtLeast(0L) / 1000.0).toInt()})"
                    if (remaining <= 0L) choose(false, "timeout")
                }
                if (restoring && now - restoreStarted >= RESTORE_TIMEOUT_MS) {
                    recoverProcess(
                        "Graphics settings could not be restored in time. The game was closed; return to the launcher to recover the last accepted settings",
                    )
                    return
                }
            }
        }

    private fun choose(
        accept: Boolean,
        reason: String,
    ) {
        if (!active || restoring || (accept && preparing)) return
        decide(trialId, accept, reason)
        update(readState())
    }

    fun cancelForLifecycle() {
        if (active) choose(false, "background")
    }

    fun handleKey(event: KeyEvent): Boolean {
        if (!active) return false
        if (event.action != KeyEvent.ACTION_DOWN || event.repeatCount != 0) return true
        when (event.keyCode) {
            KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_UP -> {
                selectedOk = true
            }

            KeyEvent.KEYCODE_DPAD_RIGHT, KeyEvent.KEYCODE_DPAD_DOWN -> {
                selectedOk = false
            }

            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER,
            KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_NUMPAD_ENTER,
            -> {
                choose(
                    selectedOk,
                    if (selectedOk) "ok" else "cancel",
                )
            }

            KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BACK, KeyEvent.KEYCODE_ESCAPE -> {
                choose(false, "cancel")
            }
        }
        updateSelection()
        return true
    }

    fun handleMotion(event: MotionEvent): Boolean {
        if (!active) return false

        fun direction(value: Float) =
            when {
                value < -0.5f -> -1
                value > 0.5f -> 1
                else -> 0
            }
        val x =
            direction(event.getAxisValue(MotionEvent.AXIS_HAT_X)).takeIf { it != 0 }
                ?: direction(event.getAxisValue(MotionEvent.AXIS_X))
        val y =
            direction(event.getAxisValue(MotionEvent.AXIS_HAT_Y)).takeIf { it != 0 }
                ?: direction(event.getAxisValue(MotionEvent.AXIS_Y))
        if (!axesReady) {
            axesReady = x == 0 && y == 0
            return true
        }
        if ((x != axisX && x != 0) || (y != axisY && y != 0)) {
            selectedOk = x < 0 || y < 0
            updateSelection()
        }
        axisX = x
        axisY = y
        return true
    }

    private fun updateSelection() {
        ok.isSelected = selectedOk
        cancel.isSelected = !selectedOk
        ok.setTextColor(if (selectedOk) Color.YELLOW else Color.WHITE)
        cancel.setTextColor(if (!selectedOk) Color.YELLOW else Color.WHITE)
    }

    private fun dismiss() {
        if (!active) return
        handler.removeCallbacks(tick)
        tickScheduled = false
        visibility = View.GONE
        preparing = false
        restoring = false
        releaseInput()
    }

    private fun changedSettings(state: JSONObject): String {
        val accepted = state.optJSONObject("accepted") ?: return ""
        val candidate = state.optJSONObject("candidate") ?: return ""
        val names =
            linkedMapOf(
                "TexFilt" to "Texture filtering",
                "AnisoLevel" to "Anisotropic filtering",
                "MsaaLevel" to "MSAA",
                "MenuTexFilt" to "Menu filtering",
                "HudTexFilt" to "HUD filtering",
                "ColorDepth" to "Color depth",
            )
        val changes =
            names
                .mapNotNull { (key, label) ->
                    if (candidate.optInt(key) == accepted.optInt(key)) {
                        null
                    } else {
                        val value = candidate.optInt(key)
                        val display =
                            when (key) {
                                "ColorDepth" -> {
                                    if (value == 1) "32-bit" else "16-bit"
                                }

                                "TexFilt" -> {
                                    listOf(
                                        "Nearest",
                                        "Bilinear",
                                        "Trilinear",
                                    ).getOrElse(value) { value.toString() }
                                }

                                "MenuTexFilt", "HudTexFilt" -> {
                                    if (value == 0) "Off" else "On"
                                }

                                else -> {
                                    if (value == 0) "Off" else "${value}x"
                                }
                            }
                        "$label: $display"
                    }
                }.toMutableList()
        if (listOf(
                "ResolutionX",
                "ResolutionY",
                "AspectX",
                "AspectY",
            ).any { candidate.optInt(it) != accepted.optInt(it) }
        ) {
            changes.add("Resolution: ${candidate.optInt("ResolutionX")} x ${candidate.optInt("ResolutionY")}")
        }
        return changes.joinToString("\n")
    }
}

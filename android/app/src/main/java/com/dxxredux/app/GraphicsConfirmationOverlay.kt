package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.Gravity
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import org.json.JSONObject
import kotlin.math.ceil

internal fun graphicsRestoreTimeoutMs(state: JSONObject): Long {
    val accepted = state.optJSONObject("accepted") ?: return 3000L
    val candidate = state.optJSONObject("candidate") ?: return 3000L
    // Context/shader reconstruction needs more time than a live option rollback
    // Keep mode keys synchronized with apply_snapshot in android_graphics_safety.cpp
    return if (listOf("ResolutionX", "ResolutionY", "AspectX", "AspectY", "ColorDepth").any {
            accepted.optInt(it) != candidate.optInt(it)
        }
    ) {
        10000L
    } else {
        3000L
    }
}

/** Uses Android's compositor and clock, independently of the game's EGL/event loop */
internal class GraphicsConfirmationOverlay(
    context: Context,
    private val arm: (Long) -> Boolean,
    private val decide: (Long, Boolean, String) -> Int,
    private val readState: () -> String,
    private val previewReady: (Long) -> Boolean,
    private val previewDone: (Long) -> Boolean,
    private val previewOption: (Long, String, Int) -> Boolean,
    private val acquireInput: () -> Unit,
    private val releaseInput: () -> Unit,
    private val recoverProcess: (String) -> Unit,
) : FrameLayout(context) {
    private val handler = Handler(Looper.getMainLooper())
    private val title = TextView(context)
    private val details = TextView(context)
    private val footer = TextView(context)
    private val ok = TextView(context)
    private val cancel = TextView(context)
    private val panel = LinearLayout(context)
    private val scroll = ScrollView(context)
    private val choices =
        GraphicsFirstRunChoices(context) { name, value ->
            if (!previewOption(trialId, name, value)) choose(false, "preview_option_failed")
            update(readState())
        }
    private var phase = "idle"
    private var previewPosted = false
    private var previewSelection = 0
    private var latestState = JSONObject()
    private var trialId = 0L
    private var deadline = 0L
    private var preparing = false
    private var restoring = false
    private var restoreStarted = 0L
    private var restoreTimeoutMs = 3000L
    private var selectedOk = false
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
            "graphics_restoring" to restoring,
            "graphics_cancel_text" to cancel.text.toString(),
            "graphics_first_draw_cancel_text" to firstDrawCancelText,
            "graphics_chooser_open" to (active && phase in setOf("offering", "editing")),
            "graphics_preview_settling" to (active && phase == "settling"),
            "graphics_preview_selection" to previewSelection,
        ) + choices.navigationState()

    fun automationTouchPoint(target: String): Pair<Float, Float> {
        check(active) { "Graphics confirmation is not active" }
        val view =
            when (target) {
                "ok" -> ok
                "cancel" -> cancel
                "outside" -> this
                "tex_filt" -> choices.button(0)
                "aniso_level" -> choices.button(1)
                "msaa_level" -> choices.button(2)
                else -> error("Unknown graphics touch target: $target")
            }
        check(view.width > 0 && view.height > 0) { "Graphics touch target is not laid out" }
        if (view.parent === choices) view.requestRectangleOnScreen(Rect(0, 0, view.width, view.height), true)
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
        panel.apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(dp(24), dp(12), dp(24), dp(12))
            background = PauseOverlayStyle.cardBackground(resources.displayMetrics.density)
        }
        title.apply {
            text = "Keep these graphics settings?"
            textSize = 22f
            setTextColor(PauseOverlayStyle.TEXT_COLOR)
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
        }
        details.apply {
            textSize = 15f
            setTextColor(PauseOverlayStyle.SECONDARY_TEXT_COLOR)
            gravity = Gravity.CENTER
            setPadding(0, dp(4), 0, dp(4))
        }
        panel.addView(title, LinearLayout.LayoutParams(-1, -2))
        val content =
            LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                addView(details, LinearLayout.LayoutParams(-1, -2))
                addView(choices, LinearLayout.LayoutParams(-1, -2))
            }
        scroll.addView(content, LayoutParams(-1, -2))
        scroll.isScrollbarFadingEnabled = false
        panel.addView(scroll, LinearLayout.LayoutParams(-1, -2, 1f))
        footer.apply {
            textSize = 14f
            setTextColor(PauseOverlayStyle.SECONDARY_TEXT_COLOR)
            setPadding(0, 0, 0, dp(4))
        }
        panel.addView(footer, LinearLayout.LayoutParams(-1, -2))
        val buttons =
            LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                isBaselineAligned = false
            }
        ok.text = "OK"
        cancel.text = "Cancel (5)"
        for (button in listOf(ok, cancel)) {
            button.apply {
                gravity = Gravity.CENTER
                textSize = 20f
                typeface = Typeface.DEFAULT_BOLD
                setTextColor(PauseOverlayStyle.TEXT_COLOR)
                isClickable = true
                isFocusable = true
                background = PauseOverlayStyle.choiceBackground(resources.displayMetrics.density)
                setPadding(dp(16), dp(8), dp(16), dp(8))
            }
        }
        ok.setOnClickListener { if (phase == "editing") finishPreview() else choose(true, "ok") }
        cancel.setOnClickListener { choose(false, "cancel") }
        buttons.addView(ok, LinearLayout.LayoutParams(0, -1, 1f).apply { marginEnd = dp(12) })
        buttons.addView(cancel, LinearLayout.LayoutParams(0, -1, 1f))
        ok.minHeight = dp(56)
        cancel.minHeight = dp(56)
        panel.addView(buttons, LinearLayout.LayoutParams(-1, -2))
        addView(panel, LayoutParams(dp(440), LayoutParams.WRAP_CONTENT, Gravity.CENTER))
        setPadding(dp(12), dp(12), dp(12), dp(12))
        updateSelection()
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    override fun onMeasure(
        widthMeasureSpec: Int,
        heightMeasureSpec: Int,
    ) {
        val editing = phase == "editing" || phase == "offering"
        val compact = editing && MeasureSpec.getSize(heightMeasureSpec) < dp(360)
        val verticalPadding = dp(if (compact) 8 else 12)
        setPadding(dp(12), verticalPadding, dp(12), verticalPadding)
        panel.setPadding(dp(24), verticalPadding, dp(24), verticalPadding)
        ok.minHeight = dp(if (compact) 48 else 56)
        cancel.minHeight = dp(if (compact) 48 else 56)
        panel.layoutParams.width =
            minOf(
                dp(if (editing) 560 else 440),
                (MeasureSpec.getSize(widthMeasureSpec) - paddingLeft - paddingRight).coerceAtLeast(0),
            )
        super.onMeasure(widthMeasureSpec, heightMeasureSpec)
    }

    fun update(stateText: String) {
        val state = JSONObject(stateText)
        val previousPhase = phase
        phase = state.optString("phase")
        latestState = state
        if (phase == "live") {
            // Monitor immediate Video Info edits without taking its input or showing a modal
            deadline = 0L
            if (!tickScheduled) {
                tickScheduled = true
                handler.postDelayed(tick, 50L)
            }
            return
        }
        if (phase == "failed" || (phase == "disabled" && active)) {
            recoverProcess("Could not save or recover graphics settings. Return to the launcher to retry recovery")
            return
        }
        if (phase !in setOf("offering", "editing", "settling", "preparing", "challenge", "restoring")) {
            dismiss()
            return
        }
        val id = state.getLong("trial_id")
        if (!active || id != trialId) {
            trialId = id
            selectedOk = state.optBoolean("first_run_trial")
            armPosted = false
            previewPosted = false
            previewSelection = 0
            restoreStarted = 0L
            restoreTimeoutMs = graphicsRestoreTimeoutMs(state)
            cancel.text = "Cancel (5)"
            firstDrawCancelText = ""
            visibility = View.VISIBLE
            bringToFront()
            acquireInput()
            details.text = changedSettings(state)
            updateSelection()
        }
        preparing = phase == "preparing"
        if (preparing && previousPhase != phase && state.optBoolean("first_run_trial")) selectedOk = true
        restoring = phase == "restoring"
        val editing = phase == "editing" || phase == "offering"
        panel.visibility = if (phase == "settling") View.INVISIBLE else View.VISIBLE
        setBackgroundColor(
            if (phase == "settling") {
                0x00000000
            } else if (editing) {
                0x33000000
            } else {
                0x77000000
            },
        )
        choices.visibility = if (editing) View.VISIBLE else View.GONE
        footer.visibility = if (editing || preparing || phase == "challenge") View.VISIBLE else View.GONE
        footer.textSize = if (editing) 14f else 18f
        footer.typeface = if (editing) Typeface.DEFAULT else Typeface.DEFAULT_BOLD
        footer.gravity = if (editing) Gravity.START else Gravity.CENTER
        if (editing) footer.text = "Change later in Settings > Video Info or the launcher's Graphics page"
        choices.update(state, previewSelection)
        ok.text = if (editing) "Done" else "OK"
        if (editing) {
            cancel.text = "Keep previous settings"
        } else if (preparing) {
            cancel.text = "Cancel (5)"
        }
        if (editing) {
            details.text = if (phase == "offering") "Game paused" else "Game paused - changes preview live"
        } else {
            details.text = changedSettings(state)
        }
        deadline = state.optLong("deadline_ms")
        if (preparing || phase == "challenge") updateCountdown()
        ok.isEnabled = phase == "editing" || (phase == "challenge" && state.optBoolean("candidate_ready", true))
        cancel.isEnabled = !restoring
        updateSelection()
        title.text =
            if (restoring) {
                "Restoring graphics settings..."
            } else if (editing) {
                "Choose graphics options"
            } else {
                "Keep these graphics settings?"
            }
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
        if (phase == "offering" && !previewPosted) {
            previewPosted = true
            post {
                if (active && phase == "offering") {
                    if (!previewReady(trialId)) choose(false, "preview_start_failed")
                    update(readState())
                }
            }
        }
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
                if (!active && phase != "live") return
                // Lifecycle transitions can delay this tick after the engine has already restored
                update(readState())
                if (!active && phase != "live") return
                val now = SystemClock.elapsedRealtime()
                val applyDeadline = latestState.optLong("apply_deadline_ms")
                if (phase in setOf("offering", "editing", "settling", "live") && applyDeadline > 0 &&
                    now >= applyDeadline
                ) {
                    decide(latestState.getLong("trial_id"), false, "preview_apply_timeout")
                    update(readState())
                }
                if (!restoring && deadline > 0L) {
                    val remaining = deadline - now
                    if (remaining <= 0L) choose(false, "timeout")
                }
                if (restoring && now - restoreStarted >= restoreTimeoutMs) {
                    recoverProcess(
                        "Graphics settings could not be restored in time. " +
                            "The game was closed; return to the launcher to recover the last accepted settings",
                    )
                    return
                }
            }
        }

    private fun updateCountdown() {
        val seconds =
            if (preparing) 5 else ceil((deadline - SystemClock.elapsedRealtime()).coerceAtLeast(0L) / 1000.0).toInt()
        cancel.text = "Cancel ($seconds)"
        footer.text = "Reverting in $seconds ${if (seconds == 1) "second" else "seconds"}"
    }

    private fun choose(
        accept: Boolean,
        reason: String,
    ) {
        if (!active || restoring || (accept && preparing)) return
        decide(trialId, accept, reason)
        update(readState())
    }

    private fun finishPreview() {
        if (phase != "editing") return
        if (!previewDone(trialId)) choose(false, "preview_done_failed")
        update(readState())
    }

    fun cancelForLifecycle() {
        if (active) choose(false, "background")
    }

    fun handleKey(event: KeyEvent): Boolean {
        if (!active) return false
        if (event.action != KeyEvent.ACTION_DOWN || event.repeatCount != 0) return true
        if (phase == "settling") {
            if (event.keyCode in
                setOf(KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BACK, KeyEvent.KEYCODE_ESCAPE)
            ) {
                choose(false, "cancel_preview")
            }
            return true
        }
        if (phase == "offering" || phase == "editing") {
            val indices = choices.availableIndices() + listOf(3, 4)
            when (event.keyCode) {
                KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_LEFT -> {
                    previewSelection =
                        indices[(indices.indexOf(previewSelection).coerceAtLeast(0) + indices.size - 1) % indices.size]
                }

                KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.KEYCODE_DPAD_RIGHT -> {
                    previewSelection =
                        indices[(indices.indexOf(previewSelection) + 1) % indices.size]
                }

                KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER,
                KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_NUMPAD_ENTER,
                -> {
                    when (previewSelection) {
                        3 -> finishPreview()
                        4 -> choose(false, "keep_previous")
                        else -> choices.activate(previewSelection)
                    }
                }

                KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BACK, KeyEvent.KEYCODE_ESCAPE -> {
                    finishPreview()
                }
            }
            updateSelection()
            if (previewSelection in 0..2) {
                val button = choices.button(previewSelection)
                button.requestRectangleOnScreen(Rect(0, 0, button.width, button.height))
            } else {
                // Reveal the last option when controller navigation reaches the pinned actions
                scroll.smoothScrollTo(0, scroll.getChildAt(0).height)
            }
            return true
        }
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
        // Observe directions before opening too, so only already-held input is suppressed
        if (!active) {
            axisX = x
            axisY = y
            return false
        }
        if ((x != axisX && x != 0) || (y != axisY && y != 0)) {
            if (phase == "editing") {
                handleKey(
                    KeyEvent(
                        KeyEvent.ACTION_DOWN,
                        if (x < 0 ||
                            y < 0
                        ) {
                            KeyEvent.KEYCODE_DPAD_UP
                        } else {
                            KeyEvent.KEYCODE_DPAD_DOWN
                        },
                    ),
                )
            } else {
                selectedOk = x < 0 || y < 0
                updateSelection()
            }
        }
        axisX = x
        axisY = y
        return true
    }

    private fun updateSelection() {
        val editing = phase == "editing" || phase == "offering"
        if (editing) choices.update(latestState, previewSelection)
        ok.isSelected = if (editing) previewSelection == 3 else selectedOk
        cancel.isSelected = if (editing) previewSelection == 4 else !selectedOk
        ok.alpha = if (ok.isEnabled) 1f else 0.5f
        cancel.alpha = if (cancel.isEnabled) 1f else 0.5f
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

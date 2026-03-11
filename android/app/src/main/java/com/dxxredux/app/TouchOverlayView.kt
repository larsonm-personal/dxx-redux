package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.PointF
import android.graphics.RectF
import android.util.AttributeSet
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.min
import kotlin.math.sign
import kotlin.math.sin

/**
 * Semi-transparent touch overlay drawn on top of the game SurfaceView.
 *
 * Left side  – virtual analog stick (yaw / pitch)
 * Right side – two fire buttons (primary = button 0, secondary = button 1)
 *
 * axis values  → [axisCallback]   → nativeJoystickAxis()
 * button press → [buttonCallback] → nativeJoystickButton()
 *
 * The overlay only draws/responds when [isActive] is true
 * (controlled by the in-game state polling in MainActivity).
 */
class TouchOverlayView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : View(context, attrs) {

    /** Called with (axisIndex, value) when the stick moves. */
    var axisCallback: ((Int, Float) -> Unit)? = null

    /** Called with (buttonIndex, pressed) when a fire button is touched/released. */
    var buttonCallback: ((Int, Boolean) -> Unit)? = null

    /** Called when the MAP button is tapped (toggles automap). */
    var mapButtonCallback: (() -> Unit)? = null

    /** Called to skip to the previous music track. */
    var prevTrackCallback: (() -> Unit)? = null
    /** Called to skip to the next music track. */
    var nextTrackCallback: (() -> Unit)? = null
    /** Called when the track info area is tapped (opens music panel). */
    var musicPanelCallback: (() -> Unit)? = null

    /** Called when a tap lands outside all overlay controls (pass-through for "press any key" screens). */
    var tapPassthroughCallback: (() -> Unit)? = null

    /** Called with (heading, pitch, thrust, bank, vertical, sideways) when automap gestures are detected.
     *  heading/pitch/bank are fractions of screen dimension; thrust is fraction of screen width;
     *  vertical/sideways are fractions of screen dimension. */
    var automapInputCallback: ((Float, Float, Float, Float, Float, Float) -> Unit)? = null

    /** Optional gyro manager — set by MainActivity to enable TOUCH_STICK activation. */
    var gyroManager: GyroInputManager? = null

    /** Whether the overlay should be visible and active. */
    var isActive: Boolean = false
        set(value) {
            if (field != value) {
                field = value
                visibility = if (value) VISIBLE else GONE
                if (!value) {
                    resetAllSticks()
                    releaseAllButtons()
                }
                invalidate()
            }
        }

    // ── Layout state ──────────────────────────────────────
    private var layout: TouchLayout = TouchLayoutRepository.defaultLayout()

    // ── Computed control states ──────────────────────────────
    private class StickState(val control: AnalogStickControl) {
        var centerX = 0f; var centerY = 0f; var radius = 0f
        var pointerId = -1
        val pos = PointF() // thumb offset in px from center
        // Floating mode: zone bounds in px
        var fzLeft = 0f; var fzTop = 0f; var fzRight = 0f; var fzBottom = 0f
        var floatingCX = 0f; var floatingCY = 0f; var floatingActive = false
    }

    private class ButtonState(val control: ButtonControl) {
        var centerX = 0f; var centerY = 0f; var radius = 0f
        var pointerId = -1; var toggled = false
    }

    private val stickStates = mutableListOf<StickState>()
    private val buttonStates = mutableListOf<ButtonState>()

    // ── MAP button geometry (kept for automap overlay compat) ──
    private var mapBtnCenterX = 0f
    private var mapBtnCenterY = 0f
    private var mapBtnRadius  = 0f
    // ── Music prev/next button geometry ──────────────────────────
    private var musicBtnY = 0f
    private var prevBtnCenterX = 0f
    private var nextBtnCenterX = 0f
    private var musicBtnRadius = 0f
    private var musicLabelX = 0f

    // ── Non-layout pointer tracking ─────────────────────────
    private val passthroughPointers = mutableSetOf<Int>()
    private var mapBtnPointerId = -1  // used by automap overlay
    private var prevBtnPointerId = -1
    private var nextBtnPointerId = -1
    private var musicLabelPointerId = -1

    // ── Automap gesture state (pointers not on stick/buttons) ──
    /** Set to true by the activity when the automap is displayed. */
    var automapActive = false
        set(value) {
            if (field != value) {
                field = value
                invalidate()
            }
        }
    private val automapPointers = mutableMapOf<Int, PointF>()
    private var automapPinchDist = 0f
    private var automapPinchAngle = 0f
    private var automapPinchMidX = 0f
    private var automapPinchMidY = 0f

    /** Called when the automap center button is tapped. */
    var automapCenterCallback: (() -> Unit)? = null
    /** Called with marker index (0-based) when a marker button is tapped. */
    var automapMarkerCallback: ((Int) -> Unit)? = null
    /** Called to query how many markers currently exist. */
    var markerCountProvider: (() -> Int)? = null

    /** Current track label text, set by the activity. */
    var trackLabel: String = ""

    // ── Automap overlay button geometry (computed in onSizeChanged) ──
    private var automapBtnSize = 0f    // button width/height
    private var automapBtnY = 0f       // top of buttons
    private var automapBtnSpacing = 0f // gap between buttons
    private val automapBtnRects = mutableListOf<RectF>() // [0]=center, [1..n]=markers
    private var automapBtnPressed = -1 // index of currently pressed button, -1=none
    private var automapBtnPointerId = -1

    // ── Paint objects ───────────────────────────────────────
    private val paintRing = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3f
        color = 0x66FFFFFF     // semi-transparent white ring
    }
    private val paintFill = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = 0x33FFFFFF     // subtle fill
    }
    private val paintThumb = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = 0x99FFFFFF.toInt()     // brighter thumb dot
    }
    private val paintBtnIdle = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = 0x33FFFFFF     // subtle fill when not pressed
    }
    private val paintBtnPressed = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = 0x66FFFFFF     // brighter when pressed
    }
    private val paintBtnLabel = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = 0xAAFFFFFF.toInt()
        textAlign = Paint.Align.CENTER
    }
    private val paintAutomapHelpText = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = 0xAAFFFFFF.toInt()
        textAlign = Paint.Align.CENTER
    }
    private val paintAutomapBtnBg = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = 0x44FFFFFF
    }
    private val paintAutomapBtnBgPressed = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = 0x88FFFFFF.toInt()
    }
    private val paintAutomapBtnText = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = 0xDDFFFFFF.toInt()
        textAlign = Paint.Align.CENTER
    }

    init { rebuildStates() }

    /** Replace the current layout and recompute all control geometry. */
    fun setLayout(newLayout: TouchLayout) {
        layout = newLayout
        rebuildStates()
        if (width > 0 && height > 0) computeGeometry(width, height)
        invalidate()
    }

    /** Get a copy of the current layout (for saving, etc.). */
    fun getLayout(): TouchLayout = layout

    private fun rebuildStates() {
        stickStates.clear()
        buttonStates.clear()
        layout.sticks.forEach { stickStates.add(StickState(it)) }
        layout.buttons.forEach { buttonStates.add(ButtonState(it)) }
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        computeGeometry(w, h)
    }

    private fun computeGeometry(w: Int, h: Int) {
        val base = min(w, h).toFloat()
        val wf = w.toFloat()
        val hf = h.toFloat()
        val defaultStickRadius = base * 0.15f
        val defaultBtnRadius = base * 0.05f
        paintBtnLabel.textSize = defaultBtnRadius * 0.7f

        // Compute stick geometry from layout
        for (s in stickStates) {
            s.radius = defaultStickRadius * s.control.sizeMult
            s.centerX = wf * s.control.xPct / 100f
            s.centerY = hf * s.control.yPct / 100f
            if (s.control.floating) {
                val z = s.control.floatingZone
                s.fzLeft = wf * z.leftPct / 100f
                s.fzTop = hf * z.topPct / 100f
                s.fzRight = wf * z.rightPct / 100f
                s.fzBottom = hf * z.bottomPct / 100f
            }
        }

        // Compute button geometry from layout; track MAP button for automap compat
        mapBtnRadius = base * 0.035f
        mapBtnCenterX = wf - wf * 0.05f - mapBtnRadius
        mapBtnCenterY = base * 0.05f + mapBtnRadius
        for (b in buttonStates) {
            b.radius = defaultBtnRadius * b.control.sizeMult
            b.centerX = wf * b.control.xPct / 100f
            b.centerY = hf * b.control.yPct / 100f
            if (b.control.binding == TouchBindings.BTN_AUTOMAP) {
                mapBtnCenterX = b.centerX
                mapBtnCenterY = b.centerY
                mapBtnRadius = b.radius
            }
        }

        // Music controls (not layout-driven)
        musicBtnRadius = base * 0.03f
        musicBtnY = base * 0.12f
        prevBtnCenterX = wf * 0.04f + musicBtnRadius
        nextBtnCenterX = prevBtnCenterX + musicBtnRadius * 2 + base * 0.02f + musicBtnRadius
        musicLabelX = nextBtnCenterX + musicBtnRadius + base * 0.02f

        // Automap overlay geometry
        automapBtnSize = base * 0.09f
        automapBtnSpacing = base * 0.02f
        automapBtnY = hf * 0.03f
        paintAutomapBtnText.textSize = automapBtnSize * 0.28f
        paintAutomapHelpText.textSize = base * 0.04f
        recomputeAutomapBtnRects()
    }

    /** Recompute automap button rectangles based on current marker count. */
    private fun recomputeAutomapBtnRects() {
        val count = 1 + (markerCountProvider?.invoke() ?: 0) // center + markers
        automapBtnRects.clear()
        var x = automapBtnSpacing
        for (i in 0 until count) {
            automapBtnRects.add(RectF(x, automapBtnY, x + automapBtnSize, automapBtnY + automapBtnSize))
            x += automapBtnSize + automapBtnSpacing
        }
    }

    // ── Drawing ─────────────────────────────────────────────
    override fun onDraw(canvas: Canvas) {
        if (!isActive) return

        if (automapActive) {
            drawAutomapOverlay(canvas)
            return
        }

        val gAlpha = layout.globalOpacity

        // ── Layout sticks ───────────────────────────────────
        for (s in stickStates) drawStick(canvas, s, gAlpha)

        // ── Layout buttons ──────────────────────────────────
        for (b in buttonStates) drawButton(canvas, b, gAlpha)

        // ── Music prev/next buttons (not layout-driven) ─────
        if (trackLabel.isNotEmpty()) {
            val fillPrev = if (prevBtnPointerId >= 0) paintBtnPressed else paintBtnIdle
            canvas.drawCircle(prevBtnCenterX, musicBtnY, musicBtnRadius, fillPrev)
            canvas.drawCircle(prevBtnCenterX, musicBtnY, musicBtnRadius, paintRing)
            val fillNext = if (nextBtnPointerId >= 0) paintBtnPressed else paintBtnIdle
            canvas.drawCircle(nextBtnCenterX, musicBtnY, musicBtnRadius, fillNext)
            canvas.drawCircle(nextBtnCenterX, musicBtnY, musicBtnRadius, paintRing)
            // Arrow glyphs
            val savedSize = paintBtnLabel.textSize
            paintBtnLabel.textSize = musicBtnRadius * 0.9f
            canvas.drawText("\u25C0", prevBtnCenterX, musicBtnY + paintBtnLabel.textSize * 0.35f, paintBtnLabel)
            canvas.drawText("\u25B6", nextBtnCenterX, musicBtnY + paintBtnLabel.textSize * 0.35f, paintBtnLabel)
            // Track label
            paintBtnLabel.textSize = musicBtnRadius * 0.7f
            val labelPaint = Paint(paintBtnLabel).apply { textAlign = Paint.Align.LEFT }
            canvas.drawText(trackLabel, musicLabelX, musicBtnY + labelPaint.textSize * 0.35f, labelPaint)
            paintBtnLabel.textSize = savedSize
        }
    }

    private fun drawStick(canvas: Canvas, s: StickState, gAlpha: Float) {
        val eff = (gAlpha * s.control.opacity).coerceIn(0f, 1f)
        val cx = if (s.control.floating && s.floatingActive) s.floatingCX else s.centerX
        val cy = if (s.control.floating && s.floatingActive) s.floatingCY else s.centerY

        paintFill.alpha = (0x33 * eff).toInt()
        canvas.drawCircle(cx, cy, s.radius, paintFill)
        paintRing.alpha = (0x66 * eff).toInt()
        canvas.drawCircle(cx, cy, s.radius, paintRing)

        val thumbX = cx + s.pos.x
        val thumbY = cy + s.pos.y
        paintThumb.alpha = (0x99 * eff).toInt()
        canvas.drawCircle(thumbX, thumbY, s.radius * 0.22f, paintThumb)
    }

    private fun drawButton(canvas: Canvas, b: ButtonState, gAlpha: Float) {
        val eff = (gAlpha * b.control.opacity).coerceIn(0f, 1f)
        val pressed = b.pointerId >= 0 || b.toggled
        val fill = if (pressed) paintBtnPressed else paintBtnIdle
        fill.alpha = ((if (pressed) 0x66 else 0x33) * eff).toInt()
        canvas.drawCircle(b.centerX, b.centerY, b.radius, fill)
        paintRing.alpha = (0x66 * eff).toInt()
        canvas.drawCircle(b.centerX, b.centerY, b.radius, paintRing)
        if (b.control.label.isNotEmpty()) {
            paintBtnLabel.alpha = (0xAA * eff).toInt()
            paintBtnLabel.textSize = b.radius * 0.7f
            canvas.drawText(b.control.label, b.centerX,
                b.centerY + paintBtnLabel.textSize * 0.35f, paintBtnLabel)
        }
    }

    private fun drawAutomapOverlay(canvas: Canvas) {
        recomputeAutomapBtnRects()
        val cornerR = automapBtnSize * 0.15f

        for ((i, rect) in automapBtnRects.withIndex()) {
            val bg = if (i == automapBtnPressed) paintAutomapBtnBgPressed else paintAutomapBtnBg
            canvas.drawRoundRect(rect, cornerR, cornerR, bg)
            canvas.drawRoundRect(rect, cornerR, cornerR, paintRing)

            val cx = rect.centerX()
            val cy = rect.centerY()
            if (i == 0) {
                canvas.drawText("center", cx, cy + paintAutomapBtnText.textSize * 0.35f, paintAutomapBtnText)
            } else {
                canvas.drawText("marker $i", cx, cy + paintAutomapBtnText.textSize * 0.35f, paintAutomapBtnText)
            }
        }

        // MAP button (top-right, same position as normal overlay)
        val fillMap = if (mapBtnPointerId >= 0) paintBtnPressed else paintBtnIdle
        canvas.drawCircle(mapBtnCenterX, mapBtnCenterY, mapBtnRadius, fillMap)
        canvas.drawCircle(mapBtnCenterX, mapBtnCenterY, mapBtnRadius, paintRing)
        val savedSize = paintBtnLabel.textSize
        paintBtnLabel.textSize = mapBtnRadius * 0.65f
        canvas.drawText("MAP", mapBtnCenterX, mapBtnCenterY + paintBtnLabel.textSize * 0.35f, paintBtnLabel)
        paintBtnLabel.textSize = savedSize

        // Help text at bottom center
        val helpY = height - height * 0.04f
        canvas.drawText(
            "Touch to spin  \u2022  Pinch to zoom  \u2022  Two-finger drag to slide",
            width / 2f, helpY, paintAutomapHelpText
        )
    }

    // ── Touch handling ──────────────────────────────────────
    // When the overlay is active we consume ALL touches so that nothing
    // leaks through to the game SurfaceView (where it would be
    // interpreted as a mouse click → fire primary).
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!isActive) return false

        // When automap is active, use dedicated automap touch handler
        if (automapActive) return handleAutomapOverlayTouch(event)

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val idx = event.actionIndex
                val px = event.getX(idx)
                val py = event.getY(idx)
                val pid = event.getPointerId(idx)
                var handled = false

                // Try layout sticks
                for (s in stickStates) {
                    if (s.pointerId >= 0) continue
                    if (s.control.floating) {
                        if (px in s.fzLeft..s.fzRight && py in s.fzTop..s.fzBottom) {
                            s.pointerId = pid
                            s.floatingCX = px; s.floatingCY = py
                            s.floatingActive = true
                            s.pos.set(0f, 0f)
                            invalidate()
                            handled = true; break
                        }
                    } else if (hypot(px - s.centerX, py - s.centerY) <= s.radius) {
                        s.pointerId = pid
                        updateStickFromTouch(s, px, py)
                        handled = true; break
                    }
                }

                // Try layout buttons
                if (!handled) {
                    for (b in buttonStates) {
                        if (b.pointerId >= 0) continue
                        if (hypot(px - b.centerX, py - b.centerY) <= b.radius * 1.3f) {
                            b.pointerId = pid
                            if (b.control.toggle) {
                                b.toggled = !b.toggled
                                if (b.control.binding == TouchBindings.BTN_AUTOMAP) {
                                    if (b.toggled) mapButtonCallback?.invoke()
                                } else {
                                    buttonCallback?.invoke(b.control.binding, b.toggled)
                                }
                            } else {
                                // Non-toggle: press on down (MAP fires on release only)
                                if (b.control.binding != TouchBindings.BTN_AUTOMAP) {
                                    buttonCallback?.invoke(b.control.binding, true)
                                }
                            }
                            if (b.control.hapticFeedback) {
                                performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                            }
                            invalidate()
                            handled = true; break
                        }
                    }
                }

                // Try music controls (not layout-driven)
                if (!handled) {
                    when {
                        prevBtnPointerId < 0 && trackLabel.isNotEmpty() &&
                                hypot(px - prevBtnCenterX, py - musicBtnY) <= musicBtnRadius * 1.3f -> {
                            prevBtnPointerId = pid; invalidate(); handled = true
                        }
                        nextBtnPointerId < 0 && trackLabel.isNotEmpty() &&
                                hypot(px - nextBtnCenterX, py - musicBtnY) <= musicBtnRadius * 1.3f -> {
                            nextBtnPointerId = pid; invalidate(); handled = true
                        }
                        musicLabelPointerId < 0 && trackLabel.isNotEmpty() &&
                                px >= musicLabelX && py >= musicBtnY - musicBtnRadius * 1.5f &&
                                py <= musicBtnY + musicBtnRadius * 1.5f -> {
                            musicLabelPointerId = pid; handled = true
                        }
                    }
                }

                if (!handled) passthroughPointers.add(pid)
            }
            MotionEvent.ACTION_MOVE -> {
                for (s in stickStates) {
                    if (s.pointerId >= 0) {
                        val i = event.findPointerIndex(s.pointerId)
                        if (i >= 0) updateStickFromTouch(s, event.getX(i), event.getY(i))
                    }
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                val pid = event.getPointerId(event.actionIndex)
                val fired = event.actionMasked == MotionEvent.ACTION_UP
                if (passthroughPointers.remove(pid) && fired) {
                    tapPassthroughCallback?.invoke()
                }
                passthroughPointers.clear()
                resetAllSticks()
                releaseAllLayoutButtons(fired)
                releasePrevButton(fired)
                releaseNextButton(fired)
                releaseMusicLabel(fired)
            }
            MotionEvent.ACTION_POINTER_UP -> {
                val pid = event.getPointerId(event.actionIndex)
                if (passthroughPointers.remove(pid)) {
                    tapPassthroughCallback?.invoke()
                } else {
                    // Check sticks
                    var found = false
                    for (s in stickStates) {
                        if (s.pointerId == pid) { resetStick(s); found = true; break }
                    }
                    // Check layout buttons
                    if (!found) {
                        for (b in buttonStates) {
                            if (b.pointerId == pid) { releaseLayoutButton(b, true); found = true; break }
                        }
                    }
                    // Check music controls
                    if (!found) when (pid) {
                        prevBtnPointerId -> releasePrevButton(true)
                        nextBtnPointerId -> releaseNextButton(true)
                        musicLabelPointerId -> releaseMusicLabel(true)
                    }
                }
            }
        }
        return true   // always consume when active
    }

    /** Handle touches in automap overlay mode: buttons at top + gesture passthrough. */
    private fun handleAutomapOverlayTouch(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val idx = event.actionIndex
                val px = event.getX(idx)
                val py = event.getY(idx)
                val pid = event.getPointerId(idx)

                // Check MAP button (top-right)
                if (mapBtnPointerId < 0) {
                    val dx = px - mapBtnCenterX; val dy = py - mapBtnCenterY
                    if (dx * dx + dy * dy <= mapBtnRadius * mapBtnRadius * 4) {
                        mapBtnPointerId = pid
                        invalidate()
                        return true
                    }
                }

                // Check automap buttons
                if (automapBtnPointerId < 0) {
                    for ((i, rect) in automapBtnRects.withIndex()) {
                        if (rect.contains(px, py)) {
                            automapBtnPointerId = pid
                            automapBtnPressed = i
                            invalidate()
                            return true
                        }
                    }
                }

                // Not on a button → track as automap gesture
                automapPointers[pid] = PointF(px, py)
                if (automapPointers.size == 2) {
                    automapPinchDist = automapFingerDist(event)
                    automapPinchAngle = automapFingerAngle(event)
                    val mid = automapFingerMidpoint(event)
                    automapPinchMidX = mid.x
                    automapPinchMidY = mid.y
                }
            }

            MotionEvent.ACTION_MOVE -> {
                if (automapPointers.isNotEmpty()) {
                    handleAutomapMove(event)
                }
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                // Fire MAP button if it was pressed
                releaseMapButton(event.actionMasked == MotionEvent.ACTION_UP)
                // Fire automap button if one was pressed
                if (automapBtnPressed >= 0 && event.actionMasked == MotionEvent.ACTION_UP) {
                    fireAutomapButton(automapBtnPressed)
                }
                automapBtnPressed = -1
                automapBtnPointerId = -1
                automapPointers.clear()
                automapPinchDist = 0f
                automapPinchAngle = 0f
                automapPinchMidX = 0f
                automapPinchMidY = 0f
                invalidate()
            }

            MotionEvent.ACTION_POINTER_UP -> {
                val idx = event.actionIndex
                val pid = event.getPointerId(idx)
                if (pid == mapBtnPointerId) {
                    releaseMapButton(true)
                } else if (pid == automapBtnPointerId) {
                    fireAutomapButton(automapBtnPressed)
                    automapBtnPressed = -1
                    automapBtnPointerId = -1
                    invalidate()
                } else {
                    automapPointers.remove(pid)
                    if (automapPointers.size >= 2) {
                        automapPinchDist = automapFingerDist(event)
                        automapPinchAngle = automapFingerAngle(event)
                        val mid = automapFingerMidpoint(event)
                        automapPinchMidX = mid.x
                        automapPinchMidY = mid.y
                    } else {
                        automapPinchDist = 0f
                        automapPinchAngle = 0f
                        automapPinchMidX = 0f
                        automapPinchMidY = 0f
                        for ((id, pt) in automapPointers) {
                            val i = event.findPointerIndex(id)
                            if (i >= 0) pt.set(event.getX(i), event.getY(i))
                        }
                    }
                }
            }
        }
        return true
    }

    private fun fireAutomapButton(index: Int) {
        if (index == 0) {
            automapCenterCallback?.invoke()
        } else {
            automapMarkerCallback?.invoke(index - 1)
        }
    }

    private fun updateStickFromTouch(s: StickState, px: Float, py: Float) {
        val cx = if (s.control.floating && s.floatingActive) s.floatingCX else s.centerX
        val cy = if (s.control.floating && s.floatingActive) s.floatingCY else s.centerY

        var dx = px - cx
        var dy = py - cy
        val dist = hypot(dx, dy)

        // Clamp to circle edge (but still respond to touches outside)
        if (dist > s.radius) {
            val angle = atan2(dy, dx)
            dx = cos(angle) * s.radius
            dy = sin(angle) * s.radius
        }

        s.pos.set(dx, dy)
        invalidate()

        // Normalize to -1..1
        var rawX = (dx / s.radius).coerceIn(-1f, 1f)
        var rawY = (dy / s.radius).coerceIn(-1f, 1f)

        // Apply deadzone
        val dz = s.control.deadzone / 100f
        rawX = applyDeadzone(rawX, dz)
        rawY = applyDeadzone(rawY, dz)

        // Apply response curve (from TouchControl.kt)
        rawX = applyResponseCurve(rawX, s.control.responseCurve, s.control.exponent)
        rawY = applyResponseCurve(rawY, s.control.responseCurve, s.control.exponent)

        // Apply sensitivity
        rawX = (rawX * s.control.sensitivity).coerceIn(-1f, 1f)
        rawY = (rawY * s.control.sensitivity).coerceIn(-1f, 1f)

        // Apply inversion
        if (s.control.invertX) rawX = -rawX
        if (s.control.invertY) rawY = -rawY

        axisCallback?.invoke(s.control.axisX, rawX)
        axisCallback?.invoke(s.control.axisY, rawY)

        // Notify gyro manager that a stick sharing its axes is active
        updateGyroStickActive()
    }

    private fun applyDeadzone(value: Float, deadzone: Float): Float {
        val a = abs(value)
        if (a < deadzone) return 0f
        return sign(value) * (a - deadzone) / (1f - deadzone)
    }

    private fun resetStick(s: StickState) {
        s.pointerId = -1
        s.pos.set(0f, 0f)
        s.floatingActive = false
        invalidate()
        axisCallback?.invoke(s.control.axisX, 0f)
        axisCallback?.invoke(s.control.axisY, 0f)
        updateGyroStickActive()
    }

    /** Update gyro manager's rightStickActive based on whether any stick sharing gyro axes is touched. */
    private fun updateGyroStickActive() {
        val gm = gyroManager ?: return
        val gyro = layout.gyro
        gm.rightStickActive = stickStates.any { s ->
            s.pointerId >= 0 && (s.control.axisX == gyro.axisX || s.control.axisY == gyro.axisY)
        }
    }

    private fun resetAllSticks() {
        for (s in stickStates) if (s.pointerId >= 0) resetStick(s)
    }

    private fun releaseLayoutButton(b: ButtonState, fired: Boolean) {
        if (b.pointerId >= 0) {
            b.pointerId = -1
            if (!b.control.toggle) {
                if (b.control.binding == TouchBindings.BTN_AUTOMAP) {
                    if (fired) mapButtonCallback?.invoke()
                } else {
                    buttonCallback?.invoke(b.control.binding, false)
                }
            }
            invalidate()
        }
    }

    private fun releaseAllLayoutButtons(fired: Boolean) {
        for (b in buttonStates) releaseLayoutButton(b, fired)
    }

    private fun releaseAllButtons() {
        releaseAllLayoutButtons(false)
        releasePrevButton(false)
        releaseNextButton(false)
        releaseMusicLabel(false)
    }

    private fun releasePrevButton(fired: Boolean) {
        if (prevBtnPointerId >= 0) {
            prevBtnPointerId = -1
            invalidate()
            if (fired) prevTrackCallback?.invoke()
        }
    }

    private fun releaseNextButton(fired: Boolean) {
        if (nextBtnPointerId >= 0) {
            nextBtnPointerId = -1
            invalidate()
            if (fired) nextTrackCallback?.invoke()
        }
    }

    private fun releaseMusicLabel(fired: Boolean) {
        if (musicLabelPointerId >= 0) {
            musicLabelPointerId = -1
            if (fired) musicPanelCallback?.invoke()
        }
    }

    private fun releaseMapButton(fired: Boolean) {
        if (mapBtnPointerId >= 0) {
            mapBtnPointerId = -1
            invalidate()
            if (fired) mapButtonCallback?.invoke()
        }
    }

    // ── Automap gesture helpers ─────────────────────────────

    /** Process MOVE events for automap pointers (drag → pan/tilt, pinch → thrust+rotate). */
    private fun handleAutomapMove(event: MotionEvent) {
        val w = width.toFloat()
        val h = height.toFloat()
        if (w <= 0f || h <= 0f) return

        if (automapPointers.size == 1) {
            val pid = automapPointers.keys.first()
            val idx = event.findPointerIndex(pid)
            if (idx >= 0) {
                val prev = automapPointers[pid]!!
                val dx = event.getX(idx) - prev.x
                val dy = event.getY(idx) - prev.y
                prev.set(event.getX(idx), event.getY(idx))
                if (dx != 0f || dy != 0f) {
                    // Single finger → pan / tilt
                    automapInputCallback?.invoke(dx / w, dy / h, 0f, 0f, 0f, 0f)
                }
            }
        } else if (automapPointers.size >= 2) {
            // Two+ fingers → pinch = zoom + rotate + slide
            for ((pid, pt) in automapPointers) {
                val idx = event.findPointerIndex(pid)
                if (idx >= 0) pt.set(event.getX(idx), event.getY(idx))
            }
            val dist = automapFingerDist(event)
            val angle = automapFingerAngle(event)
            val mid = automapFingerMidpoint(event)
            if (automapPinchDist > 0f) {
                val delta = dist - automapPinchDist
                val thrust = delta / w * 20f

                var dAngle = angle - automapPinchAngle
                while (dAngle > Math.PI.toFloat())  dAngle -= (2 * Math.PI).toFloat()
                while (dAngle < -Math.PI.toFloat()) dAngle += (2 * Math.PI).toFloat()
                val bank = dAngle / Math.PI.toFloat()

                val sideways = (mid.x - automapPinchMidX) / w
                val vertical = -(mid.y - automapPinchMidY) / h

                if (thrust != 0f || bank != 0f || sideways != 0f || vertical != 0f) {
                    automapInputCallback?.invoke(0f, 0f, thrust, bank, vertical, sideways)
                }
            }
            automapPinchDist = dist
            automapPinchAngle = angle
            automapPinchMidX = mid.x
            automapPinchMidY = mid.y
        }
    }

    /** Euclidean distance between the first two automap pointers. */
    private fun automapFingerDist(event: MotionEvent): Float {
        val ids = automapPointers.keys.toList()
        if (ids.size < 2) return 0f
        val i0 = event.findPointerIndex(ids[0])
        val i1 = event.findPointerIndex(ids[1])
        if (i0 < 0 || i1 < 0) return 0f
        val dx = event.getX(i0) - event.getX(i1)
        val dy = event.getY(i0) - event.getY(i1)
        return hypot(dx, dy)
    }

    /** Angle (radians) from the first automap pointer to the second. */
    private fun automapFingerAngle(event: MotionEvent): Float {
        val ids = automapPointers.keys.toList()
        if (ids.size < 2) return 0f
        val i0 = event.findPointerIndex(ids[0])
        val i1 = event.findPointerIndex(ids[1])
        if (i0 < 0 || i1 < 0) return 0f
        val dx = event.getX(i1) - event.getX(i0)
        val dy = event.getY(i1) - event.getY(i0)
        return atan2(dy, dx)
    }

    /** Midpoint between the first two automap pointers. */
    private fun automapFingerMidpoint(event: MotionEvent): PointF {
        val ids = automapPointers.keys.toList()
        if (ids.size < 2) return PointF(0f, 0f)
        val i0 = event.findPointerIndex(ids[0])
        val i1 = event.findPointerIndex(ids[1])
        if (i0 < 0 || i1 < 0) return PointF(0f, 0f)
        return PointF(
            (event.getX(i0) + event.getX(i1)) / 2f,
            (event.getY(i0) + event.getY(i1)) / 2f
        )
    }
}

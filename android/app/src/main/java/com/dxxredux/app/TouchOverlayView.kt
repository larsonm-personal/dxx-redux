package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.PointF
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.min
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

    /** Called with (heading, pitch, thrust, bank, vertical, sideways) when automap gestures are detected.
     *  heading/pitch/bank are fractions of screen dimension; thrust is fraction of screen width;
     *  vertical/sideways are fractions of screen dimension. */
    var automapInputCallback: ((Float, Float, Float, Float, Float, Float) -> Unit)? = null

    /** Whether the overlay should be visible and active. */
    var isActive: Boolean = false
        set(value) {
            if (field != value) {
                field = value
                visibility = if (value) VISIBLE else GONE
                if (!value) {
                    resetStick()
                    releaseAllButtons()
                }
                invalidate()
            }
        }

    // ── Stick geometry (computed in onSizeChanged) ──────────
    private var stickCenterX = 0f
    private var stickCenterY = 0f
    private var stickRadius  = 0f

    // ── Fire-button geometry ────────────────────────────────
    private var btn0CenterX = 0f   // fire primary
    private var btn0CenterY = 0f
    private var btn1CenterX = 0f   // fire secondary
    private var btn1CenterY = 0f
    private var btnRadius   = 0f
    // ── MAP button geometry ───────────────────────────────────────
    private var mapBtnCenterX = 0f
    private var mapBtnCenterY = 0f
    private var mapBtnRadius  = 0f
    // ── Current stick state ─────────────────────────────────
    private var stickPointerId = -1    // active pointer ID, -1 = no touch
    private val stickPos = PointF()    // current stick position (in px, relative to center)

    // ── Button pointer tracking (one pointer per button) ────
    private var btn0PointerId = -1
    private var btn1PointerId = -1
    private var mapBtnPointerId = -1

    // ── Automap gesture state (pointers not on stick/buttons) ──
    /** Set to true by the activity when the automap is displayed. */
    var automapActive = false
    private val automapPointers = mutableMapOf<Int, PointF>()
    private var automapPinchDist = 0f
    private var automapPinchAngle = 0f

    // Double-tap → translate mode
    private var automapLastTapTime = 0L
    private var automapLastTapPos = PointF()
    private var automapTranslateMode = false

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

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)

        // ── Stick: ~15% of screen width radius, 10% margins ──
        stickRadius = w * 0.15f
        val marginX = w * 0.10f + stickRadius
        val marginY = h * 0.10f + stickRadius
        stickCenterX = marginX
        stickCenterY = h - marginY

        // ── Fire buttons: 5% screen width radius ──
        btnRadius = w * 0.05f
        paintBtnLabel.textSize = btnRadius * 0.7f

        // Button 0 (fire primary): 10% from right, 5% from bottom
        btn0CenterX = w - w * 0.10f - btnRadius
        btn0CenterY = h - w * 0.05f - btnRadius

        // Button 1 (fire secondary): 5% from right, 10% from bottom
        btn1CenterX = w - w * 0.05f - btnRadius
        btn1CenterY = h - w * 0.10f - btnRadius

        // MAP button: smaller, top-right corner
        mapBtnRadius = w * 0.035f
        mapBtnCenterX = w - w * 0.05f - mapBtnRadius
        mapBtnCenterY = w * 0.05f + mapBtnRadius
    }

    // ── Drawing ─────────────────────────────────────────────
    override fun onDraw(canvas: Canvas) {
        if (!isActive) return

        // ── Analog stick ────────────────────────────────────
        canvas.drawCircle(stickCenterX, stickCenterY, stickRadius, paintFill)
        canvas.drawCircle(stickCenterX, stickCenterY, stickRadius, paintRing)
        val thumbX = stickCenterX + stickPos.x
        val thumbY = stickCenterY + stickPos.y
        val thumbRadius = stickRadius * 0.22f
        canvas.drawCircle(thumbX, thumbY, thumbRadius, paintThumb)

        // ── Fire button 0 (primary) ─────────────────────────
        val fill0 = if (btn0PointerId >= 0) paintBtnPressed else paintBtnIdle
        canvas.drawCircle(btn0CenterX, btn0CenterY, btnRadius, fill0)
        canvas.drawCircle(btn0CenterX, btn0CenterY, btnRadius, paintRing)
        canvas.drawText("A", btn0CenterX, btn0CenterY + paintBtnLabel.textSize * 0.35f, paintBtnLabel)

        // ── Fire button 1 (secondary) ─────────────────────
        val fill1 = if (btn1PointerId >= 0) paintBtnPressed else paintBtnIdle
        canvas.drawCircle(btn1CenterX, btn1CenterY, btnRadius, fill1)
        canvas.drawCircle(btn1CenterX, btn1CenterY, btnRadius, paintRing)
        canvas.drawText("B", btn1CenterX, btn1CenterY + paintBtnLabel.textSize * 0.35f, paintBtnLabel)

        // ── MAP button ───────────────────────────────────────
        val fillMap = if (mapBtnPointerId >= 0) paintBtnPressed else paintBtnIdle
        canvas.drawCircle(mapBtnCenterX, mapBtnCenterY, mapBtnRadius, fillMap)
        canvas.drawCircle(mapBtnCenterX, mapBtnCenterY, mapBtnRadius, paintRing)
        val savedSize = paintBtnLabel.textSize
        paintBtnLabel.textSize = mapBtnRadius * 0.65f
        canvas.drawText("MAP", mapBtnCenterX, mapBtnCenterY + paintBtnLabel.textSize * 0.35f, paintBtnLabel)
        paintBtnLabel.textSize = savedSize
    }

    // ── Touch handling ──────────────────────────────────────
    // When the overlay is active we consume ALL touches so that nothing
    // leaks through to the game SurfaceView (where it would be
    // interpreted as a mouse click → fire primary).
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!isActive) return false

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val idx = event.actionIndex
                val px = event.getX(idx)
                val py = event.getY(idx)
                val pid = event.getPointerId(idx)

                when {
                    stickPointerId < 0 && isInsideStick(px, py) -> {
                        stickPointerId = pid
                        updateStickFromTouch(px, py)
                    }
                    btn0PointerId < 0 && isInsideButton(px, py, btn0CenterX, btn0CenterY) -> {
                        btn0PointerId = pid
                        buttonCallback?.invoke(0, true)
                        invalidate()
                    }
                    btn1PointerId < 0 && isInsideButton(px, py, btn1CenterX, btn1CenterY) -> {
                        btn1PointerId = pid
                        buttonCallback?.invoke(1, true)
                        invalidate()
                    }
                    mapBtnPointerId < 0 && isInsideButton(px, py, mapBtnCenterX, mapBtnCenterY, mapBtnRadius) -> {
                        mapBtnPointerId = pid
                        invalidate()
                    }
                    else -> {
                        // Touch not on any control — track for automap gestures
                        if (automapActive) {
                            automapPointers[pid] = PointF(px, py)
                            // Check for double-tap on first automap finger
                            if (automapPointers.size == 1) {
                                val now = android.os.SystemClock.uptimeMillis()
                                val dt = now - automapLastTapTime
                                val dist = hypot(px - automapLastTapPos.x, py - automapLastTapPos.y)
                                if (dt < 300L && dist < 80f) {
                                    automapTranslateMode = true
                                }
                            }
                            if (automapPointers.size == 2) {
                                automapPinchDist = automapFingerDist(event)
                                automapPinchAngle = automapFingerAngle(event)
                                automapTranslateMode = false
                            }
                        }
                    }
                }
            }
            MotionEvent.ACTION_MOVE -> {
                // Update stick if its pointer moved
                if (stickPointerId >= 0) {
                    val idx = event.findPointerIndex(stickPointerId)
                    if (idx >= 0) {
                        updateStickFromTouch(event.getX(idx), event.getY(idx))
                    }
                }
                // Update automap gestures for unmatched pointers
                if (automapActive && automapPointers.isNotEmpty()) {
                    handleAutomapMove(event)
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (stickPointerId >= 0) resetStick()
                releaseButton0()
                releaseButton1()
                releaseMapButton(event.actionMasked == MotionEvent.ACTION_UP)
                automapPointers.clear()
                automapPinchDist = 0f
                automapPinchAngle = 0f
                automapTranslateMode = false
            }
            MotionEvent.ACTION_POINTER_UP -> {
                val idx = event.actionIndex
                val pid = event.getPointerId(idx)
                when (pid) {
                    stickPointerId -> resetStick()
                    btn0PointerId  -> releaseButton0()
                    btn1PointerId  -> releaseButton1()
                    mapBtnPointerId -> releaseMapButton(true)
                    else -> {
                        // Automap pointer lifted
                        // Record tap time for double-tap detection if single automap finger lifts
                        if (automapPointers.size == 1 && !automapTranslateMode) {
                            automapLastTapTime = android.os.SystemClock.uptimeMillis()
                            val i = event.findPointerIndex(pid)
                            if (i >= 0) automapLastTapPos.set(event.getX(i), event.getY(i))
                        }
                        automapPointers.remove(pid)
                        if (automapPointers.size >= 2) {
                            automapPinchDist = automapFingerDist(event)
                            automapPinchAngle = automapFingerAngle(event)
                        } else {
                            automapPinchDist = 0f
                            automapPinchAngle = 0f
                        }
                        if (automapPointers.isEmpty()) automapTranslateMode = false
                        // Refresh remaining pointer positions to avoid jump
                        for ((id, pt) in automapPointers) {
                            val i = event.findPointerIndex(id)
                            if (i >= 0) pt.set(event.getX(i), event.getY(i))
                        }
                    }
                }
            }
        }
        return true   // always consume when active
    }

    private fun isInsideStick(px: Float, py: Float): Boolean {
        return hypot(px - stickCenterX, py - stickCenterY) <= stickRadius
    }

    private fun isInsideButton(px: Float, py: Float, cx: Float, cy: Float, radius: Float = btnRadius): Boolean {
        return hypot(px - cx, py - cy) <= radius * 1.3f   // slightly generous hit area
    }

    private fun updateStickFromTouch(px: Float, py: Float) {
        var dx = px - stickCenterX
        var dy = py - stickCenterY
        val dist = hypot(dx, dy)

        // Clamp to circle edge (but still respond to touches outside)
        if (dist > stickRadius) {
            val angle = atan2(dy, dx)
            dx = cos(angle) * stickRadius
            dy = sin(angle) * stickRadius
        }

        stickPos.set(dx, dy)
        invalidate()

        // Convert to -1..1 range
        val axisX = (dx / stickRadius).coerceIn(-1f, 1f)
        val axisY = (dy / stickRadius).coerceIn(-1f, 1f)
        axisCallback?.invoke(0, axisX)   // axis 0 = left stick X
        axisCallback?.invoke(1, axisY)   // axis 1 = left stick Y
    }

    private fun resetStick() {
        stickPointerId = -1
        stickPos.set(0f, 0f)
        invalidate()
        axisCallback?.invoke(0, 0f)
        axisCallback?.invoke(1, 0f)
    }

    private fun releaseButton0() {
        if (btn0PointerId >= 0) {
            btn0PointerId = -1
            buttonCallback?.invoke(0, false)
            invalidate()
        }
    }

    private fun releaseButton1() {
        if (btn1PointerId >= 0) {
            btn1PointerId = -1
            buttonCallback?.invoke(1, false)
            invalidate()
        }
    }

    private fun releaseAllButtons() {
        releaseButton0()
        releaseButton1()
        releaseMapButton(false)
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
                    if (automapTranslateMode) {
                        // Double-tap drag → translate x/y
                        val sideways = dx / w
                        val vertical = -dy / h
                        automapInputCallback?.invoke(0f, 0f, 0f, 0f, vertical, sideways)
                    } else {
                        // Single finger → pan / tilt
                        automapInputCallback?.invoke(dx / w, dy / h, 0f, 0f, 0f, 0f)
                    }
                }
            }
        } else if (automapPointers.size >= 2) {
            // Two+ fingers → pinch = thrust + rotate = bank
            // Update all automap pointer positions first
            for ((pid, pt) in automapPointers) {
                val idx = event.findPointerIndex(pid)
                if (idx >= 0) pt.set(event.getX(idx), event.getY(idx))
            }
            val dist = automapFingerDist(event)
            val angle = automapFingerAngle(event)
            if (automapPinchDist > 0f) {
                val delta = dist - automapPinchDist
                // 20× multiplier for usable zoom rate on touch screens
                val thrust = delta / w * 20f

                // Rotation: delta angle (radians) → bank
                var dAngle = angle - automapPinchAngle
                while (dAngle > Math.PI.toFloat())  dAngle -= (2 * Math.PI).toFloat()
                while (dAngle < -Math.PI.toFloat()) dAngle += (2 * Math.PI).toFloat()
                val bank = dAngle / Math.PI.toFloat()

                if (thrust != 0f || bank != 0f) {
                    automapInputCallback?.invoke(0f, 0f, thrust, bank, 0f, 0f)
                }
            }
            automapPinchDist = dist
            automapPinchAngle = angle
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
}

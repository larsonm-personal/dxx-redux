package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.PointF
import android.graphics.RectF
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

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)

        // Use the shorter dimension so controls stay the same absolute
        // size regardless of portrait/landscape orientation.
        val base = min(w, h).toFloat()

        // ── Stick: ~15% of short-edge radius, 10% margins ──
        stickRadius = base * 0.15f
        val marginX = w * 0.10f + stickRadius
        val marginY = h * 0.10f + stickRadius
        stickCenterX = marginX
        stickCenterY = h - marginY

        // ── Fire buttons: 5% of short-edge radius ──
        btnRadius = base * 0.05f
        paintBtnLabel.textSize = btnRadius * 0.7f

        // Button 0 (fire primary): 10% from right, 5% from bottom
        btn0CenterX = w - w * 0.10f - btnRadius
        btn0CenterY = h - base * 0.05f - btnRadius

        // Button 1 (fire secondary): 5% from right, 10% from bottom
        btn1CenterX = w - w * 0.05f - btnRadius
        btn1CenterY = h - base * 0.10f - btnRadius

        // MAP button: smaller, top-right corner
        mapBtnRadius = base * 0.035f
        mapBtnCenterX = w - w * 0.05f - mapBtnRadius
        mapBtnCenterY = base * 0.05f + mapBtnRadius

        // ── Automap overlay geometry ──
        automapBtnSize = base * 0.09f
        automapBtnSpacing = base * 0.02f
        automapBtnY = h * 0.03f
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
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (stickPointerId >= 0) resetStick()
                releaseButton0()
                releaseButton1()
                releaseMapButton(event.actionMasked == MotionEvent.ACTION_UP)
            }
            MotionEvent.ACTION_POINTER_UP -> {
                val idx = event.actionIndex
                val pid = event.getPointerId(idx)
                when (pid) {
                    stickPointerId -> resetStick()
                    btn0PointerId  -> releaseButton0()
                    btn1PointerId  -> releaseButton1()
                    mapBtnPointerId -> releaseMapButton(true)
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

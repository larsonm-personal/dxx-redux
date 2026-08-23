package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import kotlin.math.abs
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

internal const val MENU_POINT_TAPPABLE = 1
internal const val MENU_POINT_SCROLL_OWNED = 2
private const val MENU_POINT_NATIVE_OWNED = MENU_POINT_TAPPABLE or MENU_POINT_SCROLL_OWNED

internal fun menuPinchAllowed(
    firstFlags: Int,
    secondFlags: Int,
): Boolean = (firstFlags or secondFlags) and MENU_POINT_NATIVE_OWNED == 0

internal data class MenuViewportIntent(
    val zoom: Float,
    val panFraction: Float,
)

internal fun updateMenuPinchIntent(
    oldZoom: Float,
    oldPanFraction: Float,
    oldFocalFraction: Float,
    newFocalFraction: Float,
    scaleRatio: Float,
): MenuViewportIntent {
    val newZoom = (oldZoom * scaleRatio).coerceIn(0.25f, 3f)
    val appliedRatio = if (oldZoom > 0f) newZoom / oldZoom else 1f
    val oldCenter = 0.5f + oldPanFraction
    val newCenter = newFocalFraction - (oldFocalFraction - oldCenter) * appliedRatio
    return MenuViewportIntent(newZoom, (newCenter - 0.5f).coerceIn(-1f, 1f))
}

class MenuInteractionOverlayView(
    context: Context,
) : View(context) {
    var backCallback: (() -> Unit)? = null
    var exitCallback: (() -> Unit)? = null
    var pointFlagsCallback: ((Float, Float) -> Int)? = null
    var nativeTouchCallback: ((Int, Float, Float) -> Unit)? = null
    var viewportCallback: ((Int, Int) -> Unit)? = null
    var viewportResetCallback: (() -> Unit)? = null

    var showBack = false
        set(value) {
            if (field != value) {
                field = value
                cancelGesture()
                updateVisibility()
                invalidate()
            }
        }
    var showExit = true
        set(value) {
            if (field != value) {
                field = value
                updateVisibility()
                invalidate()
            }
        }
    var keyboardActive = false
    var bottomInsetPx = 0
        set(value) {
            val clamped = value.coerceAtLeast(0)
            if (field != clamped) {
                field = clamped
                updateGeometry(width, height)
                invalidate()
            }
        }

    private val backgroundPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x44000000
            style = Paint.Style.FILL
        }
    private val pressedPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x88444444.toInt()
            style = Paint.Style.FILL
        }
    private val borderPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x88FFFFFF.toInt()
            style = Paint.Style.STROKE
        }
    private val textPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xDDFFFFFF.toInt()
            textAlign = Paint.Align.CENTER
            isFakeBoldText = true
        }

    private enum class Gesture {
        IDLE,
        BACK_BUTTON,
        EXIT_BUTTON,
        BACKGROUND_CANDIDATE,
        PAN,
        PINCH,
        CANCELLED,
    }

    private var gesture = Gesture.IDLE
    private var primaryPointerId = MotionEvent.INVALID_POINTER_ID
    private var downX = 0f
    private var downY = 0f
    private var lastY = 0f
    private var lastSpan = 0f
    private var lastFocalY = 0f
    private var zoom = 1f
    private var panFraction = 0f
    private var viewportPostPending = false
    private var backX = 0f
    private var backY = 0f
    private var backRadius = 0f
    private var exitX = 0f
    private var exitY = 0f
    private var exitRadius = 0f
    private val touchSlop = ViewConfiguration.get(context).scaledTouchSlop.toFloat()
    private val density = resources.displayMetrics.density

    init {
        isClickable = true
        contentDescription = "Menu Back and Exit controls"
        updateVisibility()
    }

    override fun onSizeChanged(
        w: Int,
        h: Int,
        oldW: Int,
        oldH: Int,
    ) {
        super.onSizeChanged(w, h, oldW, oldH)
        updateGeometry(w, h)
    }

    private fun updateGeometry(
        w: Int,
        h: Int,
    ) {
        if (w <= 0 || h <= 0) return
        val shortSide = min(w, h).toFloat()
        val exitDiameter = shortSide * 0.07f
        exitRadius = exitDiameter / 2f
        val exitMargin = exitRadius * 0.5f
        exitX = exitRadius + exitMargin
        exitY = exitRadius + exitMargin

        val backDiameter = (shortSide * 0.14f).coerceIn(56f * density, 104f * density)
        backRadius = backDiameter / 2f
        val backMargin = max(backRadius * 0.35f, 8f * density)
        backX = w - backRadius - backMargin
        backY = h - bottomInsetPx - backRadius - backMargin
        borderPaint.strokeWidth = max(2f, exitRadius * 0.04f)
    }

    override fun onDraw(canvas: Canvas) {
        if (showExit) drawButton(canvas, exitX, exitY, exitRadius, "EXIT", gesture == Gesture.EXIT_BUTTON)
        if (showBack) drawButton(canvas, backX, backY, backRadius, "BACK", gesture == Gesture.BACK_BUTTON)
    }

    private fun drawButton(
        canvas: Canvas,
        x: Float,
        y: Float,
        radius: Float,
        label: String,
        pressed: Boolean,
    ) {
        canvas.drawCircle(x, y, radius, if (pressed) pressedPaint else backgroundPaint)
        canvas.drawCircle(x, y, radius, borderPaint)
        textPaint.textSize = radius * if (label == "BACK") 0.42f else 0.6f
        canvas.drawText(label, x, y - (textPaint.descent() + textPaint.ascent()) / 2f, textPaint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (width <= 0 || height <= 0) return false
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                return handleDown(event)
            }

            MotionEvent.ACTION_POINTER_DOWN -> {
                return handlePointerDown(event)
            }

            MotionEvent.ACTION_MOVE -> {
                return handleMove(event)
            }

            MotionEvent.ACTION_POINTER_UP -> {
                return handlePointerUp(event)
            }

            MotionEvent.ACTION_UP -> {
                return handleUp(event)
            }

            MotionEvent.ACTION_CANCEL -> {
                cancelGesture()
                return true
            }
        }
        return gesture != Gesture.IDLE
    }

    private fun handleDown(event: MotionEvent): Boolean {
        if (showBack && inside(event.x, event.y, backX, backY, backRadius)) {
            gesture = Gesture.BACK_BUTTON
            invalidate()
            return true
        }
        if (showExit && inside(event.x, event.y, exitX, exitY, exitRadius)) {
            gesture = Gesture.EXIT_BUTTON
            invalidate()
            return true
        }
        if (!showBack || keyboardActive || event.x < 40f * density) return false
        val flags = pointFlags(event.x, event.y)
        if (flags and MENU_POINT_NATIVE_OWNED != 0) return false
        gesture = Gesture.BACKGROUND_CANDIDATE
        primaryPointerId = event.getPointerId(0)
        downX = event.x
        downY = event.y
        lastY = event.y
        return true
    }

    private fun handlePointerDown(event: MotionEvent): Boolean {
        if (gesture != Gesture.BACKGROUND_CANDIDATE && gesture != Gesture.PAN) return true
        if (event.pointerCount < 2) return true
        val first = event.findPointerIndex(primaryPointerId).takeIf { it >= 0 } ?: 0
        val second = event.actionIndex
        if (!menuPinchAllowed(
                pointFlags(event.getX(first), event.getY(first)),
                pointFlags(event.getX(second), event.getY(second)),
            )
        ) {
            gesture = Gesture.CANCELLED
            return true
        }
        lastSpan = pointerSpan(event, first, second)
        lastFocalY = (event.getY(first) + event.getY(second)) * 0.5f
        gesture = Gesture.PINCH
        return true
    }

    private fun handleMove(event: MotionEvent): Boolean {
        when (gesture) {
            Gesture.BACK_BUTTON, Gesture.EXIT_BUTTON -> {
                invalidate()
            }

            Gesture.BACKGROUND_CANDIDATE -> {
                val index = event.findPointerIndex(primaryPointerId)
                if (index < 0) return true
                val dx = event.getX(index) - downX
                val dy = event.getY(index) - downY
                if (hypot(dx, dy) >= touchSlop) {
                    if (abs(dy) > abs(dx)) {
                        gesture = Gesture.PAN
                        lastY = event.getY(index)
                    } else {
                        gesture = Gesture.CANCELLED
                    }
                }
            }

            Gesture.PAN -> {
                val index = event.findPointerIndex(primaryPointerId)
                if (index >= 0) {
                    val y = event.getY(index)
                    panFraction = (panFraction + (y - lastY) / height).coerceIn(-1f, 1f)
                    lastY = y
                    queueViewport()
                }
            }

            Gesture.PINCH -> {
                updatePinch(event)
            }

            else -> {}
        }
        return true
    }

    private fun updatePinch(event: MotionEvent) {
        if (event.pointerCount < 2) return
        val first = 0
        val second = 1
        val span = pointerSpan(event, first, second)
        val focalY = (event.getY(first) + event.getY(second)) * 0.5f
        if (lastSpan > 0f && span > 0f) {
            val intent =
                updateMenuPinchIntent(
                    zoom,
                    panFraction,
                    lastFocalY / height,
                    focalY / height,
                    span / lastSpan,
                )
            zoom = intent.zoom
            panFraction = intent.panFraction
            queueViewport()
        }
        lastSpan = span
        lastFocalY = focalY
    }

    private fun handlePointerUp(event: MotionEvent): Boolean {
        if (gesture == Gesture.PINCH) gesture = Gesture.CANCELLED
        return true
    }

    private fun handleUp(event: MotionEvent): Boolean {
        when (gesture) {
            Gesture.BACK_BUTTON -> {
                if (inside(event.x, event.y, backX, backY, backRadius)) {
                    performClick()
                    backCallback?.invoke()
                }
            }

            Gesture.EXIT_BUTTON -> {
                if (inside(event.x, event.y, exitX, exitY, exitRadius)) {
                    performClick()
                    exitCallback?.invoke()
                }
            }

            Gesture.BACKGROUND_CANDIDATE -> {
                replayBackgroundTap(event.x, event.y)
            }

            else -> {}
        }
        cancelGesture()
        return true
    }

    private fun replayBackgroundTap(
        upX: Float,
        upY: Float,
    ) {
        nativeTouchCallback?.invoke(0, normalizedX(downX), normalizedY(downY))
        nativeTouchCallback?.invoke(2, normalizedX(upX), normalizedY(upY))
    }

    private fun pointFlags(
        x: Float,
        y: Float,
    ): Int = pointFlagsCallback?.invoke(normalizedX(x), normalizedY(y)) ?: MENU_POINT_NATIVE_OWNED

    private fun normalizedX(x: Float): Float = (x / width).coerceIn(0f, 1f)

    private fun normalizedY(y: Float): Float = (y / height).coerceIn(0f, 1f)

    private fun pointerSpan(
        event: MotionEvent,
        first: Int,
        second: Int,
    ): Float = hypot(event.getX(first) - event.getX(second), event.getY(first) - event.getY(second))

    private fun inside(
        x: Float,
        y: Float,
        centerX: Float,
        centerY: Float,
        radius: Float,
    ): Boolean {
        val dx = x - centerX
        val dy = y - centerY
        return dx * dx + dy * dy <= radius * radius * 1.5f
    }

    private fun queueViewport() {
        if (viewportPostPending) return
        viewportPostPending = true
        postOnAnimation {
            viewportPostPending = false
            viewportCallback?.invoke((zoom * 1000f).roundToInt(), (panFraction * 10000f).roundToInt())
        }
    }

    fun resetViewport() {
        zoom = 1f
        panFraction = 0f
        viewportPostPending = false
        viewportResetCallback?.invoke()
        cancelGesture()
    }

    fun centerPanForMenuChange() {
        panFraction = 0f
        viewportCallback?.invoke((zoom * 1000f).roundToInt(), 0)
        cancelGesture()
    }

    fun cancelGesture() {
        gesture = Gesture.IDLE
        primaryPointerId = MotionEvent.INVALID_POINTER_ID
        lastSpan = 0f
        invalidate()
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    private fun updateVisibility() {
        visibility = if (showBack || showExit) VISIBLE else GONE
    }
}

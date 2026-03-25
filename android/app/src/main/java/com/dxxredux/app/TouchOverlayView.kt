package com.dxxredux.app

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.PointF
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.util.AttributeSet
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import android.view.animation.DecelerateInterpolator
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
class TouchOverlayView
    @JvmOverloads
    constructor(
        context: Context,
        attrs: AttributeSet? = null,
    ) : View(context, attrs) {
        /** Called with (axisIndex, value) when the stick moves. */
        var axisCallback: ((Int, Float) -> Unit)? = null

        /** Called with (buttonIndex, pressed) when a fire button is touched/released. */
        var buttonCallback: ((Int, Boolean) -> Unit)? = null

        /** Called with (metaActionId, pressed) for meta action dispatch. */
        var metaActionCallback: ((Int, Boolean) -> Unit)? = null

        /** Called when the MAP button is tapped (toggles automap). */
        var mapButtonCallback: (() -> Unit)? = null

        /** Called to skip to the previous music track. */
        var prevTrackCallback: (() -> Unit)? = null

        /** Called to skip to the next music track. */
        var nextTrackCallback: (() -> Unit)? = null

        /** Called when the track info area is tapped (opens music panel). */
        var musicPanelCallback: (() -> Unit)? = null

        /** Called with (action, androidKeyCode, unicodeChar) for radial menu key injection. */
        var keyCallback: ((Int, Int, Int) -> Unit)? = null

        /** Called with a cheat code string to inject each char via nativeTextInput. */
        var cheatCodeCallback: ((String) -> Unit)? = null

        /** "d1" or "d2" — determines which cheat list to show. Set by MainActivity. */
        var gameVariant: String = "d2"

        /** Returns current weapon state for weapon wheels. Set by MainActivity. */
        var weaponStateProvider: (() -> WeaponState?)? = null

        /** Called when a tap lands outside all overlay controls (pass-through for "press any key" screens). */
        var tapPassthroughCallback: (() -> Unit)? = null

        /** Called with (heading, pitch, thrust, bank, vertical, sideways) when automap gestures are detected.
         *  heading/pitch/bank are fractions of screen dimension; thrust is fraction of screen width;
         *  vertical/sideways are fractions of screen dimension. */
        var automapInputCallback: ((Float, Float, Float, Float, Float, Float) -> Unit)? = null

        /** Optional gyro manager — set by MainActivity to enable TOUCH_STICK activation. */
        var gyroManager: GyroInputManager? = null

        /** Update gyro diagnostic values (called from sensor thread). */
        fun updateGyroDiagnostic(
            yaw: Float,
            pitch: Float,
            roll: Float,
        ) {
            diagGyroYaw = yaw
            diagGyroPitch = pitch
            diagGyroRoll = roll
            if (diagnosticStates.isNotEmpty()) postInvalidate()
        }

        /** Whether the overlay should be visible and active. */
        var isActive: Boolean = false
            set(value) {
                if (field != value) {
                    field = value
                    visibility = if (value) VISIBLE else GONE
                    if (!value) {
                        resetAllSticks()
                        releaseAllButtons()
                        stopMouseDrain()
                    }
                    invalidate()
                }
            }

        // ── Layout state ──────────────────────────────────────
        private var layout: TouchLayout = TouchLayoutRepository.defaultLayout()

        // ── Computed control states ──────────────────────────────
        private class StickState(
            val control: AnalogStickControl,
        ) {
            var centerX = 0f
            var centerY = 0f
            var radius = 0f
            var pointerId = -1
            val pos = PointF() // thumb offset in px from center

            // Floating mode: zone bounds in px
            var fzLeft = 0f
            var fzTop = 0f
            var fzRight = 0f
            var fzBottom = 0f
            var floatingCX = 0f
            var floatingCY = 0f
            var floatingActive = false

            // Mouse mode: accumulated drag delta (pixels, not yet consumed)
            var mouseLastX = 0f
            var mouseLastY = 0f
            var mousePendingX = 0f
            var mousePendingY = 0f

            // Mouse mode: touch-down origin for exponential scaling
            var mouseOriginX = 0f
            var mouseOriginY = 0f

            // Button mode: direction press tracking
            var xNegPressed = false
            var xPosPressed = false
            var yNegPressed = false
            var yPosPressed = false

            // Double-tap tracking
            var lastTapTime: Long = 0
        }

        private class ButtonState(
            val control: ButtonControl,
        ) {
            var centerX = 0f
            var centerY = 0f
            var radius = 0f
            var pointerId = -1
            var toggled = false
        }

        private class RadialMenuState(
            val control: RadialMenuControl,
        ) {
            var triggerX = 0f
            var triggerY = 0f
            var triggerRadius = 0f
            var radius = 0f // wheel radius when open
            var pointerId = -1
            var activeSegment = -1 // -1 = none, RADIAL_CENTER = center, 0..n-1 = segment
            var isOpen = false

            // Weapon wheel state
            var isWeaponWheel = false
            var filteredSegments: List<RadialSegment> = emptyList()
            var weaponState: WeaponState? = null
        }

        private class SliderState(
            val control: SliderControl,
        ) {
            var centerX = 0f
            var centerY = 0f
            var trackLen = 0f // half-length of the track in px
            var thumbR = 0f // thumb radius
            var pointerId = -1
            var thumbPos = 0f // -1..1 (0 = center, clamped)
        }

        private class DiagnosticState(
            val control: DiagnosticControl,
        ) {
            var centerX = 0f
            var centerY = 0f
            var width = 0f
            var height = 0f
        }

        private class AxisRegionState(
            val control: AxisRegionControl,
        ) {
            var left = 0f
            var top = 0f
            var right = 0f
            var bottom = 0f
            var pointerId = -1
            var touchOrigin = 0f // axis-position where the touch set the zero point
            var value = 0f // current output -1..1

            // Pointer stealing: which stick we stole from (to return the drag later)
            var stealSourceStick: StickState? = null
            var savedStickCX = 0f // original stick center when stolen
            var savedStickCY = 0f
        }

        private val stickStates = mutableListOf<StickState>()
        private val buttonStates = mutableListOf<ButtonState>()
        private val radialStates = mutableListOf<RadialMenuState>()
        private val sliderStates = mutableListOf<SliderState>()
        private val diagnosticStates = mutableListOf<DiagnosticState>()
        private val axisRegionStates = mutableListOf<AxisRegionState>()

        // Gyro diagnostic values (updated in real time via GyroInputManager)
        @Volatile private var diagGyroYaw = 0f

        @Volatile private var diagGyroPitch = 0f

        @Volatile private var diagGyroRoll = 0f

        // ── Mouse-mode drag buffer tick ─────────────────────────
        private val mainHandler = Handler(Looper.getMainLooper())

        private val mouseDrainHandler = mainHandler
        private var mouseDrainRunning = false
        private val mouseDrainRunnable =
            object : Runnable {
                override fun run() {
                    drainMouseBuffers()
                    if (mouseDrainRunning) {
                        mouseDrainHandler.postDelayed(this, MOUSE_DRAIN_INTERVAL_MS)
                    }
                }
            }

        private fun startMouseDrain() {
            if (!mouseDrainRunning) {
                mouseDrainRunning = true
                mouseDrainHandler.post(mouseDrainRunnable)
            }
        }

        private fun stopMouseDrain() {
            mouseDrainRunning = false
            mouseDrainHandler.removeCallbacks(mouseDrainRunnable)
        }

        private fun drainMouseBuffers() {
            for (s in stickStates) {
                if (!s.control.mouseMode || s.pointerId < 0) continue
                val baseCap = MOUSE_MAX_AXIS_PER_TICK * MOUSE_SENSITIVITY_MULTIPLIER * MOUSE_BASE_MULTIPLIER
                val capX = baseCap * s.control.sensitivityX
                val capY = baseCap * s.control.sensitivityY
                val emitX = s.mousePendingX.coerceIn(-capX, capX)
                val emitY = s.mousePendingY.coerceIn(-capY, capY)
                s.mousePendingX -= emitX
                s.mousePendingY -= emitY
                // Discard tiny residuals to avoid drift
                if (abs(s.mousePendingX) < 0.001f) s.mousePendingX = 0f
                if (abs(s.mousePendingY) < 0.001f) s.mousePendingY = 0f
                var outX = emitX
                var outY = emitY
                if (s.control.invertX) outX = -outX
                if (s.control.invertY) outY = -outY
                axisCallback?.invoke(s.control.axisX, outX.coerceIn(-1f, 1f))
                axisCallback?.invoke(s.control.axisY, outY.coerceIn(-1f, 1f))
            }
        }

        // ── MAP button geometry (kept for automap overlay compat) ──
        private var mapBtnCenterX = 0f
        private var mapBtnCenterY = 0f
        private var mapBtnRadius = 0f

        // ── Music prev/next button geometry ──────────────────────────
        private var musicBtnY = 0f
        private var prevBtnCenterX = 0f
        private var nextBtnCenterX = 0f
        private var musicBtnRadius = 0f
        private var musicLabelX = 0f

        // ── Non-layout pointer tracking ─────────────────────────
        private val passthroughPointers = mutableSetOf<Int>()
        private var mapBtnPointerId = -1 // used by automap overlay
        private var prevBtnPointerId = -1
        private var nextBtnPointerId = -1
        private var musicLabelPointerId = -1

        // ── Automap gesture state (pointers not on stick/buttons) ──

        /** Set to true by the activity when the automap is displayed. */
        var automapActive = false
            set(value) {
                if (field != value) {
                    field = value
                    if (width > 0 && height > 0) computeGeometry(width, height)
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
        private var automapBtnSize = 0f // button width/height
        private var automapBtnY = 0f // top of buttons
        private var automapBtnSpacing = 0f // gap between buttons
        private val automapBtnRects = mutableListOf<RectF>() // [0]=center, [1..n]=markers
        private var automapBtnPressed = -1 // index of currently pressed button, -1=none
        private var automapBtnPointerId = -1

        // ── Paint objects ───────────────────────────────────────
        companion object {
            private const val RADIAL_CENTER = -2

            // Double-tap button release delay -- ensures the press survives at
            // least one game frame so level-triggered controls (fire primary) register
            private const val DOUBLE_TAP_RELEASE_DELAY_MS = 50L

            // Mouse-mode drag buffer constants
            private const val MOUSE_DRAIN_INTERVAL_MS = 16L // ~60 Hz
            private const val MOUSE_MAX_AXIS_PER_TICK = 0.6f // max axis output per tick before sensitivity
            private const val MOUSE_REFERENCE_DISTANCE = 200f // pixels of drag for 1.0 axis at sensitivity=1
            private const val MOUSE_SENSITIVITY_MULTIPLIER = 10f // hidden multiplier so slider stays low-range

            // Extra base multiplier stacking with the per-axis sensitivity so
            // mouse mode feels responsive at the same slider value as stick mode
            private const val MOUSE_BASE_MULTIPLIER = 2f

            // Admin tray action indices dispatched via adminTrayCallback
            const val ADMIN_INCREASE_VIEW = 0
            const val ADMIN_DECREASE_VIEW = 1
            const val ADMIN_TOGGLE_AUTOLEVEL = 2
            const val ADMIN_QUICK_SAVE = 3
            const val ADMIN_QUICK_LOAD = 4
            const val ADMIN_OPEN_MENU = 5
            const val ADMIN_NET_EVENTS = 6
            const val ADMIN_EXIT_LAUNCHER = 7
            const val ADMIN_NET_STATS = 8

            // Cockpit mode constants (match C CM_* defines)
            private const val CM_FULL_COCKPIT = 0
            private const val CM_STATUS_BAR = 2
            private const val CM_FULL_SCREEN = 3
        }

        private val radialPath = Path()
        private val paintRadialSeg =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
            }
        private val paintRing =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.STROKE
                strokeWidth = 3f
                color = 0x66FFFFFF // semi-transparent white ring
            }
        private val paintFill =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0x33FFFFFF // subtle fill
            }
        private val paintThumb =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0x99FFFFFF.toInt() // brighter thumb dot
            }
        private val paintBtnIdle =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0x33FFFFFF // subtle fill when not pressed
            }
        private val paintBtnPressed =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0x66FFFFFF // brighter when pressed
            }
        private val paintBtnLabel =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0xAAFFFFFF.toInt()
                textAlign = Paint.Align.CENTER
            }
        private val paintAutomapHelpText =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0xAAFFFFFF.toInt()
                textAlign = Paint.Align.CENTER
            }
        private val paintAutomapBtnBg =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0x44FFFFFF
            }
        private val paintAutomapBtnBgPressed =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0x88FFFFFF.toInt()
            }
        private val paintAutomapBtnText =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0xDDFFFFFF.toInt()
                textAlign = Paint.Align.CENTER
            }

        // ── Diagnostic overlay paints ───────────────────────────
        private val paintDiagBg =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0x44000000
            }
        private val paintDiagText =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0xCCFFFFFF.toInt()
                textAlign = Paint.Align.LEFT
                typeface = android.graphics.Typeface.MONOSPACE
            }

        // ── Cheats overlay state ────────────────────────────────
        private var cheatsOverlayOpen = false
        private var cheatsOverlayPressedIndex = -1 // which cheat button is being pressed
        private var cheatsOverlayPointerId = -1
        private val cheatsOverlayRects = mutableListOf<RectF>() // computed per-draw
        private var cheatsCloseRect = RectF()

        // ── Admin tray state ────────────────────────────────────
        // Visible bottom-center tab that opens a settings panel.
        // Items: Increase View, Decrease View, Toggle Auto-Leveling,
        //        Quick Save (Alt+F2), Quick Load (Alt+F3), Open Game Menu (ESC)
        private var adminTrayOpen = false
        private var adminTrayPressedIndex = -1
        private var adminTrayPointerId = -1
        private val adminTrayRects = mutableListOf<RectF>()
        private var adminTrayTabRect = RectF()

        // Slide animation: 0 = fully closed (off-screen), 1 = fully open
        private var adminTraySlide = 0f
        private var adminTrayAnimator: ValueAnimator? = null

        // Drag-to-dismiss tracking
        private var adminTrayDragStartY = 0f
        private var adminTrayDragging = false

        // Callback: (actionIndex) -> Unit. Actions are:
        // 0=Increase View, 1=Decrease View, 2=Toggle Auto-Leveling,
        // 3=Quick Save, 4=Quick Load, 5=Open Game Menu
        var adminTrayCallback: ((Int) -> Unit)? = null

        // Provider for dynamic labels (auto-leveling state, cockpit mode)
        var adminTrayAutoLevelingProvider: (() -> Boolean)? = null
        var adminTrayCockpitModeProvider: (() -> Int)? = null

        init {
            rebuildStates()
        }

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
            radialStates.clear()
            sliderStates.clear()
            diagnosticStates.clear()
            axisRegionStates.clear()
            layout.sticks.forEach { stickStates.add(StickState(it)) }
            layout.buttons.forEach { buttonStates.add(ButtonState(it)) }
            layout.radialMenus.forEach { radialStates.add(RadialMenuState(it)) }
            layout.sliders.forEach { sliderStates.add(SliderState(it)) }
            layout.diagnostics.forEach { diagnosticStates.add(DiagnosticState(it)) }
            layout.axisRegions.forEach { axisRegionStates.add(AxisRegionState(it)) }
        }

        override fun onSizeChanged(
            w: Int,
            h: Int,
            oldw: Int,
            oldh: Int,
        ) {
            super.onSizeChanged(w, h, oldw, oldh)
            computeGeometry(w, h)
        }

        private fun computeGeometry(
            w: Int,
            h: Int,
        ) {
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
                if (s.control.floating || s.control.mouseMode) {
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
                if (b.control.binding == TouchBindings.BTN_AUTOMAP && !automapActive) {
                    mapBtnCenterX = b.centerX
                    mapBtnCenterY = b.centerY
                    mapBtnRadius = b.radius
                }
            }

            // Music controls (not layout-driven)
            musicBtnRadius = base * 0.03f

            // Compute radial menu geometry from layout
            for (rm in radialStates) {
                rm.triggerRadius = defaultBtnRadius * rm.control.sizeMult
                rm.triggerX = wf * rm.control.xPct / 100f
                rm.triggerY = hf * rm.control.yPct / 100f
                rm.radius = base * 0.18f * rm.control.sizeMult
            }

            // Compute slider geometry from layout
            for (sl in sliderStates) {
                sl.centerX = wf * sl.control.xPct / 100f
                sl.centerY = hf * sl.control.yPct / 100f
                sl.trackLen = base * 0.12f * sl.control.sizeMult
                sl.thumbR = base * 0.018f * sl.control.sizeMult
            }

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

            // Compute diagnostic overlay geometry
            val diagTextSize = base * 0.025f * 1f // base text size
            for (d in diagnosticStates) {
                d.centerX = wf * d.control.xPct / 100f
                d.centerY = hf * d.control.yPct / 100f
                paintDiagText.textSize = diagTextSize * d.control.sizeMult
                d.width = paintDiagText.measureText("Roll: -100%") + diagTextSize * 2
                d.height = diagTextSize * d.control.sizeMult * 4.5f
            }

            // Compute axis region geometry from layout
            for (ar in axisRegionStates) {
                val z = ar.control.zone
                ar.left = wf * z.leftPct / 100f
                ar.top = hf * z.topPct / 100f
                ar.right = wf * z.rightPct / 100f
                ar.bottom = hf * z.bottomPct / 100f
            }
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
            for (b in buttonStates) {
                if (gameVariant == "d1" && b.control.binding in TouchBindings.D2_ONLY_BUTTONS) continue
                drawButton(canvas, b, gAlpha)
            }

            // ── Layout sliders ──────────────────────────────────
            for (sl in sliderStates) drawSlider(canvas, sl, gAlpha)

            // ── Axis regions ────────────────────────────────────
            for (ar in axisRegionStates) drawAxisRegion(canvas, ar, gAlpha)

            // ── Radial menus (triggers, then open wheels on top) ──
            for (rm in radialStates) {
                if (rm.control.id == "Guide" && gameVariant == "d1") continue
                if (!rm.isOpen) drawRadialMenu(canvas, rm, gAlpha)
            }
            for (rm in radialStates) {
                if (rm.isOpen) {
                    if (rm.isWeaponWheel) {
                        drawWeaponWheel(canvas, rm, gAlpha)
                    } else {
                        drawRadialMenu(canvas, rm, gAlpha)
                    }
                }
            }

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

            // ── Diagnostic overlays ─────────────────────────────
            for (d in diagnosticStates) drawDiagnostic(canvas, d, gAlpha)

            // ── Cheats overlay (drawn last, on top of everything) ──
            if (cheatsOverlayOpen) drawCheatsOverlay(canvas)

            // ── Admin tray tab (visible) or panel (when open) ───
            if (adminTrayOpen) {
                drawAdminTrayPanel(canvas)
            } else if (!cheatsOverlayOpen) {
                drawAdminTrayTab(canvas)
            }
        }

        private fun drawStick(
            canvas: Canvas,
            s: StickState,
            gAlpha: Float,
        ) {
            val eff = (gAlpha * s.control.opacity).coerceIn(0f, 1f)

            if (s.control.mouseMode) {
                // Mouse mode: draw only a transparent bounding box for the touch region
                paintRing.alpha = (0x44 * eff).toInt()
                canvas.drawRect(s.fzLeft, s.fzTop, s.fzRight, s.fzBottom, paintRing)
                return
            }

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

        private fun drawButton(
            canvas: Canvas,
            b: ButtonState,
            gAlpha: Float,
        ) {
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
                canvas.drawText(
                    b.control.label,
                    b.centerX,
                    b.centerY + paintBtnLabel.textSize * 0.35f,
                    paintBtnLabel,
                )
            }
        }

        private fun drawSlider(
            canvas: Canvas,
            sl: SliderState,
            gAlpha: Float,
        ) {
            val eff = (gAlpha * sl.control.opacity).coerceIn(0f, 1f)
            val vertical = sl.control.orientation == SliderOrientation.VERTICAL

            // Track endpoints
            val x0: Float
            val y0: Float
            val x1: Float
            val y1: Float
            if (vertical) {
                x0 = sl.centerX
                y0 = sl.centerY - sl.trackLen
                x1 = sl.centerX
                y1 = sl.centerY + sl.trackLen
            } else {
                x0 = sl.centerX - sl.trackLen
                y0 = sl.centerY
                x1 = sl.centerX + sl.trackLen
                y1 = sl.centerY
            }

            // Draw track line
            paintRing.alpha = (0x44 * eff).toInt()
            val savedStroke = paintRing.strokeWidth
            paintRing.strokeWidth = sl.thumbR * 0.6f
            canvas.drawLine(x0, y0, x1, y1, paintRing)
            paintRing.strokeWidth = savedStroke

            // Thumb position along track (-1..1 mapped to endpoints)
            val t = sl.thumbPos
            val tx: Float
            val ty: Float
            if (vertical) {
                tx = sl.centerX
                ty = sl.centerY + t * sl.trackLen
            } else {
                tx = sl.centerX + t * sl.trackLen
                ty = sl.centerY
            }
            val pressed = sl.pointerId >= 0
            val thumbFill = if (pressed) paintBtnPressed else paintBtnIdle
            thumbFill.alpha = ((if (pressed) 0x88 else 0x55) * eff).toInt()
            canvas.drawCircle(tx, ty, sl.thumbR, thumbFill)
            paintRing.alpha = (0x66 * eff).toInt()
            canvas.drawCircle(tx, ty, sl.thumbR, paintRing)

            // Label below/right of track
            if (sl.control.id.isNotEmpty()) {
                paintBtnLabel.alpha = (0x88 * eff).toInt()
                paintBtnLabel.textSize = sl.thumbR * 1.2f
                if (vertical) {
                    canvas.drawText(
                        sl.control.id.take(5),
                        sl.centerX,
                        y1 + paintBtnLabel.textSize * 1.5f,
                        paintBtnLabel,
                    )
                } else {
                    canvas.drawText(
                        sl.control.id.take(5),
                        sl.centerX,
                        sl.centerY + sl.thumbR + paintBtnLabel.textSize * 1.3f,
                        paintBtnLabel,
                    )
                }
            }
        }

        private fun drawAxisRegion(
            canvas: Canvas,
            ar: AxisRegionState,
            gAlpha: Float,
        ) {
            val eff = (gAlpha * ar.control.opacity).coerceIn(0f, 1f)
            val pressed = ar.pointerId >= 0

            // Fill rect
            val fill = if (pressed) paintBtnPressed else paintBtnIdle
            fill.alpha = ((if (pressed) 0x44 else 0x22) * eff).toInt()
            canvas.drawRect(ar.left, ar.top, ar.right, ar.bottom, fill)

            // Border
            paintRing.alpha = (0x44 * eff).toInt()
            canvas.drawRect(ar.left, ar.top, ar.right, ar.bottom, paintRing)

            // Value indicator line
            if (pressed) {
                val vertical = ar.control.orientation == SliderOrientation.VERTICAL
                paintRing.alpha = (0xAA * eff).toInt()
                val savedStroke = paintRing.strokeWidth
                paintRing.strokeWidth = 3f
                if (vertical) {
                    val midY = (ar.top + ar.bottom) / 2f
                    val halfH = (ar.bottom - ar.top) / 2f
                    val lineY = midY + ar.value * halfH
                    canvas.drawLine(ar.left, lineY, ar.right, lineY, paintRing)
                } else {
                    val midX = (ar.left + ar.right) / 2f
                    val halfW = (ar.right - ar.left) / 2f
                    val lineX = midX + ar.value * halfW
                    canvas.drawLine(lineX, ar.top, lineX, ar.bottom, paintRing)
                }
                paintRing.strokeWidth = savedStroke
            }

            // Label
            if (ar.control.id.isNotEmpty()) {
                paintBtnLabel.alpha = (0x66 * eff).toInt()
                val base = min(width, height).toFloat()
                paintBtnLabel.textSize = base * 0.02f
                val cx = (ar.left + ar.right) / 2f
                val cy = (ar.top + ar.bottom) / 2f
                canvas.drawText(ar.control.id.take(6), cx, cy + paintBtnLabel.textSize * 0.35f, paintBtnLabel)
            }
        }

        private fun drawDiagnostic(
            canvas: Canvas,
            d: DiagnosticState,
            gAlpha: Float,
        ) {
            val eff = (gAlpha * d.control.opacity).coerceIn(0f, 1f)
            val ts = paintDiagText.textSize
            val pad = ts * 0.5f
            val lineH = ts * 1.3f

            // Background box
            paintDiagBg.alpha = (0x44 * eff).toInt()
            val left = d.centerX - d.width / 2
            val top = d.centerY - d.height / 2
            canvas.drawRoundRect(left, top, left + d.width, top + d.height, pad, pad, paintDiagBg)

            // Text lines
            paintDiagText.alpha = (0xCC * eff).toInt()
            val textX = left + pad
            var textY = top + pad + ts
            val yaw = (diagGyroYaw * 100).toInt()
            val pitch = (diagGyroPitch * 100).toInt()
            val roll = (diagGyroRoll * 100).toInt()
            canvas.drawText("Yaw:   $yaw%", textX, textY, paintDiagText)
            textY += lineH
            canvas.drawText("Roll:  $pitch%", textX, textY, paintDiagText)
            textY += lineH
            canvas.drawText("Pitch: $roll%", textX, textY, paintDiagText)
        }

        /** Shared radial menu drawing — handles both trigger icon and open wheel. */
        private fun drawRadialMenu(
            canvas: Canvas,
            state: RadialMenuState,
            gAlpha: Float,
        ) {
            val eff = (gAlpha * state.control.opacity).coerceIn(0f, 1f)

            if (!state.isOpen) {
                // Draw trigger icon (small circle with label, like a button)
                val pressed = state.pointerId >= 0
                val fill = if (pressed) paintBtnPressed else paintBtnIdle
                fill.alpha = ((if (pressed) 0x66 else 0x33) * eff).toInt()
                canvas.drawCircle(state.triggerX, state.triggerY, state.triggerRadius, fill)
                paintRing.alpha = (0x66 * eff).toInt()
                canvas.drawCircle(state.triggerX, state.triggerY, state.triggerRadius, paintRing)
                paintBtnLabel.alpha = (0xAA * eff).toInt()
                paintBtnLabel.textSize = state.triggerRadius * 0.6f
                canvas.drawText(
                    state.control.id.take(4),
                    state.triggerX,
                    state.triggerY + paintBtnLabel.textSize * 0.35f,
                    paintBtnLabel,
                )
                return
            }

            // Open wheel — draw pie segments parameterized by segment count
            val cx = state.triggerX
            val cy = state.triggerY
            val segs = state.control.segments
            val n = segs.size
            if (n == 0) return

            val r = state.radius
            val expandR = r * 1.15f
            val segAngle = 360f / n
            val centerR = r * 0.22f

            for (i in 0 until n) {
                val active = state.activeSegment == i
                val segR = if (active) expandR else r
                val startDeg = -90f + i * segAngle

                // Pie slice path
                radialPath.reset()
                radialPath.moveTo(cx, cy)
                radialPath.arcTo(RectF(cx - segR, cy - segR, cx + segR, cy + segR), startDeg, segAngle)
                radialPath.close()

                // Fill — active segment is darker and more opaque
                paintRadialSeg.color = if (active) 0x88334455.toInt() else 0x44888888
                canvas.drawPath(radialPath, paintRadialSeg)

                // Outline
                paintRing.alpha = (0x44 * eff).toInt()
                canvas.drawPath(radialPath, paintRing)

                // Segment label at arc midpoint
                val midRad = Math.toRadians((startDeg + segAngle / 2).toDouble())
                val lx = cx + cos(midRad).toFloat() * segR * 0.65f
                val ly = cy + sin(midRad).toFloat() * segR * 0.65f
                paintBtnLabel.alpha = ((if (active) 0xFF else 0xAA) * eff).toInt()
                paintBtnLabel.textSize = r * 0.11f
                canvas.drawText(segs[i].label, lx, ly + paintBtnLabel.textSize * 0.35f, paintBtnLabel)
            }

            // Center circle
            val cActive = state.activeSegment == RADIAL_CENTER
            paintRadialSeg.color = if (cActive) 0x88445566.toInt() else 0x55444444
            canvas.drawCircle(cx, cy, centerR, paintRadialSeg)
            paintRing.alpha = (0x66 * eff).toInt()
            canvas.drawCircle(cx, cy, centerR, paintRing)

            if (state.control.centerLabel.isNotEmpty()) {
                paintBtnLabel.alpha = (0xCC * eff).toInt()
                paintBtnLabel.textSize = centerR * 0.45f
                canvas.drawText(
                    state.control.centerLabel,
                    cx,
                    cy + paintBtnLabel.textSize * 0.35f,
                    paintBtnLabel,
                )
            }
        }

        /** Weapon wheel drawing — counter-clockwise from bottom, with ammo display. */
        private fun drawWeaponWheel(
            canvas: Canvas,
            state: RadialMenuState,
            gAlpha: Float,
        ) {
            val eff = (gAlpha * state.control.opacity).coerceIn(0f, 1f)
            val cx = state.triggerX
            val cy = state.triggerY
            val segs = state.filteredSegments
            val n = segs.size
            val ws = state.weaponState
            val isPrimary = state.control.id == "PriWpn"

            // Placeholder when only 1 weapon (always have Laser/Concsn)
            if (n <= 1) {
                val label =
                    if (n == 1) {
                        segs[0].label
                    } else if (isPrimary) {
                        "Laser"
                    } else {
                        "Concsn"
                    }
                val bubbleR = state.radius * 0.3f
                paintRadialSeg.color = 0x88334455.toInt()
                canvas.drawCircle(cx, cy, bubbleR, paintRadialSeg)
                paintRing.alpha = (0x66 * eff).toInt()
                canvas.drawCircle(cx, cy, bubbleR, paintRing)
                paintBtnLabel.alpha = (0xCC * eff).toInt()
                paintBtnLabel.textSize = bubbleR * 0.35f
                canvas.drawText(label, cx, cy - paintBtnLabel.textSize * 0.3f, paintBtnLabel)
                canvas.drawText("only", cx, cy + paintBtnLabel.textSize * 1.0f, paintBtnLabel)
                return
            }

            val r = state.radius
            val expandR = r * 1.15f
            val segAngle = 360f / n
            val centerR = r * 0.22f

            for (i in 0 until n) {
                val active = state.activeSegment == i
                val segR = if (active) expandR else r
                // Counter-clockwise from bottom: segment i starts at 90° - (i+0.5)*segAngle
                val startDeg = 90f - (i + 0.5f) * segAngle

                radialPath.reset()
                radialPath.moveTo(cx, cy)
                radialPath.arcTo(RectF(cx - segR, cy - segR, cx + segR, cy + segR), startDeg, segAngle)
                radialPath.close()

                paintRadialSeg.color = if (active) 0x88334455.toInt() else 0x44888888
                canvas.drawPath(radialPath, paintRadialSeg)
                paintRing.alpha = (0x44 * eff).toInt()
                canvas.drawPath(radialPath, paintRing)

                // Label at segment center angle = 90° - i * segAngle
                val centerAngle = 90f - i * segAngle
                val midRad = Math.toRadians(centerAngle.toDouble())
                val lx = cx + cos(midRad).toFloat() * segR * 0.55f
                val ly = cy + sin(midRad).toFloat() * segR * 0.55f
                paintBtnLabel.alpha = ((if (active) 0xFF else 0xAA) * eff).toInt()
                paintBtnLabel.textSize = r * 0.11f
                canvas.drawText(segs[i].label, lx, ly + paintBtnLabel.textSize * 0.35f, paintBtnLabel)

                // Ammo display for secondary weapons and vulcan
                val seg = segs[i]
                val wpnIdx = seg.weaponIndex
                if (ws != null && wpnIdx >= 0) {
                    val ammoCount: Int
                    val ammoMax: Int
                    val isVulcan: Boolean

                    if (isPrimary) {
                        isVulcan = wpnIdx == 1 // VULCAN_INDEX
                        if (isVulcan) {
                            ammoCount = ws.primaryAmmo[wpnIdx]
                            ammoMax = ws.primaryAmmoMax[wpnIdx]
                        } else {
                            ammoCount = 0
                            ammoMax = 0
                        }
                    } else {
                        isVulcan = false
                        // Show base weapon ammo; if only super variant owned, show its ammo
                        if (ws.hasSecondary(wpnIdx)) {
                            ammoCount = ws.secondaryAmmo[wpnIdx]
                            ammoMax = ws.secondaryAmmoMax[wpnIdx]
                        } else {
                            ammoCount = ws.secondaryAmmo[wpnIdx + 5]
                            ammoMax = ws.secondaryAmmoMax[wpnIdx + 5]
                        }
                    }

                    if (ammoMax > 0) {
                        val pct = (ammoCount.toFloat() / ammoMax).coerceIn(0f, 1f)
                        // Ammo text below label
                        val ammoText = if (isVulcan) "${(pct * 100).toInt()}%" else "\u00D7$ammoCount"
                        paintBtnLabel.textSize = r * 0.09f
                        canvas.drawText(ammoText, lx, ly + r * 0.16f, paintBtnLabel)

                        // Small pie chart
                        val pieR = r * 0.06f
                        val pieCx = lx
                        val pieCy = ly + r * 0.26f
                        paintRadialSeg.color = 0x44666666
                        canvas.drawCircle(pieCx, pieCy, pieR, paintRadialSeg)
                        paintRadialSeg.color = ammoColor(pct, eff)
                        if (pct > 0f) {
                            canvas.drawArc(
                                RectF(pieCx - pieR, pieCy - pieR, pieCx + pieR, pieCy + pieR),
                                -90f,
                                360f * pct,
                                true,
                                paintRadialSeg,
                            )
                        }
                    }
                }
            }

            // Center circle
            val cActive = state.activeSegment == RADIAL_CENTER
            paintRadialSeg.color = if (cActive) 0x88445566.toInt() else 0x55444444
            canvas.drawCircle(cx, cy, centerR, paintRadialSeg)
            paintRing.alpha = (0x66 * eff).toInt()
            canvas.drawCircle(cx, cy, centerR, paintRing)
        }

        /** Green → Yellow → Red based on ammo fill percentage. */
        private fun ammoColor(
            pct: Float,
            eff: Float,
        ): Int {
            val alpha = (0xCC * eff).toInt().coerceIn(0, 255)
            val r = ((1f - pct) * 255).toInt().coerceIn(0, 255)
            val g = (pct * 200).toInt().coerceIn(0, 255)
            return (alpha shl 24) or (r shl 16) or (g shl 8)
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
                width / 2f,
                helpY,
                paintAutomapHelpText,
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

            // When cheats overlay is open, it consumes all touches
            if (cheatsOverlayOpen) return handleCheatsOverlayTouch(event)

            // When admin tray panel is open, it consumes all touches
            if (adminTrayOpen) return handleAdminTrayTouch(event)

            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    val idx = event.actionIndex
                    val px = event.getX(idx)
                    val py = event.getY(idx)
                    val pid = event.getPointerId(idx)
                    var handled = false

                    // Try axis regions (before sticks, since they can overlap)
                    for (ar in axisRegionStates) {
                        if (ar.pointerId >= 0) continue
                        if (px in ar.left..ar.right && py in ar.top..ar.bottom) {
                            ar.pointerId = pid
                            val vertical = ar.control.orientation == SliderOrientation.VERTICAL
                            ar.touchOrigin = if (vertical) py else px
                            ar.value = 0f
                            ar.stealSourceStick = null
                            axisCallback?.invoke(ar.control.axis, 0f)
                            invalidate()
                            handled = true
                            break
                        }
                    }

                    // Try layout sticks
                    for (s in stickStates) {
                        if (handled) break
                        if (s.pointerId >= 0) continue
                        if (s.control.mouseMode) {
                            // Mouse mode: use floating zone bounds for hit detection
                            if (px in s.fzLeft..s.fzRight && py in s.fzTop..s.fzBottom) {
                                // Double-tap detection
                                val now = android.os.SystemClock.uptimeMillis()
                                if (s.control.doubleTapBinding >= 0 && now - s.lastTapTime < 300L) {
                                    fireDoubleTapBinding(s.control.doubleTapBinding)
                                }
                                s.lastTapTime = now
                                s.pointerId = pid
                                s.mouseLastX = px
                                s.mouseLastY = py
                                s.mouseOriginX = px
                                s.mouseOriginY = py
                                s.mousePendingX = 0f
                                s.mousePendingY = 0f
                                s.floatingActive = true
                                startMouseDrain()
                                handled = true
                                break
                            }
                        } else if (s.control.floating) {
                            if (px in s.fzLeft..s.fzRight && py in s.fzTop..s.fzBottom) {
                                // Double-tap detection for floating mode
                                val now = android.os.SystemClock.uptimeMillis()
                                if (s.control.doubleTapBinding >= 0 && now - s.lastTapTime < 300L) {
                                    fireDoubleTapBinding(s.control.doubleTapBinding)
                                }
                                s.lastTapTime = now
                                s.pointerId = pid
                                s.floatingCX = px
                                s.floatingCY = py
                                s.floatingActive = true
                                s.pos.set(0f, 0f)
                                invalidate()
                                handled = true
                                break
                            }
                        } else if (hypot(px - s.centerX, py - s.centerY) <= s.radius) {
                            // Double-tap detection for fixed sticks
                            val now = android.os.SystemClock.uptimeMillis()
                            if (s.control.doubleTapBinding >= 0 && now - s.lastTapTime < 300L) {
                                fireDoubleTapBinding(s.control.doubleTapBinding)
                            }
                            s.lastTapTime = now
                            s.pointerId = pid
                            updateStickFromTouch(s, px, py)
                            handled = true
                            break
                        }
                    }

                    // Try layout buttons
                    if (!handled) {
                        for (b in buttonStates) {
                            if (gameVariant == "d1" && b.control.binding in TouchBindings.D2_ONLY_BUTTONS) continue
                            if (b.pointerId >= 0) continue
                            if (hypot(px - b.centerX, py - b.centerY) <= b.radius * 1.3f) {
                                b.pointerId = pid
                                if (b.control.binding == TouchBindings.BTN_CHEATS_MENU) {
                                    cheatsOverlayOpen = !cheatsOverlayOpen
                                } else if (b.control.binding == TouchBindings.BTN_GYRO_RECENTER) {
                                    gyroManager?.calibrate()
                                } else if (b.control.toggle) {
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
                                handled = true
                                break
                            }
                        }
                    }

                    // Try radial menu triggers
                    if (!handled) {
                        for (rm in radialStates) {
                            if (rm.pointerId >= 0) continue
                            // D1 has no Guide-Bot — skip the guidebot wheel entirely
                            if (rm.control.id == "Guide" && gameVariant == "d1") continue
                            if (hypot(px - rm.triggerX, py - rm.triggerY) <= rm.triggerRadius * 1.3f) {
                                rm.pointerId = pid
                                rm.isOpen = true
                                rm.activeSegment = -1
                                // Initialize weapon wheel state
                                rm.isWeaponWheel = rm.control.id == "PriWpn" || rm.control.id == "SecWpn"
                                if (rm.isWeaponWheel) {
                                    val ws = weaponStateProvider?.invoke()
                                    rm.weaponState = ws
                                    val isPrimary = rm.control.id == "PriWpn"
                                    rm.filteredSegments =
                                        if (ws != null) {
                                            rm.control.segments.filter { seg ->
                                                val wi = seg.weaponIndex
                                                if (wi < 0) {
                                                    true
                                                } else if (isPrimary) {
                                                    ws.hasPrimary(wi) || ws.hasPrimary(wi + 5)
                                                } else {
                                                    ws.hasSecondary(wi) || ws.hasSecondary(wi + 5)
                                                }
                                            }
                                        } else {
                                            rm.control.segments
                                        }
                                }
                                if (rm.control.hapticFeedback) {
                                    performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
                                }
                                invalidate()
                                handled = true
                                break
                            }
                        }
                    }

                    // Try layout sliders
                    if (!handled) {
                        for (sl in sliderStates) {
                            if (sl.pointerId >= 0) continue
                            val hitR = sl.trackLen + sl.thumbR * 2
                            val vertical = sl.control.orientation == SliderOrientation.VERTICAL
                            val along = if (vertical) abs(py - sl.centerY) else abs(px - sl.centerX)
                            val across = if (vertical) abs(px - sl.centerX) else abs(py - sl.centerY)
                            if (along <= hitR && across <= sl.thumbR * 3) {
                                sl.pointerId = pid
                                updateSliderFromTouch(sl, px, py)
                                handled = true
                                break
                            }
                        }
                    }

                    // Try music controls (not layout-driven)
                    if (!handled) {
                        when {
                            prevBtnPointerId < 0 &&
                                trackLabel.isNotEmpty() &&
                                hypot(px - prevBtnCenterX, py - musicBtnY) <= musicBtnRadius * 1.3f -> {
                                prevBtnPointerId = pid
                                invalidate()
                                handled = true
                            }
                            nextBtnPointerId < 0 &&
                                trackLabel.isNotEmpty() &&
                                hypot(px - nextBtnCenterX, py - musicBtnY) <= musicBtnRadius * 1.3f -> {
                                nextBtnPointerId = pid
                                invalidate()
                                handled = true
                            }
                            musicLabelPointerId < 0 &&
                                trackLabel.isNotEmpty() &&
                                px >= musicLabelX &&
                                py >= musicBtnY - musicBtnRadius * 1.5f &&
                                py <= musicBtnY + musicBtnRadius * 1.5f -> {
                                musicLabelPointerId = pid
                                handled = true
                            }
                        }
                    }

                    // Try admin tray tab
                    if (!handled && adminTrayTabRect.contains(px, py)) {
                        adminTrayOpen = true
                        animateAdminTray(true)
                        handled = true
                    }

                    if (!handled) passthroughPointers.add(pid)
                }
                MotionEvent.ACTION_MOVE -> {
                    // Check for pointer stealing: stick → axis region
                    for (s in stickStates) {
                        if (s.pointerId < 0) continue
                        val si = event.findPointerIndex(s.pointerId)
                        if (si < 0) continue
                        val sx = event.getX(si)
                        val sy = event.getY(si)
                        for (ar in axisRegionStates) {
                            if (ar.pointerId >= 0) continue
                            if (sx in ar.left..ar.right && sy in ar.top..ar.bottom) {
                                // Steal this pointer from the stick to the region
                                ar.pointerId = s.pointerId
                                val vertical = ar.control.orientation == SliderOrientation.VERTICAL
                                ar.touchOrigin = if (vertical) sy else sx
                                ar.value = 0f
                                ar.stealSourceStick = s
                                // Save stick center before reset so we can restore it
                                ar.savedStickCX = if (s.floatingActive) s.floatingCX else s.centerX
                                ar.savedStickCY = if (s.floatingActive) s.floatingCY else s.centerY
                                // Zero the stick without releasing the pointer entirely
                                s.pointerId = -1
                                s.pos.set(0f, 0f)
                                s.floatingActive = false
                                if (!s.control.buttonMode) {
                                    axisCallback?.invoke(s.control.axisX, 0f)
                                    axisCallback?.invoke(s.control.axisY, 0f)
                                }
                                updateGyroStickActive()
                                axisCallback?.invoke(ar.control.axis, 0f)
                                invalidate()
                                break
                            }
                        }
                    }

                    // Check for pointer stealing: axis region → stick (return to source)
                    for (ar in axisRegionStates) {
                        if (ar.pointerId < 0) continue
                        val ai = event.findPointerIndex(ar.pointerId)
                        if (ai < 0) continue
                        val ax = event.getX(ai)
                        val ay = event.getY(ai)
                        if (ax !in ar.left..ar.right || ay !in ar.top..ar.bottom) {
                            val src = ar.stealSourceStick
                            if (src != null && src.pointerId < 0) {
                                // Return pointer to the original stick
                                src.pointerId = ar.pointerId
                                if (src.control.floating || src.control.mouseMode) {
                                    // Reuse original center if drag started in stick, else use fixed center
                                    src.floatingCX = ar.savedStickCX
                                    src.floatingCY = ar.savedStickCY
                                    src.floatingActive = true
                                    if (src.control.mouseMode) {
                                        src.mouseLastX = ax
                                        src.mouseLastY = ay
                                        src.mouseOriginX = ar.savedStickCX
                                        src.mouseOriginY = ar.savedStickCY
                                        src.mousePendingX = 0f
                                        src.mousePendingY = 0f
                                        startMouseDrain()
                                    }
                                }
                                // Release the region
                                ar.pointerId = -1
                                ar.value = 0f
                                ar.stealSourceStick = null
                                axisCallback?.invoke(ar.control.axis, 0f)
                                // Immediately update stick with current position
                                if (src.control.mouseMode) {
                                    updateStickFromMouseDrag(src, ax, ay)
                                } else {
                                    updateStickFromTouch(src, ax, ay)
                                }
                                invalidate()
                                continue
                            } else if (src == null) {
                                // Touch originated directly on region; try to transfer to a stick
                                var transferred = false
                                for (s in stickStates) {
                                    if (s.pointerId >= 0) continue
                                    val inZone =
                                        when {
                                            s.control.mouseMode || s.control.floating ->
                                                ax in s.fzLeft..s.fzRight && ay in s.fzTop..s.fzBottom
                                            else ->
                                                hypot(ax - s.centerX, ay - s.centerY) <= s.radius
                                        }
                                    if (inZone) {
                                        s.pointerId = ar.pointerId
                                        if (s.control.mouseMode) {
                                            s.mouseLastX = ax
                                            s.mouseLastY = ay
                                            s.mouseOriginX = ax
                                            s.mouseOriginY = ay
                                            s.mousePendingX = 0f
                                            s.mousePendingY = 0f
                                            s.floatingActive = true
                                            startMouseDrain()
                                        } else if (s.control.floating) {
                                            s.floatingCX = ax
                                            s.floatingCY = ay
                                            s.floatingActive = true
                                            s.pos.set(0f, 0f)
                                        } else {
                                            updateStickFromTouch(s, ax, ay)
                                        }
                                        ar.pointerId = -1
                                        ar.value = 0f
                                        axisCallback?.invoke(ar.control.axis, 0f)
                                        invalidate()
                                        transferred = true
                                        break
                                    }
                                }
                                if (transferred) continue
                            }
                        }
                    }

                    // Update active sticks
                    for (s in stickStates) {
                        if (s.pointerId >= 0) {
                            val i = event.findPointerIndex(s.pointerId)
                            if (i >= 0) {
                                if (s.control.mouseMode) {
                                    updateStickFromMouseDrag(s, event.getX(i), event.getY(i))
                                } else {
                                    updateStickFromTouch(s, event.getX(i), event.getY(i))
                                }
                            }
                        }
                    }

                    // Update active axis regions
                    for (ar in axisRegionStates) {
                        if (ar.pointerId >= 0) {
                            val i = event.findPointerIndex(ar.pointerId)
                            if (i >= 0) updateAxisRegionFromTouch(ar, event.getX(i), event.getY(i))
                        }
                    }

                    for (rm in radialStates) {
                        if (rm.pointerId >= 0 && rm.isOpen) {
                            val i = event.findPointerIndex(rm.pointerId)
                            if (i >= 0) updateRadialSelection(rm, event.getX(i), event.getY(i))
                        }
                    }
                    for (sl in sliderStates) {
                        if (sl.pointerId >= 0) {
                            val i = event.findPointerIndex(sl.pointerId)
                            if (i >= 0) updateSliderFromTouch(sl, event.getX(i), event.getY(i))
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
                    releaseAllRadialMenus(fired)
                    releaseAllSliders()
                    releaseAllAxisRegions()
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
                            if (s.pointerId == pid) {
                                resetStick(s)
                                found = true
                                break
                            }
                        }
                        // Check layout buttons
                        if (!found) {
                            for (b in buttonStates) {
                                if (b.pointerId == pid) {
                                    releaseLayoutButton(b, true)
                                    found = true
                                    break
                                }
                            }
                        }
                        // Check radial menus
                        if (!found) {
                            for (rm in radialStates) {
                                if (rm.pointerId == pid) {
                                    releaseRadialMenu(rm, true)
                                    found = true
                                    break
                                }
                            }
                        }
                        // Check sliders
                        if (!found) {
                            for (sl in sliderStates) {
                                if (sl.pointerId == pid) {
                                    releaseSlider(sl)
                                    found = true
                                    break
                                }
                            }
                        }
                        // Check axis regions
                        if (!found) {
                            for (ar in axisRegionStates) {
                                if (ar.pointerId == pid) {
                                    releaseAxisRegion(ar)
                                    found = true
                                    break
                                }
                            }
                        }
                        // Check music controls
                        if (!found) {
                            when (pid) {
                                prevBtnPointerId -> releasePrevButton(true)
                                nextBtnPointerId -> releaseNextButton(true)
                                musicLabelPointerId -> releaseMusicLabel(true)
                            }
                        }
                    }
                }
            }
            return true // always consume when active
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
                        val dx = px - mapBtnCenterX
                        val dy = py - mapBtnCenterY
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

        private fun updateStickFromMouseDrag(
            s: StickState,
            px: Float,
            py: Float,
        ) {
            val dx = px - s.mouseLastX
            val dy = py - s.mouseLastY
            s.mouseLastX = px
            s.mouseLastY = py
            // Convert pixel delta to axis-space and accumulate
            val baseScale = MOUSE_SENSITIVITY_MULTIPLIER * MOUSE_BASE_MULTIPLIER / MOUSE_REFERENCE_DISTANCE
            val scaleX = s.control.sensitivityX * baseScale
            val scaleY = s.control.sensitivityY * baseScale
            // Exponential scaling: ramp multiplier from 1.0 to max based on
            // distance from the touch-down origin (half-screen as reference)
            val multiplier =
                if (s.control.mouseExponential) {
                    val dist = hypot(px - s.mouseOriginX, py - s.mouseOriginY)
                    val halfScreen = (height / 2f).coerceAtLeast(1f)
                    val ratio = (dist / halfScreen).coerceIn(0f, 1f)
                    1f + (s.control.mouseExponentialMax - 1f) * ratio
                } else {
                    1f
                }
            s.mousePendingX += dx * scaleX * multiplier
            s.mousePendingY += dy * scaleY * multiplier
        }

        private fun updateStickFromTouch(
            s: StickState,
            px: Float,
            py: Float,
        ) {
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
            rawX = (rawX * s.control.sensitivityX).coerceIn(-1f, 1f)
            rawY = (rawY * s.control.sensitivityY).coerceIn(-1f, 1f)

            // Apply inversion
            if (s.control.invertX) rawX = -rawX
            if (s.control.invertY) rawY = -rawY

            if (s.control.buttonMode) {
                // Button mode: fire press/release based on direction past deadzone
                dispatchStickButton(s, rawX < 0f, s.xNegPressed, s.control.negXBinding) { s.xNegPressed = it }
                dispatchStickButton(s, rawX > 0f, s.xPosPressed, s.control.posXBinding) { s.xPosPressed = it }
                dispatchStickButton(s, rawY < 0f, s.yNegPressed, s.control.negYBinding) { s.yNegPressed = it }
                dispatchStickButton(s, rawY > 0f, s.yPosPressed, s.control.posYBinding) { s.yPosPressed = it }
            } else {
                axisCallback?.invoke(s.control.axisX, rawX)
                axisCallback?.invoke(s.control.axisY, rawY)
            }

            // Notify gyro manager that a stick sharing its axes is active
            updateGyroStickActive()
        }

        /** Fire a double-tap binding with a delayed release so the press survives
         *  at least one game frame (fixes fire-primary which uses level-triggered state). */
        private fun fireDoubleTapBinding(binding: Int) {
            if (TouchBindings.isMetaAction(binding)) {
                metaActionCallback?.invoke(binding, true)
                mainHandler.postDelayed({ metaActionCallback?.invoke(binding, false) }, DOUBLE_TAP_RELEASE_DELAY_MS)
            } else {
                buttonCallback?.invoke(binding, true)
                mainHandler.postDelayed({ buttonCallback?.invoke(binding, false) }, DOUBLE_TAP_RELEASE_DELAY_MS)
            }
        }

        private fun dispatchStickButton(
            s: StickState,
            nowPressed: Boolean,
            wasPressed: Boolean,
            binding: Int,
            updateState: (Boolean) -> Unit,
        ) {
            if (nowPressed == wasPressed) return
            updateState(nowPressed)
            if (TouchBindings.isMetaAction(binding)) {
                metaActionCallback?.invoke(binding, nowPressed)
            } else {
                buttonCallback?.invoke(binding, nowPressed)
            }
        }

        private fun applyDeadzone(
            value: Float,
            deadzone: Float,
        ): Float {
            val a = abs(value)
            if (a < deadzone) return 0f
            return sign(value) * (a - deadzone) / (1f - deadzone)
        }

        private fun resetStick(s: StickState) {
            // Release any held button-mode directions
            if (s.control.buttonMode) {
                dispatchStickButton(s, false, s.xNegPressed, s.control.negXBinding) { s.xNegPressed = it }
                dispatchStickButton(s, false, s.xPosPressed, s.control.posXBinding) { s.xPosPressed = it }
                dispatchStickButton(s, false, s.yNegPressed, s.control.negYBinding) { s.yNegPressed = it }
                dispatchStickButton(s, false, s.yPosPressed, s.control.posYBinding) { s.yPosPressed = it }
            }
            // Clear mouse-mode pending drag
            if (s.control.mouseMode) {
                s.mousePendingX = 0f
                s.mousePendingY = 0f
            }
            s.pointerId = -1
            s.pos.set(0f, 0f)
            s.floatingActive = false
            invalidate()
            if (!s.control.buttonMode) {
                axisCallback?.invoke(s.control.axisX, 0f)
                axisCallback?.invoke(s.control.axisY, 0f)
            }
            updateGyroStickActive()
            // Stop mouse drain if no mouse-mode sticks are active
            if (s.control.mouseMode && stickStates.none { it.control.mouseMode && it.pointerId >= 0 }) {
                stopMouseDrain()
            }
        }

        /** Update gyro manager's rightStickActive based on whether any stick sharing gyro axes is touched. */
        private fun updateGyroStickActive() {
            val gm = gyroManager ?: return
            val gyro = layout.gyro
            gm.rightStickActive =
                stickStates.any { s ->
                    s.pointerId >= 0 && (s.control.axisX == gyro.axisX || s.control.axisY == gyro.axisY)
                }
        }

        private fun resetAllSticks() {
            for (s in stickStates) if (s.pointerId >= 0) resetStick(s)
        }

        // ── Slider helpers ──────────────────────────────────────

        private fun updateSliderFromTouch(
            sl: SliderState,
            px: Float,
            py: Float,
        ) {
            val vertical = sl.control.orientation == SliderOrientation.VERTICAL
            val raw =
                if (vertical) {
                    (py - sl.centerY) / sl.trackLen
                } else {
                    (px - sl.centerX) / sl.trackLen
                }
            sl.thumbPos = raw.coerceIn(-1f, 1f)
            invalidate()

            var value = sl.thumbPos
            value = applyResponseCurve(value, sl.control.responseCurve, sl.control.exponent)
            value = (value * sl.control.sensitivity).coerceIn(-1f, 1f)
            axisCallback?.invoke(sl.control.axis, value)
        }

        private fun releaseSlider(sl: SliderState) {
            sl.pointerId = -1
            if (sl.control.springBack) {
                sl.thumbPos = 0f
                axisCallback?.invoke(sl.control.axis, 0f)
            }
            invalidate()
        }

        private fun releaseAllSliders() {
            for (sl in sliderStates) if (sl.pointerId >= 0) releaseSlider(sl)
        }

        // ── Axis region helpers ─────────────────────────────────

        private fun updateAxisRegionFromTouch(
            ar: AxisRegionState,
            px: Float,
            py: Float,
        ) {
            val vertical = ar.control.orientation == SliderOrientation.VERTICAL
            val pos = if (vertical) py else px
            // Asymmetric range: full -1..+1 from wherever the touch started
            val posRange = if (vertical) ar.bottom - ar.touchOrigin else ar.right - ar.touchOrigin
            val negRange = if (vertical) ar.touchOrigin - ar.top else ar.touchOrigin - ar.left
            val delta = pos - ar.touchOrigin

            var raw =
                if (delta >= 0f) {
                    if (posRange < 1f) 0f else (delta / posRange).coerceAtMost(1f)
                } else {
                    if (negRange < 1f) 0f else (delta / negRange).coerceAtLeast(-1f)
                }
            raw = applyResponseCurve(raw, ar.control.responseCurve, ar.control.exponent)
            raw = (raw * ar.control.sensitivity).coerceIn(-1f, 1f)
            if (ar.control.invert) raw = -raw
            ar.value = raw
            axisCallback?.invoke(ar.control.axis, raw)
            invalidate()
        }

        private fun releaseAxisRegion(ar: AxisRegionState) {
            ar.pointerId = -1
            ar.value = 0f
            ar.stealSourceStick = null
            axisCallback?.invoke(ar.control.axis, 0f)
            invalidate()
        }

        private fun releaseAllAxisRegions() {
            for (ar in axisRegionStates) if (ar.pointerId >= 0) releaseAxisRegion(ar)
        }

        private fun releaseLayoutButton(
            b: ButtonState,
            fired: Boolean,
        ) {
            if (b.pointerId >= 0) {
                b.pointerId = -1
                if (b.control.binding == TouchBindings.BTN_CHEATS_MENU ||
                    b.control.binding == TouchBindings.BTN_GYRO_RECENTER
                ) {
                    // handled on press, no release action
                } else if (!b.control.toggle) {
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

        // ── Radial menu helpers ─────────────────────────────────

        private fun updateRadialSelection(
            rm: RadialMenuState,
            px: Float,
            py: Float,
        ) {
            val dx = px - rm.triggerX
            val dy = py - rm.triggerY
            val dist = hypot(dx, dy)
            val centerR = rm.radius * 0.22f

            val segs = if (rm.isWeaponWheel) rm.filteredSegments else rm.control.segments
            val n = segs.size

            val old = rm.activeSegment
            rm.activeSegment =
                if (n == 0 || (rm.isWeaponWheel && n <= 1)) {
                    RADIAL_CENTER
                } else if (dist < centerR) {
                    RADIAL_CENTER
                } else {
                    val angle = Math.toDegrees(atan2(dy.toDouble(), dx.toDouble())).toFloat()
                    val segAngle = 360f / n

                    if (rm.isWeaponWheel) {
                        // Counter-clockwise from bottom (90°)
                        var offset = 90f + segAngle / 2f - angle
                        offset = ((offset % 360f) + 360f) % 360f
                        (offset / segAngle).toInt().coerceIn(0, n - 1)
                    } else {
                        // Clockwise from top (-90°)
                        var adjusted = angle + 90f
                        if (adjusted < 0) adjusted += 360f
                        (adjusted / segAngle).toInt().coerceIn(0, n - 1)
                    }
                }

            if (rm.activeSegment != old) {
                if (rm.control.hapticFeedback) {
                    performHapticFeedback(HapticFeedbackConstants.CONTEXT_CLICK)
                }
                invalidate()
            }
        }

        private fun fireRadialSelection(rm: RadialMenuState) {
            val segs = if (rm.isWeaponWheel) rm.filteredSegments else rm.control.segments
            val seg =
                when {
                    rm.activeSegment >= 0 && rm.activeSegment < segs.size -> segs[rm.activeSegment]
                    else -> null
                }
            val binding =
                when {
                    seg != null -> seg.binding
                    rm.activeSegment == RADIAL_CENTER && rm.control.centerBinding >= 0 ->
                        rm.control.centerBinding
                    else -> -1
                }
            val isAction = seg?.bindingType == "action"
            if (binding >= 0) {
                if (TouchBindings.isMetaAction(binding)) {
                    metaActionCallback?.invoke(binding, true)
                    metaActionCallback?.invoke(binding, false)
                } else if (isAction) {
                    buttonCallback?.invoke(binding, true)
                    buttonCallback?.invoke(binding, false)
                } else {
                    val unicode = keycodeToUnicode(binding)
                    keyCallback?.invoke(0, binding, unicode)
                    keyCallback?.invoke(1, binding, 0)
                }
            }
        }

        private fun releaseRadialMenu(
            rm: RadialMenuState,
            fired: Boolean,
        ) {
            if (rm.pointerId >= 0) {
                if (fired) fireRadialSelection(rm)
                rm.pointerId = -1
                rm.isOpen = false
                rm.activeSegment = -1
                rm.isWeaponWheel = false
                rm.filteredSegments = emptyList()
                rm.weaponState = null
                invalidate()
            }
        }

        private fun releaseAllRadialMenus(fired: Boolean) {
            for (rm in radialStates) releaseRadialMenu(rm, fired)
        }

        private fun keycodeToUnicode(keycode: Int): Int =
            when (keycode) {
                in 7..16 -> '0'.code + keycode - 7 // KEYCODE_0(7)..KEYCODE_9(16) → '0'..'9'
                else -> 0
            }

        private fun releaseAllButtons() {
            releaseAllLayoutButtons(false)
            releaseAllRadialMenus(false)
            releaseAllSliders()
            releaseAllAxisRegions()
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
                    while (dAngle > Math.PI.toFloat()) dAngle -= (2 * Math.PI).toFloat()
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
                (event.getY(i0) + event.getY(i1)) / 2f,
            )
        }

        // ── Cheats overlay ──────────────────────────────────────

        private fun drawCheatsOverlay(canvas: Canvas) {
            val w = width.toFloat()
            val h = height.toFloat()
            // Dim background
            canvas.drawColor(0xAA000000.toInt())

            val cheats = if (gameVariant == "d1") TouchBindings.CHEATS_D1 else TouchBindings.CHEATS_D2
            val cols = 4
            val rows = (cheats.size + cols - 1) / cols
            val pad = w * 0.02f
            val closeH = h * 0.08f
            val gridTop = pad
            val gridBottom = h - closeH - pad * 2
            val cellW = (w - pad * (cols + 1)) / cols
            val cellH = ((gridBottom - gridTop) - pad * (rows - 1)) / rows

            cheatsOverlayRects.clear()
            val textPaint =
                Paint(paintBtnLabel).apply {
                    textSize = (cellH * 0.28f).coerceAtMost(w * 0.035f)
                    textAlign = Paint.Align.CENTER
                }

            for (i in cheats.indices) {
                val col = i % cols
                val row = i / cols
                val left = pad + col * (cellW + pad)
                val top = gridTop + row * (cellH + pad)
                val rect = RectF(left, top, left + cellW, top + cellH)
                cheatsOverlayRects.add(rect)

                val bg = if (i == cheatsOverlayPressedIndex) paintBtnPressed else paintBtnIdle
                bg.alpha = if (i == cheatsOverlayPressedIndex) 0xAA else 0x55
                canvas.drawRoundRect(rect, cellH * 0.15f, cellH * 0.15f, bg)
                paintRing.alpha = 0x66
                canvas.drawRoundRect(rect, cellH * 0.15f, cellH * 0.15f, paintRing)

                textPaint.alpha = 0xDD
                canvas.drawText(
                    cheats[i].label,
                    rect.centerX(),
                    rect.centerY() + textPaint.textSize * 0.35f,
                    textPaint,
                )
            }

            // Close button at bottom
            cheatsCloseRect = RectF(pad, h - closeH - pad, w - pad, h - pad)
            val closeBg = Paint(paintBtnIdle).apply { alpha = 0x66 }
            canvas.drawRoundRect(cheatsCloseRect, closeH * 0.25f, closeH * 0.25f, closeBg)
            paintRing.alpha = 0x88
            canvas.drawRoundRect(cheatsCloseRect, closeH * 0.25f, closeH * 0.25f, paintRing)
            val closePaint =
                Paint(paintBtnLabel).apply {
                    textSize = closeH * 0.4f
                    textAlign = Paint.Align.CENTER
                    alpha = 0xDD
                }
            canvas.drawText(
                "Close Cheats",
                cheatsCloseRect.centerX(),
                cheatsCloseRect.centerY() + closePaint.textSize * 0.35f,
                closePaint,
            )
        }

        private fun handleCheatsOverlayTouch(event: MotionEvent): Boolean {
            val idx = event.actionIndex
            val px = event.getX(idx)
            val py = event.getY(idx)

            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    cheatsOverlayPointerId = event.getPointerId(idx)
                    // Check close button
                    if (cheatsCloseRect.contains(px, py)) {
                        cheatsOverlayPressedIndex = -2 // sentinel for close
                        invalidate()
                        return true
                    }
                    // Check cheat buttons
                    for (i in cheatsOverlayRects.indices) {
                        if (cheatsOverlayRects[i].contains(px, py)) {
                            cheatsOverlayPressedIndex = i
                            invalidate()
                            return true
                        }
                    }
                    cheatsOverlayPressedIndex = -1
                    return true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                    if (event.getPointerId(idx) == cheatsOverlayPointerId) {
                        if (cheatsOverlayPressedIndex == -2 && cheatsCloseRect.contains(px, py)) {
                            cheatsOverlayOpen = false
                        } else if (cheatsOverlayPressedIndex >= 0) {
                            val cheats = if (gameVariant == "d1") TouchBindings.CHEATS_D1 else TouchBindings.CHEATS_D2
                            if (cheatsOverlayPressedIndex < cheats.size &&
                                cheatsOverlayPressedIndex < cheatsOverlayRects.size &&
                                cheatsOverlayRects[cheatsOverlayPressedIndex].contains(px, py)
                            ) {
                                cheatCodeCallback?.invoke(cheats[cheatsOverlayPressedIndex].code)
                                performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                            }
                        }
                        cheatsOverlayPressedIndex = -1
                        cheatsOverlayPointerId = -1
                        invalidate()
                    }
                    return true
                }
                MotionEvent.ACTION_CANCEL -> {
                    cheatsOverlayPressedIndex = -1
                    cheatsOverlayPointerId = -1
                    invalidate()
                    return true
                }
            }
            return true // consume all events while overlay is open
        }

        // ── Admin tray ──────────────────────────────────────────

        private fun adminTrayLabel(index: Int): String =
            when (index) {
                ADMIN_INCREASE_VIEW -> {
                    val mode = adminTrayCockpitModeProvider?.invoke() ?: -1
                    val suffix =
                        when (mode) {
                            CM_FULL_COCKPIT -> " [Cockpit]"
                            CM_STATUS_BAR -> " [Status]"
                            CM_FULL_SCREEN -> " [Full]"
                            else -> ""
                        }
                    "View +$suffix"
                }
                ADMIN_DECREASE_VIEW -> "View -"
                ADMIN_TOGGLE_AUTOLEVEL -> {
                    val on = adminTrayAutoLevelingProvider?.invoke() ?: true
                    if (on) "AutoLevel: ON" else "AutoLevel: OFF"
                }
                ADMIN_QUICK_SAVE -> "Quick Save"
                ADMIN_QUICK_LOAD -> "Quick Load"
                ADMIN_OPEN_MENU -> "Game Menu"
                ADMIN_NET_STATS -> "Net Stats"
                ADMIN_EXIT_LAUNCHER -> "Exit"
                ADMIN_NET_EVENTS -> "Net Events"
                else -> ""
            }

        private fun drawAdminTrayTab(canvas: Canvas) {
            val w = width.toFloat()
            val tabW = w * 0.12f
            val tabH = w * 0.03f
            val tabLeft = (w - tabW) / 2f
            val tabTop = height.toFloat() - tabH
            adminTrayTabRect = RectF(tabLeft, tabTop, tabLeft + tabW, height.toFloat())

            val bg = Paint(paintBtnIdle).apply { alpha = 0x33 }
            val cornerR = tabH * 0.5f
            canvas.drawRoundRect(adminTrayTabRect, cornerR, cornerR, bg)
            paintRing.alpha = 0x44
            canvas.drawRoundRect(adminTrayTabRect, cornerR, cornerR, paintRing)

            val textP =
                Paint(paintBtnLabel).apply {
                    textSize = tabH * 0.6f
                    textAlign = Paint.Align.CENTER
                    alpha = 0x66
                }
            canvas.drawText(
                "Settings",
                adminTrayTabRect.centerX(),
                adminTrayTabRect.centerY() + textP.textSize * 0.35f,
                textP,
            )
        }

        private fun drawAdminTrayPanel(canvas: Canvas) {
            val w = width.toFloat()
            val h = height.toFloat()

            val itemCount = 9
            val cols = 3
            val rows = (itemCount + cols - 1) / cols
            val divider = 1f // 1px divider between cells
            val panelW = w * 0.7f
            val cellH = h * 0.08f
            val handleH = h * 0.02f
            val panelH = rows * cellH + (rows - 1) * divider + handleH
            val panelLeft = (w - panelW) / 2f
            // Bottom-anchored, offset by slide progress (1 = fully visible)
            val panelTop = h - panelH * adminTraySlide
            val cornerR = panelW * 0.02f

            adminTrayRects.clear()
            val textPaint =
                Paint(paintBtnLabel).apply {
                    textSize = (cellH * 0.3f).coerceAtMost(w * 0.03f)
                    textAlign = Paint.Align.CENTER
                }

            // Panel background
            val panelBg =
                Paint(Paint.ANTI_ALIAS_FLAG).apply {
                    style = Paint.Style.FILL
                    color = 0xDD222222.toInt()
                }
            val panelRect = RectF(panelLeft, panelTop, panelLeft + panelW, panelTop + panelH)
            canvas.drawRoundRect(panelRect, cornerR, cornerR, panelBg)
            paintRing.alpha = 0x66
            canvas.drawRoundRect(panelRect, cornerR, cornerR, paintRing)

            // Drag handle bar at top of panel
            val handlePaint =
                Paint(Paint.ANTI_ALIAS_FLAG).apply {
                    style = Paint.Style.FILL
                    color = 0x66FFFFFF.toInt()
                }
            val barW = panelW * 0.15f
            val barH = handleH * 0.25f
            val barLeft = panelLeft + (panelW - barW) / 2f
            val barTop = panelTop + handleH * 0.375f
            canvas.drawRoundRect(barLeft, barTop, barLeft + barW, barTop + barH, barH / 2, barH / 2, handlePaint)

            val cellW = (panelW - (cols - 1) * divider) / cols
            val gridTop = panelTop + handleH

            for (i in 0 until itemCount) {
                val col = i % cols
                val row = i / cols
                val left = panelLeft + col * (cellW + divider)
                val top = gridTop + row * (cellH + divider)
                val rect = RectF(left, top, left + cellW, top + cellH)
                adminTrayRects.add(rect)

                val bg = if (i == adminTrayPressedIndex) paintBtnPressed else paintBtnIdle
                bg.alpha = if (i == adminTrayPressedIndex) 0xAA else 0x55
                canvas.drawRect(rect, bg)

                // Draw divider lines
                if (col > 0) {
                    val divPaint = Paint().apply { color = 0x33FFFFFF.toInt() }
                    canvas.drawRect(left - divider, top, left, top + cellH, divPaint)
                }
                if (row > 0) {
                    val divPaint = Paint().apply { color = 0x33FFFFFF.toInt() }
                    canvas.drawRect(left, top - divider, left + cellW, top, divPaint)
                }

                textPaint.alpha = 0xDD
                canvas.drawText(
                    adminTrayLabel(i),
                    rect.centerX(),
                    rect.centerY() + textPaint.textSize * 0.35f,
                    textPaint,
                )
            }
        }

        private fun animateAdminTray(open: Boolean) {
            adminTrayAnimator?.cancel()
            val target = if (open) 1f else 0f
            adminTrayAnimator =
                ValueAnimator.ofFloat(adminTraySlide, target).apply {
                    duration = 200
                    interpolator = DecelerateInterpolator()
                    addUpdateListener {
                        adminTraySlide = it.animatedValue as Float
                        if (adminTraySlide == 0f) adminTrayOpen = false
                        invalidate()
                    }
                    start()
                }
        }

        private fun handleAdminTrayTouch(event: MotionEvent): Boolean {
            val idx = event.actionIndex
            val px = event.getX(idx)
            val py = event.getY(idx)

            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    adminTrayPointerId = event.getPointerId(idx)
                    adminTrayDragStartY = py
                    adminTrayDragging = false
                    // Check grid buttons
                    for (i in adminTrayRects.indices) {
                        if (adminTrayRects[i].contains(px, py)) {
                            adminTrayPressedIndex = i
                            invalidate()
                            return true
                        }
                    }
                    // Tap outside panel closes it
                    val panelTop =
                        if (adminTrayRects.isNotEmpty()) {
                            adminTrayRects[0].top - height * 0.02f // above first row
                        } else {
                            height.toFloat()
                        }
                    if (py < panelTop) {
                        animateAdminTray(false)
                        adminTrayPressedIndex = -1
                        adminTrayPointerId = -1
                        return true
                    }
                    return true
                }
                MotionEvent.ACTION_MOVE -> {
                    if (adminTrayPointerId < 0) return true
                    val pi = event.findPointerIndex(adminTrayPointerId)
                    if (pi < 0) return true
                    val cy = event.getY(pi)
                    val dy = cy - adminTrayDragStartY
                    // Start drag after small threshold
                    if (!adminTrayDragging && dy > 10f) {
                        adminTrayDragging = true
                        adminTrayPressedIndex = -1
                    }
                    if (adminTrayDragging) {
                        // Compute panel height for slide ratio
                        val h = height.toFloat()
                        val itemCount = 9
                        val cols = 3
                        val rows = (itemCount + cols - 1) / cols
                        val cellH = h * 0.08f
                        val panelH = rows * cellH + (rows - 1) + h * 0.02f
                        adminTraySlide = (1f - dy / panelH).coerceIn(0f, 1f)
                        invalidate()
                    }
                    return true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                    if (event.getPointerId(idx) == adminTrayPointerId) {
                        if (adminTrayDragging) {
                            // If dragged past 30% threshold, close; otherwise snap open
                            animateAdminTray(adminTraySlide > 0.7f)
                        } else if (adminTrayPressedIndex >= 0 &&
                            adminTrayPressedIndex < adminTrayRects.size &&
                            adminTrayRects[adminTrayPressedIndex].contains(px, py)
                        ) {
                            adminTrayCallback?.invoke(adminTrayPressedIndex)
                            performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                            invalidate()
                        }
                        adminTrayPressedIndex = -1
                        adminTrayPointerId = -1
                        adminTrayDragging = false
                    }
                    return true
                }
                MotionEvent.ACTION_CANCEL -> {
                    if (adminTrayDragging) animateAdminTray(adminTraySlide > 0.7f)
                    adminTrayPressedIndex = -1
                    adminTrayPointerId = -1
                    adminTrayDragging = false
                    invalidate()
                    return true
                }
            }
            return true
        }
    }

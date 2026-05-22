package com.dxxredux.app

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.PointF
import android.graphics.RectF
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.util.AttributeSet
import android.util.Log
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import android.view.animation.DecelerateInterpolator
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sign
import kotlin.math.sin

internal fun adminTrayUsesCheckbox(actionIndex: Int): Boolean =
    when (actionIndex) {
        TouchOverlayView.ADMIN_NET_EVENTS,
        TouchOverlayView.ADMIN_NET_STATS,
        TouchOverlayView.ADMIN_VIDEO_INFO,
        -> true

        else -> false
    }

internal fun adminTrayClosesAfterActivate(actionIndex: Int): Boolean =
    when (actionIndex) {
        TouchOverlayView.ADMIN_INCREASE_VIEW,
        TouchOverlayView.ADMIN_MUSIC,
        TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL,
        -> false

        else -> !adminTrayUsesCheckbox(actionIndex)
    }

internal fun adminTrayActionEnabled(
    actionIndex: Int,
    enabledProvider: ((Int) -> Boolean)? = null,
): Boolean = enabledProvider?.invoke(actionIndex) != false

internal fun adminTrayVisibleActions(
    gamepadOnlyMode: Boolean,
    hasTouchAutomapButton: Boolean,
): List<Int> {
    val actions =
        mutableListOf(
            TouchOverlayView.ADMIN_INCREASE_VIEW,
            TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL,
            TouchOverlayView.ADMIN_NET_STATS,
            TouchOverlayView.ADMIN_QUICK_LOAD,
            TouchOverlayView.ADMIN_OPEN_MENU,
            TouchOverlayView.ADMIN_NET_EVENTS,
            TouchOverlayView.ADMIN_EXIT_LAUNCHER,
            TouchOverlayView.ADMIN_QUICK_SAVE,
            TouchOverlayView.ADMIN_VIDEO_INFO,
        )
    if (gamepadOnlyMode || !hasTouchAutomapButton) {
        actions.add(TouchOverlayView.ADMIN_AUTOMAP)
    }
    if (gamepadOnlyMode) {
        actions.add(TouchOverlayView.ADMIN_HEADLIGHT)
        actions.add(TouchOverlayView.ADMIN_WARP)
        actions.add(TouchOverlayView.ADMIN_MUSIC)
        actions.add(TouchOverlayView.ADMIN_ACCEPT_JOIN)
    }
    return actions
}

private const val CONTROLLER_MENU_FOCUS_COLOR = 0xFF00CC66.toInt()

internal enum class ControllerMenuSurface {
    NONE,
    REMAINING_ACTIONS,
    ADMIN_TRAY,
}

internal fun nextControllerMenuSurface(
    current: ControllerMenuSurface,
    hasRemainingActions: Boolean,
): ControllerMenuSurface =
    when (current) {
        ControllerMenuSurface.NONE -> {
            if (hasRemainingActions) {
                ControllerMenuSurface.REMAINING_ACTIONS
            } else {
                ControllerMenuSurface.ADMIN_TRAY
            }
        }

        ControllerMenuSurface.REMAINING_ACTIONS -> {
            ControllerMenuSurface.ADMIN_TRAY
        }

        ControllerMenuSurface.ADMIN_TRAY -> {
            ControllerMenuSurface.NONE
        }
    }

internal fun moveRemainingActionSelection(
    currentIndex: Int,
    actionCount: Int,
    rowCount: Int,
    keyCode: Int,
): Int {
    if (actionCount <= 0) return -1

    val safeRowCount = rowCount.coerceAtLeast(1)
    val safeIndex = currentIndex.coerceIn(0, actionCount - 1)
    val columnCount = (actionCount + safeRowCount - 1) / safeRowCount
    val currentColumn = safeIndex / safeRowCount
    val currentRow = safeIndex % safeRowCount

    fun columnLength(column: Int): Int = (actionCount - column * safeRowCount).coerceIn(0, safeRowCount)

    return when (keyCode) {
        android.view.KeyEvent.KEYCODE_DPAD_UP -> {
            if (currentRow > 0) {
                safeIndex - 1
            } else {
                safeIndex
            }
        }

        android.view.KeyEvent.KEYCODE_DPAD_DOWN -> {
            if (currentRow + 1 < columnLength(currentColumn)) {
                safeIndex + 1
            } else {
                safeIndex
            }
        }

        android.view.KeyEvent.KEYCODE_DPAD_LEFT -> {
            if (currentColumn > 0) {
                val previousColumn = currentColumn - 1
                val previousColumnStart = previousColumn * safeRowCount
                previousColumnStart + min(currentRow, columnLength(previousColumn) - 1)
            } else {
                safeIndex
            }
        }

        android.view.KeyEvent.KEYCODE_DPAD_RIGHT -> {
            if (currentColumn + 1 < columnCount) {
                val nextColumn = currentColumn + 1
                val nextColumnStart = nextColumn * safeRowCount
                nextColumnStart + min(currentRow, columnLength(nextColumn) - 1)
            } else {
                safeIndex
            }
        }

        else -> {
            safeIndex
        }
    }
}

private const val SECONDARY_PROXIMITY_INDEX = 2
private const val SECONDARY_SMART_MINE_INDEX = 7

private val d1PrimarySelectionNames =
    listOf(
        "Laser",
        "Vulcan",
        "Spreadfire",
        "Plasma",
        "Fusion",
    )

private val d2PrimarySelectionNames =
    listOf(
        "Laser",
        "Vulcan",
        "Spreadfire",
        "Plasma",
        "Fusion",
        "Super Laser",
        "Gauss",
        "Helix",
        "Phoenix",
        "Omega",
    )

private val d1SecondarySelectionNames =
    listOf(
        "Concussion",
        "Homing",
        "Proximity Bomb",
        "Smart Missile",
        "Mega Missile",
    )

private val d2SecondarySelectionNames =
    listOf(
        "Concussion",
        "Homing",
        "Proximity Bomb",
        "Smart Missile",
        "Mega Missile",
        "Flash Missile",
        "Guided Missile",
        "Smart Mine",
        "Mercury Missile",
        "Earthshaker",
    )

private fun buttonUsesBindingIndicator(
    button: ButtonControl,
    binding: Int,
): Boolean =
    button.binding == binding ||
        (button.longPressEnabled && button.longPressBinding == binding)

internal fun buttonUsesGyroToggleIndicator(button: ButtonControl): Boolean =
    buttonUsesBindingIndicator(button, TouchBindings.META_GYRO_TOGGLE)

internal fun buttonUsesHeadlightIndicator(button: ButtonControl): Boolean =
    buttonUsesBindingIndicator(button, TouchBindings.BTN_HEADLIGHT)

private fun currentBombName(
    gameVariant: String,
    weaponState: WeaponState?,
): String? =
    if (gameVariant != "d2" || weaponState == null) {
        null
    } else {
        when (weaponState.currentBomb) {
            SECONDARY_PROXIMITY_INDEX -> "Proximity Bomb"
            SECONDARY_SMART_MINE_INDEX -> "Smart Mine"
            else -> null
        }
    }

private fun currentPrimaryName(
    gameVariant: String,
    weaponState: WeaponState?,
): String? {
    val currentPrimary = weaponState?.currentPrimary ?: return null
    val names = if (gameVariant == "d1") d1PrimarySelectionNames else d2PrimarySelectionNames
    return names.getOrNull(currentPrimary)
}

private fun currentSecondaryName(
    gameVariant: String,
    weaponState: WeaponState?,
): String? {
    val currentSecondary = weaponState?.currentSecondary ?: return null
    val names = if (gameVariant == "d1") d1SecondarySelectionNames else d2SecondarySelectionNames
    return names.getOrNull(currentSecondary)
}

internal fun buttonDisplayLabel(
    button: ButtonControl,
    gameVariant: String,
    weaponState: WeaponState?,
): String {
    val action =
        when (button.binding) {
            TouchBindings.BTN_DROP_BOMB -> "Drop"

            TouchBindings.BTN_TOGGLE_BOMB,
            TouchBindings.BTN_CYCLE_PRIMARY,
            TouchBindings.BTN_CYCLE_SECONDARY,
            -> "Cycle"

            TouchBindings.BTN_FIRE_PRIMARY,
            TouchBindings.BTN_FIRE_SECONDARY,
            -> "Fire"

            else -> null
        }
    val selection =
        when (button.binding) {
            TouchBindings.BTN_DROP_BOMB,
            TouchBindings.BTN_TOGGLE_BOMB,
            -> currentBombName(gameVariant, weaponState)

            TouchBindings.BTN_FIRE_PRIMARY,
            TouchBindings.BTN_CYCLE_PRIMARY,
            -> currentPrimaryName(gameVariant, weaponState)

            TouchBindings.BTN_FIRE_SECONDARY,
            TouchBindings.BTN_CYCLE_SECONDARY,
            -> currentSecondaryName(gameVariant, weaponState)

            else -> null
        }

    return if (action != null && selection != null) "$action\n$selection" else button.label
}

internal fun buttonHasActiveIndicatorState(
    button: ButtonControl,
    gameVariant: String,
    weaponState: WeaponState?,
    gyroConfigured: Boolean,
    gyroActiveInGame: Boolean,
): Boolean {
    val gyroActive = buttonUsesGyroToggleIndicator(button) && gyroConfigured && gyroActiveInGame
    val headlightActive =
        buttonUsesHeadlightIndicator(button) &&
            gameVariant == "d2" &&
            weaponState?.isHeadlightOn == true
    return gyroActive || headlightActive
}

internal fun dragZoneButtonLatchAllowed(
    gameVariant: String,
    binding: Int,
    pointerId: Int,
    toggle: Boolean,
): Boolean {
    if (gameVariant == "d1" && binding in TouchBindings.D2_ONLY_BUTTONS) return false
    if (pointerId >= 0 || toggle) return false
    return when (binding) {
        TouchBindings.BTN_CHEATS_MENU,
        TouchBindings.BTN_GYRO_RECENTER,
        TouchBindings.BTN_AUTOMAP,
        -> false

        else -> true
    }
}

internal fun buttonExtendsDragZoneStart(
    zoneLeft: Float,
    zoneTop: Float,
    zoneRight: Float,
    zoneBottom: Float,
    buttonCenterX: Float,
    buttonCenterY: Float,
    buttonRadius: Float,
    touchX: Float,
    touchY: Float,
): Boolean =
    buttonCenterX in zoneLeft..zoneRight &&
        buttonCenterY in zoneTop..zoneBottom &&
        hypot(touchX - buttonCenterX, touchY - buttonCenterY) <= buttonRadius * 1.3f

internal data class RemainingTouchAction(
    val binding: Int,
    val label: String,
)

private val remainingBaseActionBindings =
    listOf(
        TouchBindings.BTN_AUTOMAP,
        TouchBindings.BTN_FIRE_FLARE,
        TouchBindings.BTN_DROP_BOMB,
        TouchBindings.BTN_HEADLIGHT,
        TouchBindings.BTN_TOGGLE_BOMB,
        TouchBindings.BTN_ENERGY_SHIELD,
        TouchBindings.META_DROP_MARKER,
        TouchBindings.META_DEMO_RECORD_TOGGLE,
        TouchBindings.META_REWIND,
    )

private val remainingMultiplayerActionBindings =
    listOf(
        TouchBindings.META_MULTIPLAYER_HUD,
        TouchBindings.META_DROP_FLAG,
    )

private val remainingPrimaryWeaponBindings =
    listOf(
        TouchBindings.META_WEAPON_1,
        TouchBindings.META_WEAPON_2,
        TouchBindings.META_WEAPON_3,
        TouchBindings.META_WEAPON_4,
        TouchBindings.META_WEAPON_5,
    )

private val remainingSecondaryWeaponBindings =
    listOf(
        TouchBindings.META_WEAPON_6,
        TouchBindings.META_WEAPON_7,
        TouchBindings.META_WEAPON_8,
        TouchBindings.META_WEAPON_9,
        TouchBindings.META_WEAPON_10,
    )

private val remainingGuideBindings =
    listOf(
        TouchBindings.META_GUIDE_BOT_MENU,
        TouchBindings.META_GUIDE_RELEASE_CONTROL,
    )

internal fun touchLayoutBoundActionBindings(layout: TouchLayout): Set<Int> {
    val bindings = linkedSetOf<Int>()

    layout.buttons.forEach { button ->
        bindings.add(button.binding)
        if (button.longPressEnabled && button.longPressBinding >= 0) {
            bindings.add(button.longPressBinding)
        }
    }
    layout.sticks.forEach { stick ->
        if (stick.buttonMode) {
            bindings.add(stick.negXBinding)
            bindings.add(stick.posXBinding)
            bindings.add(stick.negYBinding)
            bindings.add(stick.posYBinding)
        }
        if (stick.doubleTapBinding >= 0) {
            bindings.add(stick.doubleTapBinding)
        }
    }
    layout.radialMenus.forEach { radial ->
        radial.segments.forEach { segment ->
            if (segment.bindingType == "action" || TouchBindings.isMetaAction(segment.binding)) {
                bindings.add(segment.binding)
            }
        }
        if (radial.centerBinding >= 0) {
            bindings.add(radial.centerBinding)
        }
    }
    layout.dpads.forEach { dpad ->
        bindings.add(dpad.upBinding)
        bindings.add(dpad.downBinding)
        bindings.add(dpad.leftBinding)
        bindings.add(dpad.rightBinding)
    }

    return bindings
}

private fun remainingCandidateBindings(
    layout: TouchLayout,
    isMultiplayerGame: Boolean,
): List<Int> =
    buildList {
        val hasPrimaryWheel = layout.radialMenus.any { it.id == "PriWpn" }
        val hasSecondaryWheel = layout.radialMenus.any { it.id == "SecWpn" }

        addAll(remainingBaseActionBindings)
        if (layout.gyro.enabled) add(TouchBindings.META_GYRO_TOGGLE)
        if (isMultiplayerGame) addAll(remainingMultiplayerActionBindings)
        if (!hasPrimaryWheel) {
            add(TouchBindings.BTN_CYCLE_PRIMARY)
            addAll(remainingPrimaryWeaponBindings)
        }
        if (!hasSecondaryWheel) {
            add(TouchBindings.BTN_CYCLE_SECONDARY)
            addAll(remainingSecondaryWeaponBindings)
        }
        if (layout.radialMenus.none { it.id == "Guide" }) addAll(remainingGuideBindings)
    }

internal fun remainingActionLabel(
    binding: Int,
    gameVariant: String,
    weaponState: WeaponState? = null,
): String =
    when (binding) {
        TouchBindings.BTN_TOGGLE_BOMB -> {
            val currentBomb = currentBombName(gameVariant, weaponState)
            if (currentBomb != null) {
                "Toggle Bomb [current: $currentBomb]"
            } else {
                TouchBindings.ALL_BUTTON_LABELS[binding]
                    ?: TouchBindings.bindingToName(binding).removePrefix("Meta: ")
            }
        }

        else -> {
            TouchBindings.ALL_BUTTON_LABELS[binding]
                ?: TouchBindings.bindingToName(binding).removePrefix("Meta: ")
        }
    }

internal fun remainingKeyTouchActions(
    layout: TouchLayout,
    gameVariant: String,
    isMultiplayerGame: Boolean = false,
    weaponState: WeaponState? = null,
): List<RemainingTouchAction> {
    val boundBindings = touchLayoutBoundActionBindings(layout)
    return remainingCandidateBindings(layout, isMultiplayerGame)
        .filter { binding ->
            gameVariant != "d1" ||
                (binding !in TouchBindings.D2_ONLY_BUTTONS && binding !in TouchBindings.D2_ONLY_META_ACTIONS)
        }.filter { it !in boundBindings }
        .distinct()
        .map { RemainingTouchAction(it, remainingActionLabel(it, gameVariant, weaponState)) }
}

private const val MOUSE_HISTORY_WINDOW_MS = 16f
private const val MOUSE_HISTORY_DECAY_PER_WINDOW = 0.75f
private const val MOUSE_NO_ACCEL_DISTANCE_PER_WINDOW = 8f
private const val MOUSE_EXCESS_DISTANCE_REFERENCE = 18f
private const val MOUSE_DISTANCE_FALLBACK_START = 24f
private const val MOUSE_DISTANCE_FALLBACK_WEIGHT = 0.2f

internal data class MouseAccelerationHistory(
    val recentDistancePx: Float,
    val recentGracePx: Float,
)

internal fun updateMouseAccelerationHistory(
    previousDistancePx: Float,
    previousGracePx: Float,
    stepDistancePx: Float,
    dtMs: Long,
): MouseAccelerationHistory {
    val windowCount = (dtMs.coerceAtLeast(1L).toFloat() / MOUSE_HISTORY_WINDOW_MS).coerceIn(1f, 6f)
    val decay = MOUSE_HISTORY_DECAY_PER_WINDOW.pow(windowCount)
    return MouseAccelerationHistory(
        recentDistancePx = previousDistancePx * decay + stepDistancePx,
        recentGracePx = previousGracePx * decay + MOUSE_NO_ACCEL_DISTANCE_PER_WINDOW * windowCount,
    )
}

internal fun mouseAccelerationMultiplier(
    enabled: Boolean,
    maxMultiplier: Float,
    recentDistancePx: Float,
    recentGracePx: Float,
    distancePx: Float,
    viewportHeightPx: Float,
): Float {
    if (!enabled) return 1f
    val clampedMax = maxMultiplier.coerceAtLeast(1f)
    val historyRatio =
        ((recentDistancePx - recentGracePx).coerceAtLeast(0f) / MOUSE_EXCESS_DISTANCE_REFERENCE).coerceIn(0f, 1f)
    val distanceRatio =
        if (viewportHeightPx > 1f) {
            ((distancePx - MOUSE_DISTANCE_FALLBACK_START).coerceAtLeast(0f) / (viewportHeightPx * 0.5f)).coerceIn(
                0f,
                1f,
            ) * MOUSE_DISTANCE_FALLBACK_WEIGHT
        } else {
            0f
        }
    val ratio = maxOf(historyRatio, distanceRatio)
    return 1f + (clampedMax - 1f) * ratio
}

/**
 * Semi-transparent touch overlay drawn on top of the game SurfaceView.
 *
 * Left side  – virtual analog stick (yaw / pitch)
 * Right side – two fire buttons (primary = button 0, secondary = button 1)
 *
 * axis values  → [axisCallback]   → InputMixer → nativeJoystickAxis()
 * button press → InputMixer.setButton() → nativeJoystickButton()
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

        /** Input mixer for OR-combining button states from multiple sources. */
        var inputMixer: InputMixer? = null

        private var mouseDiagCount = 0

        private fun logMouseDiag(message: String) {
            mouseDiagCount += 1
            if (mouseDiagCount <= 20 || mouseDiagCount % 25 == 0) {
                DebugLog.log(DebugLogCategory.GAME, "[touch-mouse] $message")
                Log.d("DXX-TouchMouse", message)
            }
        }

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

        private var gyroConfigured = false
        private var gyroActiveInGame = false

        /** Returns true while this activity is running a multiplayer session. */
        var isMultiplayerGameProvider: (() -> Boolean)? = null

        /** Returns true if local player owns the Guide-Bot in coop. Set by MainActivity. */
        var isEscortOwnerProvider: (() -> Boolean)? = null

        /** Returns owning player's callsign for Guide wheel non-owner display. */
        var escortOwnerCallsignProvider: (() -> String)? = null

        /** Returns true when guidebot has been released (cage walls destroyed). */
        var isBuddyReleasedProvider: (() -> Boolean)? = null

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

            // Mouse mode: drag-start anchor and recent motion state
            var mouseOriginX = 0f
            var mouseOriginY = 0f
            var mouseLastSampleTimeMs = 0L
            var mouseRecentDistancePx = 0f
            var mouseRecentGracePx = 0f

            // Button mode: direction press tracking
            var xNegPressed = false
            var xPosPressed = false
            var yNegPressed = false
            var yPosPressed = false

            // Double-tap tracking
            var lastTapTime: Long = 0
            var tapCount = 0 // running tap count within double-tap window
            var dtLatched = false // true when double-tap latch is held on
        }

        private class ButtonState(
            val control: ButtonControl,
        ) {
            var centerX = 0f
            var centerY = 0f
            var radius = 0f
            var pointerId = -1
            var toggled = false
            var longPressTriggered = false
            var longPressRunnable: Runnable? = null
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
            var quiescentLabel: String = "" // current weapon name for closed state

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

            // Music-type geometry (computed in computeGeometry)
            var musicPrevCX = 0f
            var musicNextCX = 0f
            var musicBtnY = 0f
            var musicBtnR = 0f
            var musicLabelX = 0f
            var musicPrevPid = -1
            var musicNextPid = -1
            var musicLabelPid = -1

            // Settings-type state
            var menuPid = -1
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

        /**
         * Dispatch a touch button event through the InputMixer.
         * Meta actions bypass the mixer and go to metaActionCallback.
         * [sourceTag] uniquely identifies the touch control so the mixer can
         * OR-combine multiple simultaneous sources for the same binding.
         */
        private fun dispatchTouchButton(
            binding: Int,
            pressed: Boolean,
            sourceTag: String = "touch",
        ) {
            if (TouchBindings.isMetaAction(binding)) {
                metaActionCallback?.invoke(binding, pressed)
                return
            }
            inputMixer?.setButton(binding, sourceTag, pressed)
        }

        private fun buttonSourceTag(b: ButtonState): String = "touch:btn${buttonStates.indexOf(b)}"

        private fun buttonLongPressTag(b: ButtonState): String = "${buttonSourceTag(b)}:long"

        private fun canLatchButtonIntoDragZone(b: ButtonState): Boolean =
            dragZoneButtonLatchAllowed(
                gameVariant = gameVariant,
                binding = b.control.binding,
                pointerId = b.pointerId,
                toggle = b.control.toggle,
            )

        private fun pressLayoutButton(
            b: ButtonState,
            pointerId: Int,
        ) {
            b.pointerId = pointerId
            b.longPressTriggered = false
            if (b.control.binding == TouchBindings.BTN_CHEATS_MENU ||
                b.control.binding == TouchBindings.BTN_GYRO_RECENTER
            ) {
                pressLayoutButtonBinding(b.control.binding, buttonSourceTag(b))
            } else if (b.control.toggle) {
                b.toggled = !b.toggled
                if (b.control.binding == TouchBindings.BTN_AUTOMAP) {
                    if (b.toggled) mapButtonCallback?.invoke()
                } else {
                    dispatchTouchButton(b.control.binding, b.toggled, buttonSourceTag(b))
                }
            } else {
                pressLayoutButtonBinding(b.control.binding, buttonSourceTag(b))
            }
            if (b.control.longPressEnabled && b.control.longPressBinding >= 0) {
                val longPressRunnable =
                    Runnable {
                        if (b.pointerId < 0) return@Runnable
                        b.longPressRunnable = null
                        b.longPressTriggered = true
                        pressLayoutButtonBinding(b.control.longPressBinding, buttonLongPressTag(b))
                        if (b.control.hapticFeedback) {
                            performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
                        }
                        invalidate()
                    }
                b.longPressRunnable = longPressRunnable
                mainHandler.postDelayed(longPressRunnable, b.control.longPressDurationMs.toLong())
            }
            if (b.control.hapticFeedback) {
                performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
            }
            invalidate()
        }

        private fun tryPressDragZoneButton(
            pointerId: Int,
            px: Float,
            py: Float,
        ) {
            for (b in buttonStates) {
                if (!canLatchButtonIntoDragZone(b)) continue
                if (hypot(px - b.centerX, py - b.centerY) <= b.radius * 1.3f) {
                    pressLayoutButton(b, pointerId)
                    return
                }
            }
        }

        private fun pointInStickDragZone(
            s: StickState,
            px: Float,
            py: Float,
        ): Boolean = px in s.fzLeft..s.fzRight && py in s.fzTop..s.fzBottom

        private fun findDragZoneStartExtensionButton(
            s: StickState,
            px: Float,
            py: Float,
        ): ButtonState? {
            for (b in buttonStates) {
                if (!canLatchButtonIntoDragZone(b)) continue
                if (
                    buttonExtendsDragZoneStart(
                        zoneLeft = s.fzLeft,
                        zoneTop = s.fzTop,
                        zoneRight = s.fzRight,
                        zoneBottom = s.fzBottom,
                        buttonCenterX = b.centerX,
                        buttonCenterY = b.centerY,
                        buttonRadius = b.radius,
                        touchX = px,
                        touchY = py,
                    )
                ) {
                    return b
                }
            }
            return null
        }

        private fun pressLayoutButtonBinding(
            binding: Int,
            sourceTag: String,
        ) {
            when (binding) {
                TouchBindings.BTN_CHEATS_MENU -> cheatsOverlayOpen = !cheatsOverlayOpen
                TouchBindings.BTN_GYRO_RECENTER -> gyroManager?.calibrate()
                TouchBindings.BTN_AUTOMAP -> Unit
                else -> dispatchTouchButton(binding, true, sourceTag)
            }
        }

        private fun releaseLayoutButtonBinding(
            binding: Int,
            fired: Boolean,
            sourceTag: String,
        ) {
            when (binding) {
                TouchBindings.BTN_CHEATS_MENU,
                TouchBindings.BTN_GYRO_RECENTER,
                -> {
                    Unit
                }

                TouchBindings.BTN_AUTOMAP -> {
                    if (fired) mapButtonCallback?.invoke()
                }

                else -> {
                    dispatchTouchButton(binding, false, sourceTag)
                }
            }
        }

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
                logMouseDiag(
                    "drain axis=(${s.control.axisX},${s.control.axisY}) pending=(${"%.4f".format(
                        s.mousePendingX,
                    )},${"%.4f".format(s.mousePendingY)}) " +
                        "emit=(${"%.4f".format(
                            emitX,
                        )},${"%.4f".format(emitY)}) out=(${"%.4f".format(outX)},${"%.4f".format(outY)}) " +
                        "cap=(${"%.3f".format(
                            capX,
                        )},${"%.3f".format(
                            capY,
                        )}) sens=(${"%.2f".format(s.control.sensitivityX)},${"%.2f".format(s.control.sensitivityY)}) " +
                        "mouseExp=${s.control.mouseExponential} curve=${s.control.responseCurve} deadzone=${s.control.deadzone}",
                )
                axisCallback?.invoke(s.control.axisX, outX.coerceIn(-1f, 1f))
                axisCallback?.invoke(s.control.axisY, outY.coerceIn(-1f, 1f))
            }
        }

        private fun beginMouseDrag(
            s: StickState,
            pointerId: Int,
            px: Float,
            py: Float,
            originX: Float = px,
            originY: Float = py,
        ) {
            s.pointerId = pointerId
            s.mouseLastX = px
            s.mouseLastY = py
            s.mouseOriginX = originX
            s.mouseOriginY = originY
            s.mousePendingX = 0f
            s.mousePendingY = 0f
            s.mouseLastSampleTimeMs = android.os.SystemClock.uptimeMillis()
            s.mouseRecentDistancePx = 0f
            s.mouseRecentGracePx = 0f
            s.floatingActive = true
            startMouseDrain()
        }

        // ── MAP button geometry (kept for automap overlay compat) ──
        private var mapBtnCenterX = 0f
        private var mapBtnCenterY = 0f
        private var mapBtnRadius = 0f

        // ── Non-layout pointer tracking ─────────────────────────
        private val passthroughPointers = mutableSetOf<Int>()
        private var mapBtnPointerId = -1 // used by automap overlay

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
            const val ADMIN_TOGGLE_AUTOLEVEL = 1
            const val ADMIN_NET_STATS = 2
            const val ADMIN_QUICK_LOAD = 3
            const val ADMIN_OPEN_MENU = 4
            const val ADMIN_NET_EVENTS = 5
            const val ADMIN_EXIT_LAUNCHER = 6
            const val ADMIN_QUICK_SAVE = 7
            const val ADMIN_VIDEO_INFO = 8

            // Gamepad-only items (appended when no touchscreen)
            const val ADMIN_AUTOMAP = 9
            const val ADMIN_HEADLIGHT = 10
            const val ADMIN_WARP = 11
            const val ADMIN_MUSIC = 12
            const val ADMIN_ACCEPT_JOIN = 13

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
        private val paintBtnLatched =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0x1A4CAF50 // Material Green at ~10% opacity
            }
        private val paintBtnIdleDisabled =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0x334A4A4A
            }
        private val paintBtnPressedDisabled =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0x665E5E5E
            }
        private val paintBtnLatchedDisabled =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = 0x334A4A4A
            }
        private val paintBtnLabel =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = -0x55000001
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
        private var remainingActionOpen = false
        private var remainingActionSelectedIndex = -1
        private var remainingActionPointerId = -1
        private var remainingActionPressedIndex = -1
        private var remainingActionRowCount = 1
        private var remainingActionButtonRect = RectF()
        private val remainingActionItemRects = mutableListOf<RectF>()

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
        var adminTrayOpenedCallback: (() -> Unit)? = null
        var adminTrayClosedCallback: (() -> Unit)? = null

        // Provider for dynamic labels (auto-leveling state, cockpit mode)
        var adminTrayAutoLevelingProvider: (() -> Boolean)? = null
        var adminTrayCockpitModeProvider: (() -> Int)? = null
        var adminTrayToggleStateProvider: ((Int) -> Boolean)? = null
        var adminTrayEnabledStateProvider: ((Int) -> Boolean)? = null

        // Gamepad-only mode: no touchscreen, admin tray gets extra items + D-pad nav
        var gamepadOnlyMode = false

        // Providers for dynamic gamepad-only labels
        var adminTrayWarpLabelProvider: (() -> String)? = null
        var adminTrayAcceptLabelProvider: (() -> String)? = null

        // D-pad selection index (-1 = no selection, used in gamepad mode)
        private var adminTraySelectedIndex = -1

        init {
            rebuildStates()
        }

        /** Replace the current layout and recompute all control geometry. */
        fun setLayout(newLayout: TouchLayout) {
            layout = newLayout
            gyroConfigured = newLayout.gyro.enabled
            if (!gyroConfigured) gyroActiveInGame = false
            rebuildStates()
            if (width > 0 && height > 0) computeGeometry(width, height)
            invalidate()
        }

        fun updateGyroState(
            configured: Boolean,
            active: Boolean,
        ) {
            val nextActive = configured && active
            if (gyroConfigured == configured && gyroActiveInGame == nextActive) return
            gyroConfigured = configured
            gyroActiveInGame = nextActive
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

            // Compute radial menu geometry from layout
            for (rm in radialStates) {
                rm.triggerRadius = defaultBtnRadius * rm.control.sizeMult
                rm.triggerX = wf * rm.control.xPct / 100f
                rm.triggerY = hf * rm.control.yPct / 100f
                rm.radius = base * 0.18f * rm.control.ringSizeMult
            }

            // Compute slider geometry from layout
            for (sl in sliderStates) {
                sl.centerX = wf * sl.control.xPct / 100f
                sl.centerY = hf * sl.control.yPct / 100f
                sl.trackLen = base * 0.12f * sl.control.sizeMult
                sl.thumbR = base * 0.018f * sl.control.sizeMult
            }

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
                when (d.control.type) {
                    DiagnosticType.GYRO -> {
                        d.width = paintDiagText.measureText("Roll: -100%") + diagTextSize * 2
                        d.height = diagTextSize * d.control.sizeMult * 4.5f
                    }

                    DiagnosticType.MUSIC -> {
                        val r = base * 0.03f * d.control.sizeMult
                        d.musicBtnR = r
                        d.musicBtnY = d.centerY
                        // Left-align: centerX is the left edge of the control
                        d.musicPrevCX = d.centerX + r + diagTextSize * 0.5f
                        d.musicNextCX = d.musicPrevCX + r * 2 + base * 0.02f * d.control.sizeMult
                        d.musicLabelX = d.musicNextCX + r + base * 0.02f * d.control.sizeMult
                        d.width = (d.musicLabelX - d.centerX) +
                            paintDiagText.measureText("Track 00/00: xxxxxxxx") + diagTextSize
                        d.height = r * 3f
                    }

                    DiagnosticType.SETTINGS -> {
                        val r = base * 0.03f * d.control.sizeMult
                        d.width = r * 2.5f
                        d.height = r * 2.5f
                    }
                }
            }

            // Compute axis region geometry from layout
            for (ar in axisRegionStates) {
                val z = ar.control.zone
                ar.left = wf * z.leftPct / 100f
                ar.top = hf * z.topPct / 100f
                ar.right = wf * z.rightPct / 100f
                ar.bottom = hf * z.bottomPct / 100f
            }

            recomputeRemainingActionGeometry()
        }

        private fun currentRemainingTouchActions(
            weaponState: WeaponState? = weaponStateProvider?.invoke(),
        ): List<RemainingTouchAction> =
            remainingKeyTouchActions(
                layout = layout,
                gameVariant = gameVariant,
                isMultiplayerGame = isMultiplayerGameProvider?.invoke() == true,
                weaponState = weaponState,
            )

        private fun recomputeRemainingActionGeometry(weaponState: WeaponState? = weaponStateProvider?.invoke()) {
            remainingActionItemRects.clear()
            remainingActionButtonRect = RectF()
            remainingActionRowCount = 1

            val actions = currentRemainingTouchActions(weaponState)
            val wf = width.toFloat()
            val hf = height.toFloat()
            if (actions.isEmpty() || wf <= 0f || hf <= 0f) {
                return
            }

            val base = min(wf, hf)
            val margin = base * 0.03f
            val control = layout.moreActions
            val buttonH = hf * 0.05f * control.sizeMult
            val buttonW = maxOf(wf * 0.16f, base * 0.16f) * control.sizeMult
            val buttonLeft = (wf * control.xPct / 100f - buttonW / 2f).coerceIn(margin, wf - margin - buttonW)
            val buttonTop = (hf * control.yPct / 100f - buttonH / 2f).coerceIn(margin, hf - margin - buttonH)
            remainingActionButtonRect = RectF(buttonLeft, buttonTop, buttonLeft + buttonW, buttonTop + buttonH)

            val itemH = hf * 0.06f * control.sizeMult
            val gap = base * 0.012f
            val itemLabelPaint =
                Paint(paintBtnLabel).apply {
                    textSize = buttonH * 0.34f
                }
            val itemLabelWidth = actions.maxOfOrNull { itemLabelPaint.measureText(it.label) } ?: 0f
            val itemW = maxOf(buttonW * 1.35f, wf * 0.22f * control.sizeMult, itemLabelWidth + itemH * 0.7f)
            val maxRows = (((hf - margin * 2f) + gap) / (itemH + gap)).toInt().coerceAtLeast(1)
            val columnCount = ((actions.size + maxRows - 1) / maxRows).coerceAtLeast(1)
            val rowCount = ((actions.size + columnCount - 1) / columnCount).coerceAtLeast(1)
            remainingActionRowCount = rowCount
            val totalWidth = columnCount * itemW + (columnCount - 1).coerceAtLeast(0) * gap
            val totalHeight = rowCount * itemH + (rowCount - 1).coerceAtLeast(0) * gap
            val top =
                if (remainingActionButtonRect.top - gap - totalHeight >= margin) {
                    remainingActionButtonRect.top - gap - totalHeight
                } else if (remainingActionButtonRect.bottom + gap + totalHeight <= hf - margin) {
                    remainingActionButtonRect.bottom + gap
                } else {
                    val maxTop = hf - margin - totalHeight
                    if (maxTop >= margin) {
                        (remainingActionButtonRect.centerY() - totalHeight / 2f).coerceIn(margin, maxTop)
                    } else {
                        margin
                    }
                }
            val left =
                if (remainingActionButtonRect.left + totalWidth <= wf - margin) {
                    remainingActionButtonRect.left
                } else {
                    remainingActionButtonRect.right - totalWidth
                }.let { candidate ->
                    val maxLeft = wf - margin - totalWidth
                    if (maxLeft >= margin) candidate.coerceIn(margin, maxLeft) else margin
                }

            actions.indices.forEach { index ->
                val column = index / rowCount
                val row = index % rowCount
                val itemLeft = left + column * (itemW + gap)
                val itemTop = top + row * (itemH + gap)
                remainingActionItemRects.add(
                    RectF(
                        itemLeft,
                        itemTop,
                        itemLeft + itemW,
                        itemTop + itemH,
                    ),
                )
            }
        }

        private fun closeRemainingActions() {
            remainingActionOpen = false
            remainingActionSelectedIndex = -1
            remainingActionPointerId = -1
            remainingActionPressedIndex = -1
            invalidate()
        }

        private fun openRemainingActions(fromGamepad: Boolean = false) {
            val actions = currentRemainingTouchActions()
            if (actions.isEmpty()) {
                closeRemainingActions()
                return
            }
            closeAdminTray()
            remainingActionOpen = true
            remainingActionSelectedIndex = if (fromGamepad) 0 else -1
            remainingActionPointerId = -1
            remainingActionPressedIndex = -1
            invalidate()
        }

        private fun currentControllerMenuSurface(): ControllerMenuSurface =
            when {
                adminTrayOpen -> ControllerMenuSurface.ADMIN_TRAY
                remainingActionOpen -> ControllerMenuSurface.REMAINING_ACTIONS
                else -> ControllerMenuSurface.NONE
            }

        fun isControllerMenuOpen(): Boolean = currentControllerMenuSurface() != ControllerMenuSurface.NONE

        fun cycleControllerMenu() {
            when (
                nextControllerMenuSurface(
                    currentControllerMenuSurface(),
                    currentRemainingTouchActions().isNotEmpty(),
                )
            ) {
                ControllerMenuSurface.NONE -> closeControllerMenu()
                ControllerMenuSurface.REMAINING_ACTIONS -> openRemainingActions(fromGamepad = true)
                ControllerMenuSurface.ADMIN_TRAY -> openAdminTray(fromGamepad = true)
            }
        }

        fun closeControllerMenu() {
            closeRemainingActions()
            closeAdminTray()
        }

        fun handleControllerMenuKey(
            keyCode: Int,
            action: Int,
        ): Boolean =
            when (currentControllerMenuSurface()) {
                ControllerMenuSurface.ADMIN_TRAY -> {
                    handleAdminTrayGamepadKey(keyCode, action)
                }

                ControllerMenuSurface.REMAINING_ACTIONS -> {
                    val actions = currentRemainingTouchActions()
                    if (actions.isEmpty()) {
                        closeRemainingActions()
                        false
                    } else if (action != 0) {
                        keyCode == android.view.KeyEvent.KEYCODE_DPAD_UP ||
                            keyCode == android.view.KeyEvent.KEYCODE_DPAD_DOWN ||
                            keyCode == android.view.KeyEvent.KEYCODE_DPAD_LEFT ||
                            keyCode == android.view.KeyEvent.KEYCODE_DPAD_RIGHT ||
                            keyCode == android.view.KeyEvent.KEYCODE_BUTTON_A ||
                            keyCode == android.view.KeyEvent.KEYCODE_DPAD_CENTER ||
                            keyCode == android.view.KeyEvent.KEYCODE_BUTTON_B ||
                            keyCode == android.view.KeyEvent.KEYCODE_BACK
                    } else {
                        recomputeRemainingActionGeometry()
                        if (remainingActionSelectedIndex !in actions.indices) {
                            remainingActionSelectedIndex = 0
                        }

                        when (keyCode) {
                            android.view.KeyEvent.KEYCODE_BUTTON_B,
                            android.view.KeyEvent.KEYCODE_BACK,
                            -> {
                                closeRemainingActions()
                                true
                            }

                            android.view.KeyEvent.KEYCODE_DPAD_UP,
                            android.view.KeyEvent.KEYCODE_DPAD_DOWN,
                            android.view.KeyEvent.KEYCODE_DPAD_LEFT,
                            android.view.KeyEvent.KEYCODE_DPAD_RIGHT,
                            -> {
                                val nextIndex =
                                    moveRemainingActionSelection(
                                        currentIndex = remainingActionSelectedIndex,
                                        actionCount = actions.size,
                                        rowCount = remainingActionRowCount,
                                        keyCode = keyCode,
                                    )
                                if (nextIndex != remainingActionSelectedIndex) {
                                    remainingActionSelectedIndex = nextIndex
                                    performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                                }
                                invalidate()
                                true
                            }

                            android.view.KeyEvent.KEYCODE_BUTTON_A,
                            android.view.KeyEvent.KEYCODE_DPAD_CENTER,
                            -> {
                                val selectedIndex = remainingActionSelectedIndex.coerceIn(0, actions.lastIndex)
                                closeRemainingActions()
                                triggerRemainingAction(actions[selectedIndex].binding)
                                performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                                true
                            }

                            else -> {
                                false
                            }
                        }
                    }
                }

                ControllerMenuSurface.NONE -> {
                    false
                }
            }

        private fun triggerRemainingAction(binding: Int) {
            val sourceTag = "touch:remaining:$binding"
            pressLayoutButtonBinding(binding, sourceTag)
            mainHandler.postDelayed(
                { releaseLayoutButtonBinding(binding, true, sourceTag) },
                DOUBLE_TAP_RELEASE_DELAY_MS,
            )
        }

        private fun drawRemainingActions(
            canvas: Canvas,
            weaponState: WeaponState?,
        ) {
            if (adminTrayOpen) {
                closeRemainingActions()
                return
            }
            val actions = currentRemainingTouchActions(weaponState)
            if (actions.isEmpty()) {
                remainingActionOpen = false
                remainingActionPointerId = -1
                remainingActionPressedIndex = -1
                remainingActionButtonRect = RectF()
                remainingActionItemRects.clear()
                return
            }

            recomputeRemainingActionGeometry(weaponState)
            val eff = (layout.globalOpacity * layout.moreActions.opacity).coerceIn(0f, 1f)

            val buttonBg =
                if (!remainingActionOpen &&
                    remainingActionPressedIndex == -2
                ) {
                    paintBtnPressed
                } else {
                    paintBtnIdle
                }
            buttonBg.alpha =
                ((if (remainingActionOpen || remainingActionPressedIndex == -2) 0x88 else 0x44) * eff).toInt()
            val buttonCorner = remainingActionButtonRect.height() * 0.45f
            canvas.drawRoundRect(remainingActionButtonRect, buttonCorner, buttonCorner, buttonBg)
            paintRing.alpha = (0x66 * eff).toInt()
            canvas.drawRoundRect(remainingActionButtonRect, buttonCorner, buttonCorner, paintRing)

            val buttonText =
                Paint(paintBtnLabel).apply {
                    textSize = remainingActionButtonRect.height() * 0.38f
                    textAlign = Paint.Align.CENTER
                    alpha = (0xAA * eff).toInt()
                }
            canvas.drawText(
                "More",
                remainingActionButtonRect.centerX(),
                remainingActionButtonRect.centerY() + buttonText.textSize * 0.35f,
                buttonText,
            )

            if (!remainingActionOpen) {
                return
            }

            val itemText =
                Paint(paintBtnLabel).apply {
                    textSize = remainingActionButtonRect.height() * 0.34f
                    textAlign = Paint.Align.LEFT
                    alpha = (0xCC * eff).toInt()
                }
            val selectionPaint =
                Paint(Paint.ANTI_ALIAS_FLAG).apply {
                    style = Paint.Style.STROKE
                    color = CONTROLLER_MENU_FOCUS_COLOR
                    strokeWidth = maxOf(3f, remainingActionButtonRect.height() * 0.05f)
                }
            actions.forEachIndexed { index, action ->
                val rect = remainingActionItemRects[index]
                val isPressed = remainingActionPressedIndex == index
                val isSelected = remainingActionSelectedIndex == index
                val bg = if (isPressed || isSelected) paintBtnPressed else paintBtnIdle
                bg.alpha =
                    (
                        (
                            if (isPressed) {
                                0x99
                            } else if (isSelected) {
                                0x77
                            } else {
                                0x55
                            }
                        ) * eff
                    ).toInt()
                val corner = rect.height() * 0.3f
                canvas.drawRoundRect(rect, corner, corner, bg)
                paintRing.alpha = (0x66 * eff).toInt()
                canvas.drawRoundRect(rect, corner, corner, paintRing)
                if (isSelected) {
                    canvas.drawRoundRect(rect, corner, corner, selectionPaint)
                }
                canvas.drawText(
                    action.label,
                    rect.left + rect.height() * 0.35f,
                    rect.centerY() + itemText.textSize * 0.35f,
                    itemText,
                )
            }
        }

        private fun handleRemainingActionsTouch(event: MotionEvent): Boolean {
            val actions = currentRemainingTouchActions()
            if (actions.isEmpty()) {
                closeRemainingActions()
                return false
            }

            recomputeRemainingActionGeometry()
            val idx = event.actionIndex
            val px = event.getX(idx)
            val py = event.getY(idx)
            val pid = event.getPointerId(idx)

            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    if (remainingActionOpen) {
                        val pressedIndex = remainingActionItemRects.indexOfFirst { it.contains(px, py) }
                        if (pressedIndex >= 0) {
                            remainingActionSelectedIndex = pressedIndex
                            remainingActionPointerId = pid
                            remainingActionPressedIndex = pressedIndex
                            invalidate()
                        } else {
                            closeRemainingActions()
                        }
                        return true
                    }
                    if (remainingActionButtonRect.contains(px, py)) {
                        remainingActionPointerId = pid
                        remainingActionPressedIndex = -2
                        invalidate()
                        return true
                    }
                }

                MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                    if (pid != remainingActionPointerId) {
                        return remainingActionOpen
                    }
                    if (!remainingActionOpen &&
                        remainingActionPressedIndex == -2 &&
                        remainingActionButtonRect.contains(px, py)
                    ) {
                        openRemainingActions()
                        return true
                    }
                    if (remainingActionOpen && remainingActionPressedIndex in actions.indices) {
                        val pressedIndex = remainingActionPressedIndex
                        val fired = remainingActionItemRects[pressedIndex].contains(px, py)
                        closeRemainingActions()
                        if (fired) {
                            triggerRemainingAction(actions[pressedIndex].binding)
                            performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                        }
                        return true
                    }
                    remainingActionPointerId = -1
                    remainingActionPressedIndex = -1
                    invalidate()
                    return false
                }

                MotionEvent.ACTION_CANCEL -> {
                    if (pid == remainingActionPointerId || remainingActionPointerId < 0) {
                        if (remainingActionOpen) {
                            closeRemainingActions()
                        } else {
                            remainingActionPointerId = -1
                            remainingActionPressedIndex = -1
                            invalidate()
                        }
                        return true
                    }
                }
            }

            return remainingActionOpen
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
            val ws = weaponStateProvider?.invoke()

            // ── Layout sticks ───────────────────────────────────
            for (s in stickStates) drawStick(canvas, s, gAlpha)

            // ── Layout buttons ──────────────────────────────────
            for (b in buttonStates) {
                if (gameVariant == "d1" && b.control.binding in TouchBindings.D2_ONLY_BUTTONS) continue
                drawButton(canvas, b, gAlpha, ws)
            }

            // ── Layout sliders ──────────────────────────────────
            for (sl in sliderStates) drawSlider(canvas, sl, gAlpha)

            // ── Axis regions ────────────────────────────────────
            for (ar in axisRegionStates) drawAxisRegion(canvas, ar, gAlpha)

            // ── Radial menus (triggers, then open wheels on top) ──
            // Poll weapon state to update quiescent labels
            for (rm in radialStates) {
                if (rm.control.id == "Guide" && gameVariant == "d1") {
                    continue
                }
                if (rm.control.id == "Guide") {
                    val released = isBuddyReleasedProvider?.invoke() != false
                    if (!released) {
                        rm.quiescentLabel = "Locked"
                    } else if (isEscortOwnerProvider?.invoke() == false) {
                        val owner = escortOwnerCallsignProvider?.invoke().orEmpty()
                        rm.quiescentLabel = if (owner.isNotEmpty()) owner else "Guide"
                    }
                }
                if (!rm.isOpen && ws != null && (rm.control.id == "PriWpn" || rm.control.id == "SecWpn")) {
                    val isPrimary = rm.control.id == "PriWpn"
                    val curIdx = if (isPrimary) ws.currentPrimary else ws.currentSecondary
                    rm.quiescentLabel = rm.control.segments
                        .firstOrNull { it.weaponIndex == curIdx || it.weaponIndex == curIdx - 5 }
                        ?.label ?: rm.control.id.take(4)
                }
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

            // ── Diagnostic overlays ─────────────────────────────
            val adminTrayPanelRect = if (adminTrayOpen) computeAdminTrayPanelRect() else null
            for (d in diagnosticStates) {
                if (d.control.type == DiagnosticType.SETTINGS &&
                    adminTrayPanelRect != null &&
                    settingsDiagnosticOccludedByTray(d, adminTrayPanelRect)
                ) {
                    continue
                }
                drawDiagnostic(canvas, d, gAlpha)
            }

            drawRemainingActions(canvas, ws)

            // ── Cheats overlay (drawn last, on top of everything) ──
            if (cheatsOverlayOpen) drawCheatsOverlay(canvas)

            // ── Admin tray tab (visible) or panel (when open) ───
            // Hide default tab when a settings diagnostic is configured (it replaces the tab)
            // In gamepad-only mode, no tab is drawn (Start button opens the tray)
            val hasSettingsDiag = diagnosticStates.any { it.control.type == DiagnosticType.SETTINGS }
            if (adminTrayOpen) {
                drawAdminTrayPanel(canvas)
            } else if (!gamepadOnlyMode && !cheatsOverlayOpen && !hasSettingsDiag) {
                drawAdminTrayTab(canvas)
            }
        }

        private fun drawStick(
            canvas: Canvas,
            s: StickState,
            gAlpha: Float,
        ) {
            val eff = (gAlpha * s.control.opacity).coerceIn(0f, 1f)

            val xLabel = TouchBindings.AXIS_LABELS[s.control.axisX] ?: "?"
            val yLabel = TouchBindings.AXIS_LABELS[s.control.axisY] ?: "?"

            if (s.control.mouseMode) {
                // Mouse mode: draw only a transparent bounding box for the touch region
                paintRing.alpha = (0x44 * eff).toInt()
                canvas.drawRect(s.fzLeft, s.fzTop, s.fzRight, s.fzBottom, paintRing)
                val mcx = (s.fzLeft + s.fzRight) / 2f
                val mcy = (s.fzTop + s.fzBottom) / 2f
                paintBtnLabel.alpha = (0x66 * eff).toInt()
                paintBtnLabel.textSize = (s.fzRight - s.fzLeft).coerceAtMost(s.fzBottom - s.fzTop) * 0.06f
                canvas.drawText(xLabel, mcx, mcy - paintBtnLabel.textSize * 0.3f, paintBtnLabel)
                canvas.drawText(yLabel, mcx, mcy + paintBtnLabel.textSize * 1.0f, paintBtnLabel)
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

            // Axis name labels
            paintBtnLabel.alpha = (0x66 * eff).toInt()
            paintBtnLabel.textSize = s.radius * 0.22f
            canvas.drawText(xLabel, cx, cy - paintBtnLabel.textSize * 0.3f, paintBtnLabel)
            canvas.drawText(yLabel, cx, cy + paintBtnLabel.textSize * 1.0f, paintBtnLabel)
        }

        private fun drawButton(
            canvas: Canvas,
            b: ButtonState,
            gAlpha: Float,
            weaponState: WeaponState?,
        ) {
            val eff = (gAlpha * b.control.opacity).coerceIn(0f, 1f)
            val pressed = b.pointerId >= 0 || b.toggled
            val latched =
                (b.toggled && b.control.toggle && b.pointerId < 0) ||
                    buttonHasActiveIndicatorState(
                        button = b.control,
                        gameVariant = gameVariant,
                        weaponState = weaponState,
                        gyroConfigured = gyroConfigured,
                        gyroActiveInGame = gyroActiveInGame,
                    )
            val disabledGyroButton = buttonUsesGyroToggleIndicator(b.control) && !gyroConfigured
            val fill =
                if (latched) {
                    if (disabledGyroButton) paintBtnLatchedDisabled else paintBtnLatched
                } else if (pressed) {
                    if (disabledGyroButton) paintBtnPressedDisabled else paintBtnPressed
                } else {
                    if (disabledGyroButton) paintBtnIdleDisabled else paintBtnIdle
                }
            val fillAlpha =
                if (latched) {
                    0x44
                } else if (pressed) {
                    0x66
                } else {
                    0x33
                }
            fill.alpha = (fillAlpha * eff).toInt()
            canvas.drawCircle(b.centerX, b.centerY, b.radius, fill)
            paintRing.alpha = (0x66 * eff).toInt()
            canvas.drawCircle(b.centerX, b.centerY, b.radius, paintRing)
            val label = buttonDisplayLabel(b.control, gameVariant, weaponState)
            if (label.isNotEmpty()) {
                drawButtonLabel(canvas, b.centerX, b.centerY, b.radius, eff, label)
            }
        }

        private fun drawButtonLabel(
            canvas: Canvas,
            centerX: Float,
            centerY: Float,
            radius: Float,
            eff: Float,
            label: String,
        ) {
            val lines = label.lines().filter { it.isNotEmpty() }
            if (lines.isEmpty()) return

            paintBtnLabel.alpha = (0xAA * eff).toInt()
            paintBtnLabel.textSize = if (lines.size > 1) radius * 0.38f else radius * 0.7f

            val minTextSize = radius * 0.18f
            val maxWidth = radius * 1.6f
            while (
                lines.maxOf { paintBtnLabel.measureText(it) } > maxWidth &&
                paintBtnLabel.textSize > minTextSize
            ) {
                paintBtnLabel.textSize *= 0.92f
            }

            val lineHeight = if (lines.size > 1) paintBtnLabel.fontSpacing * 0.9f else paintBtnLabel.fontSpacing
            val firstBaseline =
                centerY -
                    lineHeight * (lines.size - 1) / 2f -
                    (paintBtnLabel.descent() + paintBtnLabel.ascent()) / 2f

            lines.forEachIndexed { index, line ->
                canvas.drawText(line, centerX, firstBaseline + lineHeight * index, paintBtnLabel)
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
            val slLabel = TouchBindings.AXIS_LABELS[sl.control.axis] ?: sl.control.id
            paintBtnLabel.alpha = (0x88 * eff).toInt()
            paintBtnLabel.textSize = sl.thumbR * 1.2f
            if (vertical) {
                canvas.drawText(
                    slLabel,
                    sl.centerX,
                    y1 + paintBtnLabel.textSize * 1.5f,
                    paintBtnLabel,
                )
            } else {
                canvas.drawText(
                    slLabel,
                    sl.centerX,
                    sl.centerY + sl.thumbR + paintBtnLabel.textSize * 1.3f,
                    paintBtnLabel,
                )
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
            val arLabel = TouchBindings.AXIS_LABELS[ar.control.axis] ?: ar.control.id
            paintBtnLabel.alpha = (0x66 * eff).toInt()
            val base = min(width, height).toFloat()
            paintBtnLabel.textSize = base * 0.02f
            val cx = (ar.left + ar.right) / 2f
            val cy = (ar.top + ar.bottom) / 2f
            canvas.drawText(arLabel, cx, cy + paintBtnLabel.textSize * 0.35f, paintBtnLabel)
        }

        private fun drawDiagnostic(
            canvas: Canvas,
            d: DiagnosticState,
            gAlpha: Float,
        ) {
            when (d.control.type) {
                DiagnosticType.GYRO -> drawDiagnosticGyro(canvas, d, gAlpha)
                DiagnosticType.MUSIC -> drawDiagnosticMusic(canvas, d, gAlpha)
                DiagnosticType.SETTINGS -> drawDiagnosticSettings(canvas, d, gAlpha)
            }
        }

        private fun drawDiagnosticGyro(
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

        private fun drawDiagnosticMusic(
            canvas: Canvas,
            d: DiagnosticState,
            gAlpha: Float,
        ) {
            if (trackLabel.isEmpty()) return
            val eff = (gAlpha * d.control.opacity).coerceIn(0f, 1f)
            val r = d.musicBtnR
            val y = d.musicBtnY

            // Prev button
            val fillPrev = if (d.musicPrevPid >= 0) paintBtnPressed else paintBtnIdle
            fillPrev.alpha = ((if (d.musicPrevPid >= 0) 0x66 else 0x33) * eff).toInt()
            canvas.drawCircle(d.musicPrevCX, y, r, fillPrev)
            paintRing.alpha = (0x66 * eff).toInt()
            canvas.drawCircle(d.musicPrevCX, y, r, paintRing)

            // Next button
            val fillNext = if (d.musicNextPid >= 0) paintBtnPressed else paintBtnIdle
            fillNext.alpha = ((if (d.musicNextPid >= 0) 0x66 else 0x33) * eff).toInt()
            canvas.drawCircle(d.musicNextCX, y, r, fillNext)
            canvas.drawCircle(d.musicNextCX, y, r, paintRing)

            // Arrow glyphs
            val savedSize = paintBtnLabel.textSize
            paintBtnLabel.textSize = r * 0.9f
            paintBtnLabel.alpha = (0xAA * eff).toInt()
            canvas.drawText("\u25C0", d.musicPrevCX, y + paintBtnLabel.textSize * 0.35f, paintBtnLabel)
            canvas.drawText("\u25B6", d.musicNextCX, y + paintBtnLabel.textSize * 0.35f, paintBtnLabel)

            // Track label
            paintBtnLabel.textSize = r * 0.7f
            val labelPaint =
                Paint(paintBtnLabel).apply {
                    textAlign = Paint.Align.LEFT
                    alpha = (0xCC * eff).toInt()
                }
            canvas.drawText(trackLabel, d.musicLabelX, y + labelPaint.textSize * 0.35f, labelPaint)
            paintBtnLabel.textSize = savedSize
        }

        private fun drawDiagnosticSettings(
            canvas: Canvas,
            d: DiagnosticState,
            gAlpha: Float,
        ) {
            val eff = (gAlpha * d.control.opacity).coerceIn(0f, 1f)
            val r = min(d.width, d.height) / 2f
            val pressed = d.menuPid >= 0
            val fill = if (pressed) paintBtnPressed else paintBtnIdle
            fill.alpha = ((if (pressed) 0x66 else 0x33) * eff).toInt()
            canvas.drawCircle(d.centerX, d.centerY, r, fill)
            paintRing.alpha = (0x66 * eff).toInt()
            canvas.drawCircle(d.centerX, d.centerY, r, paintRing)

            // 3x3 dot grid icon (matches settings admin tray)
            paintDiagText.alpha = (0xCC * eff).toInt()
            val dotR = r * 0.08f
            val gap = r * 0.35f
            for (row in -1..1) {
                for (col in -1..1) {
                    canvas.drawCircle(
                        d.centerX + col * gap,
                        d.centerY + row * gap,
                        dotR,
                        paintDiagText,
                    )
                }
            }
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
                val label = state.quiescentLabel.ifEmpty { state.control.id.take(4) }
                paintBtnLabel.textSize = state.triggerRadius * (if (label.length > 5) 0.4f else 0.6f)
                canvas.drawText(
                    label,
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
                if (active) {
                    paintBtnLabel.textSize = r * 0.22f
                    paintBtnLabel.typeface = Typeface.DEFAULT_BOLD
                } else {
                    paintBtnLabel.textSize = r * 0.11f
                }
                canvas.drawText(segs[i].label, lx, ly + paintBtnLabel.textSize * 0.35f, paintBtnLabel)
                if (active) paintBtnLabel.typeface = Typeface.DEFAULT
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
                if (active) {
                    paintBtnLabel.textSize = r * 0.22f
                    paintBtnLabel.typeface = Typeface.DEFAULT_BOLD
                } else {
                    paintBtnLabel.textSize = r * 0.11f
                }
                canvas.drawText(segs[i].label, lx, ly + paintBtnLabel.textSize * 0.35f, paintBtnLabel)
                if (active) paintBtnLabel.typeface = Typeface.DEFAULT

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

            // MAP button (top-right, enlarged to match automap overlay buttons)
            val mapR = maxOf(mapBtnRadius, automapBtnSize)
            val fillMap = if (mapBtnPointerId >= 0) paintBtnPressed else paintBtnIdle
            canvas.drawCircle(mapBtnCenterX, mapBtnCenterY, mapR, fillMap)
            canvas.drawCircle(mapBtnCenterX, mapBtnCenterY, mapR, paintRing)
            val savedSize = paintBtnLabel.textSize
            paintBtnLabel.textSize = mapR * 0.65f
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

            // When admin tray panel is open, a visible settings button may close it;
            // otherwise the tray consumes all touches.
            if (adminTrayOpen) {
                if (handleSettingsDiagnosticWhileTrayOpen(event)) return true
                return handleAdminTrayTouch(event)
            }

            if (handleRemainingActionsTouch(event)) return true

            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    val idx = event.actionIndex
                    val px = event.getX(idx)
                    val py = event.getY(idx)
                    val pid = event.getPointerId(idx)
                    var handled = false

                    // Settings button takes top priority over all controls
                    if (!handled) {
                        val settingsDiag = findSettingsDiagnosticAt(px, py)
                        if (settingsDiag != null && settingsDiag.menuPid < 0) {
                            settingsDiag.menuPid = pid
                            invalidate()
                            handled = true
                        }
                    }

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
                            val startExtensionButton = findDragZoneStartExtensionButton(s, px, py)
                            if (pointInStickDragZone(s, px, py) || startExtensionButton != null) {
                                // Double-tap / latch detection
                                val now = android.os.SystemClock.uptimeMillis()
                                val isDoubleTap = s.control.doubleTapBinding >= 0 && now - s.lastTapTime < 300L
                                if (isDoubleTap) {
                                    s.tapCount++
                                    handleDoubleTap(s)
                                } else {
                                    s.tapCount = 1
                                    handleSingleTapRelease(s)
                                }
                                s.lastTapTime = now
                                beginMouseDrag(s, pid, px, py)
                                if (startExtensionButton != null) {
                                    pressLayoutButton(startExtensionButton, pid)
                                } else {
                                    tryPressDragZoneButton(pid, px, py)
                                }
                                handled = true
                                break
                            }
                        } else if (s.control.floating) {
                            val startExtensionButton = findDragZoneStartExtensionButton(s, px, py)
                            if (pointInStickDragZone(s, px, py) || startExtensionButton != null) {
                                // Double-tap / latch detection for floating mode
                                val now = android.os.SystemClock.uptimeMillis()
                                val isDoubleTap = s.control.doubleTapBinding >= 0 && now - s.lastTapTime < 300L
                                if (isDoubleTap) {
                                    s.tapCount++
                                    handleDoubleTap(s)
                                } else {
                                    s.tapCount = 1
                                    handleSingleTapRelease(s)
                                }
                                s.lastTapTime = now
                                s.pointerId = pid
                                s.floatingCX = px
                                s.floatingCY = py
                                s.floatingActive = true
                                s.pos.set(0f, 0f)
                                if (startExtensionButton != null) {
                                    pressLayoutButton(startExtensionButton, pid)
                                } else {
                                    tryPressDragZoneButton(pid, px, py)
                                }
                                invalidate()
                                handled = true
                                break
                            }
                        } else if (hypot(px - s.centerX, py - s.centerY) <= s.radius) {
                            // Double-tap / latch detection for fixed sticks
                            val now = android.os.SystemClock.uptimeMillis()
                            val isDoubleTap = s.control.doubleTapBinding >= 0 && now - s.lastTapTime < 300L
                            if (isDoubleTap) {
                                s.tapCount++
                                handleDoubleTap(s)
                            } else {
                                s.tapCount = 1
                                handleSingleTapRelease(s)
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
                                pressLayoutButton(b, pid)
                                handled = true
                                break
                            }
                        }
                    }

                    // Try radial menu triggers
                    if (!handled) {
                        for (rm in radialStates) {
                            if (rm.pointerId >= 0) continue
                            // D1 has no Guide-Bot; block if buddy not released
                            if (rm.control.id == "Guide" &&
                                (gameVariant == "d1" || isBuddyReleasedProvider?.invoke() == false)
                            ) {
                                continue
                            }
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

                    // Try music controls (layout-driven via MUSIC diagnostics)
                    if (!handled && trackLabel.isNotEmpty()) {
                        for (d in diagnosticStates) {
                            if (d.control.type != DiagnosticType.MUSIC) continue
                            val r = d.musicBtnR
                            val y = d.musicBtnY
                            when {
                                d.musicPrevPid < 0 &&
                                    hypot(px - d.musicPrevCX, py - y) <= r * 1.3f -> {
                                    d.musicPrevPid = pid
                                    invalidate()
                                    handled = true
                                }

                                d.musicNextPid < 0 &&
                                    hypot(px - d.musicNextCX, py - y) <= r * 1.3f -> {
                                    d.musicNextPid = pid
                                    invalidate()
                                    handled = true
                                }

                                d.musicLabelPid < 0 &&
                                    px >= d.musicLabelX &&
                                    py >= y - r * 1.5f &&
                                    py <= y + r * 1.5f -> {
                                    d.musicLabelPid = pid
                                    handled = true
                                }
                            }
                            if (handled) break
                        }
                    }

                    // Try admin tray tab (hidden when a settings diagnostic replaces it)
                    val hasSettingsDiag = diagnosticStates.any { it.control.type == DiagnosticType.SETTINGS }
                    if (!handled && !hasSettingsDiag && adminTrayTabRect.contains(px, py)) {
                        openAdminTray()
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
                                        beginMouseDrag(src, ar.pointerId, ax, ay, ar.savedStickCX, ar.savedStickCY)
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
                                            s.control.mouseMode || s.control.floating -> {
                                                ax in s.fzLeft..s.fzRight && ay in s.fzTop..s.fzBottom
                                            }

                                            else -> {
                                                hypot(ax - s.centerX, ay - s.centerY) <= s.radius
                                            }
                                        }
                                    if (inZone) {
                                        if (s.control.mouseMode) {
                                            beginMouseDrag(s, ar.pointerId, ax, ay)
                                        } else if (s.control.floating) {
                                            s.pointerId = ar.pointerId
                                            s.floatingCX = ax
                                            s.floatingCY = ay
                                            s.floatingActive = true
                                            s.pos.set(0f, 0f)
                                        } else {
                                            s.pointerId = ar.pointerId
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
                    releaseAllMusicDiagnostics(fired)
                    releaseAllMenuDiagnostics(fired)
                }

                MotionEvent.ACTION_POINTER_UP -> {
                    val pid = event.getPointerId(event.actionIndex)
                    if (passthroughPointers.remove(pid)) {
                        tapPassthroughCallback?.invoke()
                    } else {
                        for (s in stickStates) {
                            if (s.pointerId == pid) {
                                resetStick(s)
                            }
                        }
                        for (b in buttonStates) {
                            if (b.pointerId == pid) {
                                releaseLayoutButton(b, true)
                            }
                        }
                        for (rm in radialStates) {
                            if (rm.pointerId == pid) {
                                releaseRadialMenu(rm, true)
                            }
                        }
                        for (sl in sliderStates) {
                            if (sl.pointerId == pid) {
                                releaseSlider(sl)
                            }
                        }
                        for (ar in axisRegionStates) {
                            if (ar.pointerId == pid) {
                                releaseAxisRegion(ar)
                            }
                        }
                        for (d in diagnosticStates) {
                            if (d.control.type != DiagnosticType.MUSIC) continue
                            when (pid) {
                                d.musicPrevPid -> releaseMusicPrev(d, true)
                                d.musicNextPid -> releaseMusicNext(d, true)
                                d.musicLabelPid -> releaseMusicLabel(d, true)
                            }
                        }
                        for (d in diagnosticStates) {
                            if (d.control.type != DiagnosticType.SETTINGS) continue
                            if (d.menuPid == pid) {
                                releaseMenuDiag(d, true)
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

                    // Check MAP button (top-right, enlarged to match automap buttons)
                    if (mapBtnPointerId < 0) {
                        val mapR = maxOf(mapBtnRadius, automapBtnSize)
                        val dx = px - mapBtnCenterX
                        val dy = py - mapBtnCenterY
                        if (dx * dx + dy * dy <= mapR * mapR * 4) {
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
            val stepDistance = hypot(dx, dy)
            val now = android.os.SystemClock.uptimeMillis()
            val history =
                updateMouseAccelerationHistory(
                    s.mouseRecentDistancePx,
                    s.mouseRecentGracePx,
                    stepDistance,
                    now - s.mouseLastSampleTimeMs,
                )
            s.mouseRecentDistancePx = history.recentDistancePx
            s.mouseRecentGracePx = history.recentGracePx
            s.mouseLastSampleTimeMs = now
            s.mouseLastX = px
            s.mouseLastY = py
            // Convert pixel delta to axis-space and accumulate
            val baseScale = MOUSE_SENSITIVITY_MULTIPLIER * MOUSE_BASE_MULTIPLIER / MOUSE_REFERENCE_DISTANCE
            val scaleX = s.control.sensitivityX * baseScale
            val scaleY = s.control.sensitivityY * baseScale
            val multiplier =
                mouseAccelerationMultiplier(
                    s.control.mouseExponential,
                    s.control.mouseExponentialMax,
                    s.mouseRecentDistancePx,
                    s.mouseRecentGracePx,
                    hypot(px - s.mouseOriginX, py - s.mouseOriginY),
                    height.toFloat(),
                )
            s.mousePendingX += dx * scaleX * multiplier
            s.mousePendingY += dy * scaleY * multiplier
            logMouseDiag(
                "drag axis=(${s.control.axisX},${s.control.axisY}) d=(${"%.3f".format(
                    dx,
                )},${"%.3f".format(dy)}) step=${"%.3f".format(stepDistance)} " +
                    "hist=(${"%.3f".format(s.mouseRecentDistancePx)},${"%.3f".format(s.mouseRecentGracePx)}) " +
                    "scale=(${"%.4f".format(scaleX)},${"%.4f".format(scaleY)}) mult=${"%.3f".format(multiplier)} " +
                    "pending=(${"%.4f".format(
                        s.mousePendingX,
                    )},${"%.4f".format(s.mousePendingY)}) mouseExp=${s.control.mouseExponential} " +
                    "curve=${s.control.responseCurve} deadzone=${s.control.deadzone}",
            )
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
                val tag = "touch:sbtn${stickStates.indexOf(s)}"
                dispatchStickButton(s, rawX < 0f, s.xNegPressed, s.control.negXBinding, tag) { s.xNegPressed = it }
                dispatchStickButton(s, rawX > 0f, s.xPosPressed, s.control.posXBinding, tag) { s.xPosPressed = it }
                dispatchStickButton(s, rawY < 0f, s.yNegPressed, s.control.negYBinding, tag) { s.yNegPressed = it }
                dispatchStickButton(s, rawY > 0f, s.yPosPressed, s.control.posYBinding, tag) { s.yPosPressed = it }
            } else {
                axisCallback?.invoke(s.control.axisX, rawX)
                axisCallback?.invoke(s.control.axisY, rawY)
            }

            // Notify gyro manager that a stick sharing its axes is active
            updateGyroStickActive()
        }

        /** Fire a double-tap binding with a delayed release so the press survives
         *  at least one game frame (fixes fire-primary which uses level-triggered state). */
        private fun fireDoubleTapPulse(
            binding: Int,
            sourceTag: String = "touch:dtap",
        ) {
            if (TouchBindings.isMetaAction(binding)) {
                metaActionCallback?.invoke(binding, true)
                mainHandler.postDelayed({ metaActionCallback?.invoke(binding, false) }, DOUBLE_TAP_RELEASE_DELAY_MS)
            } else {
                dispatchTouchButton(binding, true, sourceTag)
                mainHandler.postDelayed({ dispatchTouchButton(binding, false, sourceTag) }, DOUBLE_TAP_RELEASE_DELAY_MS)
            }
        }

        /** Set or release a latched double-tap binding. */
        private fun setDoubleTapLatch(
            binding: Int,
            pressed: Boolean,
            sourceTag: String = "touch:dtap",
        ) {
            if (TouchBindings.isMetaAction(binding)) {
                metaActionCallback?.invoke(binding, pressed)
            } else {
                dispatchTouchButton(binding, pressed, sourceTag)
            }
        }

        /** Process a double-tap event for a stick, respecting the configured mode. */
        private fun handleDoubleTap(s: StickState) {
            val binding = s.control.doubleTapBinding
            if (binding < 0) return
            val tag = "touch:dtap${stickStates.indexOf(s)}"
            when (s.control.doubleTapMode) {
                DoubleTapMode.REPEAT_FIRE -> {
                    fireDoubleTapPulse(binding, tag)
                }

                DoubleTapMode.SINGLE_FIRE -> {
                    // Only fire on even tap counts (every second tap in the double-tap window)
                    if (s.tapCount % 2 == 0) fireDoubleTapPulse(binding, tag)
                }

                DoubleTapMode.LATCH_DOUBLE -> {
                    s.dtLatched = !s.dtLatched
                    setDoubleTapLatch(binding, s.dtLatched, tag)
                    invalidate()
                }

                DoubleTapMode.LATCH_SINGLE -> {
                    if (!s.dtLatched) {
                        s.dtLatched = true
                        setDoubleTapLatch(binding, true, tag)
                        invalidate()
                    } else {
                        // Double-tap while latched also releases
                        s.dtLatched = false
                        setDoubleTapLatch(binding, false, tag)
                        invalidate()
                    }
                }

                DoubleTapMode.HOLD_FIRE -> {
                    // Press on double-tap down, release handled in resetStick
                    s.dtLatched = true
                    setDoubleTapLatch(binding, true, tag)
                    invalidate()
                }
            }
        }

        /** Handle a single tap on a stick with LATCH_SINGLE mode -- releases latch. */
        private fun handleSingleTapRelease(s: StickState) {
            if (s.control.doubleTapMode != DoubleTapMode.LATCH_SINGLE) return
            if (!s.dtLatched) return
            val binding = s.control.doubleTapBinding
            if (binding < 0) return
            val tag = "touch:dtap${stickStates.indexOf(s)}"
            s.dtLatched = false
            setDoubleTapLatch(binding, false, tag)
            invalidate()
        }

        private fun dispatchStickButton(
            s: StickState,
            nowPressed: Boolean,
            wasPressed: Boolean,
            binding: Int,
            sourceTag: String,
            updateState: (Boolean) -> Unit,
        ) {
            if (nowPressed == wasPressed) return
            updateState(nowPressed)
            if (TouchBindings.isMetaAction(binding)) {
                metaActionCallback?.invoke(binding, nowPressed)
            } else {
                dispatchTouchButton(binding, nowPressed, sourceTag)
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
                val tag = "touch:sbtn${stickStates.indexOf(s)}"
                dispatchStickButton(s, false, s.xNegPressed, s.control.negXBinding, tag) { s.xNegPressed = it }
                dispatchStickButton(s, false, s.xPosPressed, s.control.posXBinding, tag) { s.xPosPressed = it }
                dispatchStickButton(s, false, s.yNegPressed, s.control.negYBinding, tag) { s.yNegPressed = it }
                dispatchStickButton(s, false, s.yPosPressed, s.control.posYBinding, tag) { s.yPosPressed = it }
            }
            // Release HOLD_FIRE double-tap on finger lift
            if (s.dtLatched && s.control.doubleTapMode == DoubleTapMode.HOLD_FIRE) {
                val binding = s.control.doubleTapBinding
                if (binding >= 0) {
                    val tag = "touch:dtap${stickStates.indexOf(s)}"
                    s.dtLatched = false
                    setDoubleTapLatch(binding, false, tag)
                }
            }
            // Clear mouse-mode pending drag
            if (s.control.mouseMode) {
                s.mousePendingX = 0f
                s.mousePendingY = 0f
                s.mouseLastSampleTimeMs = 0L
                s.mouseRecentDistancePx = 0f
                s.mouseRecentGracePx = 0f
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
                b.longPressRunnable?.let { mainHandler.removeCallbacks(it) }
                b.longPressRunnable = null
                b.pointerId = -1
                if (!b.control.toggle) {
                    releaseLayoutButtonBinding(b.control.binding, fired, buttonSourceTag(b))
                }
                if (b.longPressTriggered) {
                    releaseLayoutButtonBinding(b.control.longPressBinding, fired, buttonLongPressTag(b))
                }
                b.longPressTriggered = false
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
                    seg != null -> {
                        seg.binding
                    }

                    rm.activeSegment == RADIAL_CENTER && rm.control.centerBinding >= 0 -> {
                        rm.control.centerBinding
                    }

                    else -> {
                        -1
                    }
                }
            val isAction = seg?.bindingType == "action"
            if (binding >= 0) {
                if (TouchBindings.isMetaAction(binding)) {
                    metaActionCallback?.invoke(binding, true)
                    metaActionCallback?.invoke(binding, false)
                } else if (isAction) {
                    dispatchTouchButton(binding, true, "touch:radial")
                    dispatchTouchButton(binding, false, "touch:radial")
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
                in 7..16 -> '0'.code + keycode - 7

                // KEYCODE_0(7)..KEYCODE_9(16) → '0'..'9'
                else -> 0
            }

        private fun releaseAllButtons() {
            releaseAllLayoutButtons(false)
            releaseAllRadialMenus(false)
            releaseAllSliders()
            releaseAllAxisRegions()
            releaseAllMusicDiagnostics(false)
            releaseAllMenuDiagnostics(false)
        }

        private fun releaseMusicPrev(
            d: DiagnosticState,
            fired: Boolean,
        ) {
            if (d.musicPrevPid >= 0) {
                d.musicPrevPid = -1
                invalidate()
                if (fired) prevTrackCallback?.invoke()
            }
        }

        private fun releaseMusicNext(
            d: DiagnosticState,
            fired: Boolean,
        ) {
            if (d.musicNextPid >= 0) {
                d.musicNextPid = -1
                invalidate()
                if (fired) nextTrackCallback?.invoke()
            }
        }

        private fun releaseMusicLabel(
            d: DiagnosticState,
            fired: Boolean,
        ) {
            if (d.musicLabelPid >= 0) {
                d.musicLabelPid = -1
                if (fired) musicPanelCallback?.invoke()
            }
        }

        private fun releaseAllMusicDiagnostics(fired: Boolean) {
            for (d in diagnosticStates) {
                if (d.control.type != DiagnosticType.MUSIC) continue
                releaseMusicPrev(d, fired)
                releaseMusicNext(d, fired)
                releaseMusicLabel(d, fired)
            }
        }

        private fun releaseMenuDiag(
            d: DiagnosticState,
            fired: Boolean,
        ) {
            if (d.menuPid >= 0) {
                d.menuPid = -1
                invalidate()
                if (fired) {
                    toggleAdminTray()
                }
            }
        }

        private fun releaseAllMenuDiagnostics(fired: Boolean) {
            for (d in diagnosticStates) {
                if (d.control.type != DiagnosticType.SETTINGS) continue
                releaseMenuDiag(d, fired)
            }
        }

        private fun computeAdminTrayPanelRect(): RectF {
            val w = width.toFloat()
            val h = height.toFloat()
            if (w <= 0f || h <= 0f || adminTraySlide <= 0f) return RectF()

            val itemCount = adminTrayItemCount()
            val cols = 3
            val rows = (itemCount + cols - 1) / cols
            val divider = 1f
            val panelW = w * 0.7f
            val cellH = h * 0.08f
            val handleH = if (gamepadOnlyMode) 0f else h * 0.02f
            val panelH = rows * cellH + (rows - 1) * divider + handleH
            val panelLeft = (w - panelW) / 2f
            val panelTop = h - panelH * adminTraySlide
            return RectF(panelLeft, panelTop, panelLeft + panelW, panelTop + panelH)
        }

        private fun settingsDiagnosticBounds(d: DiagnosticState): RectF {
            val r = min(d.width, d.height) / 2f
            return RectF(d.centerX - r, d.centerY - r, d.centerX + r, d.centerY + r)
        }

        private fun settingsDiagnosticOccludedByTray(
            d: DiagnosticState,
            panelRect: RectF = computeAdminTrayPanelRect(),
        ): Boolean {
            if (!adminTrayOpen || panelRect.isEmpty) return false
            return RectF.intersects(panelRect, settingsDiagnosticBounds(d))
        }

        private fun settingsDiagnosticHit(
            d: DiagnosticState,
            px: Float,
            py: Float,
        ): Boolean {
            val r = min(d.width, d.height) / 2f
            return hypot(px - d.centerX, py - d.centerY) <= r * 1.3f
        }

        private fun findSettingsDiagnosticAt(
            px: Float,
            py: Float,
            visibleOnlyWhileTrayOpen: Boolean = false,
        ): DiagnosticState? {
            val panelRect = if (visibleOnlyWhileTrayOpen) computeAdminTrayPanelRect() else RectF()
            for (d in diagnosticStates) {
                if (d.control.type != DiagnosticType.SETTINGS) continue
                if (visibleOnlyWhileTrayOpen && settingsDiagnosticOccludedByTray(d, panelRect)) continue
                if (settingsDiagnosticHit(d, px, py)) return d
            }
            return null
        }

        private fun handleSettingsDiagnosticWhileTrayOpen(event: MotionEvent): Boolean {
            val idx = event.actionIndex
            val px = event.getX(idx)
            val py = event.getY(idx)
            val pid = event.getPointerId(idx)

            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    val settingsDiag = findSettingsDiagnosticAt(px, py, visibleOnlyWhileTrayOpen = true)
                    if (settingsDiag != null && settingsDiag.menuPid < 0) {
                        settingsDiag.menuPid = pid
                        invalidate()
                        return true
                    }
                }

                MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                    for (d in diagnosticStates) {
                        if (d.control.type != DiagnosticType.SETTINGS || d.menuPid != pid) continue
                        releaseMenuDiag(d, findSettingsDiagnosticAt(px, py, visibleOnlyWhileTrayOpen = true) == d)
                        return true
                    }
                }

                MotionEvent.ACTION_CANCEL -> {
                    for (d in diagnosticStates) {
                        if (d.control.type != DiagnosticType.SETTINGS || d.menuPid != pid) continue
                        releaseMenuDiag(d, false)
                        return true
                    }
                }
            }
            return false
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
                    "Cycle View$suffix"
                }

                ADMIN_TOGGLE_AUTOLEVEL -> {
                    val on = adminTrayAutoLevelingProvider?.invoke() ?: true
                    if (on) "AutoLevel: ON" else "AutoLevel: OFF"
                }

                ADMIN_QUICK_SAVE -> {
                    "Quick Save"
                }

                ADMIN_QUICK_LOAD -> {
                    "Quick Load"
                }

                ADMIN_OPEN_MENU -> {
                    "Game Menu"
                }

                ADMIN_NET_STATS -> {
                    "Net Stats"
                }

                ADMIN_EXIT_LAUNCHER -> {
                    "Exit"
                }

                ADMIN_NET_EVENTS -> {
                    "Net Events"
                }

                ADMIN_VIDEO_INFO -> {
                    "Video Info"
                }

                ADMIN_AUTOMAP -> {
                    "Automap"
                }

                ADMIN_HEADLIGHT -> {
                    "Headlight"
                }

                ADMIN_WARP -> {
                    adminTrayWarpLabelProvider?.invoke() ?: "Warp: --"
                }

                ADMIN_MUSIC -> {
                    "Music"
                }

                ADMIN_ACCEPT_JOIN -> {
                    adminTrayAcceptLabelProvider?.invoke() ?: "Accept: --"
                }

                else -> {
                    ""
                }
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

        private fun currentAdminTrayActions(): List<Int> =
            adminTrayVisibleActions(
                gamepadOnlyMode = gamepadOnlyMode,
                hasTouchAutomapButton = layout.buttons.any { it.binding == TouchBindings.BTN_AUTOMAP },
            )

        private fun adminTrayItemCount(): Int = currentAdminTrayActions().size

        private fun drawAdminTrayPanel(canvas: Canvas) {
            val w = width.toFloat()
            val h = height.toFloat()

            val actions = currentAdminTrayActions()
            val itemCount = actions.size
            val cols = 3
            val rows = (itemCount + cols - 1) / cols
            val divider = 1f // 1px divider between cells
            val panelW = w * 0.7f
            val cellH = h * 0.08f
            val handleH = if (gamepadOnlyMode) 0f else h * 0.02f
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

            // Drag handle bar at top of panel (touch mode only)
            if (!gamepadOnlyMode) {
                val handlePaint =
                    Paint(Paint.ANTI_ALIAS_FLAG).apply {
                        style = Paint.Style.FILL
                        color = 0x66FFFFFF
                    }
                val barW = panelW * 0.15f
                val barH = handleH * 0.25f
                val barLeft = panelLeft + (panelW - barW) / 2f
                val barTop = panelTop + handleH * 0.375f
                canvas.drawRoundRect(
                    barLeft,
                    barTop,
                    barLeft + barW,
                    barTop + barH,
                    barH / 2,
                    barH / 2,
                    handlePaint,
                )
            }

            val cellW = (panelW - (cols - 1) * divider) / cols
            val gridTop = panelTop + handleH

            val selectPaint =
                Paint(Paint.ANTI_ALIAS_FLAG).apply {
                    style = Paint.Style.STROKE
                    color = CONTROLLER_MENU_FOCUS_COLOR
                    strokeWidth = 3f
                }

            val checkboxFill =
                Paint(Paint.ANTI_ALIAS_FLAG).apply {
                    style = Paint.Style.FILL
                    color = 0x22000000
                }
            val checkboxStroke =
                Paint(Paint.ANTI_ALIAS_FLAG).apply {
                    style = Paint.Style.STROKE
                    color = 0xAAFFFFFF.toInt()
                    strokeWidth = 2f
                }
            val checkboxCheck =
                Paint(Paint.ANTI_ALIAS_FLAG).apply {
                    style = Paint.Style.STROKE
                    color = 0xFF4CAF50.toInt()
                    strokeWidth = 4f
                    strokeCap = Paint.Cap.ROUND
                    strokeJoin = Paint.Join.ROUND
                }

            for (i in 0 until itemCount) {
                val action = actions[i]
                val col = i % cols
                val row = i / cols
                val left = panelLeft + col * (cellW + divider)
                val top = gridTop + row * (cellH + divider)
                val rect = RectF(left, top, left + cellW, top + cellH)
                adminTrayRects.add(rect)

                val enabled = adminTrayActionEnabled(action, adminTrayEnabledStateProvider)
                val checked = adminTrayToggleStateProvider?.invoke(action) == true

                val bg =
                    if (!enabled) {
                        if (checked && adminTrayUsesCheckbox(action)) {
                            paintBtnLatchedDisabled
                        } else if (i == adminTrayPressedIndex) {
                            paintBtnPressedDisabled
                        } else {
                            paintBtnIdleDisabled
                        }
                    } else if (i == adminTrayPressedIndex) {
                        paintBtnPressed
                    } else if (checked && adminTrayUsesCheckbox(action)) {
                        paintBtnLatched
                    } else {
                        paintBtnIdle
                    }
                bg.alpha = if (i == adminTrayPressedIndex) 0xAA else 0x55
                canvas.drawRect(rect, bg)

                // D-pad selection highlight
                if (adminTraySelectedIndex >= 0 && i == adminTraySelectedIndex) {
                    canvas.drawRect(rect, selectPaint)
                }

                // Draw divider lines
                if (col > 0) {
                    val divPaint = Paint().apply { color = 0x33FFFFFF }
                    canvas.drawRect(left - divider, top, left, top + cellH, divPaint)
                }
                if (row > 0) {
                    val divPaint = Paint().apply { color = 0x33FFFFFF }
                    canvas.drawRect(left, top - divider, left + cellW, top, divPaint)
                }

                textPaint.alpha = if (enabled) 0xDD else 0x66
                if (adminTrayUsesCheckbox(action)) {
                    val checkboxSize = min(cellH * 0.26f, cellW * 0.18f)
                    val checkboxLeft = rect.left + cellW * 0.12f
                    val checkboxTop = rect.centerY() - checkboxSize / 2f
                    val checkboxRect =
                        RectF(
                            checkboxLeft,
                            checkboxTop,
                            checkboxLeft + checkboxSize,
                            checkboxTop + checkboxSize,
                        )
                    checkboxFill.alpha = if (enabled) 0xAA else 0x44
                    checkboxStroke.alpha = if (enabled) 0xCC else 0x66
                    checkboxCheck.alpha = if (enabled) 0xFF else 0x66
                    canvas.drawRect(checkboxRect, checkboxFill)
                    canvas.drawRect(checkboxRect, checkboxStroke)
                    if (checked) {
                        val left = checkboxRect.left
                        val top = checkboxRect.top
                        val size = checkboxRect.width()
                        canvas.drawLine(
                            left + size * 0.2f,
                            top + size * 0.55f,
                            left + size * 0.43f,
                            top + size * 0.78f,
                            checkboxCheck,
                        )
                        canvas.drawLine(
                            left + size * 0.43f,
                            top + size * 0.78f,
                            left + size * 0.82f,
                            top + size * 0.22f,
                            checkboxCheck,
                        )
                    }
                    textPaint.textAlign = Paint.Align.LEFT
                    canvas.drawText(
                        adminTrayLabel(action),
                        checkboxRect.right + cellW * 0.08f,
                        rect.centerY() + textPaint.textSize * 0.35f,
                        textPaint,
                    )
                    textPaint.textAlign = Paint.Align.CENTER
                } else {
                    canvas.drawText(
                        adminTrayLabel(action),
                        rect.centerX(),
                        rect.centerY() + textPaint.textSize * 0.35f,
                        textPaint,
                    )
                }
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
                        if (!open && adminTraySlide == 0f) {
                            if (adminTrayOpen) {
                                adminTrayClosedCallback?.invoke()
                            }
                            adminTrayOpen = false
                            adminTraySelectedIndex = -1
                        }
                        invalidate()
                    }
                    start()
                }
        }

        /** Toggle the admin tray open/closed (for gamepad Start button). */
        fun toggleAdminTray() {
            if (adminTrayOpen) {
                closeAdminTray()
            } else {
                openAdminTray()
            }
        }

        private fun defaultAdminTraySelectedIndex(): Int =
            if (gamepadOnlyMode &&
                adminTrayItemCount() >= 2
            ) {
                adminTrayItemCount() - 2
            } else {
                0
            }

        fun openAdminTray(fromGamepad: Boolean = false) {
            closeRemainingActions()
            if (adminTrayOpen) {
                if (fromGamepad && adminTraySelectedIndex < 0) {
                    adminTraySelectedIndex = defaultAdminTraySelectedIndex()
                    invalidate()
                }
                return
            }
            adminTrayOpen = true
            if (fromGamepad || gamepadOnlyMode) {
                adminTraySelectedIndex = defaultAdminTraySelectedIndex()
            } else {
                adminTraySelectedIndex = -1
            }
            adminTrayOpenedCallback?.invoke()
            animateAdminTray(true)
        }

        fun closeAdminTray() {
            if (!adminTrayOpen && adminTraySlide <= 0f) return
            animateAdminTray(false)
        }

        /** Whether the admin tray is currently open. */
        fun isAdminTrayOpen(): Boolean = adminTrayOpen

        /**
         * Handle a gamepad key event while the admin tray is open.
         * Returns true if consumed. Called from MainActivity for D-pad/A/B routing.
         * [action]: 0 = down, 1 = up.
         */
        fun handleAdminTrayGamepadKey(
            keyCode: Int,
            action: Int,
        ): Boolean {
            if (!adminTrayOpen) return false
            if (action != 0) return true // consume up events but only act on down
            val cols = 3
            val visibleActions = currentAdminTrayActions()
            val count = visibleActions.size
            if (count > 0 && adminTraySelectedIndex !in 0 until count) {
                adminTraySelectedIndex = defaultAdminTraySelectedIndex().coerceIn(0, count - 1)
            }
            when (keyCode) {
                android.view.KeyEvent.KEYCODE_DPAD_UP -> {
                    if (adminTraySelectedIndex >= cols) {
                        adminTraySelectedIndex -= cols
                    }
                    invalidate()
                    return true
                }

                android.view.KeyEvent.KEYCODE_DPAD_DOWN -> {
                    if (adminTraySelectedIndex + cols < count) {
                        adminTraySelectedIndex += cols
                    }
                    invalidate()
                    return true
                }

                android.view.KeyEvent.KEYCODE_DPAD_LEFT -> {
                    if (adminTraySelectedIndex % cols > 0) {
                        adminTraySelectedIndex--
                    }
                    invalidate()
                    return true
                }

                android.view.KeyEvent.KEYCODE_DPAD_RIGHT -> {
                    if (adminTraySelectedIndex % cols < cols - 1 &&
                        adminTraySelectedIndex + 1 < count
                    ) {
                        adminTraySelectedIndex++
                    }
                    invalidate()
                    return true
                }

                android.view.KeyEvent.KEYCODE_BUTTON_A,
                android.view.KeyEvent.KEYCODE_DPAD_CENTER,
                -> {
                    if (adminTraySelectedIndex in 0 until count &&
                        adminTrayActionEnabled(visibleActions[adminTraySelectedIndex], adminTrayEnabledStateProvider)
                    ) {
                        val selectedAction = visibleActions[adminTraySelectedIndex]
                        adminTrayCallback?.invoke(selectedAction)
                        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                        if (adminTrayClosesAfterActivate(selectedAction)) {
                            closeAdminTray()
                        } else {
                            invalidate()
                        }
                    }
                    return true
                }

                android.view.KeyEvent.KEYCODE_BUTTON_B,
                android.view.KeyEvent.KEYCODE_BACK,
                -> {
                    closeAdminTray()
                    return true
                }
            }
            // Consume all other buttons while tray is open (prevent game input)
            return true
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
                    val panelTop = computeAdminTrayPanelRect().top
                    if (py < panelTop) {
                        closeAdminTray()
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
                        val itemCount = adminTrayItemCount()
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
                            if (adminTraySlide > 0.7f) {
                                animateAdminTray(true)
                            } else {
                                closeAdminTray()
                            }
                        } else if (adminTrayPressedIndex >= 0 &&
                            adminTrayPressedIndex < adminTrayRects.size &&
                            adminTrayRects[adminTrayPressedIndex].contains(px, py) &&
                            adminTrayPressedIndex < currentAdminTrayActions().size
                        ) {
                            val pressedAction = currentAdminTrayActions()[adminTrayPressedIndex]
                            if (adminTrayActionEnabled(pressedAction, adminTrayEnabledStateProvider)) {
                                adminTrayCallback?.invoke(pressedAction)
                                performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                                if (adminTrayClosesAfterActivate(pressedAction)) {
                                    closeAdminTray()
                                }
                            }
                            invalidate()
                        }
                        adminTrayPressedIndex = -1
                        adminTrayPointerId = -1
                        adminTrayDragging = false
                    }
                    return true
                }

                MotionEvent.ACTION_CANCEL -> {
                    if (adminTrayDragging) {
                        if (adminTraySlide > 0.7f) {
                            animateAdminTray(true)
                        } else {
                            closeAdminTray()
                        }
                    }
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

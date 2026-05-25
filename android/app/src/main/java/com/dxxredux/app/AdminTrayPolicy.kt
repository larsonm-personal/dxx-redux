package com.dxxredux.app

import kotlin.math.min
import kotlin.math.roundToInt

internal fun adminTrayUsesCheckbox(actionIndex: Int): Boolean =
    when (actionIndex) {
        TouchOverlayView.ADMIN_NET_EVENTS,
        TouchOverlayView.ADMIN_NET_STATS,
        TouchOverlayView.ADMIN_VIDEO_INFO,
        -> true

        else -> false
    }

internal fun adminTrayUsesSlider(actionIndex: Int): Boolean =
    when (actionIndex) {
        TouchOverlayView.ADMIN_BRIGHTNESS -> true
        else -> false
    }

internal fun adminTrayClosesAfterActivate(actionIndex: Int): Boolean =
    if (adminTrayUsesSlider(actionIndex)) {
        false
    } else {
        when (actionIndex) {
            TouchOverlayView.ADMIN_INCREASE_VIEW,
            TouchOverlayView.ADMIN_MUSIC,
            TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL,
            -> false

            else -> !adminTrayUsesCheckbox(actionIndex)
        }
    }

internal fun clampAdminTrayBrightness(value: Int): Int = value.coerceIn(0, 16)

internal fun stepAdminTrayBrightness(
    value: Int,
    delta: Int,
): Int = clampAdminTrayBrightness(value + delta)

internal fun adminTrayBrightnessFromFraction(fraction: Float): Int =
    clampAdminTrayBrightness((fraction.coerceIn(0f, 1f) * 16f).roundToInt())

internal fun adminTrayActionEnabled(
    actionIndex: Int,
    enabledProvider: ((Int) -> Boolean)? = null,
): Boolean = enabledProvider?.invoke(actionIndex) != false

internal fun adminTrayVisibleActions(
    gamepadOnlyMode: Boolean,
    hasTouchAutomapButton: Boolean,
    isMultiplayerGame: Boolean = false,
    hasPendingLaunchInfo: Boolean = false,
): List<Int> {
    val showNetworkActions = isMultiplayerGame || hasPendingLaunchInfo
    val actions =
        mutableListOf(
            TouchOverlayView.ADMIN_INCREASE_VIEW,
            TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL,
            TouchOverlayView.ADMIN_QUICK_LOAD,
            TouchOverlayView.ADMIN_OPEN_MENU,
            TouchOverlayView.ADMIN_EXIT_LAUNCHER,
            TouchOverlayView.ADMIN_QUICK_SAVE,
            TouchOverlayView.ADMIN_VIDEO_INFO,
            TouchOverlayView.ADMIN_BRIGHTNESS,
        )
    if (showNetworkActions) {
        actions.add(2, TouchOverlayView.ADMIN_NET_STATS)
        actions.add(5, TouchOverlayView.ADMIN_NET_EVENTS)
    }
    if (gamepadOnlyMode) {
        if (isMultiplayerGame) actions.add(TouchOverlayView.ADMIN_WARP)
        actions.add(TouchOverlayView.ADMIN_MUSIC)
        if (showNetworkActions) actions.add(TouchOverlayView.ADMIN_ACCEPT_JOIN)
    }
    return actions
}

internal const val CONTROLLER_MENU_FOCUS_COLOR = 0xFF00E676.toInt()
private const val CONTROLLER_MENU_ADVANCE_WINDOW_MS = 2500L

internal enum class ControllerMenuSurface {
    NONE,
    REMAINING_ACTIONS,
    ADMIN_TRAY,
}

internal fun nextControllerMenuSurface(
    current: ControllerMenuSurface,
    hasRemainingActions: Boolean,
    canAdvanceFromRemainingActions: Boolean = true,
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
            if (hasRemainingActions && canAdvanceFromRemainingActions) {
                ControllerMenuSurface.ADMIN_TRAY
            } else {
                ControllerMenuSurface.NONE
            }
        }

        ControllerMenuSurface.ADMIN_TRAY -> {
            ControllerMenuSurface.NONE
        }
    }

internal fun remainingActionsCanAdvanceToAdminTray(
    actionTakenSinceOpen: Boolean,
    openDurationMs: Long,
    advanceWindowMs: Long = CONTROLLER_MENU_ADVANCE_WINDOW_MS,
): Boolean = !actionTakenSinceOpen && openDurationMs <= advanceWindowMs

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

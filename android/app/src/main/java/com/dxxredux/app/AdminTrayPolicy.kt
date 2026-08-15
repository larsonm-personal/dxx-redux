package com.dxxredux.app

import kotlin.math.min
import kotlin.math.roundToInt

internal fun adminTrayUsesCheckbox(actionIndex: Int): Boolean =
    when (actionIndex) {
        TouchOverlayView.ADMIN_NET_EVENTS,
        TouchOverlayView.ADMIN_NET_STATS,
        TouchOverlayView.ADMIN_AUTOMAP_SECRET_REVEAL,
        TouchOverlayView.ADMIN_AUTOMAP_REACTOR,
        TouchOverlayView.ADMIN_VIDEO_INFO,
        -> true

        else -> false
    }

internal fun adminTrayCyclesState(actionIndex: Int): Boolean = actionIndex == TouchOverlayView.ADMIN_AUTOMAP_OBJECTIVES

internal fun adminTrayUsesSlider(actionIndex: Int): Boolean =
    when (actionIndex) {
        TouchOverlayView.ADMIN_BRIGHTNESS,
        TouchOverlayView.ADMIN_FOV,
        -> true

        else -> false
    }

internal fun adminTrayClosesAfterActivate(actionIndex: Int): Boolean =
    if (adminTrayUsesSlider(actionIndex) || adminTrayCyclesState(actionIndex)) {
        false
    } else {
        when (actionIndex) {
            TouchOverlayView.ADMIN_INCREASE_VIEW,
            TouchOverlayView.ADMIN_CYCLE_LEFT_VIEW,
            TouchOverlayView.ADMIN_CYCLE_RIGHT_VIEW,
            TouchOverlayView.ADMIN_DIFFICULTY,
            TouchOverlayView.ADMIN_CHEATS,
            TouchOverlayView.ADMIN_MUSIC,
            TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL,
            -> false

            else -> !adminTrayUsesCheckbox(actionIndex)
        }
    }

// These values mirror level_metadata_objective_mode in level_metadata_scan.h.
internal const val OBJECTIVE_MODE_OFF = 0
internal const val OBJECTIVE_MODE_ALL = 1
internal const val OBJECTIVE_MODE_REMAINING = 2
internal const val OBJECTIVE_MODE_NEXT = 3

internal fun objectiveOverlayLabel(mode: Int): String =
    "Objectives: " +
        when (mode) {
            OBJECTIVE_MODE_ALL -> "All"
            OBJECTIVE_MODE_REMAINING -> "Remaining"
            OBJECTIVE_MODE_NEXT -> "Next"
            else -> "Off"
        }

internal fun clampAdminTrayBrightness(value: Int): Int = value.coerceIn(0, 16)

internal fun stepAdminTrayBrightness(
    value: Int,
    delta: Int,
): Int = clampAdminTrayBrightness(value + delta)

internal fun adminTrayBrightnessFromFraction(fraction: Float): Int =
    clampAdminTrayBrightness((fraction.coerceIn(0f, 1f) * 16f).roundToInt())

private val ADMIN_TRAY_FOV_VALUES = intArrayOf(0, 100, 110, 120)
internal val ADMIN_TRAY_DIFFICULTY_NAMES =
    arrayOf("Trainee", "Rookie", "Hotshot", "Ace", "Insane")

internal fun clampAdminTrayFov(value: Int): Int = if (value in ADMIN_TRAY_FOV_VALUES) value else 0

internal fun adminTrayFovIndex(value: Int): Int =
    ADMIN_TRAY_FOV_VALUES.indexOf(clampAdminTrayFov(value)).coerceAtLeast(0)

internal fun stepAdminTrayFov(
    value: Int,
    delta: Int,
): Int = ADMIN_TRAY_FOV_VALUES[(adminTrayFovIndex(value) + delta).coerceIn(0, ADMIN_TRAY_FOV_VALUES.lastIndex)]

internal fun adminTrayFovFromFraction(fraction: Float): Int =
    ADMIN_TRAY_FOV_VALUES[
        (fraction.coerceIn(0f, 1f) * ADMIN_TRAY_FOV_VALUES.lastIndex).roundToInt(),
    ]

internal fun adminTrayFovLabel(value: Int): String {
    val fov = clampAdminTrayFov(value)
    return if (fov == 0) "Base" else "$fov deg"
}

internal fun clampAdminTrayDifficulty(value: Int): Int = value.coerceIn(0, ADMIN_TRAY_DIFFICULTY_NAMES.lastIndex)

internal fun adminTrayDifficultyLabel(value: Int): String = ADMIN_TRAY_DIFFICULTY_NAMES[clampAdminTrayDifficulty(value)]

internal fun adminTrayActionEnabled(
    actionIndex: Int,
    enabledProvider: ((Int) -> Boolean)? = null,
): Boolean = enabledProvider?.invoke(actionIndex) != false

internal fun adminTrayVisibleActions(
    gamepadOnlyMode: Boolean,
    hasTouchAutomapButton: Boolean,
    isMultiplayerGame: Boolean = false,
    hasPendingLaunchInfo: Boolean = false,
    hasGuidebotAbdicateAction: Boolean = false,
    hasCameraWindowCycleActions: Boolean = false,
    automapActive: Boolean = false,
    canShowDifficultyChange: Boolean = false,
    canShowCoopLevelRestart: Boolean = false,
    previewMode: Boolean = false,
    mapCheatsAccessible: Boolean = true,
    reactorPauseAllowed: Boolean = true,
): List<Int> {
    if (previewMode) {
        return if (automapActive) {
            listOf(
                TouchOverlayView.ADMIN_AUTOMAP_SECRET_REVEAL,
                TouchOverlayView.ADMIN_AUTOMAP_OBJECTIVES,
            )
        } else {
            emptyList()
        }
    }
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
            TouchOverlayView.ADMIN_FOV,
            TouchOverlayView.ADMIN_MUSIC,
        )
    if (hasCameraWindowCycleActions) {
        actions.add(1, TouchOverlayView.ADMIN_CYCLE_LEFT_VIEW)
        actions.add(2, TouchOverlayView.ADMIN_CYCLE_RIGHT_VIEW)
    }
    if (canShowDifficultyChange) {
        actions.add(TouchOverlayView.ADMIN_DIFFICULTY)
    }
    if (!isMultiplayerGame && !hasPendingLaunchInfo && !automapActive) {
        actions.add(TouchOverlayView.ADMIN_CHEATS)
    }
    if (showNetworkActions) {
        actions.add(2, TouchOverlayView.ADMIN_NET_STATS)
        actions.add(5, TouchOverlayView.ADMIN_NET_EVENTS)
    }
    if (isMultiplayerGame && hasGuidebotAbdicateAction) {
        actions.add(6, TouchOverlayView.ADMIN_ABDICATE_GUIDEBOT)
    }
    if (canShowCoopLevelRestart && !automapActive) {
        actions.add(actions.indexOf(TouchOverlayView.ADMIN_OPEN_MENU) + 1, TouchOverlayView.ADMIN_RESTART_LEVEL)
    }
    if (gamepadOnlyMode) {
        if (isMultiplayerGame) actions.add(TouchOverlayView.ADMIN_WARP)
        if (showNetworkActions) actions.add(TouchOverlayView.ADMIN_ACCEPT_JOIN)
    }
    if (automapActive) {
        actions.removeAll(
            listOf(
                TouchOverlayView.ADMIN_QUICK_LOAD,
                TouchOverlayView.ADMIN_OPEN_MENU,
                TouchOverlayView.ADMIN_EXIT_LAUNCHER,
                TouchOverlayView.ADMIN_QUICK_SAVE,
            ),
        )
        if (mapCheatsAccessible) {
            actions.add(TouchOverlayView.ADMIN_AUTOMAP_SECRET_REVEAL)
            if (reactorPauseAllowed) {
                actions.add(TouchOverlayView.ADMIN_AUTOMAP_REACTOR)
            }
            actions.add(TouchOverlayView.ADMIN_AUTOMAP_OBJECTIVES)
        }
    }
    return actions
}

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

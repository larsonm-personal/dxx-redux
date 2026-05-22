package com.dxxredux.app

import android.view.KeyEvent

internal fun moveLinearSelection(
    currentIndex: Int,
    itemCount: Int,
    keyCode: Int,
): Int {
    if (itemCount <= 0) return -1

    val safeIndex = currentIndex.coerceIn(0, itemCount - 1)
    return when (keyCode) {
        KeyEvent.KEYCODE_DPAD_UP,
        KeyEvent.KEYCODE_DPAD_LEFT,
        -> {
            (safeIndex - 1).coerceAtLeast(0)
        }

        KeyEvent.KEYCODE_DPAD_DOWN,
        KeyEvent.KEYCODE_DPAD_RIGHT,
        -> {
            (safeIndex + 1).coerceAtMost(itemCount - 1)
        }

        else -> {
            safeIndex
        }
    }
}

internal fun scrollOffsetToKeepRowVisible(
    currentOffset: Float,
    selectedIndex: Int,
    rowHeight: Float,
    viewportHeight: Float,
): Float {
    if (selectedIndex < 0 || rowHeight <= 0f || viewportHeight <= 0f) {
        return currentOffset.coerceAtLeast(0f)
    }

    val itemTop = selectedIndex * rowHeight
    val itemBottom = itemTop + rowHeight
    return when {
        itemTop < currentOffset -> {
            itemTop
        }

        itemBottom > currentOffset + viewportHeight -> {
            itemBottom - viewportHeight
        }

        else -> {
            currentOffset
        }
    }.coerceAtLeast(0f)
}

internal fun shouldCloseControllerSettingsStackForMenu(
    pressed: Boolean,
    settingsRootVisible: Boolean,
    musicPanelVisible: Boolean,
    videoInfoVisible: Boolean,
): Boolean = pressed && (settingsRootVisible || musicPanelVisible || videoInfoVisible)

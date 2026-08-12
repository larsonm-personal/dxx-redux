package com.dxxredux.app

import kotlin.math.min

internal data class ActionButtonBounds(
    val left: Float,
    val top: Float,
    val right: Float,
    val bottom: Float,
)

internal object TopEdgeActionButtonLayout {
    fun acceptJoin(
        width: Int,
        height: Int,
    ): ActionButtonBounds {
        val edge = min(width, height).toFloat()
        val buttonWidth = edge * 0.4f
        val buttonHeight = edge * 0.07f
        val left = (width - buttonWidth) / 2f
        val top = edge * 0.015f
        return ActionButtonBounds(left, top, left + buttonWidth, top + buttonHeight)
    }

    fun warp(
        width: Int,
        height: Int,
    ): ActionButtonBounds {
        val edge = min(width, height).toFloat()
        val left = edge * 0.1f
        val top = edge * 0.015f
        return ActionButtonBounds(left, top, left + width * 0.18f, top + height * 0.08f)
    }
}

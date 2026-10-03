package com.dxxredux.app

import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.StateListDrawable

internal object PauseOverlayStyle {
    val BACKGROUND_COLOR = 0xE6222222.toInt()
    val BORDER_COLOR = 0xEEFFFFFF.toInt()
    val TEXT_COLOR = 0xFFFFFFFF.toInt()
    val SECONDARY_TEXT_COLOR = 0xDDFFFFFF.toInt()
    val FOCUS_COLOR = 0xFF00E676.toInt()
    const val WIDTH_RATIO = 0.44f
    const val CORNER_RATIO = 0.2f
    const val BORDER_RATIO = 0.025f

    const val QUICK_LOAD_QUESTION = "load quick save?"
    const val QUICK_LOAD_YES = "yes"
    const val QUICK_LOAD_NO = "no"

    fun cardBackground(density: Float) =
        GradientDrawable().apply {
            setColor(BACKGROUND_COLOR)
            cornerRadius = 24f * density
            setStroke((3f * density).toInt().coerceAtLeast(3), BORDER_COLOR)
        }

    fun choiceBackground(density: Float): StateListDrawable {
        fun background(selected: Boolean) =
            GradientDrawable().apply {
                setColor(if (selected) 0x55FFFFFF else 0x22FFFFFF)
                cornerRadius = 12f * density
                setStroke(
                    ((if (selected) 3f else 2f) * density).toInt().coerceAtLeast(2),
                    if (selected) FOCUS_COLOR else BORDER_COLOR,
                )
            }
        return StateListDrawable().apply {
            addState(intArrayOf(android.R.attr.state_selected), background(true))
            addState(intArrayOf(android.R.attr.state_pressed), background(true))
            addState(intArrayOf(android.R.attr.state_focused), background(true))
            addState(intArrayOf(), background(false))
        }
    }
}

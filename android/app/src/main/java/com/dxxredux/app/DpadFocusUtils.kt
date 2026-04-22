package com.dxxredux.app

import androidx.compose.foundation.border
import androidx.compose.foundation.focusable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

/** Bright border shown on focused elements for TV/controller navigation. */
val tvFocusBorderColor = Color(0xFF00E676)
private val tvFocusBorderShape = RoundedCornerShape(6.dp)
private val tvFocusBorderWidth = 3.dp

/** Adds the shared bright TV focus border to any composable. */
fun Modifier.tvFocusBorder(): Modifier =
    composed {
        var focused by remember { mutableStateOf(false) }
        this
            .onFocusChanged { focused = it.isFocused }
            .then(
                if (focused) {
                    Modifier.border(tvFocusBorderWidth, tvFocusBorderColor, tvFocusBorderShape)
                } else {
                    Modifier
                },
            )
    }

/**
 * Makes a composable focusable with a visible highlight border when focused.
 * Use on custom clickable surfaces that aren't Material3 buttons (which
 * already have built-in focus indicators).
 */
fun Modifier.tvFocusable(): Modifier =
    composed {
        var focused by remember { mutableStateOf(false) }
        this
            .onFocusChanged { focused = it.isFocused }
            .then(
                if (focused) {
                    Modifier.border(tvFocusBorderWidth, tvFocusBorderColor, tvFocusBorderShape)
                } else {
                    Modifier
                },
            ).focusable()
    }

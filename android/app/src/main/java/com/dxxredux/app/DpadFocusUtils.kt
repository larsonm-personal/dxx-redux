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
private val focusBorderColor = Color(0xFF64B5F6)

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
                    Modifier.border(2.dp, focusBorderColor, RoundedCornerShape(4.dp))
                } else {
                    Modifier
                },
            ).focusable()
    }

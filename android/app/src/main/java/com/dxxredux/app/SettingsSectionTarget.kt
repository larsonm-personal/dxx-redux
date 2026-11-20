package com.dxxredux.app

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.border
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.relocation.BringIntoViewRequester
import androidx.compose.foundation.relocation.bringIntoViewRequester
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay

/** Passive navigation only: this never invokes a control's change or click callback */
@OptIn(ExperimentalFoundationApi::class)
internal fun Modifier.settingsSectionTarget(
    sectionId: String,
    target: String?,
    focusContainer: Boolean = false,
): Modifier =
    composed {
        if (sectionId != target) return@composed this
        val bringIntoView = remember { BringIntoViewRequester() }
        val focus = remember { FocusRequester() }
        var size by remember { mutableStateOf(IntSize.Zero) }
        var applied by rememberSaveable(sectionId, target) { mutableStateOf(false) }
        var highlighted by remember { mutableStateOf(false) }
        LaunchedEffect(size != IntSize.Zero) {
            if (size == IntSize.Zero || applied) return@LaunchedEffect
            // Wait until both the target and its scrolling parent have been placed
            withFrameNanos { }
            applied = true
            highlighted = true
            focus.requestFocus()
            // A section can be taller than the viewport: reveal its measured leading edge
            bringIntoView.bringIntoView(Rect(0f, 0f, size.width.toFloat(), 1f))
            delay(2000)
            highlighted = false
        }
        this
            .onGloballyPositioned { size = it.size }
            .bringIntoViewRequester(bringIntoView)
            .focusRequester(focus)
            .then(if (focusContainer) Modifier.focusable() else Modifier)
            .semantics { if (highlighted) stateDescription = "Search result" }
            .then(if (highlighted) Modifier.border(2.dp, MaterialTheme.colorScheme.primary) else Modifier)
    }

@Composable
internal fun SettingsTargetSection(
    sectionId: String,
    target: String?,
    content: @Composable ColumnScope.() -> Unit,
) {
    Column(
        modifier = Modifier.fillMaxWidth().settingsSectionTarget(sectionId, target, focusContainer = true),
        content = content,
    )
}

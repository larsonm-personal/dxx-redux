package com.dxxredux.app

import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowLeft
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable
internal fun BoxScope.SharedScrollArrows(scrollState: ScrollState) {
    if (scrollState.canScrollBackward) {
        ScrollArrowSurface(Modifier.align(Alignment.TopCenter).padding(top = 4.dp)) {
            Icon(Icons.Default.KeyboardArrowUp, "Scroll up", Modifier.size(24.dp))
        }
    }
    if (scrollState.canScrollForward) {
        ScrollArrowSurface(Modifier.align(Alignment.BottomCenter).padding(bottom = 4.dp)) {
            Icon(Icons.Default.KeyboardArrowDown, "Scroll down", Modifier.size(24.dp))
        }
    }
}

@Composable
internal fun BoxScope.SharedHorizontalScrollArrows(scrollState: ScrollState) {
    SharedHorizontalScrollArrows(
        canScrollBackward = scrollState.canScrollBackward,
        canScrollForward = scrollState.canScrollForward,
    )
}

@Composable
internal fun BoxScope.SharedHorizontalScrollArrows(
    canScrollBackward: Boolean,
    canScrollForward: Boolean,
    onScrollBackward: (() -> Unit)? = null,
    onScrollForward: (() -> Unit)? = null,
) {
    if (canScrollBackward) {
        ScrollArrowSurface(Modifier.align(Alignment.CenterStart).padding(start = 4.dp), onScrollBackward) {
            Icon(Icons.AutoMirrored.Filled.KeyboardArrowLeft, "Scroll left", Modifier.size(24.dp))
        }
    }
    if (canScrollForward) {
        ScrollArrowSurface(Modifier.align(Alignment.CenterEnd).padding(end = 4.dp), onScrollForward) {
            Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, "Scroll right", Modifier.size(24.dp))
        }
    }
}

@Composable
internal fun BoxScope.SharedLazyListScrollArrows(listState: LazyListState) {
    if (listState.canScrollBackward) {
        ScrollArrowSurface(Modifier.align(Alignment.TopCenter).padding(top = 4.dp)) {
            Icon(Icons.Default.KeyboardArrowUp, "Scroll up", Modifier.size(24.dp))
        }
    }
    if (listState.canScrollForward) {
        ScrollArrowSurface(Modifier.align(Alignment.BottomCenter).padding(bottom = 4.dp)) {
            Icon(Icons.Default.KeyboardArrowDown, "Scroll down", Modifier.size(24.dp))
        }
    }
}

@Composable
internal fun BoxScope.SharedLazyRowScrollArrows(listState: LazyListState) {
    if (listState.canScrollBackward) {
        ScrollArrowSurface(Modifier.align(Alignment.CenterStart).padding(start = 4.dp)) {
            Icon(Icons.AutoMirrored.Filled.KeyboardArrowLeft, "Scroll left", Modifier.size(24.dp))
        }
    }
    if (listState.canScrollForward) {
        ScrollArrowSurface(Modifier.align(Alignment.CenterEnd).padding(end = 4.dp)) {
            Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, "Scroll right", Modifier.size(24.dp))
        }
    }
}

@Composable
private fun ScrollArrowSurface(
    modifier: Modifier,
    onClick: (() -> Unit)? = null,
    content: @Composable () -> Unit,
) {
    Surface(
        modifier =
            if (onClick == null) {
                modifier
            } else {
                modifier.clickable(onClick = onClick)
            },
        shape = CircleShape,
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.85f),
        shadowElevation = 2.dp,
    ) {
        androidx.compose.runtime.CompositionLocalProvider(
            androidx.compose.material3.LocalContentColor provides MaterialTheme.colorScheme.onSurfaceVariant,
        ) {
            content()
        }
    }
}

package com.dxxredux.app

import kotlin.math.min

internal const val MUSIC_PREVIOUS_GLYPH = "\u25C0"
internal const val MUSIC_NEXT_GLYPH = "\u25B6"

internal data class MusicDiagnosticGeometry(
    val buttonRadius: Float,
    val arrowTextSize: Float,
    val labelTextSize: Float,
    val buttonY: Float,
    val previousButtonX: Float,
    val nextButtonX: Float,
    val labelX: Float,
    val buttonGroupLeft: Float,
    val buttonGroupTop: Float,
    val buttonGroupRight: Float,
    val buttonGroupBottom: Float,
)

internal fun musicDiagnosticGeometry(
    centerX: Float,
    centerY: Float,
    surfaceWidth: Float,
    surfaceHeight: Float,
    sizeMultiplier: Float,
): MusicDiagnosticGeometry {
    val baseSize = min(surfaceWidth, surfaceHeight)
    val radius = baseSize * 0.03f * sizeMultiplier
    val gap = baseSize * 0.02f * sizeMultiplier
    val halfGroupWidth = radius * 2f + gap / 2f
    return MusicDiagnosticGeometry(
        buttonRadius = radius,
        arrowTextSize = radius * 0.9f,
        labelTextSize = radius * 0.7f,
        buttonY = centerY,
        previousButtonX = centerX - radius - gap / 2f,
        nextButtonX = centerX + radius + gap / 2f,
        labelX = centerX + halfGroupWidth + gap,
        buttonGroupLeft = centerX - halfGroupWidth,
        buttonGroupTop = centerY - radius,
        buttonGroupRight = centerX + halfGroupWidth,
        buttonGroupBottom = centerY + radius,
    )
}

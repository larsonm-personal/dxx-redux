package com.dxxredux.app

internal data class MusicDiagnosticGeometry(
    val buttonRadius: Float,
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
    baseSize: Float,
    sizeMultiplier: Float,
): MusicDiagnosticGeometry {
    val radius = baseSize * 0.03f * sizeMultiplier
    val gap = baseSize * 0.02f * sizeMultiplier
    val halfGroupWidth = radius * 2f + gap / 2f
    return MusicDiagnosticGeometry(
        buttonRadius = radius,
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

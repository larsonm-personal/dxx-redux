package com.dxxredux.app

import kotlin.math.abs

internal fun scrollStripFractionalIndex(
    mainDelta: Float,
    halfSpan: Float,
    initialIndex: Int,
    itemCount: Int,
): Float {
    if (itemCount <= 1 || halfSpan <= 0f) return 0f
    val start = initialIndex.coerceIn(0, itemCount - 1).toFloat()
    return if (mainDelta <= 0f) {
        val progress = (-mainDelta / halfSpan).coerceIn(0f, 1f)
        start + progress * (itemCount - 1 - start)
    } else {
        val progress = (mainDelta / halfSpan).coerceIn(0f, 1f)
        start * (1f - progress)
    }
}

internal fun scrollStripItemScale(
    itemIndex: Int,
    fractionalIndex: Float,
    selectedScale: Float,
): Float {
    val proximity = (1f - abs(itemIndex - fractionalIndex) * 2f).coerceIn(0f, 1f)
    val smooth = proximity * proximity * (3f - 2f * proximity)
    return 1f + (selectedScale.coerceAtLeast(1f) - 1f) * smooth
}

internal fun clampScrollStripCenterPct(
    centerPct: Float,
    dragSpanWidthPct: Float,
    canvasWidth: Float,
    canvasHeight: Float,
    vertical: Boolean,
): Float {
    val axisSize = if (vertical) canvasHeight else canvasWidth
    if (axisSize <= 0f || canvasWidth <= 0f) return centerPct.coerceIn(0f, 100f)
    val halfSpanPct = dragSpanWidthPct.coerceIn(0f, 100f) * canvasWidth / axisSize / 2f
    return centerPct.coerceIn(halfSpanPct, 100f - halfSpanPct)
}

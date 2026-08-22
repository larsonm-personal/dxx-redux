package com.dxxredux.app

internal fun mouseEdgeAxisContribution(
    enabled: Boolean,
    positionPx: Float,
    originPx: Float,
    lowPx: Float,
    highPx: Float,
    edgeRegionPct: Float,
    edgeMaxRatePct: Float,
): Float {
    if (!enabled || highPx <= lowPx) return 0f

    val edgeWidth =
        (highPx - lowPx) *
            edgeRegionPct.coerceIn(
                TouchBindings.MIN_MOUSE_EDGE_REGION_PCT,
                TouchBindings.MAX_MOUSE_EDGE_REGION_PCT,
            ) / 100f
    if (edgeWidth <= 0f) return 0f

    val minimumStartTravel = edgeWidth / 2f
    val negativeRamp = ((lowPx + edgeWidth - positionPx) / edgeWidth).coerceIn(0f, 1f)
    val positiveRamp = ((positionPx - (highPx - edgeWidth)) / edgeWidth).coerceIn(0f, 1f)
    val negative = if (originPx - positionPx >= minimumStartTravel) negativeRamp else 0f
    val positive = if (positionPx - originPx >= minimumStartTravel) positiveRamp else 0f
    val maxRate =
        edgeMaxRatePct.coerceIn(
            TouchBindings.MIN_MOUSE_EDGE_MAX_RATE_PCT,
            TouchBindings.MAX_MOUSE_EDGE_MAX_RATE_PCT,
        ) / 100f
    return (positive - negative) * maxRate
}

internal fun combineMouseDragAndEdge(
    drag: Float,
    edge: Float,
    inverted: Boolean,
): Float {
    val combined = drag + edge
    return (if (inverted) -combined else combined).coerceIn(-1f, 1f)
}

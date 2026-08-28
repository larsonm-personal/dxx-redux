package com.dxxredux.app

internal data class MouseEdgeAxisVisualBands(
    val negativeFullRatePx: Float,
    val negativeRampInnerPx: Float,
    val positiveRampInnerPx: Float,
    val positiveFullRatePx: Float,
    val maxStrength: Float,
)

internal fun mouseEdgeAxisVisualBands(
    enabled: Boolean,
    lowPx: Float,
    highPx: Float,
    screenLowPx: Float,
    screenHighPx: Float,
    edgeRegionPct: Float,
    screenEdgeZonePct: Float,
    edgeMaxRatePct: Float,
): MouseEdgeAxisVisualBands? {
    if (!enabled || highPx <= lowPx || screenHighPx <= screenLowPx) return null

    val edgeWidth =
        (highPx - lowPx) *
            edgeRegionPct.coerceIn(
                TouchBindings.MIN_MOUSE_EDGE_REGION_PCT,
                TouchBindings.MAX_MOUSE_EDGE_REGION_PCT,
            ) / 100f
    val maxStrength =
        edgeMaxRatePct.coerceIn(
            TouchBindings.MIN_MOUSE_EDGE_MAX_RATE_PCT,
            TouchBindings.MAX_MOUSE_EDGE_MAX_RATE_PCT,
        ) / 100f
    if (edgeWidth <= 0f || maxStrength <= 0f) return null

    val screenEdgeWidth =
        (screenHighPx - screenLowPx) *
            screenEdgeZonePct.coerceIn(
                TouchBindings.MIN_MOUSE_SCREEN_EDGE_ZONE_PCT,
                TouchBindings.MAX_MOUSE_SCREEN_EDGE_ZONE_PCT,
            ) / 100f
    val negativeFullRatePx = (screenLowPx + screenEdgeWidth).coerceIn(lowPx, highPx)
    val positiveFullRatePx = (screenHighPx - screenEdgeWidth).coerceIn(lowPx, highPx)
    return MouseEdgeAxisVisualBands(
        negativeFullRatePx = negativeFullRatePx,
        negativeRampInnerPx = (negativeFullRatePx + edgeWidth).coerceIn(lowPx, highPx),
        positiveRampInnerPx = (positiveFullRatePx - edgeWidth).coerceIn(lowPx, highPx),
        positiveFullRatePx = positiveFullRatePx,
        maxStrength = maxStrength,
    )
}

internal fun mouseEdgeAxisVisualStrength(
    bands: MouseEdgeAxisVisualBands,
    positionPx: Float,
): Float {
    val negativeRampWidth = bands.negativeRampInnerPx - bands.negativeFullRatePx
    val positiveRampWidth = bands.positiveFullRatePx - bands.positiveRampInnerPx
    val negative =
        when {
            positionPx <= bands.negativeFullRatePx -> {
                1f
            }

            negativeRampWidth > 0f -> {
                ((bands.negativeRampInnerPx - positionPx) / negativeRampWidth).coerceIn(0f, 1f)
            }

            else -> {
                0f
            }
        }
    val positive =
        when {
            positionPx >= bands.positiveFullRatePx -> {
                1f
            }

            positiveRampWidth > 0f -> {
                ((positionPx - bands.positiveRampInnerPx) / positiveRampWidth).coerceIn(0f, 1f)
            }

            else -> {
                0f
            }
        }
    return maxOf(negative, positive) * bands.maxStrength
}

internal fun mouseEdgeAxisContribution(
    enabled: Boolean,
    positionPx: Float,
    originPx: Float,
    lowPx: Float,
    highPx: Float,
    screenLowPx: Float,
    screenHighPx: Float,
    edgeRegionPct: Float,
    screenEdgeZonePct: Float,
    edgeMaxRatePct: Float,
): Float {
    if (!enabled || highPx <= lowPx || screenHighPx <= screenLowPx) return 0f

    val edgeWidth =
        (highPx - lowPx) *
            edgeRegionPct.coerceIn(
                TouchBindings.MIN_MOUSE_EDGE_REGION_PCT,
                TouchBindings.MAX_MOUSE_EDGE_REGION_PCT,
            ) / 100f
    if (edgeWidth <= 0f) return 0f

    val screenEdgeWidth =
        (screenHighPx - screenLowPx) *
            screenEdgeZonePct.coerceIn(
                TouchBindings.MIN_MOUSE_SCREEN_EDGE_ZONE_PCT,
                TouchBindings.MAX_MOUSE_SCREEN_EDGE_ZONE_PCT,
            ) / 100f
    val negativeFullRatePx = (screenLowPx + screenEdgeWidth).coerceIn(lowPx, highPx)
    val positiveFullRatePx = (screenHighPx - screenEdgeWidth).coerceIn(lowPx, highPx)
    val minimumStartTravel = edgeWidth / 2f
    val negativeRamp = ((negativeFullRatePx + edgeWidth - positionPx) / edgeWidth).coerceIn(0f, 1f)
    val positiveRamp = ((positionPx - (positiveFullRatePx - edgeWidth)) / edgeWidth).coerceIn(0f, 1f)
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

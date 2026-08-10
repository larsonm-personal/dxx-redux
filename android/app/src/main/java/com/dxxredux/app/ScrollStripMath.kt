package com.dxxredux.app

import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.hypot
import kotlin.math.min
import kotlin.math.sin

internal const val DEFAULT_SCROLL_STRIP_SELECTED_SCALE = 2.6f
internal const val DEFAULT_SCROLL_STRIP_CARD_SCALE = 1f
internal const val MIN_SCROLL_STRIP_CARD_SCALE = 0.4f
internal const val MAX_SCROLL_STRIP_CARD_SCALE = 1.6f
internal const val LEGACY_SCROLL_STRIP_CARD_SCALE = 0.7f
internal const val SCROLL_STRIP_ROW_SPACING_RADII = 2.5f
internal val SCROLL_STRIP_ACTIVE_FILL_COLOR = 0xCC2E7D32.toInt()
internal val SCROLL_STRIP_INACTIVE_FILL_COLOR = 0x99555555.toInt()

internal fun scrollStripItemFillColor(activeAtLiftOff: Boolean): Int =
    if (activeAtLiftOff) SCROLL_STRIP_ACTIVE_FILL_COLOR else SCROLL_STRIP_INACTIVE_FILL_COLOR

internal fun selectorTriggerHit(
    touchX: Float,
    touchY: Float,
    centerX: Float,
    centerY: Float,
    radius: Float,
): Boolean = hypot(touchX - centerX, touchY - centerY) <= radius * 1.3f

internal fun scrollStripRowCrossOffset(
    triggerRadius: Float,
    rowOffset: ScrollStripRowOffset,
    cardScale: Float = LEGACY_SCROLL_STRIP_CARD_SCALE,
): Float = scrollStripRowSpacing(triggerRadius, cardScale) * rowOffset.crossDirection

internal fun scrollStripRowSpacing(
    triggerRadius: Float,
    cardScale: Float,
): Float =
    triggerRadius *
        SCROLL_STRIP_ROW_SPACING_RADII *
        cardScale.coerceIn(MIN_SCROLL_STRIP_CARD_SCALE, MAX_SCROLL_STRIP_CARD_SCALE) /
        LEGACY_SCROLL_STRIP_CARD_SCALE

internal fun scrollStripBaseTextSize(
    triggerRadius: Float,
    cardScale: Float,
): Float =
    triggerRadius *
        0.42f *
        cardScale.coerceIn(MIN_SCROLL_STRIP_CARD_SCALE, MAX_SCROLL_STRIP_CARD_SCALE) /
        LEGACY_SCROLL_STRIP_CARD_SCALE

internal data class ScrollStripCardSize(
    val width: Float,
    val height: Float,
)

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

internal fun scrollStripTouchingOffsets(
    cards: List<ScrollStripCardSize>,
    scales: FloatArray,
    fractionalIndex: Float,
    rotationDegrees: Float,
    vertical: Boolean,
): FloatArray {
    if (cards.isEmpty()) return FloatArray(0)
    require(scales.size == cards.size)

    val radians = Math.toRadians(rotationDegrees.toDouble())
    val localWidthPerMain = abs(if (vertical) sin(radians) else cos(radians)).toFloat()
    val localHeightPerMain = abs(if (vertical) cos(radians) else sin(radians)).toFloat()
    val centers = FloatArray(cards.size)
    for (index in 1 until cards.size) {
        val left = cards[index - 1]
        val right = cards[index]
        val widthSum = left.width * scales[index - 1] + right.width * scales[index]
        val heightSum = left.height * scales[index - 1] + right.height * scales[index]
        val widthContact =
            if (localWidthPerMain > 1e-6f) widthSum / (2f * localWidthPerMain) else Float.POSITIVE_INFINITY
        val heightContact =
            if (localHeightPerMain > 1e-6f) heightSum / (2f * localHeightPerMain) else Float.POSITIVE_INFINITY
        centers[index] = centers[index - 1] + min(widthContact, heightContact)
    }

    val clampedIndex = fractionalIndex.coerceIn(0f, cards.lastIndex.toFloat())
    val lowerIndex = floor(clampedIndex).toInt()
    val fraction = clampedIndex - lowerIndex
    val focus =
        if (lowerIndex == cards.lastIndex) {
            centers[lowerIndex]
        } else {
            centers[lowerIndex] + (centers[lowerIndex + 1] - centers[lowerIndex]) * fraction
        }
    return FloatArray(cards.size) { centers[it] - focus }
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

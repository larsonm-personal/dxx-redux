package com.dxxredux.app

private val AUTOMAP_TOUCH_MOVEMENT_BUTTONS =
    setOf(
        TouchBindings.BTN_ACCELERATE,
        TouchBindings.BTN_REVERSE,
        TouchBindings.BTN_SLIDE_ON,
        TouchBindings.BTN_SLIDE_LEFT,
        TouchBindings.BTN_SLIDE_RIGHT,
        TouchBindings.BTN_SLIDE_UP,
        TouchBindings.BTN_SLIDE_DOWN,
        TouchBindings.BTN_BANK_ON,
        TouchBindings.BTN_BANK_LEFT,
        TouchBindings.BTN_BANK_RIGHT,
    )

internal fun automapTouchButtonVisible(binding: Int): Boolean =
    binding == TouchBindings.BTN_AUTOMAP || binding in AUTOMAP_TOUCH_MOVEMENT_BUTTONS

internal fun automapTouchActions(
    markerCount: Int,
    includeMarkers: Boolean,
): List<RemainingTouchAction> =
    buildList {
        add(
            RemainingTouchAction(
                label = "Recenter Map",
                adminAction = TouchOverlayView.ADMIN_AUTOMAP_RECENTER,
            ),
        )
        if (includeMarkers) {
            for (idx in 0 until markerCount.coerceIn(0, 10)) {
                add(
                    RemainingTouchAction(
                        label = "Jump to Marker ${idx + 1}",
                        adminAction = TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE + idx,
                    ),
                )
            }
        }
    }

internal fun automapMarkerAdminActionIndex(action: Int): Int? {
    val idx = action - TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE
    return idx.takeIf { it in 0 until 10 }
}

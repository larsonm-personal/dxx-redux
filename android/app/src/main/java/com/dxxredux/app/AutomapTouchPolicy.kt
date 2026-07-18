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

private const val AUTOMAP_TOUCH_MARKER_SLOT_COUNT = 9

internal enum class AutomapMarkerMenuMode {
    ROOT,
    SET,
    JUMP,
}

private const val AUTOMAP_MARKER_SLOT_UNAVAILABLE = -1
private const val AUTOMAP_MARKER_SLOT_FREE = 0
private const val AUTOMAP_MARKER_SLOT_PLACED = 1

internal fun automapTouchButtonVisible(binding: Int): Boolean =
    binding == TouchBindings.BTN_AUTOMAP || binding in AUTOMAP_TOUCH_MOVEMENT_BUTTONS

internal fun automapTouchActions(
    includeMarkers: Boolean,
    markerMenuMode: AutomapMarkerMenuMode = AutomapMarkerMenuMode.ROOT,
    markerSlots: IntArray = IntArray(AUTOMAP_TOUCH_MARKER_SLOT_COUNT) { AUTOMAP_MARKER_SLOT_FREE },
    previewMode: Boolean = false,
): List<RemainingTouchAction> =
    buildList {
        add(
            RemainingTouchAction(
                label = if (previewMode) "Close Preview" else "Close Map",
                adminAction = TouchOverlayView.ADMIN_AUTOMAP,
            ),
        )
        val showMarkers = includeMarkers && !previewMode
        if (!showMarkers || markerMenuMode == AutomapMarkerMenuMode.ROOT) {
            add(
                RemainingTouchAction(
                    label = "Recenter Map",
                    adminAction = TouchOverlayView.ADMIN_AUTOMAP_RECENTER,
                ),
            )
        }
        if (!showMarkers) {
            return@buildList
        }

        when (markerMenuMode) {
            AutomapMarkerMenuMode.ROOT -> {
                add(
                    RemainingTouchAction(
                        label = "Drop Marker",
                        adminAction = TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_MENU,
                    ),
                )
                add(
                    RemainingTouchAction(
                        label = "Name Marker",
                        adminAction = TouchOverlayView.ADMIN_AUTOMAP_NAME_MARKER,
                    ),
                )
                add(
                    RemainingTouchAction(
                        label = "Jump to Marker",
                        adminAction = TouchOverlayView.ADMIN_AUTOMAP_JUMP_MARKER_MENU,
                    ),
                )
            }

            AutomapMarkerMenuMode.SET -> {
                add(
                    RemainingTouchAction(
                        label = "Back",
                        adminAction = TouchOverlayView.ADMIN_AUTOMAP_MARKER_MENU_ROOT,
                    ),
                )
                for (idx in markerSlots.indicesForStatus(AUTOMAP_MARKER_SLOT_FREE)) {
                    add(
                        RemainingTouchAction(
                            label = "Drop Marker ${idx + 1}",
                            adminAction = TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_BASE + idx,
                        ),
                    )
                }
            }

            AutomapMarkerMenuMode.JUMP -> {
                add(
                    RemainingTouchAction(
                        label = "Back",
                        adminAction = TouchOverlayView.ADMIN_AUTOMAP_MARKER_MENU_ROOT,
                    ),
                )
                for (idx in markerSlots.indicesForStatus(AUTOMAP_MARKER_SLOT_PLACED)) {
                    add(
                        RemainingTouchAction(
                            label = "Jump to Marker ${idx + 1}",
                            adminAction = TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE + idx,
                        ),
                    )
                }
            }
        }
    }

private fun IntArray.indicesForStatus(status: Int): List<Int> =
    (0 until AUTOMAP_TOUCH_MARKER_SLOT_COUNT).filter { idx ->
        getOrElse(idx) { AUTOMAP_MARKER_SLOT_UNAVAILABLE } == status
    }

internal fun automapMarkerAdminActionIndex(action: Int): Int? {
    val idx = action - TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE
    return idx.takeIf { it in 0 until AUTOMAP_TOUCH_MARKER_SLOT_COUNT }
}

internal fun automapSetMarkerAdminActionIndex(action: Int): Int? {
    val idx = action - TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_BASE
    return idx.takeIf { it in 0 until AUTOMAP_TOUCH_MARKER_SLOT_COUNT }
}

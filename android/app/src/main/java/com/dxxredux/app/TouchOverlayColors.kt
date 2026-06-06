package com.dxxredux.app

internal const val TOUCH_ACTIVE_HIGHLIGHT_RGB = 0x36BC40
internal const val TOUCH_ACTIVE_BUTTON_ALPHA = 0x88
internal const val TOUCH_ACTIVE_SUBTLE_ALPHA = 0x66
internal const val TOUCH_ACTIVE_SLIDER_ALPHA = 0xFF
internal const val TOUCH_ACTIVE_SLIDER_FILL_ALPHA = 0xFF
internal const val TOUCH_ACTIVE_OPAQUE_ALPHA = 0xFF

internal fun touchActiveHighlightColor(alpha: Int): Int = ((alpha and 0xFF) shl 24) or TOUCH_ACTIVE_HIGHLIGHT_RGB

internal val controllerMenuFocusColor: Int = touchActiveHighlightColor(TOUCH_ACTIVE_OPAQUE_ALPHA)

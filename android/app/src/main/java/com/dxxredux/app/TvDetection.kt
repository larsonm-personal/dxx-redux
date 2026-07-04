package com.dxxredux.app

import android.app.UiModeManager
import android.content.Context
import android.content.pm.PackageManager
import android.content.res.Configuration

fun Context.hasTouchscreen(): Boolean = packageManager.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)

fun Context.isAndroidTv(): Boolean {
    val uiModeManager = getSystemService(Context.UI_MODE_SERVICE) as? UiModeManager
    if (uiModeManager?.currentModeType == Configuration.UI_MODE_TYPE_TELEVISION) {
        return true
    }
    return packageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK)
}

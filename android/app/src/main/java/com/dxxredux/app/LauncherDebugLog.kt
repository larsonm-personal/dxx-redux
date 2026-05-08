package com.dxxredux.app

import android.content.Context
import android.util.Log

object LauncherDebugLog {
    private const val TAG = "DXX-Launcher"

    @JvmStatic
    fun isEnabled(context: Context): Boolean = DebugLog.isCategoryEnabled(context, DebugLogCategory.LAUNCHER)

    @JvmStatic
    fun log(message: String) {
        DebugLog.log(DebugLogCategory.LAUNCHER, message)
        Log.i(TAG, message)
    }
}

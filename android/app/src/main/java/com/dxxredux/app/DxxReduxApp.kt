package com.dxxredux.app

import android.app.Application
import android.content.Context
import android.util.Log
import androidx.core.content.pm.PackageInfoCompat
import xcrash.ICrashCallback
import xcrash.XCrash

class DxxReduxApp : Application() {
    override fun attachBaseContext(base: Context) {
        super.attachBaseContext(base)

        val appContext = applicationContext ?: base.applicationContext ?: base
        val logDir = CrashLog.getTombstoneDir(appContext)
        logDir.mkdirs()
        val crashCallback =
            ICrashCallback { logPath, emergency ->
                CrashLog.appendXCrashSections(appContext, logPath, emergency)
            }

        val result =
            XCrash.init(
                appContext,
                XCrash
                    .InitParameters()
                    .setAppVersion(buildXCrashVersion(appContext))
                    .setLogDir(logDir.absolutePath)
                    .setJavaLogCountMax(5)
                    .setNativeLogCountMax(5)
                    .setAnrLogCountMax(3)
                    .setJavaCallback(crashCallback)
                    .setNativeCallback(crashCallback)
                    .setAnrCallback(crashCallback),
            )

        if (result != 0) {
            Log.w(TAG, "xCrash init returned $result")
        }
    }

    private fun buildXCrashVersion(context: Context): String {
        val packageInfo =
            try {
                context.packageManager.getPackageInfo(context.packageName, 0)
            } catch (_: Exception) {
                null
            }
        val versionName = packageInfo?.versionName?.takeUnless { it.isNullOrBlank() } ?: "unknown"
        val versionCode = packageInfo?.let { PackageInfoCompat.getLongVersionCode(it).toString() } ?: "unknown"

        return buildString {
            append("$versionName ($versionCode) ")
            append("${BuildInfo.GIT_COMMIT_COUNT} (${BuildInfo.GIT_SHORT_HASH}) ")
            append(BuildInfo.BUILD_TYPE)
        }
    }

    companion object {
        private const val TAG = "DxxReduxApp"
    }
}

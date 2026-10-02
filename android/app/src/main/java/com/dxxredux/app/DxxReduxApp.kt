package com.dxxredux.app

import android.app.Application
import android.content.Context
import android.util.Log
import androidx.core.content.pm.PackageInfoCompat
import xcrash.ICrashCallback
import xcrash.XCrash

class DxxReduxApp : Application() {
    override fun onCreate() {
        super.onCreate()
        NativeGraphicsSafety.install()
    }

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
                this,
                XCrash
                    .InitParameters()
                    .setAppVersion(buildXCrashVersion(appContext))
                    .setLogDir(logDir.absolutePath)
                    .setJavaLogCountMax(CrashLog.MAX_FILES)
                    .setNativeLogCountMax(CrashLog.MAX_FILES)
                    .setAnrLogCountMax(CrashLog.MAX_FILES)
                    .setJavaCallback(crashCallback)
                    .setNativeCallback(crashCallback)
                    .setAnrCallback(crashCallback),
            )

        if (result != 0) {
            Log.w(TAG, "xCrash init returned $result")
        }
        // Also clean up reports written by native exits that bypass the xCrash callback
        CrashLog.pruneOldFiles(logDir)
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
            append(" ${AppBuildDetails.distributionLabel}")
            // xCrash also uses this value in filenames, including exported reports
            append(" built=${BuildInfo.BUILD_DATE}T${BuildInfo.BUILD_TIME.replace(":", "").replace(" ", "")}")
            append(" minsdk=${BuildConfig.MIN_SDK} targetsdk=${BuildConfig.TARGET_SDK}")
        }
    }

    companion object {
        private const val TAG = "DxxReduxApp"
    }
}

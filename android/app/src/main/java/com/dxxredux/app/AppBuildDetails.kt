package com.dxxredux.app

/** Build-time distribution identity, shared by About and diagnostic headers */
internal object AppBuildDetails {
    val distributionLabel: String =
        when {
            BuildConfig.LEGACY_RELEASE -> "Sideload (GitHub, legacy)"
            BuildConfig.GITHUB_RELEASE -> "Sideload (GitHub)"
            else -> "Play Store"
        }
    val sdkLabel: String = "minsdk: api ${BuildConfig.MIN_SDK}, targetsdk: api ${BuildConfig.TARGET_SDK}"

    fun aboutText(): String =
        "Distribution: $distributionLabel\n" +
            "Version: ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})\n" +
            "$sdkLabel\n" +
            "Date: ${BuildInfo.BUILD_DATE} ${BuildInfo.BUILD_TIME}"

    fun diagnosticHeader(): String =
        "Package: ${BuildConfig.APPLICATION_ID}\n" +
            "Distribution: $distributionLabel\n" +
            "App version: ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})\n" +
            "Build: ${BuildInfo.GIT_COMMIT_COUNT} (${BuildInfo.GIT_SHORT_HASH}) ${BuildInfo.BUILD_TYPE}\n" +
            "Built: ${BuildInfo.BUILD_DATE} ${BuildInfo.BUILD_TIME}\n" +
            "$sdkLabel\n"
}

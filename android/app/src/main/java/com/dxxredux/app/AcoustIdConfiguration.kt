package com.dxxredux.app

import org.json.JSONObject

internal enum class AcoustIdConfigurationStatus(
    val isAvailable: Boolean,
    val message: String,
) {
    AVAILABLE(true, "AcoustID web lookups are available"),
    NOT_PACKAGED(false, "AcoustID web lookups are unavailable in this build"),
    INVALID(false, "The packaged AcoustID configuration is invalid"),
}

internal object AcoustIdConfiguration {
    fun parseApiKey(raw: String): String? {
        val cfg = JSONObject(Json5.strip(raw))
        val value = cfg.opt("api_key") as? String ?: return null
        return value.takeIf {
            it.matches(Regex("[A-Za-z0-9]{8,64}")) &&
                it != "YOUR_ACOUSTID_API_KEY_HERE"
        }
    }
}

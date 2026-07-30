package com.dxxredux.app

import android.content.Context
import android.util.Log
import kotlinx.coroutines.delay
import okhttp3.FormBody
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.io.FileNotFoundException
import java.util.concurrent.TimeUnit

/**
 * Optional AcoustID web lookup for tracks not matched by the local
 * fingerprint database.  Off by default -- requires an API key in
 * acoustid_config.json5.
 *
 * Rate limited to 3 requests/second (350ms minimum between calls)
 * with exponential backoff on HTTP 429 or server errors.
 */
object AcoustIdClient {
    private const val TAG = "DXX-AcoustID"
    private const val LOOKUP_URL = "https://api.acoustid.org/v2/lookup"
    private const val MIN_DELAY_MS = 350L
    private const val MAX_RETRIES = 3
    private const val INITIAL_BACKOFF_MS = 1000L

    private var apiKey: String? = null
    private var lastRequestTimeMs: Long = 0L

    private val httpClient =
        OkHttpClient
            .Builder()
            .connectTimeout(15, TimeUnit.SECONDS)
            .readTimeout(15, TimeUnit.SECONDS)
            .build()

    /** Load the generated acoustid_config.json5 asset and report why it is unavailable. */
    internal fun configure(context: Context): AcoustIdConfigurationStatus {
        if (apiKey != null) return AcoustIdConfigurationStatus.AVAILABLE
        return try {
            val raw =
                context.assets
                    .open("acoustid_config.json5")
                    .bufferedReader()
                    .use { it.readText() }
            apiKey = AcoustIdConfiguration.parseApiKey(raw)
            if (apiKey == null) {
                AcoustIdConfigurationStatus.INVALID
            } else {
                AcoustIdConfigurationStatus.AVAILABLE
            }
        } catch (_: FileNotFoundException) {
            AcoustIdConfigurationStatus.NOT_PACKAGED
        } catch (_: Exception) {
            AcoustIdConfigurationStatus.INVALID
        }
    }

    /** True if an API key is configured and lookups are available. */
    fun isAvailable(): Boolean = apiKey != null

    /**
     * Look up a fingerprint on AcoustID.
     * Returns a source-validated label with AcoustID evidence, or null if no match.
     *
     * Suspends to enforce rate limiting (350ms between calls).
     * Retries with exponential backoff on 429/5xx errors.
     */
    internal suspend fun lookupFingerprint(
        fingerprint: String,
        durationSec: Int,
        maintainedLabel: String,
    ): AcoustIdLabelMatch? {
        val key = apiKey ?: return null

        // Rate limit
        val now = System.currentTimeMillis()
        val elapsed = now - lastRequestTimeMs
        if (elapsed < MIN_DELAY_MS) {
            delay(MIN_DELAY_MS - elapsed)
        }

        var backoffMs = INITIAL_BACKOFF_MS

        for (attempt in 0..MAX_RETRIES) {
            lastRequestTimeMs = System.currentTimeMillis()
            try {
                val body =
                    FormBody
                        .Builder()
                        .add("client", key)
                        .add("meta", "recordings releases")
                        .add("duration", durationSec.toString())
                        .add("fingerprint", fingerprint)
                        .build()
                val request =
                    Request
                        .Builder()
                        .url(LOOKUP_URL)
                        .post(body)
                        .build()
                val response = httpClient.newCall(request).execute()
                val responseBody = response.body.string()

                if (response.code == 429 || response.code >= 500) {
                    Log.w(TAG, "HTTP ${response.code}, backing off ${backoffMs}ms")
                    delay(backoffMs)
                    backoffMs *= 2
                    continue
                }

                if (!response.isSuccessful) return null

                val json = JSONObject(responseBody)
                if (json.optString("status") != "ok") {
                    val errorMsg = json.optJSONObject("error")?.optString("message") ?: ""
                    if (errorMsg.contains("rate", ignoreCase = true) ||
                        errorMsg.contains("limit", ignoreCase = true)
                    ) {
                        Log.w(TAG, "Rate limited: $errorMsg, backing off ${backoffMs}ms")
                        delay(backoffMs)
                        backoffMs *= 2
                        continue
                    }
                    return null
                }

                return AcoustIdLabelPolicy.select(json, maintainedLabel)
            } catch (e: Exception) {
                Log.w(TAG, "Lookup failed (attempt $attempt): ${e.message}")
                if (attempt < MAX_RETRIES) {
                    delay(backoffMs)
                    backoffMs *= 2
                } else {
                    return null
                }
            }
        }
        return null
    }
}

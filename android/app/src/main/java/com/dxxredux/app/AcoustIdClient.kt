package com.dxxredux.app

import android.content.Context
import android.util.Log
import kotlinx.coroutines.delay
import okhttp3.FormBody
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
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

    /** Load API key from acoustid_config.json5 asset. Returns true if key found. */
    fun configure(context: Context): Boolean {
        if (apiKey != null) return true
        return try {
            val raw =
                context.assets
                    .open("acoustid_config.json5")
                    .bufferedReader()
                    .readText()
            val cfg = JSONObject(Json5.strip(raw))
            apiKey = cfg.optString("api_key").takeIf { it.isNotEmpty() }
            apiKey != null
        } catch (_: Exception) {
            false
        }
    }

    /** True if an API key is configured and lookups are available. */
    fun isAvailable(): Boolean = apiKey != null

    /**
     * Look up a fingerprint on AcoustID.
     * Returns "Artist - Title" or just "Title", or null if no match.
     *
     * Suspends to enforce rate limiting (350ms between calls).
     * Retries with exponential backoff on 429/5xx errors.
     */
    suspend fun lookupFingerprint(
        fingerprint: String,
        durationSec: Int,
    ): String? {
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
                        .add("meta", "recordings")
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

                val results = json.optJSONArray("results") ?: return null
                for (i in 0 until results.length()) {
                    val result = results.getJSONObject(i)
                    val recordings = result.optJSONArray("recordings") ?: continue
                    for (j in 0 until recordings.length()) {
                        val rec = recordings.getJSONObject(j)
                        val title = rec.optString("title").takeIf { it.isNotEmpty() } ?: continue
                        val artists = rec.optJSONArray("artists")
                        val artistName =
                            if (artists != null && artists.length() > 0) {
                                (0 until artists.length())
                                    .map { artists.getJSONObject(it).optString("name") }
                                    .filter { it.isNotEmpty() }
                                    .joinToString(", ")
                            } else {
                                ""
                            }
                        return if (artistName.isNotEmpty()) "$artistName - $title" else title
                    }
                }
                return null
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

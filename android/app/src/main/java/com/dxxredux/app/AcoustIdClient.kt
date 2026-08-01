package com.dxxredux.app

import android.content.Context
import android.util.Log
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.suspendCancellableCoroutine
import okhttp3.Call
import okhttp3.Callback
import okhttp3.FormBody
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import org.json.JSONObject
import java.io.FileNotFoundException
import java.io.IOException
import java.util.concurrent.TimeUnit
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

internal sealed interface AcoustIdLookupResult {
    data class Match(
        val match: AcoustIdLabelMatch,
    ) : AcoustIdLookupResult

    data object NoMatch : AcoustIdLookupResult

    data class RetryableFailure(
        val reason: String,
    ) : AcoustIdLookupResult

    data class ConfigurationFailure(
        val reason: String,
    ) : AcoustIdLookupResult
}

internal data class AcoustIdHttpResponse(
    val code: Int,
    val body: String,
)

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

    private val httpClient by lazy {
        OkHttpClient
            .Builder()
            .connectTimeout(15, TimeUnit.SECONDS)
            .readTimeout(15, TimeUnit.SECONDS)
            .build()
    }

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
     * Returns a typed result that keeps authoritative empty results separate from failures.
     *
     * Suspends to enforce rate limiting (350ms between calls).
     * Retries with exponential backoff on 429/5xx errors.
     */
    internal suspend fun lookupFingerprint(
        fingerprint: String,
        durationSec: Int,
        maintainedLabel: String,
    ): AcoustIdLookupResult {
        val key = apiKey ?: return AcoustIdLookupResult.ConfigurationFailure("API key unavailable")

        // Rate limit
        val now = System.currentTimeMillis()
        val elapsed = now - lastRequestTimeMs
        if (elapsed < MIN_DELAY_MS) {
            delay(MIN_DELAY_MS - elapsed)
        }

        return lookupWithTransport(
            maintainedLabel = maintainedLabel,
            transport = {
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
                executeRequest(request)
            },
        )
    }

    internal suspend fun lookupWithTransport(
        maintainedLabel: String,
        transport: suspend () -> AcoustIdHttpResponse,
        wait: suspend (Long) -> Unit = { delay(it) },
        logWarning: (String) -> Unit = { Log.w(TAG, it) },
    ): AcoustIdLookupResult {
        var backoffMs = INITIAL_BACKOFF_MS
        var lastFailure: AcoustIdLookupResult.RetryableFailure? = null
        for (attempt in 0..MAX_RETRIES) {
            lastRequestTimeMs = System.currentTimeMillis()
            val outcome =
                try {
                    classifyResponse(transport(), maintainedLabel)
                } catch (e: CancellationException) {
                    throw e
                } catch (e: Exception) {
                    AcoustIdLookupResult.RetryableFailure(e.message ?: e.javaClass.simpleName)
                }
            currentCoroutineContext().ensureActive()
            if (outcome !is AcoustIdLookupResult.RetryableFailure) return outcome
            lastFailure = outcome
            logWarning("Lookup failed (attempt ${attempt + 1}): ${outcome.reason}")
            if (attempt < MAX_RETRIES) {
                try {
                    wait(backoffMs)
                } catch (e: CancellationException) {
                    throw e
                }
                backoffMs *= 2
            }
        }
        return lastFailure ?: AcoustIdLookupResult.RetryableFailure("Lookup exhausted")
    }

    internal fun classifyResponse(
        response: AcoustIdHttpResponse,
        maintainedLabel: String,
    ): AcoustIdLookupResult {
        if (response.code == 429 || response.code >= 500) {
            return AcoustIdLookupResult.RetryableFailure("HTTP ${response.code}")
        }
        if (response.code !in 200..299) {
            return AcoustIdLookupResult.ConfigurationFailure("HTTP ${response.code}")
        }
        val json =
            try {
                JSONObject(response.body)
            } catch (e: Exception) {
                return AcoustIdLookupResult.RetryableFailure("Malformed response: ${e.message}")
            }
        if (json.optString("status") != "ok") {
            val errorMessage = json.optJSONObject("error")?.optString("message").orEmpty()
            return if (errorMessage.contains("rate", ignoreCase = true) ||
                errorMessage.contains("limit", ignoreCase = true)
            ) {
                AcoustIdLookupResult.RetryableFailure(errorMessage.ifBlank { "API rate limit" })
            } else {
                AcoustIdLookupResult.ConfigurationFailure(errorMessage.ifBlank { "API error" })
            }
        }
        val results =
            json.optJSONArray("results")
                ?: return AcoustIdLookupResult.RetryableFailure("Response omitted results")
        if (results.length() == 0) return AcoustIdLookupResult.NoMatch
        return AcoustIdLabelPolicy.select(json, maintainedLabel)?.let(AcoustIdLookupResult::Match)
            ?: AcoustIdLookupResult.NoMatch
    }

    private suspend fun executeRequest(request: Request): AcoustIdHttpResponse =
        suspendCancellableCoroutine { continuation ->
            val call = httpClient.newCall(request)
            continuation.invokeOnCancellation { call.cancel() }
            call.enqueue(
                object : Callback {
                    override fun onFailure(
                        call: Call,
                        e: IOException,
                    ) {
                        if (continuation.isActive) continuation.resumeWithException(e)
                    }

                    override fun onResponse(
                        call: Call,
                        response: Response,
                    ) {
                        try {
                            response.use {
                                val result = AcoustIdHttpResponse(it.code, it.body.string())
                                if (continuation.isActive) continuation.resume(result)
                            }
                        } catch (e: Exception) {
                            if (continuation.isActive) continuation.resumeWithException(e)
                        }
                    }
                },
            )
        }
}

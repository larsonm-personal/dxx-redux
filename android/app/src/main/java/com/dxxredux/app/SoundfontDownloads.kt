package com.dxxredux.app

import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.suspendCancellableCoroutine
import okhttp3.Call
import okhttp3.Callback
import okhttp3.HttpUrl.Companion.toHttpUrl
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import org.json.JSONObject
import java.io.File
import java.io.FilterInputStream
import java.io.IOException
import java.util.concurrent.TimeUnit
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

data class SoundfontDownload(
    val name: String,
    val url: String,
    val description: String,
    val websiteUrl: String,
    val license: String,
) {
    init {
        require(
            name.isNotBlank() && description.isNotBlank() && license.isNotBlank(),
        ) { "Incomplete soundfont information" }
        require(url.toHttpUrl().isHttps && websiteUrl.toHttpUrl().isHttps) { "Soundfont URLs must use HTTPS" }
    }

    internal fun toJson(): JSONObject =
        JSONObject()
            .put("name", name)
            .put("url", url)
            .put("description", description)
            .put("websiteUrl", websiteUrl)
            .put("license", license)

    companion object {
        internal fun fromJson(value: JSONObject): SoundfontDownload =
            SoundfontDownload(
                value.getString("name"),
                value.getString("url"),
                value.getString("description"),
                value.getString("websiteUrl"),
                value.getString("license"),
            )
    }
}

/** Downloads stay separate from activation so cancellation never changes preferences. */
internal class SoundfontDownloader(
    private val client: OkHttpClient = defaultClient,
) {
    suspend fun download(
        entry: SoundfontDownload,
        store: SoundfontStore,
        requireSpace: (Long) -> Unit,
        validate: (File) -> Boolean,
        onProgress: (Long, Long?) -> Unit,
    ): SoundfontStore.Font =
        suspendCancellableCoroutine { continuation ->
            val call = client.newCall(Request.Builder().url(entry.url).build())
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
                            val font =
                                response.use {
                                    continuation.context.ensureActive()
                                    if (response.code !=
                                        200
                                    ) {
                                        throw IOException("Soundfont download failed (HTTP ${response.code})")
                                    }
                                    val body = response.body
                                    val length = body.contentLength().takeIf { it >= 0 }
                                    if (length != null && length > SoundfontStore.MAX_BYTES) {
                                        throw IOException("Soundfonts must be 64 MiB or smaller")
                                    }
                                    requireSpace(length ?: SoundfontStore.MAX_BYTES)
                                    var received = 0L
                                    val input =
                                        object : FilterInputStream(body.byteStream()) {
                                            override fun read(
                                                buffer: ByteArray,
                                                offset: Int,
                                                count: Int,
                                            ): Int {
                                                continuation.context.ensureActive()
                                                val n = super.read(buffer, offset, count)
                                                continuation.context.ensureActive()
                                                if (n > 0) {
                                                    received += n
                                                    onProgress(received, length)
                                                }
                                                return n
                                            }
                                        }
                                    store.import(input, entry.name, entry) { file ->
                                        continuation.context.ensureActive()
                                        if (length != null &&
                                            received != length
                                        ) {
                                            throw IOException("The download was incomplete")
                                        }
                                        validate(file).also { continuation.context.ensureActive() }
                                    }
                                }
                            if (continuation.isActive) continuation.resume(font)
                        } catch (e: Exception) {
                            if (continuation.isActive) continuation.resumeWithException(e)
                        }
                    }
                },
            )
        }

    companion object {
        private val defaultClient by lazy {
            OkHttpClient
                .Builder()
                .connectTimeout(15, TimeUnit.SECONDS)
                .readTimeout(30, TimeUnit.SECONDS)
                .callTimeout(5, TimeUnit.MINUTES)
                .followRedirects(true)
                .followSslRedirects(false)
                .build()
        }
    }
}

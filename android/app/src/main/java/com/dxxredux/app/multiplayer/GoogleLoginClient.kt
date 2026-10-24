package com.dxxredux.app.multiplayer

import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import okhttp3.Call
import okhttp3.Callback
import okhttp3.HttpUrl
import okhttp3.HttpUrl.Companion.toHttpUrl
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import okio.Buffer
import java.io.IOException
import java.security.MessageDigest
import java.util.concurrent.TimeUnit
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

/** Must match server/src/google_login.rs; the selected destination owns the nonce */
internal fun googleLoginOrigin(webSocketUrl: String): String {
    require(webSocketUrl.startsWith("wss://")) { "Google sign-in requires TLS" }
    val url = ("https://" + webSocketUrl.removePrefix("wss://")).toHttpUrl()
    require(url.username.isEmpty() && url.password.isEmpty() && url.query == null && url.fragment == null)
    require(url.encodedPath == "/ws" || url.encodedPath == "/")
    val host = if (url.host.contains(':')) "[${url.host}]" else url.host
    return "https://$host:${url.port}"
}

internal fun googleLoginNonce(
    origin: String,
    challenge: String,
): String {
    require(challenge.length == 64 && challenge.all { it in '0'..'9' || it in 'a'..'f' })
    val bytes =
        MessageDigest
            .getInstance("SHA-256")
            .digest("dxx-google-login-v1\n$origin\n$challenge".toByteArray(Charsets.UTF_8))
    return bytes.joinToString("") { "%02x".format(it.toInt() and 255) }
}

/** Deliberately not a data class: diagnostics must never stringify the bearer credential */
internal class GoogleServerSession(
    val origin: String,
    val credential: String,
    val expiresNanos: Long,
) {
    fun validFor(selectedOrigin: String): Boolean = origin == selectedOrigin && System.nanoTime() < expiresNanos
}

internal class GoogleLoginExpiredException :
    Exception("Sign-in timed out; choose Connect and select your account again")

internal enum class GoogleProviderFailure(
    val userMessage: String,
) {
    NO_CREDENTIAL(
        "No Google account is available for sign-in. Add or sign in to a Google account in Android Settings, then choose Connect again",
    ),
    CANCELLED("Google sign-in was cancelled or could not obtain permission; choose Connect to try again"),
    INTERRUPTED("Google sign-in was interrupted; choose Connect to try again"),
    UNAVAILABLE(
        "Google sign-in is unavailable. Check that Google Play Services is enabled and up to date, then choose Connect again",
    ),
    UNKNOWN(
        "Google could not complete sign-in. Check your internet connection and Google Play Services, then choose Connect again",
    ),
}

internal class GoogleProviderException(
    val failure: GoogleProviderFailure,
) : Exception(failure.userMessage)

internal fun googleLoginFailureMessage(error: Exception): String =
    when (error) {
        is GoogleLoginExpiredException -> "Sign-in timed out; choose Connect and select your account again"
        is GoogleProviderException -> error.failure.userMessage
        else -> "Google sign-in did not complete; choose Connect to try again"
    }

internal class GoogleLoginClient(
    webSocketUrl: String,
    private val http: OkHttpClient = defaultHttp,
    private val nanoTime: () -> Long = System::nanoTime,
) {
    val origin = googleLoginOrigin(webSocketUrl)
    private val base: HttpUrl = origin.toHttpUrl()

    @Serializable
    private class Challenge(
        val challenge: String,
        @SerialName("expires_in_seconds") val expires: Long,
    )

    @Serializable
    private class Exchange(
        val challenge: String,
        @SerialName("google_id_token") val token: String,
    )

    @Serializable
    private class Credential(
        @SerialName("session_token") val token: String,
        @SerialName("expires_in_seconds") val expires: Long,
    )

    @Serializable
    private class Logout(
        @SerialName("session_token") val token: String,
    )

    @Serializable
    private class Cancel(
        val challenge: String,
    )

    /** Human interaction is outside the five-second HTTP deadline */
    suspend fun signIn(provider: suspend (String) -> String): GoogleServerSession {
        val challengeStarted = nanoTime()
        val challenge = protocolJson.decodeFromString(Challenge.serializer(), post("challenge", "{}"))
        require(challenge.expires in 1..300)
        val nonce = googleLoginNonce(origin, challenge.challenge)
        try {
            val token = provider(nonce)
            require(token.length in 1..8192)
            if (nanoTime() - challengeStarted >= TimeUnit.SECONDS.toNanos(challenge.expires)) {
                throw GoogleLoginExpiredException()
            }
            val issuedAfter = System.nanoTime()
            val credential =
                protocolJson.decodeFromString(
                    Credential.serializer(),
                    post(
                        "google",
                        protocolJson.encodeToString(Exchange.serializer(), Exchange(challenge.challenge, token)),
                    ),
                )
            require(credential.token.length == 64 && credential.token.all { it in '0'..'9' || it in 'a'..'f' })
            require(credential.expires in 1..43200)
            return GoogleServerSession(
                origin,
                credential.token,
                issuedAfter + TimeUnit.SECONDS.toNanos(credential.expires),
            )
        } catch (error: Exception) {
            // The exchange may have issued a credential even if its response was lost
            // Cleanup has the same finite HTTP timeout and must survive job cancellation
            withContext(NonCancellable) {
                runCatching {
                    post("cancel", protocolJson.encodeToString(Cancel.serializer(), Cancel(challenge.challenge)))
                }
            }
            throw error
        }
    }

    suspend fun revoke(session: GoogleServerSession) {
        require(session.origin == origin)
        post("logout", protocolJson.encodeToString(Logout.serializer(), Logout(session.credential)))
    }

    private suspend fun post(
        endpoint: String,
        json: String,
    ): String =
        suspendCancellableCoroutine { continuation ->
            val request =
                Request
                    .Builder()
                    .url(base.newBuilder().encodedPath("/api/v1/auth/$endpoint").build())
                    .post(json.toRequestBody("application/json".toMediaType()))
                    .build()
            val call = http.newCall(request)
            continuation.invokeOnCancellation { call.cancel() }
            call.enqueue(
                object : Callback {
                    override fun onFailure(
                        call: Call,
                        e: IOException,
                    ) {
                        continuation.resumeWithException(IOException("Matchmaking sign-in request failed"))
                    }

                    override fun onResponse(
                        call: Call,
                        response: Response,
                    ) {
                        try {
                            response.use {
                                check(it.isSuccessful) { "Matchmaking sign-in rejected (${it.code})" }
                                val buffer = Buffer()
                                val source = it.body.source()
                                while (buffer.size < 4097) {
                                    if (source.read(buffer, 4097 - buffer.size) == -1L) break
                                }
                                val bytes = buffer.readByteArray()
                                require(bytes.size <= 4096) { "Oversized sign-in response" }
                                continuation.resume(bytes.toString(Charsets.UTF_8))
                            }
                        } catch (error: Exception) {
                            continuation.resumeWithException(error)
                        }
                    }
                },
            )
        }

    companion object {
        private val defaultHttp =
            OkHttpClient
                .Builder()
                .followRedirects(false)
                .followSslRedirects(false)
                .callTimeout(5, TimeUnit.SECONDS)
                .connectTimeout(3, TimeUnit.SECONDS)
                .build()
    }
}

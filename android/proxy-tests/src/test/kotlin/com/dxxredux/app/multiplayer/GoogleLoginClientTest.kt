package com.dxxredux.app.multiplayer

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.awaitCancellation
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Protocol
import okhttp3.Response
import okhttp3.ResponseBody.Companion.toResponseBody
import okio.Buffer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.IOException
import java.util.Collections
import java.util.concurrent.TimeUnit

class GoogleLoginClientTest {
    @Test
    fun providerFailuresGiveFixedRecoveryMessagesWithoutLeakingExceptionDetails() {
        for (failure in GoogleProviderFailure.entries) {
            assertEquals(failure.userMessage, googleLoginFailureMessage(GoogleProviderException(failure)))
        }
        assertEquals(
            "Google sign-in did not complete; choose Connect to try again",
            googleLoginFailureMessage(IOException("private-token-and-account-details")),
        )
        assertEquals(
            "Sign-in timed out; choose Connect and select your account again",
            googleLoginFailureMessage(GoogleLoginExpiredException()),
        )
    }

    @Test
    fun expiredProviderInteractionIsSpecificAndCancelsWithoutTokenExchange() =
        runBlocking {
            val paths = Collections.synchronizedList(mutableListOf<String>())
            var now = 0L
            val http =
                OkHttpClient
                    .Builder()
                    .addInterceptor { chain ->
                        val request = chain.request()
                        val endpoint = request.url.pathSegments.last()
                        paths.add(endpoint)
                        Response
                            .Builder()
                            .request(request)
                            .protocol(Protocol.HTTP_1_1)
                            .code(200)
                            .message("OK")
                            .body(
                                (
                                    if (endpoint == "challenge") {
                                        "{\"challenge\":\"${"a".repeat(64)}\",\"expires_in_seconds\":300}"
                                    } else {
                                        ""
                                    }
                                ).toResponseBody("application/json".toMediaType()),
                            ).build()
                    }.build()
            try {
                val client = GoogleLoginClient("wss://match.example/ws", http) { now }
                val error =
                    runCatching {
                        client.signIn {
                            now = TimeUnit.SECONDS.toNanos(300)
                            "fixture-token"
                        }
                    }.exceptionOrNull()
                assertTrue(error is GoogleLoginExpiredException)
                assertEquals("Sign-in timed out; choose Connect and select your account again", error?.message)
                assertEquals(listOf("challenge", "cancel"), paths.toList())
            } finally {
                http.dispatcher.executorService.shutdown()
                http.connectionPool.evictAll()
            }
        }

    @Test
    fun cancellationAndLostExchangeResponseSendChallengeCleanup() =
        runBlocking {
            withTimeout(10000) {
                val paths = Collections.synchronizedList(mutableListOf<String>())
                val challenge = "a".repeat(64)
                val http =
                    OkHttpClient
                        .Builder()
                        .addInterceptor { chain ->
                            val request = chain.request()
                            val endpoint = request.url.pathSegments.last()
                            paths.add(endpoint)
                            when (endpoint) {
                                "google" -> {
                                    throw IOException("Simulated response loss after issuance")
                                }

                                "cancel" -> {
                                    val body = Buffer()
                                    request.body!!.writeTo(body)
                                    assertEquals("{\"challenge\":\"$challenge\"}", body.readUtf8())
                                }
                            }
                            Response
                                .Builder()
                                .request(request)
                                .protocol(Protocol.HTTP_1_1)
                                .code(200)
                                .message("OK")
                                .body(
                                    (
                                        if (endpoint == "challenge") {
                                            "{\"challenge\":\"$challenge\",\"expires_in_seconds\":300}"
                                        } else {
                                            ""
                                        }
                                    ).toResponseBody("application/json".toMediaType()),
                                ).build()
                        }.build()
                val client = GoogleLoginClient("wss://match.example/ws", http)
                val providerStarted = CompletableDeferred<Unit>()
                val job =
                    launch {
                        client.signIn {
                            providerStarted.complete(Unit)
                            awaitCancellation()
                        }
                    }
                providerStarted.await()
                job.cancelAndJoin()
                assertEquals(listOf("challenge", "cancel"), paths.toList())
                paths.clear()
                val error = runCatching { client.signIn { "fixture-token" } }.exceptionOrNull()
                assertTrue(error is IOException)
                assertEquals(listOf("challenge", "google", "cancel"), paths.toList())
                http.dispatcher.executorService.shutdown()
                http.connectionPool.evictAll()
            }
        }

    @Test
    fun originAndNonceMatchServerWireRule() {
        val origin = googleLoginOrigin("wss://MATCH.example/ws")
        assertEquals("https://match.example:443", origin)
        assertEquals(origin, googleLoginOrigin("wss://match.example:443/"))
        assertEquals("https://[::1]:444", googleLoginOrigin("wss://[::1]:444/ws"))
        assertEquals(
            "16fba402e45fb2f7f41ab19b676daeb91badc6d442b0998803804a9c0d898f5c",
            googleLoginNonce(origin, "a".repeat(64)),
        )
        assertNotEquals(
            googleLoginNonce(origin, "a".repeat(64)),
            googleLoginNonce("https://other.example:443", "a".repeat(64)),
        )
        for (url in listOf(
            "ws://match.example/ws",
            "wss://user@match.example/ws",
            "wss://match.example/path",
            "wss://match.example/ws?q=1",
            "wss://match.example/ws#fragment",
        )) {
            assertThrows(IllegalArgumentException::class.java) { googleLoginOrigin(url) }
        }
        assertThrows(IllegalArgumentException::class.java) { googleLoginNonce(origin, "short") }
    }

    @Test
    fun cachedSessionIsOriginBoundAndExpiresWithoutRenewal() {
        val origin = "https://match.example:443"
        val session = GoogleServerSession(origin, "secret", System.nanoTime() + TimeUnit.SECONDS.toNanos(10))
        assertTrue(session.validFor(origin))
        assertFalse(session.validFor("https://other.example:443"))
        assertFalse(GoogleServerSession(origin, "secret", System.nanoTime() - 1).validFor(origin))
        assertFalse(session.toString().contains("secret"))
    }
}

package com.dxxredux.app

import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.yield
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.IOException

class AcoustIdClientTest {
    @Test
    fun classifiesMatchAndAuthoritativeNoMatch() =
        runBlocking {
            val match = lookup(response(200, MATCH_RESPONSE))
            val empty = lookup(response(200, """{"status":"ok","results":[]}"""))

            assertTrue(match is AcoustIdLookupResult.Match)
            assertEquals("Artist - game01", (match as AcoustIdLookupResult.Match).match.name)
            assertSame(AcoustIdLookupResult.NoMatch, empty)
        }

    @Test
    fun retriesTransientHttpIoAndMalformedResponses() =
        runBlocking {
            val responses =
                ArrayDeque<Any>(
                    listOf(
                        response(429, ""),
                        IOException("offline"),
                        response(200, "not-json"),
                        response(200, MATCH_RESPONSE),
                    ),
                )
            var attempts = 0
            val result =
                AcoustIdClient.lookupWithTransport(
                    maintainedLabel = "game01.ogg",
                    transport = {
                        attempts++
                        when (val next = responses.removeFirst()) {
                            is IOException -> throw next
                            else -> next as AcoustIdHttpResponse
                        }
                    },
                    wait = {},
                    logWarning = {},
                )

            assertTrue(result is AcoustIdLookupResult.Match)
            assertEquals(4, attempts)
        }

    @Test
    fun exhaustedTransientFailureRemainsRetryable() =
        runBlocking {
            var attempts = 0
            val result =
                AcoustIdClient.lookupWithTransport(
                    maintainedLabel = "game01.ogg",
                    transport = {
                        attempts++
                        response(503, "")
                    },
                    wait = {},
                    logWarning = {},
                )

            assertTrue(result is AcoustIdLookupResult.RetryableFailure)
            assertEquals(4, attempts)
        }

    @Test
    fun configurationAndApiErrorsDoNotRetryInternally() =
        runBlocking {
            var attempts = 0
            val httpResult =
                AcoustIdClient.lookupWithTransport(
                    maintainedLabel = "game01.ogg",
                    transport = {
                        attempts++
                        response(401, "")
                    },
                    wait = {},
                    logWarning = {},
                )
            assertTrue(httpResult is AcoustIdLookupResult.ConfigurationFailure)
            assertEquals(1, attempts)

            val apiResult =
                lookup(
                    response(
                        200,
                        """{"status":"error","error":{"message":"invalid client key"}}""",
                    ),
                )
            assertTrue(apiResult is AcoustIdLookupResult.ConfigurationFailure)
        }

    @Test
    fun cancellationDuringRequestPropagatesWithoutRetry() {
        var started = false
        var cancelled = false
        runBlocking {
            val job =
                launch {
                    AcoustIdClient.lookupWithTransport(
                        maintainedLabel = "game01.ogg",
                        transport = {
                            suspendCancellableCoroutine<AcoustIdHttpResponse> { continuation ->
                                started = true
                                continuation.invokeOnCancellation { cancelled = true }
                            }
                        },
                        wait = {},
                        logWarning = {},
                    )
                }
            while (!started) yield()
            job.cancelAndJoin()
        }
        assertTrue(cancelled)
    }

    @Test
    fun cancellationDuringBackoffPropagatesWithoutAnotherRequest() {
        var attempts = 0
        var waiting = false
        runBlocking {
            val job =
                launch {
                    AcoustIdClient.lookupWithTransport(
                        maintainedLabel = "game01.ogg",
                        transport = {
                            attempts++
                            response(429, "")
                        },
                        wait = {
                            suspendCancellableCoroutine<Unit> { waiting = true }
                        },
                        logWarning = {},
                    )
                }
            while (!waiting) yield()
            job.cancelAndJoin()
        }
        assertEquals(1, attempts)
    }

    private suspend fun lookup(response: AcoustIdHttpResponse): AcoustIdLookupResult =
        AcoustIdClient.lookupWithTransport(
            maintainedLabel = "game01.ogg",
            transport = { response },
            wait = {},
            logWarning = {},
        )

    private fun response(
        code: Int,
        body: String,
    ) = AcoustIdHttpResponse(code, body)

    companion object {
        private const val MATCH_RESPONSE =
            """{"status":"ok","results":[{"score":0.95,"recordings":[{"id":"recording-1","title":"game01","artists":[{"name":"Artist"}]}]}]}"""
    }
}

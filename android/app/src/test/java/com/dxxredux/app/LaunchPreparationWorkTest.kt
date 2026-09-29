package com.dxxredux.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.async
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

class LaunchPreparationWorkTest {
    @Test
    fun timeoutInterruptsWorkerAndAllowsRetry() =
        runBlocking {
            val work = LaunchPreparationWork()
            val entered = CountDownLatch(1)
            val retired = CountDownLatch(1)
            val first =
                async(Dispatchers.Default) {
                    try {
                        work.run(500) {
                            entered.countDown()
                            try {
                                Thread.sleep(60_000)
                            } finally {
                                retired.countDown()
                            }
                        }
                        error("Expected preparation deadline")
                    } catch (_: TimeoutCancellationException) {
                    }
                }
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            first.await()
            assertTrue(retired.await(5, TimeUnit.SECONDS))
            assertEquals("retry", work.run(5_000) { "retry" })
        }

    @Test
    fun timedOutNativeWriterKeepsOwnershipUntilItActuallyReturns() =
        runBlocking {
            val work = LaunchPreparationWork()
            val entered = CountDownLatch(1)
            val release = CountDownLatch(1)
            val first =
                async(Dispatchers.Default) {
                    try {
                        work.run(500) {
                            entered.countDown()
                            while (release.count > 0) {
                                try {
                                    release.await()
                                } catch (_: InterruptedException) {
                                }
                            }
                            "obsolete result"
                        }
                        error("Obsolete result escaped its deadline")
                    } catch (_: TimeoutCancellationException) {
                    }
                }
            try {
                assertTrue(entered.await(5, TimeUnit.SECONDS))
                first.await()
                try {
                    work.run(100) { error("Replacement overlapped old writer") }
                    error("Expected waiting retry to time out")
                } catch (_: TimeoutCancellationException) {
                }
            } finally {
                release.countDown()
            }
            assertEquals("clean retry", work.run(5_000) { "clean retry" })
        }
}

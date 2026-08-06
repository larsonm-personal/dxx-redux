package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.file.Files
import java.util.Collections
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

class LevelMetadataWorkerIdentityTest {
    @Test
    fun identityRequiresTheExactRequestAndProcessGeneration() {
        val identity = LevelMetadataWorkerIdentity("request-a", 321, 9001)

        assertTrue(identity.matches("request-a", 321, 9001))
        assertFalse(identity.matches("request-b", 321, 9001))
        assertFalse(identity.matches("request-a", 654, 9001))
        assertFalse(identity.matches("request-a", 321, 9002))
        assertFalse(identity.matches("request-a", 321, null))
    }

    @Test
    fun identityJsonRoundTripsAndRejectsIncompleteRecords() {
        val identity = LevelMetadataWorkerIdentity("request-a", 321, 9001)

        assertEquals(identity, LevelMetadataWorkerIdentity.fromJson(identity.toJson()))
        assertNull(LevelMetadataWorkerIdentity.fromJson("{}"))
        assertNull(LevelMetadataWorkerIdentity.fromJson("not json"))
        assertNull(
            LevelMetadataWorkerIdentity.fromJson(
                """{"request_id":"request-a","pid":321,"process_start_ticks":-1}""",
            ),
        )
    }

    @Test
    fun procStatParserUsesStartTimeAfterTheCompleteCommandName() {
        val stat = "123 (worker name (nested)) S " + (1..19).joinToString(" ")

        assertEquals(19L, parseProcessStartTicks(stat))
        assertNull(parseProcessStartTicks("123 malformed"))
        assertNull(parseProcessStartTicks("123 (worker) S 1 2"))
    }

    @Test
    fun staleCleanupCannotClearANewerOwnerGeneration() {
        val directory = Files.createTempDirectory("level-metadata-owner").toFile()
        val ownerFile = directory.resolve("owner.json")
        val older = LevelMetadataWorkerIdentity("request-a", 321, 9001)
        val newer = LevelMetadataWorkerIdentity("request-b", 654, 9002)
        try {
            LevelMetadataWorkerOwnerStore.publish(ownerFile, older)
            assertEquals(older, LevelMetadataWorkerOwnerStore.read(ownerFile))

            LevelMetadataWorkerOwnerStore.publish(ownerFile, newer)
            LevelMetadataWorkerOwnerStore.clearIfOwned(ownerFile, older)
            assertEquals(newer, LevelMetadataWorkerOwnerStore.read(ownerFile))

            LevelMetadataWorkerOwnerStore.clearIfOwned(ownerFile, newer)
            assertNull(LevelMetadataWorkerOwnerStore.read(ownerFile))
        } finally {
            directory.deleteRecursively()
        }
    }

    @Test
    fun serviceStopsOnlyAfterEveryDeliveredStartCompletes() {
        val lifetime = LevelMetadataServiceLifetime()

        lifetime.started(1)
        lifetime.started(2)
        lifetime.started(3)
        assertNull(lifetime.completed())
        assertNull(lifetime.completed())
        assertEquals(3, lifetime.completed())
    }

    @Test
    fun laterStartSupersedesAPreviouslyDrainedStopId() {
        val lifetime = LevelMetadataServiceLifetime()

        lifetime.started(10)
        assertEquals(10, lifetime.completed())
        lifetime.started(11)
        assertEquals(11, lifetime.completed())
    }

    @Test
    fun serviceCommandQueueKeepsAcceptedWorkAliveAndRunsStartsInOrder() {
        val executor = Executors.newSingleThreadExecutor()
        val order = Collections.synchronizedList(mutableListOf<String>())
        val drained = Collections.synchronizedList(mutableListOf<Int>())
        val firstEntered = CountDownLatch(1)
        val releaseFirst = CountDownLatch(1)
        val queue = LevelMetadataServiceCommandQueue(drained::add, executor)

        queue.submit(1) {
            order += "a-start"
            firstEntered.countDown()
            releaseFirst.await()
            order += "a-end"
        }
        assertTrue(firstEntered.await(5, TimeUnit.SECONDS))
        queue.submit(2) { order += "b" }
        queue.submit(3) { order += "c" }
        assertTrue(drained.isEmpty())
        assertEquals(listOf("a-start"), order.toList())

        releaseFirst.countDown()
        queue.shutdown()
        assertTrue(executor.awaitTermination(5, TimeUnit.SECONDS))
        assertEquals(listOf("a-start", "a-end", "b", "c"), order.toList())
        assertEquals(listOf(3), drained.toList())
    }
}

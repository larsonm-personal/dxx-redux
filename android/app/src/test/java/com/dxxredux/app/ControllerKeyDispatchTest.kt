package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ControllerKeyDispatchTest {
    @Test
    fun releaseStaysWithPressWhenDestinationChanges() {
        val router = ControllerKeyDispatch()
        val edges = mutableListOf<String>()
        var destination = "game"

        fun press(repeat: Int = 0) =
            router.press(1, repeat, false) {
                val owner = destination
                val send: (Boolean) -> Unit = { down -> edges.add("$owner:$down") }
                send
            }
        press()
        destination = "menu"
        press()
        press(1)
        assertTrue(router.release(1))
        assertFalse(router.release(1))
        press()
        router.releaseAll()
        assertEquals(listOf("game:true", "game:false", "menu:true", "menu:false"), edges)
    }

    @Test
    fun directionsRepeatButDuplicateHardwareEdgesAndOrphanRepeatsDoNot() {
        val router = ControllerKeyDispatch()
        val edges = mutableListOf<Boolean>()

        fun press(repeat: Int) = router.press(22, repeat, true) { { edges.add(it) } }
        press(1)
        press(0)
        press(0)
        press(1)
        router.releaseAll()
        assertEquals(listOf(true, true, false), edges)
    }

    private class Scheduler : NavigationRepeatScheduler {
        var now = 0L
        val pending = mutableMapOf<Runnable, Long>()

        override fun postDelayed(
            action: Runnable,
            delayMs: Long,
        ) {
            pending[action] = now + delayMs
        }

        override fun removeCallbacks(action: Runnable) {
            pending.remove(action)
        }

        fun advance(ms: Long) {
            val end = now + ms
            while (true) {
                val next = pending.minByOrNull { it.value } ?: break
                if (next.value > end) break
                now = next.value
                pending.remove(next.key)
                next.key.run()
            }
            now = end
        }
    }

    @Test
    fun heldDirectionRepeatsOnScheduleWithoutHardwareRepeatsAndStopsOnRelease() {
        val scheduler = Scheduler()
        val router = ControllerKeyDispatch(scheduler, NavigationRepeatTiming(500, 125))
        val edges = mutableListOf<Boolean>()

        fun press(repeat: Int = 0) = router.press(22, repeat, true) { { edges.add(it) } }
        press()
        scheduler.advance(499)
        assertEquals(listOf(true), edges)
        // A driver that also sends repeats must not double the repeat rate
        press(1)
        press()
        scheduler.advance(1)
        assertEquals(listOf(true, true), edges)
        scheduler.advance(250)
        assertEquals(4, edges.size)
        router.release(22)
        scheduler.advance(1000)
        assertEquals(listOf(true, true, true, true, false), edges)
        assertTrue(scheduler.pending.isEmpty())
    }

    @Test
    fun releaseAllCancelsRepeatsAndGameplayButtonsNeverScheduleThem() {
        val scheduler = Scheduler()
        val router = ControllerKeyDispatch(scheduler, NavigationRepeatTiming(500, 125))
        val edges = mutableListOf<String>()
        router.press(22, 0, true) { { edges.add("nav:$it") } }
        router.press(0, 0, false) { { edges.add("game:$it") } }
        scheduler.advance(500)
        router.releaseAll()
        scheduler.advance(1000)
        assertEquals(listOf("nav:true", "game:true", "nav:true", "nav:false", "game:false"), edges)
        assertTrue(scheduler.pending.isEmpty())
        router.press(22, 1, true) { error("Orphan repeat must be ignored") }
    }
}

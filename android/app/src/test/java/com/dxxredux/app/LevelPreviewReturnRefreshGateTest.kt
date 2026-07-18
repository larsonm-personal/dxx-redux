package com.dxxredux.app

import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class LevelPreviewReturnRefreshGateTest {
    @Before
    fun setUp() {
        LevelPreviewReturnRefreshGate.resetForTest()
    }

    @After
    fun tearDown() {
        LevelPreviewReturnRefreshGate.resetForTest()
    }

    @Test
    fun previewReturnIsConsumedExactlyOnce() {
        assertFalse(LevelPreviewReturnRefreshGate.consumeReturn())
        LevelPreviewReturnRefreshGate.markLaunch()
        assertTrue(LevelPreviewReturnRefreshGate.consumeReturn())
        assertFalse(LevelPreviewReturnRefreshGate.consumeReturn())
    }

    @Test
    fun duplicateLaunchMarksCoalesceIntoOneReturn() {
        LevelPreviewReturnRefreshGate.markLaunch()
        LevelPreviewReturnRefreshGate.markLaunch()
        assertTrue(LevelPreviewReturnRefreshGate.consumeReturn())
        assertFalse(LevelPreviewReturnRefreshGate.consumeReturn())
    }
}

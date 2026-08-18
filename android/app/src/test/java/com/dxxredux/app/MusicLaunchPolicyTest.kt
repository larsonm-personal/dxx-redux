package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class MusicLaunchPolicyTest {
    @Test
    fun selectedMissionSourceUsesBundledSoundtrack() {
        val policy = resolveMusicLaunchPolicy("mission", missionHasSoundtrack = true)

        assertEquals("1", policy.musicType)
        assertTrue(policy.useMissionZipSoundtrack)
        assertFalse(policy.useCdTrackOrder)
        assertFalse(policy.useCustomAudioFiles)
    }

    @Test
    fun latestExplicitCdSelectionWinsWhenMissionHasSoundtrack() {
        val policy = resolveMusicLaunchPolicy("cd", missionHasSoundtrack = true)

        assertEquals("2", policy.musicType)
        assertFalse(policy.useMissionZipSoundtrack)
        assertTrue(policy.useCdTrackOrder)
        assertFalse(policy.useCustomAudioFiles)
    }

    @Test
    fun latestExplicitFileSelectionWinsWhenMissionHasSoundtrack() {
        val policy = resolveMusicLaunchPolicy("files", missionHasSoundtrack = true)

        assertEquals("3", policy.musicType)
        assertFalse(policy.useMissionZipSoundtrack)
        assertFalse(policy.useCdTrackOrder)
        assertTrue(policy.useCustomAudioFiles)
    }

    @Test
    fun baseMidiSelectionDoesNotBecomeMissionSelection() {
        val policy = resolveMusicLaunchPolicy("midi", missionHasSoundtrack = true)

        assertEquals("1", policy.musicType)
        assertFalse(policy.useMissionZipSoundtrack)
    }

    @Test
    fun missionSelectionFallsBackToBuiltinWhenModHasNoSoundtrack() {
        val policy = resolveMusicLaunchPolicy("mission", missionHasSoundtrack = false)

        assertEquals("1", policy.musicType)
        assertFalse(policy.useMissionZipSoundtrack)
    }

    @Test
    fun missingOrUnknownSourceUsesPlayableMidiDefault() {
        for (source in listOf(null, "unknown")) {
            val policy = resolveMusicLaunchPolicy(source, missionHasSoundtrack = false)

            assertEquals("1", policy.musicType)
            assertFalse(policy.useCdTrackOrder)
            assertFalse(policy.useCustomAudioFiles)
        }
    }
}

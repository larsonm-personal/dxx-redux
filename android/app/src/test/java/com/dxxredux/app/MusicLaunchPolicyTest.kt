package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class MusicLaunchPolicyTest {
    @Test
    fun overrideWinsEvenWhenMissionSoundtrackExists() {
        val policy =
            resolveMusicLaunchPolicy(
                musicMode = "files",
                musicTypeOverride = 2,
                missionHasSoundtrack = true,
                useMissionSoundtrackWhenAvailable = true,
            )

        assertEquals("2", policy.musicType)
        assertFalse(policy.useMissionZipSoundtrack)
        assertTrue(policy.useCdTrackOrder)
        assertFalse(policy.useCustomAudioFiles)
    }

    @Test
    fun missionSoundtrackWinsAcrossGlobalModesWhenEnabled() {
        for (mode in listOf("cd", "midi", "files")) {
            val policy =
                resolveMusicLaunchPolicy(
                    musicMode = mode,
                    musicTypeOverride = null,
                    missionHasSoundtrack = true,
                    useMissionSoundtrackWhenAvailable = true,
                )

            assertEquals("1", policy.musicType)
            assertTrue(policy.useMissionZipSoundtrack)
            assertFalse(policy.useCdTrackOrder)
            assertFalse(policy.useCustomAudioFiles)
        }
    }

    @Test
    fun disabledMissionPreferenceFallsBackToGlobalCdMode() {
        val policy =
            resolveMusicLaunchPolicy(
                musicMode = "cd",
                musicTypeOverride = null,
                missionHasSoundtrack = true,
                useMissionSoundtrackWhenAvailable = false,
            )

        assertEquals("2", policy.musicType)
        assertFalse(policy.useMissionZipSoundtrack)
        assertTrue(policy.useCdTrackOrder)
        assertFalse(policy.useCustomAudioFiles)
    }

    @Test
    fun disabledMissionPreferenceFallsBackToGlobalFilesMode() {
        val policy =
            resolveMusicLaunchPolicy(
                musicMode = "files",
                musicTypeOverride = null,
                missionHasSoundtrack = true,
                useMissionSoundtrackWhenAvailable = false,
            )

        assertEquals("3", policy.musicType)
        assertFalse(policy.useMissionZipSoundtrack)
        assertFalse(policy.useCdTrackOrder)
        assertTrue(policy.useCustomAudioFiles)
    }

    @Test
    fun noMissionSoundtrackKeepsExistingGlobalModeBehavior() {
        val midi =
            resolveMusicLaunchPolicy(
                musicMode = "midi",
                musicTypeOverride = null,
                missionHasSoundtrack = false,
                useMissionSoundtrackWhenAvailable = true,
            )
        val cd =
            resolveMusicLaunchPolicy(
                musicMode = "cd",
                musicTypeOverride = null,
                missionHasSoundtrack = false,
                useMissionSoundtrackWhenAvailable = true,
            )
        val files =
            resolveMusicLaunchPolicy(
                musicMode = "files",
                musicTypeOverride = null,
                missionHasSoundtrack = false,
                useMissionSoundtrackWhenAvailable = true,
            )

        assertEquals("1", midi.musicType)
        assertEquals("2", cd.musicType)
        assertEquals("3", files.musicType)
        assertTrue(cd.useCdTrackOrder)
        assertTrue(files.useCustomAudioFiles)
    }

    @Test
    fun missingOrUnknownModeUsesPlayableMidiDefault() {
        for (mode in listOf(null, "unknown")) {
            val policy =
                resolveMusicLaunchPolicy(
                    musicMode = mode,
                    musicTypeOverride = null,
                    missionHasSoundtrack = false,
                    useMissionSoundtrackWhenAvailable = true,
                )

            assertEquals("1", policy.musicType)
            assertFalse(policy.useCdTrackOrder)
            assertFalse(policy.useCustomAudioFiles)
        }
    }
}

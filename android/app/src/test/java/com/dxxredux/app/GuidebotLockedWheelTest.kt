package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class GuidebotLockedWheelTest {
    @Test
    fun lockedGuideSpawnRingUsesOuterThirdOfWheel() {
        val radius = 90f

        assertFalse(lockedGuideSpawnRingSelected(59.9f, radius))
        assertTrue(lockedGuideSpawnRingSelected(60f, radius))
        assertTrue(lockedGuideSpawnRingSelected(90f, radius))
        assertTrue(lockedGuideSpawnRingSelected(108f, radius))
        assertFalse(lockedGuideSpawnRingSelected(108.1f, radius))
    }

    @Test
    fun lockedGuideSpawnRingRejectsInvalidRadius() {
        assertFalse(lockedGuideSpawnRingSelected(1f, 0f))
        assertFalse(lockedGuideSpawnRingSelected(1f, -1f))
    }

    @Test
    fun guidePresetUsesNextSliceWithoutReleaseOrCenter() {
        val guideSegments = TouchBindings.RADIAL_PRESET_SEGMENTS.getValue("Guide")
        val bindings = guideSegments.map { it.binding }

        assertTrue(TouchBindings.META_GUIDE_NEXT_GOAL in bindings)
        assertFalse(TouchBindings.META_GUIDE_RELEASE_CONTROL in bindings)
        assertFalse(TouchBindings.RADIAL_PRESET_CENTER.containsKey("Guide"))
    }

    @Test
    fun guideReleaseControlIsLegacyNamedButNotPickerVisible() {
        assertFalse(TouchBindings.META_BUTTON_LABELS.containsKey(TouchBindings.META_GUIDE_RELEASE_CONTROL))
        assertEquals(
            "Meta: GB: Release Control",
            TouchBindings.bindingToName(TouchBindings.META_GUIDE_RELEASE_CONTROL),
        )
        assertEquals(
            TouchBindings.META_GUIDE_RELEASE_CONTROL,
            TouchBindings.nameToBinding("Meta: GB: Release Control"),
        )
    }

    @Test
    fun guideWheelVisibleSegmentsHideReleaseAndUnrevealedSecret() {
        val segments =
            listOf(
                RadialSegment("Secret", TouchBindings.META_GUIDE_FIND_SECRET),
                RadialSegment("Release", TouchBindings.META_GUIDE_RELEASE_CONTROL),
                RadialSegment("Next", TouchBindings.META_GUIDE_NEXT_GOAL),
            )

        assertEquals(
            listOf("Next"),
            guideWheelVisibleSegments(segments, secretAreaRevealed = false).map { it.label },
        )
        assertEquals(
            listOf("Secret", "Next"),
            guideWheelVisibleSegments(segments, secretAreaRevealed = true).map { it.label },
        )
    }

    @Test
    fun guideWheelMigrationMovesNextCenterToSliceAndRemovesRelease() {
        val layout =
            TouchLayout(
                version = 5,
                radialMenus =
                    listOf(
                        RadialMenuControl(
                            id = "Guide",
                            xPct = 50f,
                            yPct = 50f,
                            segments =
                                listOf(
                                    RadialSegment("Energy", TouchBindings.META_GUIDE_FIND_ENERGY),
                                    RadialSegment("Secret", TouchBindings.META_GUIDE_FIND_SECRET),
                                    RadialSegment("Release", TouchBindings.META_GUIDE_RELEASE_CONTROL),
                                ),
                            centerLabel = "Next",
                            centerBinding = TouchBindings.META_GUIDE_NEXT_GOAL,
                        ),
                    ),
            )

        val migrated = TouchLayoutRepository.migrateForCurrentVersion(layout).radialMenus.single()
        val bindings = migrated.segments.map { it.binding }

        assertEquals(-1, migrated.centerBinding)
        assertEquals("", migrated.centerLabel)
        assertEquals(TouchBindings.META_GUIDE_NEXT_GOAL, migrated.segments.last().binding)
        assertTrue(TouchBindings.META_GUIDE_FIND_ENERGY in bindings)
        assertFalse(TouchBindings.META_GUIDE_RELEASE_CONTROL in bindings)
    }
}

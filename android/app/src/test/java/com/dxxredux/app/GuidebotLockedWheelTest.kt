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
    fun lockedGuideDeployStripUsesAnAboveOrLeftTarget() {
        val radius = 10f
        val target = lockedGuideDeployCrossOffset(radius, LEGACY_SCROLL_STRIP_CARD_SCALE)

        assertEquals(-25f, target, 0.001f)
        assertFalse(lockedGuideDeploySelected(0f, radius, LEGACY_SCROLL_STRIP_CARD_SCALE))
        assertFalse(lockedGuideDeploySelected(-12.49f, radius, LEGACY_SCROLL_STRIP_CARD_SCALE))
        assertTrue(lockedGuideDeploySelected(-12.5f, radius, LEGACY_SCROLL_STRIP_CARD_SCALE))
        assertFalse(lockedGuideDeploySelected(25f, radius, LEGACY_SCROLL_STRIP_CARD_SCALE))
    }

    @Test
    fun lockedGuideDeployStripTargetTracksCardScale() {
        assertEquals(
            -25f / LEGACY_SCROLL_STRIP_CARD_SCALE,
            lockedGuideDeployCrossOffset(10f, DEFAULT_SCROLL_STRIP_CARD_SCALE),
            0.001f,
        )
    }

    @Test
    fun guidePresetUsesNextAndUnexploredSlicesWithoutReleaseOrCenter() {
        val guideSegments = TouchBindings.RADIAL_PRESET_SEGMENTS.getValue("Guide")
        val bindings = guideSegments.map { it.binding }

        assertTrue(TouchBindings.META_GUIDE_NEXT_GOAL in bindings)
        assertTrue(TouchBindings.META_GUIDE_WARP_TO_ME in bindings)
        assertTrue(TouchBindings.META_GUIDE_FIND_UNEXPLORED in bindings)
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
        assertEquals(
            "Meta: GB: Warp to Me",
            TouchBindings.bindingToName(TouchBindings.META_GUIDE_WARP_TO_ME),
        )
    }

    @Test
    fun guideWheelVisibleSegmentsHideReleaseAndUnrevealedSecret() {
        val segments =
            listOf(
                RadialSegment("Secret", TouchBindings.META_GUIDE_FIND_SECRET),
                RadialSegment("Release", TouchBindings.META_GUIDE_RELEASE_CONTROL),
                RadialSegment("Warp to Me", TouchBindings.META_GUIDE_WARP_TO_ME),
                RadialSegment("Next", TouchBindings.META_GUIDE_NEXT_GOAL),
            )

        assertEquals(
            listOf("Warp to Me", "Next"),
            guideWheelVisibleSegments(segments, secretAreaRevealed = false).map { it.label },
        )
        assertEquals(
            listOf("Secret", "Warp to Me", "Next"),
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
        assertTrue(TouchBindings.META_GUIDE_WARP_TO_ME in bindings)
        assertTrue(TouchBindings.META_GUIDE_FIND_UNEXPLORED in bindings)
        assertFalse(TouchBindings.META_GUIDE_RELEASE_CONTROL in bindings)
    }

    @Test
    fun guideWheelVersionElevenMigrationRenamesWarpToMe() {
        val layout =
            TouchLayout(
                version = 10,
                radialMenus =
                    listOf(
                        RadialMenuControl(
                            id = "Guide",
                            xPct = 50f,
                            yPct = 50f,
                            segments =
                                listOf(
                                    RadialSegment("Warp Me", TouchBindings.META_GUIDE_WARP_TO_ME),
                                ),
                        ),
                    ),
            )

        val migrated = TouchLayoutRepository.migrateForCurrentVersion(layout)

        assertEquals(11, migrated.version)
        assertEquals("Warp to Me", migrated.radialMenus.single().segments.single().label)
    }

    @Test
    fun guideWheelMigrationPreservesCustomCenterAction() {
        val customBinding = TouchBindings.META_GUIDE_FIND_SECRET
        val layout =
            TouchLayout(
                version = 7,
                radialMenus =
                    listOf(
                        RadialMenuControl(
                            id = "Guide",
                            xPct = 50f,
                            yPct = 50f,
                            segments = emptyList(),
                            centerLabel = "Secret",
                            centerBinding = customBinding,
                        ),
                    ),
            )

        val migrated = TouchLayoutRepository.migrateForCurrentVersion(layout).radialMenus.single()

        assertEquals("Secret", migrated.centerLabel)
        assertEquals(customBinding, migrated.centerBinding)
        assertEquals(
            listOf(TouchBindings.META_GUIDE_FIND_UNEXPLORED),
            migrated.segments.map { it.binding },
        )
    }

    @Test
    fun guideWheelVersionEightMigrationRestoresMissingUnexploredAction() {
        val layout =
            TouchLayout(
                version = 8,
                radialMenus =
                    listOf(
                        RadialMenuControl(
                            id = "Guide",
                            xPct = 50f,
                            yPct = 50f,
                            segments =
                                listOf(
                                    RadialSegment("Energy", TouchBindings.META_GUIDE_FIND_ENERGY),
                                    RadialSegment("Next", TouchBindings.META_GUIDE_NEXT_GOAL),
                                ),
                        ),
                    ),
            )

        val migrated = TouchLayoutRepository.migrateForCurrentVersion(layout)
        val guide = migrated.radialMenus.single()

        assertEquals(11, migrated.version)
        assertEquals("", guide.centerLabel)
        assertEquals(-1, guide.centerBinding)
        assertTrue(guide.segments.any { it.binding == TouchBindings.META_GUIDE_FIND_UNEXPLORED })
    }
}

package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SettingsCatalogTest {
    @Test
    fun aliasesResolveToRealPassiveDestinations() {
        val cases =
            listOf(
                Triple(" FIELD-of VIEW ", "graphics.fov", LauncherDestination.GRAPHICS),
                Triple("antialiasing", "graphics.msaa", LauncherDestination.GRAPHICS),
                Triple("gamepad mapping", "controller.mapping", LauncherDestination.CONTROLLER_MAPPING),
                Triple("tilt", "touch.gyro", LauncherDestination.TOUCH_EDITOR),
                Triple("redbook", "music.cd", LauncherDestination.MUSIC),
                Triple("config export", "advanced.export", LauncherDestination.ADVANCED),
            )
        for ((query, id, destination) in cases) {
            val result = SettingsCatalog.search(query).first()
            assertEquals(query, id, result.id)
            assertEquals(query, destination, result.destination)
        }
        // Navigation exposes passive target identities, never executes a chip or destructive action
        assertEquals(SettingsSections.MUSIC_CD, SettingsCatalog.search("redbook").first().section)
        assertEquals(SettingsSections.TOUCH_ENABLED, SettingsCatalog.search("Enable touch controls").first().section)
        assertEquals(SettingsSections.GAME_REWIND_AMOUNT, SettingsCatalog.search("Rewind amount").first().section)
        assertEquals(SettingsSections.ADVANCED_RESET, SettingsCatalog.search("Reset all controls").first().section)
    }

    @Test
    fun matchingRanksTitlesBeforeAliasesAndReturnsStableResults() {
        assertEquals("music", SettingsCatalog.search("Music").first().id)
        assertEquals("graphics.msaa", SettingsCatalog.search("Anti-aliasing (MSAA)").first().id)
        assertEquals("game.rewind_amount", SettingsCatalog.search("rewind amount").first().id)
        assertEquals(SettingsSections.GRAPHICS_SELECTIVE_FILTER, SettingsCatalog.search("reticle").first().section)
        assertEquals(SettingsSections.GRAPHICS_SELECTIVE_FILTER, SettingsCatalog.search("gauges").first().section)
        val repeated = SettingsCatalog.search("controller")
        assertEquals(repeated, SettingsCatalog.search("controller"))
        assertEquals("controller", repeated.first().id)
        assertEquals(SettingsCatalog.categories, SettingsCatalog.search("  \t\n "))
        assertTrue(SettingsCatalog.search("no-such-setting-qzx").isEmpty())
    }

    @Test
    fun catalogIdentitiesAreUniqueAndNeverNavigateHomeOrLaunchAnything() {
        assertEquals(
            SettingsCatalog.entries.size,
            SettingsCatalog.entries
                .map { it.id }
                .toSet()
                .size,
        )
        assertEquals(9, SettingsCatalog.categories.size)
        for (entry in SettingsCatalog.entries) {
            assertFalse(entry.id.isBlank())
            assertFalse(entry.title.isBlank())
            assertFalse(entry.path.isBlank())
            assertFalse(
                entry.destination in
                    setOf(LauncherDestination.HOME, LauncherDestination.SETTINGS, LauncherDestination.MULTIPLAYER),
            )
        }
    }
}

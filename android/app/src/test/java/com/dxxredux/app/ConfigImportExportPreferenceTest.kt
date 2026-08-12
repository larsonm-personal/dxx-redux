package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ConfigImportExportPreferenceTest {
    private fun minimalCombinedConfig(): JSONObject =
        JSONObject()
            .put("type", "combined_config")
            .put("version", 3)
            .put("host_defaults", JSONObject().put("game", "d2"))

    @Test
    fun exportedPreferencesRoundTripWithDeclaredTypes() {
        for (booleanValue in listOf(false, true)) {
            val source =
                ConfigImportExport.EXPORTED_PREFERENCES.associate { pref ->
                    pref.key to
                        when (pref.type) {
                            ConfigImportExport.ExportedPreferenceType.BOOLEAN -> booleanValue
                            ConfigImportExport.ExportedPreferenceType.STRING -> "value-${pref.key}"
                        }
                }

            val json = ConfigImportExport.exportPreferenceValues(source)
            val decoded = ConfigImportExport.decodePreferenceValues(json)

            assertNull(decoded.error)
            assertEquals(source, decoded.values.mapKeys { it.key.key })
            for (pref in ConfigImportExport.EXPORTED_PREFERENCES) {
                when (pref.type) {
                    ConfigImportExport.ExportedPreferenceType.BOOLEAN -> {
                        assertTrue(json.get(pref.key) is Boolean)
                    }

                    ConfigImportExport.ExportedPreferenceType.STRING -> {
                        assertTrue(json.get(pref.key) is String)
                    }
                }
            }
        }
    }

    @Test
    fun acoustIdAndHeadlightPreferencesAreDeclaredBooleans() {
        val booleanKeys =
            ConfigImportExport.EXPORTED_PREFERENCES
                .filter { it.type == ConfigImportExport.ExportedPreferenceType.BOOLEAN }
                .map { it.key }
                .toSet()

        assertTrue(PREF_ALLOW_ACOUSTID_WEB_LOOKUPS in booleanKeys)
        assertTrue(PREF_HEADLIGHT_OFF_BY_DEFAULT in booleanKeys)
    }

    @Test
    fun absentPreferencesProduceAnEmptyValidatedImport() {
        val decoded = ConfigImportExport.decodePreferenceValues(JSONObject())

        assertNull(decoded.error)
        assertTrue(decoded.values.isEmpty())
    }

    @Test
    fun wrongJsonTypeReturnsFieldErrorBeforePublishingValues() {
        for (pref in ConfigImportExport.EXPORTED_PREFERENCES) {
            val wrongValues =
                when (pref.type) {
                    ConfigImportExport.ExportedPreferenceType.BOOLEAN -> {
                        listOf("true", 1, JSONObject(), JSONArray(), JSONObject.NULL)
                    }

                    ConfigImportExport.ExportedPreferenceType.STRING -> {
                        listOf(true, 1, JSONObject(), JSONArray(), JSONObject.NULL)
                    }
                }
            for (wrongValue in wrongValues) {
                val decoded =
                    ConfigImportExport.decodePreferenceValues(
                        JSONObject().put(pref.key, wrongValue),
                    )

                assertTrue(decoded.error?.contains(pref.key) == true)
                assertFalse(decoded.values.isNotEmpty())
            }
        }
    }

    @Test
    fun combinedConfigRequiresSupportedVersionAndTypedSections() {
        assertNull(ConfigImportExport.validateCombinedConfig(minimalCombinedConfig()))
        for (version in listOf(null, 2, 4, "3")) {
            val json = minimalCombinedConfig()
            if (version == null) json.remove("version") else json.put("version", version)
            assertTrue(ConfigImportExport.validateCombinedConfig(json)?.contains("version") == true)
        }
        for (json in listOf(
            minimalCombinedConfig().put("touch_layout_slots", JSONObject()),
            minimalCombinedConfig().put("controller_config_slots", JSONObject()),
            minimalCombinedConfig().put("host_defaults", JSONArray()),
        )) {
            assertTrue(ConfigImportExport.validateCombinedConfig(json) != null)
        }
    }

    @Test
    fun combinedConfigRejectsWrongPreferenceAndHostFieldTypes() {
        val preference =
            minimalCombinedConfig().put("app_settings", JSONObject().put("touch_overlay_enabled", "true"))
        val host = minimalCombinedConfig().put("host_defaults", JSONObject().put("max_players", "4"))
        assertTrue(ConfigImportExport.validateCombinedConfig(preference)?.contains("touch_overlay_enabled") == true)
        assertTrue(ConfigImportExport.validateCombinedConfig(host)?.contains("max_players") == true)
    }
}

package com.dxxredux.app

import org.json.JSONException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Test

class AcoustIdConfigurationTest {
    @Test
    fun parsesDocumentedJsoncConfiguration() {
        val key =
            AcoustIdConfiguration.parseApiKey(
                """
                {
                    // Local application key
                    "api_key": "AbCd123456",
                }
                """.trimIndent(),
            )

        assertEquals("AbCd123456", key)
    }

    @Test
    fun rejectsPlaceholderAndMalformedKeys() {
        assertNull(AcoustIdConfiguration.parseApiKey("""{"api_key":"YOUR_ACOUSTID_API_KEY_HERE"}"""))
        assertNull(AcoustIdConfiguration.parseApiKey("""{"api_key":"contains space"}"""))
        assertNull(AcoustIdConfiguration.parseApiKey("""{"api_key":12345678}"""))
        assertThrows(JSONException::class.java) {
            AcoustIdConfiguration.parseApiKey("""{"api_key":""")
        }
    }

    @Test
    fun disabledLookupsSkipConfiguration() {
        var configureCalls = 0

        val status =
            configureAcoustIdIfEnabled(false) {
                configureCalls++
                AcoustIdConfigurationStatus.AVAILABLE
            }

        assertEquals(AcoustIdConfigurationStatus.NOT_PACKAGED, status)
        assertEquals(0, configureCalls)
    }
}

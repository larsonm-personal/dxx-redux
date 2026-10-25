package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Test

class XlCompatibilityTest {
    @Test
    fun packageEvidenceDoesNotDisableAnalysisOrQualifyTheMission() {
        val inventory =
            XlCompatibility.parse(
                report().replace("\"detected\":true", "\"detected\":true,\"blocks_analysis\":false"),
            )!!
        val level = XlCompatibility.parse(report())!!
        assertEquals("blocked", inventory.status)
        assertEquals("blocked", compatibilityGroupStatus(listOf(inventory)))
        assertEquals(emptyList<XlCompatibilityReport>(), listOf(inventory).analysisBlockers())
        assertEquals(listOf(level), listOf(inventory, level).analysisBlockers())
        // Feature eligibility does not give the metadata engine XL runtime support
        val eligible = level.copy(status = "eligible")
        assertEquals(listOf(eligible), listOf(inventory, eligible).analysisBlockers())
    }

    @Test
    fun onlyAnExplicitBooleanCanRemoveAnAnalysisRestriction() {
        for (value in listOf("true", "null", "\"false\"", "0", "{}")) {
            val parsed =
                XlCompatibility.parse(
                    report().replace("\"detected\":true", "\"detected\":true,\"blocks_analysis\":$value"),
                )!!
            assertEquals(listOf(parsed), listOf(parsed).analysisBlockers())
        }
        val absent = XlCompatibility.parse(report())!!
        assertEquals(listOf(absent), listOf(absent).analysisBlockers())
    }

    private fun report(
        extra: String = "",
        status: String = "blocked",
    ) =
        """{"detected":true,"source":"test.rl2","status":"blocked","summary":"Cannot load","rows":[{"feature":"Wall teleports","requirement":"3","support":"Unsupported","status":"$status","evidence":"Serialized records"$extra}]}"""

    @Test
    fun countsAndSupportRemainDistinctFromLevelAdmission() {
        val report = XlCompatibility.parse(report(",\"count\":3"))!!
        assertEquals("blocked", report.status)
        assertEquals("Wall teleports: 3", report.rows.single().shortTitle())
        assertEquals("Unsupported", compatibilityStatusLabel(report.rows.single().status))
        assertEquals("Experimental / partial", compatibilityStatusLabel("partial"))
        assertEquals("Supported", compatibilityStatusLabel("full"))
    }

    @Test
    fun unknownAndAbsentAreDifferent() {
        val unknown = XlCompatibility.parse(report(",\"count\":null", "unknown"))!!.rows.single()
        val absent = XlCompatibility.parse(report(",\"count\":0", "not_required"))!!.rows.single()
        assertEquals("Wall teleports: ?", unknown.shortTitle())
        assertEquals("Not inspected", compatibilityStatusLabel(unknown.status))
        assertEquals("Wall teleports: 0", absent.shortTitle())
        assertEquals("Not present", compatibilityStatusLabel(absent.status))
        assertEquals("Not inspected", compatibilityStatusLabel("future-status"))
    }

    @Test
    fun oldMissingAndMalformedMetadataNeverInventCounts() {
        val old = XlCompatibility.parse(report())!!.rows.single()
        assertFalse(old.counted)
        assertNull(old.count)
        assertEquals("Wall teleports", old.shortTitle())
        assertNull(XlCompatibility.parse("{}"))
        assertNull(XlCompatibility.parse("broken"))
        assertNull(XlCompatibility.parse("""{"detected":false}"""))
        assertNull(
            XlCompatibility
                .parse(report(",\"count\":-1"))!!
                .rows
                .single()
                .count,
        )
    }

    @Test
    fun explanationsUseNativeCatalogAndKeepQualificationSeparate() {
        val parsed =
            XlCompatibility.parse(
                report(
                    ",\"id\":\"water_segments\",\"count\":1,\"explanation\":\"Whole water volume; conditional 75% motion\",\"limitations\":\"Bounded pilot only; visual parity unqualified\"",
                    "partial",
                ),
            )!!
        val row = parsed.rows.single()
        assertEquals("water_segments", row.id)
        assertEquals("Whole water volume; conditional 75% motion", row.xlExplanation())
        assertEquals("Bounded pilot only; visual parity unqualified", row.reduxExplanation())
        assertEquals("partial", row.status)
        assertEquals("blocked", parsed.status)
        val legacy = XlCompatibility.parse(report())!!.rows.single()
        assertEquals("", legacy.id)
        assertFalse(legacy.xlExplanation().contains("75%"))
        org.junit.Assert.assertTrue(legacy.xlExplanation().contains("No verified feature explanation"))
        org.junit.Assert.assertTrue(
            legacy.copy(status = "unknown").reduxExplanation().contains("not been fully inspected"),
        )
        org.junit.Assert.assertTrue(
            legacy.copy(status = "not_required").reduxExplanation().contains("does not qualify the level"),
        )
    }

    @Test
    fun eligibilityIsConditionalAndCannotQualifyUnknownSiblings() {
        val eligible = XlCompatibility.parse(report(status = "eligible"))!!.copy(status = "eligible")
        assertEquals("Eligible; runtime checks needed", compatibilityStatusLabel(eligible.rows.single().status))
        org.junit.Assert.assertTrue(
            eligible.rows
                .single()
                .reduxExplanation()
                .contains("XL-enabled build"),
        )
        org.junit.Assert.assertTrue(
            eligible.rows
                .single()
                .reduxExplanation()
                .contains("not verified gameplay"),
        )
        assertEquals("eligible", compatibilityGroupStatus(listOf(eligible)))
        assertEquals("unknown", compatibilityGroupStatus(listOf(eligible, eligible.copy(status = "unknown"))))
        assertEquals("blocked", compatibilityGroupStatus(listOf(eligible, eligible.copy(status = "blocked"))))
        assertEquals("unknown", compatibilityGroupStatus(emptyList()))
    }
}

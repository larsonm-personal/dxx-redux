package com.dxxredux.app.multiplayer

import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class MissionContentProtocolTest {
    private val hash = "ab".repeat(32)

    @Test
    fun wrapperRequirementRoundTripsThroughGameInfo() {
        val requirement = wrapperRequirement(42L, offer = true)
        val gameInfo =
            buildJsonObject {
                put("game", "d2")
                put("mission", "castaway")
                requirement.toGameInfoFields().forEach(::put)
            }

        val parsed = missionRequirementFromGameInfo(gameInfo)

        assertNotNull(parsed)
        assertEquals(requirement, parsed)
    }

    @Test
    fun transferCapDisablesOfferingWithoutInvalidatingCompatibilityIdentity() {
        val oversized = wrapperRequirement(MISSION_TRANSFER_MAX_BYTES + 1L, offer = false)
        val forgedOffer = oversized.copy(offerAvailable = true)

        assertTrue(oversized.isValid)
        assertFalse(forgedOffer.isValid)
    }

    @Test
    fun baseAndLooseSourcesCanNeverAdvertiseDownload() {
        val builtin =
            MissionRequirement(
                revision = "builtin:d2:d2",
                game = "d2",
                missionKey = "d2",
                displayName = "Descent 2",
                kind = MissionRequirement.KIND_BUILTIN,
                offerAvailable = true,
            )

        assertFalse(builtin.isValid)
    }

    @Test
    fun statusReportMustUseRequirementRevisionAndExactTotal() {
        val requirement = wrapperRequirement(42L, offer = true)
        val valid =
            MissionStatusReport(
                revision = requirement.revision,
                status = MissionCompatibilityStatus.DOWNLOADING,
                verifiedBytes = 21L,
                totalBytes = 42L,
            )

        assertTrue(valid.validFor(requirement))
        assertFalse(valid.copy(totalBytes = 41L, verifiedBytes = 21L).validFor(requirement))
        assertFalse(valid.copy(revision = "stale").validFor(requirement))
    }

    private fun wrapperRequirement(
        size: Long,
        offer: Boolean,
    ): MissionRequirement =
        MissionRequirement(
            revision = "$hash:$size:d2:castaway",
            game = "d2",
            missionKey = "castaway",
            displayName = "PTMC Castaway Redux",
            kind = MissionRequirement.KIND_WRAPPER,
            descriptorPath = "missions/castaway.mn2",
            wrapperFilename = "castaway_redux.zip",
            sizeBytes = size,
            sha256 = hash,
            offerAvailable = offer,
        )
}

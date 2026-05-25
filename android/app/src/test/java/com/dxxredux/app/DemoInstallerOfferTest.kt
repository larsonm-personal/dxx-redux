package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class DemoInstallerOfferTest {
    @Test
    fun hiddenWhenPreferenceIsDisabled() {
        assertTrue(
            visibleDemoInstallerOffers(
                showDemoInstallerOffer = false,
                d1Ready = false,
                d2Ready = false,
            ).isEmpty(),
        )
    }

    @Test
    fun offersOnlyMissingGames() {
        assertEquals(
            listOf("d2", "d1"),
            visibleDemoInstallerOffers(
                showDemoInstallerOffer = true,
                d1Ready = false,
                d2Ready = false,
            ).map { it.game },
        )
        assertEquals(
            listOf("d1"),
            visibleDemoInstallerOffers(
                showDemoInstallerOffer = true,
                d1Ready = false,
                d2Ready = true,
            ).map { it.game },
        )
        assertEquals(
            listOf("d2"),
            visibleDemoInstallerOffers(
                showDemoInstallerOffer = true,
                d1Ready = true,
                d2Ready = false,
            ).map { it.game },
        )
    }

    @Test
    fun hostedOffersMatchKnownStuffitPackages() {
        val byGame = DEMO_DOWNLOADS.associateBy { it.game }

        assertEquals("Descent Shareware.sit", byGame.getValue("d1").archiveName)
        assertEquals("Descent II Preview.sit", byGame.getValue("d2").archiveName)
        assertEquals(
            byGame.getValue("d1").files,
            DemoInstallerPackages.matchByName(byGame.getValue("d1").archiveName)?.expectedFiles,
        )
        assertEquals(
            byGame.getValue("d2").files,
            DemoInstallerPackages.matchByName(byGame.getValue("d2").archiveName)?.expectedFiles,
        )
    }
}

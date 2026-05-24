package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class DemoInstallerPackagesTest {
    @Test
    fun recognizesKnownDemoArchivesByName() {
        assertTrue(DemoInstallerPackages.isKnownArchiveName("DESC14SW.EXE"))
        assertTrue(DemoInstallerPackages.isKnownArchiveName("Descent Shareware.sit"))
        assertTrue(DemoInstallerPackages.isKnownArchiveName("Descent_demo.HQX"))
        assertTrue(DemoInstallerPackages.isKnownArchiveName("descent 2 demo 1-0.zip"))
        assertTrue(DemoInstallerPackages.isKnownArchiveName("Descent II Preview.sit"))
        assertTrue(DemoInstallerPackages.isKnownArchiveName("descent2preview.sit_.hqx"))
        assertFalse(DemoInstallerPackages.isKnownArchiveName("setup_descent_2_1.1.exe"))
    }

    @Test
    fun matchesKnownDemoArchivesByHash() {
        val pkg = DemoInstallerPackages.matchBySha256(
            "a7c31eae6dfd22e1f6a4c0b9fb2dfb2e25197831bc43c3e9d65734c7fa446c4d",
        )
        assertNotNull(pkg)
        assertEquals("descent 2 demo 1-0.zip", pkg!!.filename)
        assertEquals(listOf("d2demo.hog", "d2demo.ham", "d2demo.pig", "d2demo.dem"), pkg.expectedFiles)
    }

    @Test
    fun matchesMacDemoStuffitArchivesByHash() {
        val d1Pkg = DemoInstallerPackages.matchBySha256(
            "f45c338df4bc4ceda38e6541f14b8dc93b543fd07d90a2c5d5118d2001c12ad2",
        )
        assertNotNull(d1Pkg)
        assertEquals("Descent Shareware.sit", d1Pkg!!.filename)
        assertEquals(listOf("descent.hog", "descent.pig"), d1Pkg.expectedFiles)

        val d2Pkg = DemoInstallerPackages.matchBySha256(
            "4b5b7739b9da59472bcdca92f23957f90247bedd84ef8bded57d37d5d229f6d6",
        )
        assertNotNull(d2Pkg)
        assertEquals("Descent II Preview.sit", d2Pkg!!.filename)
        assertEquals(listOf("d2demo.hog", "d2demo.ham", "d2demo.pig", "descent2.s11", "exit.ham"), d2Pkg.expectedFiles)
    }

    @Test
    fun matchesBinHexDemoStuffitArchivesByHash() {
        val d1Pkg = DemoInstallerPackages.matchBySha256(
            "e485a1570cb6079d3ec55a52ed9150792f5ef450b653e5db9748a305fed2dfe4",
        )
        assertNotNull(d1Pkg)
        assertEquals("Descent_demo.HQX", d1Pkg!!.filename)
        assertEquals(listOf("descent.hog", "descent.pig"), d1Pkg.expectedFiles)

        val d2Pkg = DemoInstallerPackages.matchBySha256(
            "b7c55f60f11a1d0d72658f8a30fecdebef9251e0e86eeff747888fc4f56fcd19",
        )
        assertNotNull(d2Pkg)
        assertEquals("descent2preview.sit_.hqx", d2Pkg!!.filename)
        assertEquals(listOf("d2demo.hog", "d2demo.ham", "d2demo.pig", "descent2.s11", "exit.ham"), d2Pkg.expectedFiles)
    }
}

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
        assertTrue(DemoInstallerPackages.isKnownArchiveName("descent 2 demo 1-0.zip"))
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
}
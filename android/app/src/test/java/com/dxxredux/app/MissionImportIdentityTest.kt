package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class MissionImportIdentityTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    @Test
    fun copiedAcceptedMissionMustRetainItsFullIdentity() {
        val source = temporaryFolder.newFile("source.zip").apply { writeBytes(ByteArray(4097) { it.toByte() }) }
        val accepted = MissionContentIdentity.compute(source, 1024)
        val destination = temporaryFolder.newFile("destination.zip")
        source.copyTo(destination, overwrite = true)
        val manager = ModManager(temporaryFolder.newFolder("files"))

        assertEquals(accepted, manager.resolveImportedMissionIdentity(destination, accepted, copiedSource = true))

        destination.writeBytes(destination.readBytes().apply { this[2048] = (this[2048] + 1).toByte() })
        assertThrows(IllegalArgumentException::class.java) {
            manager.resolveImportedMissionIdentity(destination, accepted, copiedSource = true)
        }
    }
}

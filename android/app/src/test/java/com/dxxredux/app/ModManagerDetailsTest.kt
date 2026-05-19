package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.security.MessageDigest
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlin.io.path.createTempDirectory

class ModManagerDetailsTest {
    @Test
    fun detailsSummarizeArchiveCategoriesPatchesAndBaseRequirements() {
        val filesDir = createTempDirectory("mod-details").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val baseHam = File(setDir, "descent2.ham").apply { writeText("base-ham") }
        val baseSha256 = sha256(baseHam)
        val dxaFile = File(modsDir, "detail_test.dxa")

        ZipOutputStream(dxaFile.outputStream()).use { zip ->
            zip.writeEntry(
                "metadata/manifest.json",
                """
                {
                  "schema": "com.dxxredux.test-dxa.v1",
                  "game": "d2",
                  "compatibility": {
                    "requiredBaseFiles": [
                      {
                        "game": "d2",
                        "role": "baselineHam",
                        "filename": "descent2.ham",
                        "sha256": "$baseSha256",
                        "version": "Test HAM",
                        "required": true,
                        "reason": "Patch tests use this baseline",
                        "patchPaths": ["patches/d2/ham_patch.rfc6902.json"]
                      }
                    ]
                  }
                }
                """.trimIndent(),
            )
            zip.writeEntry(
                "patches/d2/ham_patch.rfc6902.json",
                "[{\"op\":\"test\",\"path\":\"/x\",\"value\":1}]",
            )
            zip.writeEntry("descent2.s11", "sound bank")
            zip.writeEntry("textures/rock001.png", "texture")
            zip.writeEntry("sounds/laser.raw", "raw sound")
        }
        val manager = ModManager(filesDir)
        val mod =
            ModManager.ModInfo(
                filename = dxaFile.name,
                displayName = "Detail Test",
                enabled = true,
                addedAt = 0,
                sizeBytes = dxaFile.length(),
                game = "d2",
                order = 0,
            )
        val details = manager.getModDetails(mod, setDir)
        val categories = details.categories.associateBy { it.label }

        assertEquals(5, details.fileCount)
        assertEquals("com.dxxredux.test-dxa.v1", details.manifestSchema)
        assertTrue(
            categories
                .getValue("Base game file replacements")
                .examples
                .single()
                .contains("11 kHz sound data"),
        )
        assertEquals(1, categories.getValue("Texture replacements").count)
        assertEquals(1, categories.getValue("Individual sound file replacements").count)
        assertEquals("patches/d2/ham_patch.rfc6902.json", details.patches.single().path)
        assertEquals(1, details.patches.single().operationCount)
        assertEquals(listOf("descent2.ham"), details.patches.single().affectedFiles)
        assertEquals("Test HAM", details.baseRequirements.single().expectedVersion)
        assertTrue(details.baseRequirements.single().ok)
        assertTrue(details.problems.isEmpty())
    }

    private fun ZipOutputStream.writeEntry(
        name: String,
        text: String,
    ) {
        putNextEntry(ZipEntry(name))
        write(text.toByteArray())
        closeEntry()
    }

    private fun sha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        digest.update(file.readBytes())
        return digest.digest().joinToString("") { "%02x".format(it) }
    }
}

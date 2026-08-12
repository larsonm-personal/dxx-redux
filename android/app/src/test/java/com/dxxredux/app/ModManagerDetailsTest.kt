package com.dxxredux.app

import org.json.JSONArray
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.security.MessageDigest
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlin.io.path.createTempDirectory

class ModManagerDetailsTest {
    private val patchPath = "patches/d2/ham_patch.rfc6902.json"

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

        assertEquals(dxaFile.absolutePath, details.archivePath)
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

    @Test
    fun detailsProblemsIncludeBaseMismatchHashes() {
        val filesDir = createTempDirectory("mod-detail-mismatch").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val actualHam = File(setDir, "descent2.ham").apply { writeText("actual-ham") }
        val expectedHam = File(filesDir, "expected.ham").apply { writeText("expected-ham") }
        val actualSha256 = sha256(actualHam)
        val expectedSha256 = sha256(expectedHam)
        val dxaFile = File(modsDir, "mismatch.dxa")
        writePatchDxa(dxaFile, "mismatch", expectedSha256, "[]")
        val manager = ModManager(filesDir)
        val mod =
            ModManager.ModInfo(
                filename = dxaFile.name,
                displayName = "Mismatch",
                enabled = true,
                addedAt = 0,
                sizeBytes = dxaFile.length(),
                game = "d2",
                order = 0,
            )

        val details = manager.getModDetails(mod, setDir)
        val requirement = details.baseRequirements.single()
        val problem = details.problems.single()

        assertEquals(expectedSha256, requirement.expectedSha256)
        assertEquals(actualSha256, requirement.actualSha256)
        assertTrue(problem.contains("Base mismatch: descent2.ham"))
        assertTrue(problem.contains("Expected sha256=$expectedSha256"))
        assertTrue(problem.contains("Actual sha256=$actualSha256"))
    }

    @Test
    fun detailsCategoriesMarkExamplesTruncatedAfterFirstThreeFiles() {
        val filesDir = createTempDirectory("mod-detail-files").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val dxaFile = File(modsDir, "textures.dxa")
        ZipOutputStream(dxaFile.outputStream()).use { zip ->
            zip.writeEntry("textures/ceil001.png", "a")
            zip.writeEntry("textures/ceil015.png", "b")
            zip.writeEntry("textures/ceil016.png", "c")
            zip.writeEntry("textures/ceil017.png", "d")
        }
        val manager = ModManager(filesDir)
        val mod =
            ModManager.ModInfo(
                filename = dxaFile.name,
                displayName = "Textures",
                enabled = true,
                addedAt = 0,
                sizeBytes = dxaFile.length(),
                game = "d2",
                order = 0,
            )

        val category = manager.getModDetails(mod, setDir).categories.single()

        assertEquals("Texture replacements", category.label)
        assertEquals(4, category.count)
        assertEquals(listOf("ceil001.png", "ceil015.png", "ceil016.png"), category.examples)
        assertTrue(category.examplesTruncated)
    }

    @Test
    fun detailsPatchProblemDoesNotNameEmptyOtherMods() {
        val filesDir = createTempDirectory("mod-detail-patch-problem").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val baseHam = File(setDir, "descent2.ham").apply { writeText("base-ham") }
        val baseSha256 = sha256(baseHam)
        val dxaFile = File(modsDir, "bad_patch.dxa")
        writePatchDxa(dxaFile, "bad", baseSha256, "[{\"op\":\"replace\",\"path\":\"/bad\",\"value\":1}]")
        writeModManifest(filesDir, dxaFile.name to "Bad Patch")
        val manager = ModManager(filesDir)
        val mod = manager.listMods().single()

        val problem = manager.getModDetails(mod, setDir).problems.single()

        assertTrue(problem.startsWith("Patch problem: patches/d2/ham_patch.rfc6902.json"))
        assertFalse(problem.contains("also used by"))
        assertTrue(problem.contains("unsupported patch path /bad"))
    }

    @Test
    fun compatibilityRejectsPatchThatNativeConsumerCannotApply() {
        val filesDir = createTempDirectory("mod-native-schema-patch").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val baseHam = File(setDir, "descent2.ham").apply { writeText("base-ham") }
        val dxaFile = File(modsDir, "remove_patch.dxa")
        writePatchDxa(
            dxaFile,
            "remove",
            sha256(baseHam),
            """[{"op":"remove","path":"/sections/sounds/1/Sound"}]""",
        )
        writeModManifest(filesDir, dxaFile.name to "Remove Patch")

        val report = ModManager(filesDir).checkEnabledModCompatibility("d2", setDir)

        assertFalse(report.ok)
        assertTrue(report.toUserMessage().contains("unsupported JSON Patch operation 'remove'"))
    }

    @Test
    fun compatibilityAllowsSamePatchPathWhenWriteScopesDoNotOverlap() {
        val filesDir = createTempDirectory("mod-compatible-patches").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val baseHam = File(setDir, "descent2.ham").apply { writeText("base-ham") }
        val baseSha256 = sha256(baseHam)

        val soundPatch =
            """
            [
                {"op":"test","path":"/sections/sounds/110/Sound","value":255},
                {"op":"replace","path":"/sections/sounds/110/Sound","value":194}
            ]
            """.trimIndent()
        val texturePatch =
            """
            [
                {"op":"test","path":"/sections/wclips/9/PlayTime","value":104857},
                {"op":"replace","path":"/sections/wclips/9/PlayTime","value":65536}
            ]
            """.trimIndent()
        writePatchDxa(File(modsDir, "sound.dxa"), "sound", baseSha256, soundPatch)
        writePatchDxa(File(modsDir, "texture.dxa"), "texture", baseSha256, texturePatch)

        writeModManifest(filesDir, "sound.dxa" to "Sound Patch", "texture.dxa" to "Texture Patch")
        val manager = ModManager(filesDir)

        val report = manager.checkEnabledModCompatibility("d2", setDir)
        assertTrue(report.toUserMessage(), report.ok)

        manager.writeEnabledModPaths("d2")
        val activePaths = File(filesDir, "d2x-redux/.active_mod_paths").readLines()
        assertTrue(activePaths.first().endsWith(".generated_mod_patches"))
        val mergedPatch = File(activePaths.first(), patchPath)
        assertTrue(mergedPatch.isFile)
        assertEquals(4, JSONArray(mergedPatch.readText()).length())
    }

    @Test
    fun compatibilityReportsSamePatchPathWhenWriteScopesOverlap() {
        val filesDir = createTempDirectory("mod-conflicting-patches").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val baseHam = File(setDir, "descent2.ham").apply { writeText("base-ham") }
        val baseSha256 = sha256(baseHam)

        val firstPatch =
            """
            [
                {"op":"test","path":"/sections/wclips/9/PlayTime","value":104857},
                {"op":"replace","path":"/sections/wclips/9/PlayTime","value":104858}
            ]
            """.trimIndent()
        val secondPatch =
            """
            [
                {"op":"test","path":"/sections/wclips/9/PlayTime","value":104857},
                {"op":"replace","path":"/sections/wclips/9/PlayTime","value":65536}
            ]
            """.trimIndent()
        writePatchDxa(File(modsDir, "first.dxa"), "first", baseSha256, firstPatch)
        writePatchDxa(File(modsDir, "second.dxa"), "second", baseSha256, secondPatch)

        writeModManifest(filesDir, "first.dxa" to "First Patch", "second.dxa" to "Second Patch")
        val manager = ModManager(filesDir)

        val report = manager.checkEnabledModCompatibility("d2", setDir)
        assertFalse(report.ok)
        assertEquals(patchPath, report.patchConflicts.single().patchPath)
        assertTrue(
            report.patchConflicts
                .single()
                .details
                .single()
                .contains("/sections/wclips/9/PlayTime"),
        )
    }

    @Test
    fun compatibilityAllowsSamePatchPathWhenOverlapsWriteSameValue() {
        val filesDir = createTempDirectory("mod-same-value-patches").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val baseHam = File(setDir, "descent2.ham").apply { writeText("base-ham") }
        val baseSha256 = sha256(baseHam)

        val firstPatch =
            """
            [
                {"op":"test","path":"/sections/wclips/9/PlayTime","value":104857},
                {"op":"replace","path":"/sections/wclips/9/PlayTime","value":65536}
            ]
            """.trimIndent()
        val secondPatch =
            """
            [
                {"op":"test","path":"/sections/wclips/9/PlayTime","value":104857},
                {"op":"replace","path":"/sections/wclips/9/PlayTime","value":65536}
            ]
            """.trimIndent()
        writePatchDxa(File(modsDir, "first.dxa"), "first", baseSha256, firstPatch)
        writePatchDxa(File(modsDir, "second.dxa"), "second", baseSha256, secondPatch)

        writeModManifest(filesDir, "first.dxa" to "First Patch", "second.dxa" to "Second Patch")
        val manager = ModManager(filesDir)

        val report = manager.checkEnabledModCompatibility("d2", setDir)
        assertTrue(report.toUserMessage(), report.ok)
    }

    private fun ZipOutputStream.writeEntry(
        name: String,
        text: String,
    ) {
        putNextEntry(ZipEntry(name))
        write(text.toByteArray())
        closeEntry()
    }

    private fun writePatchDxa(
        dxaFile: File,
        packName: String,
        baseSha256: String,
        patchText: String,
    ) {
        ZipOutputStream(dxaFile.outputStream()).use { zip ->
            zip.writeEntry(
                "metadata/manifest.json",
                """
                {
                    "schema": "com.dxxredux.test-dxa.v1",
                    "pack": "$packName",
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
                                "patchPaths": ["$patchPath"]
                            }
                        ]
                    }
                }
                """.trimIndent(),
            )
            zip.writeEntry(patchPath, patchText)
        }
    }

    private fun writeModManifest(
        filesDir: File,
        vararg mods: Pair<String, String>,
    ) {
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val modEntries =
            mods.mapIndexed { index, (filename, displayName) ->
                val sizeBytes = File(modsDir, filename).length()
                """
                {
                  "filename": "$filename",
                  "displayName": "$displayName",
                  "enabled": true,
                  "addedAt": 0,
                  "sizeBytes": $sizeBytes,
                  "game": "d2",
                  "order": $index
                }
                """.trimIndent()
            }
        File(modsDir, "mod_manifest.json").writeText("{\"mods\":[${modEntries.joinToString(",")}]}")
    }

    private fun sha256(file: File): String = hash(file, "SHA-256")

    private fun hash(
        file: File,
        algorithm: String,
    ): String {
        val digest = MessageDigest.getInstance(algorithm)
        digest.update(file.readBytes())
        return digest.digest().joinToString("") { "%02x".format(it) }
    }
}

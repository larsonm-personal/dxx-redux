package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
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
    fun d1LevelSurfacePatchesAreAcceptedAndConflictsRemainVisible() {
        val filesDir = createTempDirectory("mod-d1-surfaces").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val path = "patches/d1/level_surface_patch.rfc6902.json"

        fun writeSurface(
            filename: String,
            level: String,
            texture: Int,
        ) {
            ZipOutputStream(File(modsDir, filename).outputStream()).use { zip ->
                zip.writeEntry(
                    path,
                    """[
                        {"op":"test","path":"/levels/$level/sha256","value":"source-hash"},
                        {"op":"test","path":"/levels/$level/surfaces/134:0","value":{"tmap1":14}},
                        {"op":"replace","path":"/levels/$level/surfaces/134:0","value":{"tmap1":$texture}}
                    ]""",
                )
            }
        }
        writeSurface("first.dxa", "level03.rdl", 8)
        writeSurface("second.dxa", "level17.rdl", 9)
        writeModManifest(filesDir, "first.dxa" to "First", "second.dxa" to "Second", game = "d1")
        val manager = ModManager(filesDir)
        val accepted = manager.checkEnabledModCompatibility("d1", setDir)
        assertTrue(accepted.toUserMessage(), accepted.ok)
        writeSurface("second.dxa", "level03.rdl", 9)
        val conflict = manager.checkEnabledModCompatibility("d1", setDir)
        assertFalse(conflict.ok)
        assertTrue(conflict.toUserMessage().contains("tmap1"))
    }

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
    fun detailsStreamCategoriesAndCapRetainedDocuments() {
        val filesDir = createTempDirectory("mod-detail-document-cap").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val dxaFile = File(modsDir, "many-documents.dxa")
        ZipOutputStream(dxaFile.outputStream()).use { zip ->
            repeat(80) { index -> zip.writeEntry("docs/document-${index.toString().padStart(2, '0')}.txt", "x") }
            zip.writeEntry("textures/rock001.png", "texture")
        }
        val manager = ModManager(filesDir)
        val mod =
            ModManager.ModInfo(
                filename = dxaFile.name,
                displayName = "Many Documents",
                enabled = false,
                addedAt = 0,
                sizeBytes = dxaFile.length(),
                game = "d2",
                order = 0,
            )

        val details = manager.getModDetails(mod, setDir)
        val categories = details.categories.associateBy { it.label }

        assertEquals(81, details.fileCount)
        assertEquals(80, details.documentCount)
        assertEquals(64, details.documents.size)
        assertTrue(details.documentsTruncated)
        assertEquals(80, categories.getValue("Documentation").count)
        assertEquals(1, categories.getValue("Texture replacements").count)
        assertTrue(details.problems.any { it.contains("limited to 64 of 80 documents") })
    }

    @Test
    fun detailsRejectArchivePastAdmissionEntryLimit() {
        val filesDir = createTempDirectory("mod-detail-entry-cap").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val dxaFile = File(modsDir, "too-many-entries.dxa")
        ZipOutputStream(dxaFile.outputStream()).use { zip ->
            repeat(ExtractionLimits.MAX_ENTRIES + 1) { index -> zip.writeEntry("files/$index.bin", "x") }
        }
        val manager = ModManager(filesDir)
        val mod =
            ModManager.ModInfo(
                filename = dxaFile.name,
                displayName = "Too Many Entries",
                enabled = false,
                addedAt = 0,
                sizeBytes = dxaFile.length(),
                game = "d2",
                order = 0,
            )

        val details = manager.getModDetails(mod, setDir)

        assertEquals(0, details.fileCount)
        assertTrue(details.problems.single().contains("more than ${ExtractionLimits.MAX_ENTRIES} entries"))
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

    @Test
    fun mergedTextureRowPreservesSoundFieldsInBothModOrders() {
        for (soundFirst in listOf(true, false)) {
            val filesDir = createTempDirectory("mod-row-sound-composition").toFile()
            val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
            val modsDir = File(filesDir, "mods").also { it.mkdirs() }
            val baseSha256 = sha256(File(setDir, "descent2.ham").apply { writeText("base-ham") })
            val original =
                JSONObject(
                    """{"Index":5,"PlayTime":65536,"NumFrames":1,"Frames":[1],"OpenSound":146,"CloseSound":147,"Flags":0,"Filename":"door05","Pad":0}""",
                )
            val changed = JSONObject(original.toString()).put("PlayTime", 52428)
            val texturePatch =
                JSONArray()
                    .put(JSONObject().put("op", "test").put("path", "/sections/wclips/5").put("value", original))
                    .put(JSONObject().put("op", "replace").put("path", "/sections/wclips/5").put("value", changed))
            val soundPatch = """[
                {"op":"test","path":"/sections/wclips/5/OpenSound","value":146},
                {"op":"replace","path":"/sections/wclips/5/OpenSound","value":88}
            ]"""
            writePatchDxa(File(modsDir, "sound.dxa"), "sound", baseSha256, soundPatch)
            writePatchDxa(File(modsDir, "texture.dxa"), "texture", baseSha256, texturePatch.toString())
            val order = listOf("sound.dxa" to "Sound", "texture.dxa" to "Texture")
            writeModManifest(filesDir, *(if (soundFirst) order else order.reversed()).toTypedArray())
            val manager = ModManager(filesDir)
            val report = manager.checkEnabledModCompatibility("d2", setDir)
            assertTrue(report.toUserMessage(), report.ok)
            manager.writeEnabledModPaths("d2")
            val root = File(filesDir, "d2x-redux/.active_mod_paths").readLines().first()
            val merged = JSONArray(File(root, patchPath).readText())
            val writes =
                (0 until merged.length()).map { merged.getJSONObject(it) }.filter {
                    it.getString("op") !=
                        "test"
                }
            assertEquals(1, writes.size)
            val row = writes.single().getJSONObject("value")
            assertEquals(52428, row.getInt("PlayTime"))
            assertEquals(88, row.getInt("OpenSound"))
            assertEquals(147, row.getInt("CloseSound"))
        }
    }

    @Test
    fun compatibilityRejectsExcessivePatchOperationCount() {
        val filesDir = createTempDirectory("mod-patch-operation-limit").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val baseSha256 = sha256(File(setDir, "descent2.ham").apply { writeText("base-ham") })
        val patch = JSONArray()
        repeat(4097) { patch.put(JSONObject()) }
        writePatchDxa(File(modsDir, "too_many.dxa"), "too-many", baseSha256, patch.toString())
        writeModManifest(filesDir, "too_many.dxa" to "Too Many Operations")

        val report = ModManager(filesDir).checkEnabledModCompatibility("d2", setDir)

        assertFalse(report.ok)
        assertTrue(report.toUserMessage().contains("exceeds 4096 patch operations"))
    }

    @Test
    fun conflictDiagnosticsAreCapped() {
        val filesDir = createTempDirectory("mod-patch-conflict-cap").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        val modsDir = File(filesDir, "mods").also { it.mkdirs() }
        val d1PatchPath = "patches/d1/level_surface_patch.rfc6902.json"

        fun writePatch(
            filename: String,
            value: Int,
        ) {
            val operations = JSONArray()
            repeat(70) { segment ->
                operations.put(
                    JSONObject()
                        .put("op", "replace")
                        .put("path", "/levels/level03.rdl/surfaces/$segment:0")
                        .put("value", JSONObject().put("tmap1", value)),
                )
            }
            ZipOutputStream(File(modsDir, filename).outputStream()).use { zip ->
                zip.writeEntry(d1PatchPath, operations.toString())
            }
        }

        writePatch("first.dxa", 1)
        writePatch("second.dxa", 2)
        writeModManifest(filesDir, "first.dxa" to "First", "second.dxa" to "Second", game = "d1")

        val conflict = ModManager(filesDir).checkEnabledModCompatibility("d1", setDir).patchConflicts.single()

        assertEquals(65, conflict.details.size)
        assertEquals("Additional conflict details omitted", conflict.details.last())
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
        game: String = "d2",
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
                  "game": "$game",
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

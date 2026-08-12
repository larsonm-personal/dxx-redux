package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.util.Locale
import kotlin.io.path.createTempDirectory

class AssetManifestTest {
    @Test
    fun duplicateManifestEntryWithMatchingDiskSizeIsNotStale() {
        val setDir = createTempDirectory("asset-manifest-duplicates").toFile()
        File(setDir, "descent.pig").writeBytes(ByteArray(17))
        writeManifest(
            setDir,
            entry("descent.pig", sizeBytes = 17, importedAt = 200),
            entry("descent.pig", sizeBytes = 1, importedAt = 100),
        )

        val stale = AssetManifest(setDir).findStaleFiles(setOf("descent.pig"))

        assertTrue(stale.isEmpty())
    }

    @Test
    fun diskCaseDuplicateUsesLowercaseCanonicalFile() {
        val setDir = createTempDirectory("asset-manifest-disk-case-duplicates").toFile()
        File(setDir, "DESCENT.HOG").writeBytes(ByteArray(1))
        File(setDir, "descent.hog").writeBytes(ByteArray(17))
        writeManifest(
            setDir,
            entry("descent.hog", sizeBytes = 17, importedAt = 200),
        )

        val stale = AssetManifest(setDir).findStaleFiles(setOf("descent.hog"))

        assertTrue(stale.isEmpty())
    }

    @Test
    fun upsertRemovesDuplicateManifestEntries() {
        val setDir = createTempDirectory("asset-manifest-upsert-duplicates").toFile()
        File(setDir, "descent.pig").writeBytes(ByteArray(17))
        writeManifest(
            setDir,
            entry("descent.pig", sizeBytes = 1, importedAt = 100),
            entry("descent.pig", sizeBytes = 2, importedAt = 200),
        )

        AssetManifest(setDir).upsert("descent.pig", "abcdef", 17)

        val raw = JSONArray(File(setDir, "assets.json").readText())
        assertEquals(1, raw.length())
        assertEquals(17L, raw.getJSONObject(0).getLong("sizeBytes"))
    }

    @Test
    fun independentlyLoadedWritersPreserveBothUpdates() {
        val setDir = createTempDirectory("asset-manifest-two-writers").toFile()
        val first = AssetManifest(setDir)
        val second = AssetManifest(setDir)

        first.upsert("descent.hog", "aa", 1)
        second.upsert("descent.pig", "bb", 2)

        assertEquals(setOf("descent.hog", "descent.pig"), AssetManifest(setDir).load().map { it.filename }.toSet())
    }

    @Test
    fun portableIdentityIsStableAndRepairsTurkishFoldedDiskNames() {
        val previous = Locale.getDefault()
        val setDir = createTempDirectory("asset-manifest-turkish").toFile()
        try {
            Locale.setDefault(Locale.forLanguageTag("tr-TR"))
            val malformedName = "descent.p\u0131g"
            File(setDir, malformedName).writeBytes(ByteArray(17))
            writeManifest(setDir, entry(malformedName, sizeBytes = 17, importedAt = 100))

            val manifest = AssetManifest(setDir)
            assertEquals("descent.pig", manifest.load().single().filename)
            assertTrue(manifest.findStaleFiles(setOf("descent.pig")).isEmpty())
            assertTrue(File(setDir, "descent.pig").isFile)

            manifest.upsert("ALIEN1.PIG", "ABCDEF", 17)
            assertTrue(manifest.getEntry("alien1.pig") != null)
        } finally {
            Locale.setDefault(previous)
        }
    }

    private fun writeManifest(
        setDir: File,
        vararg entries: JSONObject,
    ) {
        val array = JSONArray()
        for (entry in entries) array.put(entry)
        File(setDir, "assets.json").writeText(array.toString(2))
    }

    private fun entry(
        filename: String,
        sizeBytes: Long,
        importedAt: Long,
    ): JSONObject =
        JSONObject()
            .put("filename", filename)
            .put("sha256", "0123456789abcdef")
            .put("sizeBytes", sizeBytes)
            .put("importedAt", importedAt)
}

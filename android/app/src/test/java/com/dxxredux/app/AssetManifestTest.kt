package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
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

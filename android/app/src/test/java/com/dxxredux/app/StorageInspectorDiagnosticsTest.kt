package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class StorageInspectorDiagnosticsTest {
    @Test
    fun exportIncludesFileOwnershipAndEverySafReference() {
        val snapshot =
            StorageInspectorDiagnosticSnapshot(
                generatedAtUtc = "2026-08-16T12:34:56.789Z",
                build = StorageInspectorBuildDiagnostic("123", "abcdef0", "debug"),
                device = StorageInspectorDeviceDiagnostic("Example", "Phone", 35, listOf("arm64-v8a")),
                filesDir = "/data/user/0/com.dxxredux.app/files",
                activeImportRoot = "/storage/example/imported",
                importRootOverrideActive = true,
                fileCount = 1,
                totalSizeBytes = 42,
                files =
                    listOf(
                        StorageInspectorFileDiagnostic(
                            location = "external",
                            relativePath = "mods/cache/level01.rl2",
                            absolutePath = "/storage/example/imported/mods/cache/level01.rl2",
                            purpose = "Linked mission ZIP file",
                            sizeBytes = 42,
                            lastModifiedUnixMs = 1234,
                            linkedMissionZipOwner = "example.zip",
                            linkedMissionZipSourceExists = false,
                        ),
                    ),
                safEntries =
                    listOf(
                        StorageInspectorSafDiagnostic(
                            key = "cd:disc-id:content://example/disc.cue",
                            label = "CD Source: Example",
                            displayUri = "content://example/disc.cue",
                            accessible = false,
                            referencedUris =
                                listOf(
                                    StorageInspectorSafUriDiagnostic(
                                        "content://example/disc.bin",
                                        "file_descriptor",
                                        false,
                                    ),
                                    StorageInspectorSafUriDiagnostic(
                                        "content://example/disc.cue",
                                        "stream",
                                        true,
                                    ),
                                ),
                            kind = "cd_audio_source",
                            cdSourceId = "disc-id",
                        ),
                    ),
                persistedUriPermissions =
                    listOf(StorageInspectorPermissionDiagnostic("content://example/tree", true, false)),
            )

        val root = JSONObject(StorageInspectorDiagnostics.encode(snapshot))
        val file = root.getJSONArray("files").getJSONObject(0)
        val saf = root.getJSONArray("saf_entries").getJSONObject(0)

        assertEquals(1, root.getInt("schema_version"))
        assertEquals(1, root.getInt("file_count"))
        assertEquals("example.zip", file.getString("linked_mission_zip_owner"))
        assertEquals(false, file.getBoolean("linked_mission_zip_source_exists"))
        assertEquals(2, saf.getJSONArray("referenced_uris").length())
        assertEquals(false, saf.getBoolean("accessible"))
        assertEquals(1, root.getJSONArray("persisted_uri_permissions").length())
        assertTrue(StorageInspectorDiagnostics.encode(snapshot).endsWith("\n"))
    }
}

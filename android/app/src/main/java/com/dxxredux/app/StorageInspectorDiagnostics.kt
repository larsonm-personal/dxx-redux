package com.dxxredux.app

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json

@Serializable
internal data class StorageInspectorDiagnosticSnapshot(
    @SerialName("schema_version") val schemaVersion: Int = 1,
    @SerialName("generated_at_utc") val generatedAtUtc: String,
    val build: StorageInspectorBuildDiagnostic,
    val device: StorageInspectorDeviceDiagnostic,
    @SerialName("files_dir") val filesDir: String,
    @SerialName("active_import_root") val activeImportRoot: String,
    @SerialName("import_root_override_active") val importRootOverrideActive: Boolean,
    @SerialName("file_count") val fileCount: Int,
    @SerialName("total_size_bytes") val totalSizeBytes: Long,
    val files: List<StorageInspectorFileDiagnostic>,
    @SerialName("saf_entries") val safEntries: List<StorageInspectorSafDiagnostic>,
    @SerialName("persisted_uri_permissions")
    val persistedUriPermissions: List<StorageInspectorPermissionDiagnostic>,
)

@Serializable
internal data class StorageInspectorBuildDiagnostic(
    @SerialName("commit_count") val commitCount: String,
    @SerialName("short_hash") val shortHash: String,
    val type: String,
)

@Serializable
internal data class StorageInspectorDeviceDiagnostic(
    val manufacturer: String,
    val model: String,
    @SerialName("sdk_int") val sdkInt: Int,
    @SerialName("supported_abis") val supportedAbis: List<String>,
)

@Serializable
internal data class StorageInspectorFileDiagnostic(
    val location: String,
    @SerialName("relative_path") val relativePath: String,
    @SerialName("absolute_path") val absolutePath: String,
    val purpose: String,
    @SerialName("size_bytes") val sizeBytes: Long,
    @SerialName("last_modified_unix_ms") val lastModifiedUnixMs: Long,
    @SerialName("helper_symlink_target_name") val helperSymlinkTargetName: String? = null,
    @SerialName("linked_mission_zip_owner") val linkedMissionZipOwner: String? = null,
    @SerialName("linked_mission_zip_source_exists") val linkedMissionZipSourceExists: Boolean = false,
    @SerialName("is_linked_mission_zip_owner") val isLinkedMissionZipOwner: Boolean = false,
)

@Serializable
internal data class StorageInspectorSafDiagnostic(
    val key: String,
    val label: String,
    @SerialName("display_uri") val displayUri: String,
    val accessible: Boolean,
    @SerialName("referenced_uris") val referencedUris: List<StorageInspectorSafUriDiagnostic>,
    val kind: String,
    @SerialName("custom_set_id") val customSetId: String? = null,
    @SerialName("custom_filename") val customFilename: String? = null,
    @SerialName("cd_source_id") val cdSourceId: String? = null,
)

@Serializable
internal data class StorageInspectorSafUriDiagnostic(
    val uri: String,
    @SerialName("access_method") val accessMethod: String,
    val accessible: Boolean,
)

@Serializable
internal data class StorageInspectorPermissionDiagnostic(
    val uri: String,
    @SerialName("read_permission") val readPermission: Boolean,
    @SerialName("write_permission") val writePermission: Boolean,
)

internal object StorageInspectorDiagnostics {
    private val json =
        Json {
            prettyPrint = true
            encodeDefaults = true
        }

    fun encode(snapshot: StorageInspectorDiagnosticSnapshot): String = json.encodeToString(snapshot) + "\n"
}

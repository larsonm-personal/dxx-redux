package com.dxxredux.app

import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

object SaveExplorerBridge {
    private const val TAG = "DXX-SaveExplorer"

    init {
        System.loadLibrary("dxx-redux-d2")
    }

    data class SaveExplorerSlot(
        val path: String,
        val relativePath: String,
        val game: String,
        val scope: String,
        val pilot: String,
        val missionKey: String,
        val saveKind: String,
        val saveTimeUnixSeconds: Long,
        val callsign: String,
        val description: String,
        val missionName: String,
        val levelNum: Int,
        val levelName: String,
        val levelSeconds: Long,
        val totalSeconds: Long,
        val difficultyChanged: Boolean,
        val difficultyMin: Int,
        val difficultyMax: Int,
        val slot: Int,
        val hasThumbnail: Boolean,
        val thumbnailWidth: Int,
        val thumbnailHeight: Int,
        val metadataBacked: Boolean,
        val loadable: Boolean,
        val orphan: Boolean,
        val orphanReason: String,
        val sizeBytes: Long,
        val modifiedUnixSeconds: Long,
    ) {
        fun toResumeCandidate(): ResumeSaveBridge.ResumeSaveCandidate? {
            if (!loadable) return null
            val thumbnailRgb6 =
                if (hasThumbnail) {
                    ResumeSaveBridge.readThumbnailRgb6(path)
                } else {
                    null
                }
            return ResumeSaveBridge.ResumeSaveCandidate(
                path = path,
                relativePath = relativePath,
                game = game,
                saveKind = saveKind,
                saveTimeUnixSeconds = saveTimeUnixSeconds,
                callsign = callsign,
                description = description,
                missionName = missionName,
                levelNum = levelNum,
                levelName = levelName,
                levelSeconds = levelSeconds,
                totalSeconds = totalSeconds,
                difficultyChanged = difficultyChanged,
                difficultyMin = difficultyMin,
                difficultyMax = difficultyMax,
                slot = slot,
                hasThumbnail = hasThumbnail && thumbnailRgb6 != null,
                thumbnailWidth = thumbnailWidth,
                thumbnailHeight = thumbnailHeight,
                metadataBacked = metadataBacked,
                thumbnailRgb6 = thumbnailRgb6,
            )
        }
    }

    data class DeleteResult(
        val deleted: Boolean,
        val reason: String,
    )

    fun listSlots(filesDir: File): List<SaveExplorerSlot> {
        val raw =
            try {
                nativeListSaveSlots(filesDir.absolutePath)
            } catch (e: UnsatisfiedLinkError) {
                Log.w(TAG, "Save-explorer bridge unavailable", e)
                null
            } ?: return emptyList()

        return try {
            val array = JSONObject(raw).optJSONArray("slots") ?: JSONArray()
            buildList {
                for (index in 0 until array.length()) {
                    add(parseSlot(array.getJSONObject(index)))
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to parse save-explorer JSON", e)
            emptyList()
        }
    }

    fun deleteSlot(
        filesDir: File,
        slot: SaveExplorerSlot,
    ): DeleteResult {
        val raw =
            try {
                nativeDeleteSaveSlot(
                    filesDir.absolutePath,
                    slot.path,
                    slot.saveTimeUnixSeconds,
                    slot.slot,
                )
            } catch (e: UnsatisfiedLinkError) {
                Log.w(TAG, "Save-explorer bridge unavailable", e)
                null
            } ?: return DeleteResult(false, "bridge_unavailable")

        return try {
            val obj = JSONObject(raw)
            DeleteResult(
                deleted = obj.optBoolean("deleted"),
                reason = obj.optString("reason"),
            )
        } catch (e: Exception) {
            Log.w(TAG, "Failed to parse save-explorer delete JSON", e)
            DeleteResult(false, "bad_delete_result")
        }
    }

    private fun parseSlot(obj: JSONObject): SaveExplorerSlot =
        SaveExplorerSlot(
            path = obj.optString("path"),
            relativePath = obj.optString("relative_path"),
            game = obj.optString("game"),
            scope = obj.optString("scope", "single"),
            pilot = obj.optString("pilot"),
            missionKey = obj.optString("mission_key"),
            saveKind = obj.optString("save_kind"),
            saveTimeUnixSeconds = obj.optLong("save_time_unix_seconds"),
            callsign = obj.optString("callsign"),
            description = obj.optString("description"),
            missionName = obj.optString("mission_name"),
            levelNum = obj.optInt("level_num"),
            levelName = obj.optString("level_name"),
            levelSeconds = obj.optLong("level_seconds"),
            totalSeconds = obj.optLong("total_seconds"),
            difficultyChanged = obj.optBoolean("difficulty_changed"),
            difficultyMin = obj.optInt("difficulty_min"),
            difficultyMax = obj.optInt("difficulty_max"),
            slot = obj.optInt("slot", -1),
            hasThumbnail = obj.optBoolean("has_thumbnail"),
            thumbnailWidth = obj.optInt("thumbnail_width"),
            thumbnailHeight = obj.optInt("thumbnail_height"),
            metadataBacked = obj.optBoolean("metadata_backed"),
            loadable = obj.optBoolean("loadable"),
            orphan = obj.optBoolean("orphan"),
            orphanReason = obj.optString("orphan_reason"),
            sizeBytes = obj.optLong("size_bytes"),
            modifiedUnixSeconds = obj.optLong("modified_unix_seconds"),
        )

    private external fun nativeListSaveSlots(filesDir: String): String?

    private external fun nativeDeleteSaveSlot(
        filesDir: String,
        path: String,
        expectedTimestamp: Long,
        expectedSlot: Int,
    ): String?
}

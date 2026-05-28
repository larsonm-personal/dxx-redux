package com.dxxredux.app

import android.util.Log
import org.json.JSONObject
import java.io.File

object ResumeSaveBridge {
    private const val TAG = "DXX-ResumeSave"

    init {
        System.loadLibrary("dxx-redux-d2")
    }

    data class ResumeSaveCandidate(
        val path: String,
        val relativePath: String,
        val game: String,
        val saveKind: String,
        val saveTimeUnixSeconds: Long,
        val callsign: String,
        val description: String,
        val missionName: String,
        val levelNum: Int,
        val levelName: String,
        val levelSeconds: Long,
        val totalSeconds: Long,
        val slot: Int,
        val hasThumbnail: Boolean,
        val thumbnailWidth: Int,
        val thumbnailHeight: Int,
        val metadataBacked: Boolean,
        val thumbnailRgb6: ByteArray?,
    ) {
        fun toJson(): JSONObject =
            JSONObject().apply {
                put("path", path)
                put("relative_path", relativePath)
                put("game", game)
                put("save_kind", saveKind)
                put("save_time_unix_seconds", saveTimeUnixSeconds)
                put("callsign", callsign)
                put("description", description)
                put("mission_name", missionName)
                put("level_num", levelNum)
                put("level_name", levelName)
                put("level_seconds", levelSeconds)
                put("total_seconds", totalSeconds)
                put("slot", slot)
                put("has_thumbnail", hasThumbnail)
                put("thumbnail_width", thumbnailWidth)
                put("thumbnail_height", thumbnailHeight)
                put("metadata_backed", metadataBacked)
            }
    }

    data class ResumeSaveOptions(
        val latestOverall: ResumeSaveCandidate?,
        val highestProgress: ResumeSaveCandidate?,
        val lastExit: ResumeSaveCandidate?,
        val lastMinimize: ResumeSaveCandidate?,
    )

    fun findNewest(filesDir: File): ResumeSaveCandidate? = findNewest(filesDir.absolutePath)

    fun findNewest(filesDir: String): ResumeSaveCandidate? {
        val raw =
            try {
                nativeFindNewestSave(filesDir)
            } catch (e: UnsatisfiedLinkError) {
                Log.w(TAG, "Resume-save bridge unavailable", e)
                null
            } ?: return null

        return try {
            parseCandidate(JSONObject(raw))
        } catch (e: Exception) {
            Log.w(TAG, "Failed to parse native resume-save JSON", e)
            null
        }
    }

    fun findOptions(filesDir: File): ResumeSaveOptions? = findOptions(filesDir.absolutePath)

    fun findOptions(filesDir: String): ResumeSaveOptions? {
        val raw =
            try {
                nativeFindSaveOptions(filesDir)
            } catch (e: UnsatisfiedLinkError) {
                Log.w(TAG, "Resume-save bridge unavailable", e)
                null
            } ?: return null

        return try {
            val obj = JSONObject(raw)
            ResumeSaveOptions(
                latestOverall = obj.optCandidate("latest_overall"),
                highestProgress = obj.optCandidate("highest_progress"),
                lastExit = obj.optCandidate("last_exit"),
                lastMinimize = obj.optCandidate("last_minimize"),
            )
        } catch (e: Exception) {
            Log.w(TAG, "Failed to parse native resume-save options JSON", e)
            null
        }
    }

    private fun JSONObject.optCandidate(name: String): ResumeSaveCandidate? =
        if (has(name) && !isNull(name)) {
            parseCandidate(getJSONObject(name))
        } else {
            null
        }

    private fun parseCandidate(obj: JSONObject): ResumeSaveCandidate {
        val path = obj.optString("path")
        val nativeHasThumbnail = obj.optBoolean("has_thumbnail")
        val thumbnailRgb6 =
            if (nativeHasThumbnail && path.isNotBlank()) {
                try {
                    nativeReadThumbnailRgb6(path)
                } catch (e: Exception) {
                    Log.w(TAG, "Failed to read resume-save thumbnail", e)
                    null
                }
            } else {
                null
            }
        return ResumeSaveCandidate(
            path = path,
            relativePath = obj.optString("relative_path"),
            game = obj.optString("game"),
            saveKind = obj.optString("save_kind"),
            saveTimeUnixSeconds = obj.optLong("save_time_unix_seconds"),
            callsign = obj.optString("callsign"),
            description = obj.optString("description"),
            missionName = obj.optString("mission_name"),
            levelNum = obj.optInt("level_num"),
            levelName = obj.optString("level_name"),
            levelSeconds = obj.optLong("level_seconds"),
            totalSeconds = obj.optLong("total_seconds"),
            slot = obj.optInt("slot", -1),
            hasThumbnail = nativeHasThumbnail && thumbnailRgb6 != null,
            thumbnailWidth = obj.optInt("thumbnail_width"),
            thumbnailHeight = obj.optInt("thumbnail_height"),
            metadataBacked = obj.optBoolean("metadata_backed"),
            thumbnailRgb6 = thumbnailRgb6,
        )
    }

    private external fun nativeFindNewestSave(filesDir: String): String?

    private external fun nativeFindSaveOptions(filesDir: String): String?

    private external fun nativeReadThumbnailRgb6(savePath: String): ByteArray?
}

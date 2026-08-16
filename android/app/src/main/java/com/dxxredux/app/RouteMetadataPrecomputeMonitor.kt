package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

internal data class RouteMetadataPrecomputeSnapshot(
    val totalLevels: Int = 0,
    val finishedLevels: Int = 0,
    val failedLevels: Int = 0,
    val currentMission: String = "",
    val currentLevel: String = "",
    val currentPriority: String = "",
    val updatedAtMs: Long = 0L,
)

internal class RouteMetadataPrecomputeMonitor(
    private val filesDir: File,
) {
    val logFile = File(filesDir, "route_metadata_precompute.log")
    private val stateFile = File(filesDir, "route_metadata_precompute_status.json")

    fun update(
        jobs: List<RouteMetadataPrecomputeJob>,
        entries: Map<String, RouteMetadataLedgerEntry>,
        current: RouteMetadataPrecomputeJob? = null,
        priority: RouteMetadataPriority? = null,
    ) = synchronized(FILE_LOCK) {
        val finished = jobs.count { entries[it.id]?.status in TERMINAL_STATUSES }
        val failed = jobs.count { entries[it.id]?.status == RouteMetadataLedgerStatus.FAILED }
        val previous = readState()
        writeState(
            snapshot =
                RouteMetadataPrecomputeSnapshot(
                    totalLevels = jobs.size,
                    finishedLevels = finished,
                    failedLevels = failed,
                    currentMission = current?.target?.displayName.orEmpty(),
                    currentLevel = current?.let(::levelLabel).orEmpty(),
                    currentPriority = priority?.wireName.orEmpty(),
                    updatedAtMs = System.currentTimeMillis(),
                ),
            loggedMissions = previous.second,
        )
    }

    fun levelFinished(
        job: RouteMetadataPrecomputeJob,
        status: RouteMetadataLedgerStatus,
        elapsedMs: Long,
        jobs: List<RouteMetadataPrecomputeJob>,
        entries: Map<String, RouteMetadataLedgerEntry>,
    ) = synchronized(FILE_LOCK) {
        append(
            "LEVEL mission=${job.target.displayName} level=${levelLabel(job)} " +
                "status=${status.wireName} duration_ms=$elapsedMs",
        )
        val missionJobs = jobs.filter { it.sourceIdentity == job.sourceIdentity }
        val state = readState()
        if (missionJobs.isNotEmpty() &&
            job.sourceIdentity !in state.second &&
            missionJobs.all { entries[it.id]?.status in TERMINAL_STATUSES }
        ) {
            val failed = missionJobs.count { entries[it.id]?.status == RouteMetadataLedgerStatus.FAILED }
            append(
                "MISSION mission=${job.target.displayName} levels=${missionJobs.size} " +
                    "failed=$failed status=${if (failed == 0) "complete" else "complete_with_failures"}",
            )
            writeState(state.first, state.second + job.sourceIdentity)
        }
    }

    fun prioritySwitch(
        mission: String,
        level: String,
        reason: String,
        priority: RouteMetadataPriority,
    ) = synchronized(FILE_LOCK) {
        append("SWITCH mission=$mission level=$level priority=${priority.wireName} reason=$reason")
    }

    fun readSnapshot(): RouteMetadataPrecomputeSnapshot = synchronized(FILE_LOCK) { readState().first }

    fun readRecentLines(limit: Int = 80): List<String> =
        synchronized(FILE_LOCK) {
            runCatching { logFile.readLines(Charsets.UTF_8).takeLast(limit.coerceAtLeast(1)) }
                .getOrDefault(emptyList())
        }

    private fun append(message: String) {
        logFile.parentFile?.mkdirs()
        val timestamp = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US).format(Date())
        logFile.appendText("$timestamp $message\n", Charsets.UTF_8)
    }

    private fun readState(): Pair<RouteMetadataPrecomputeSnapshot, Set<String>> =
        runCatching {
            val root = JSONObject(stateFile.readText(Charsets.UTF_8))
            val missions = root.optJSONArray("logged_missions") ?: JSONArray()
            RouteMetadataPrecomputeSnapshot(
                totalLevels = root.optInt("total_levels"),
                finishedLevels = root.optInt("finished_levels"),
                failedLevels = root.optInt("failed_levels"),
                currentMission = root.optString("current_mission"),
                currentLevel = root.optString("current_level"),
                currentPriority = root.optString("current_priority"),
                updatedAtMs = root.optLong("updated_at_ms"),
            ) to
                buildSet {
                    repeat(missions.length()) { index -> add(missions.optString(index)) }
                }
        }.getOrDefault(RouteMetadataPrecomputeSnapshot() to emptySet())

    private fun writeState(
        snapshot: RouteMetadataPrecomputeSnapshot,
        loggedMissions: Set<String>,
    ) {
        AtomicFilePublication.writeUtf8(
            stateFile,
            JSONObject()
                .put("schema", "dxx-route-precompute-status-v1")
                .put("total_levels", snapshot.totalLevels)
                .put("finished_levels", snapshot.finishedLevels)
                .put("failed_levels", snapshot.failedLevels)
                .put("current_mission", snapshot.currentMission)
                .put("current_level", snapshot.currentLevel)
                .put("current_priority", snapshot.currentPriority)
                .put("updated_at_ms", snapshot.updatedAtMs)
                .put("logged_missions", JSONArray(loggedMissions.sorted()))
                .toString(2) + "\n",
        )
    }

    private fun levelLabel(job: RouteMetadataPrecomputeJob): String =
        "${job.target.levelNum}:${job.target.levelFile.orEmpty()}"

    private companion object {
        val FILE_LOCK = Any()
        val TERMINAL_STATUSES = setOf(RouteMetadataLedgerStatus.COMPLETE, RouteMetadataLedgerStatus.FAILED)
    }
}

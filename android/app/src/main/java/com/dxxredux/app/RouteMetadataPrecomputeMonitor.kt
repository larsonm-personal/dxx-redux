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
    val currentDetail: String = "",
    val currentProgressCompleted: Int = 0,
    val currentProgressTotal: Int = 0,
    val phase: String = "discovering",
    val statusMessage: String = "",
    val updatedAtMs: Long = 0L,
)

internal class RouteMetadataPrecomputeMonitor(
    private val filesDir: File,
) {
    val logFile = File(filesDir, "route_metadata_precompute.log")
    private val stateFile = File(filesDir, "route_metadata_precompute_status.json")

    fun discoveryStarted() =
        synchronized(FILE_LOCK) {
            val state = readState()
            writeState(
                state.first.copy(
                    phase = "discovering",
                    statusMessage = "Scanning installed game data and missions",
                    updatedAtMs = System.currentTimeMillis(),
                ),
                state.second,
            )
            append("DISCOVERY status=started")
        }

    fun discoveryFinished(levelCount: Int) =
        synchronized(FILE_LOCK) {
            append("DISCOVERY status=complete levels=$levelCount")
        }

    fun discoveryFailed(error: Throwable) =
        synchronized(FILE_LOCK) {
            val state = readState()
            val detail = error.message.orEmpty().ifBlank { error.javaClass.simpleName }
            writeState(
                state.first.copy(
                    phase = "discovery_failed",
                    statusMessage = detail,
                    updatedAtMs = System.currentTimeMillis(),
                ),
                state.second,
            )
            append("DISCOVERY status=failed problem=${singleLine(detail)}")
        }

    fun update(
        jobs: List<RouteMetadataPrecomputeJob>,
        entries: Map<String, RouteMetadataLedgerEntry>,
        current: RouteMetadataPrecomputeJob? = null,
        priority: RouteMetadataPriority? = null,
    ) = synchronized(FILE_LOCK) {
        val finished = jobs.count { entries[it.id]?.status in TERMINAL_STATUSES }
        val failed = jobs.count { entries[it.id]?.status == RouteMetadataLedgerStatus.FAILED }
        val previous = readState()
        val sameCurrent =
            current != null &&
                previous.first.currentMission == current.target.displayName &&
                previous.first.currentLevel == levelLabel(current)
        writeState(
            snapshot =
                RouteMetadataPrecomputeSnapshot(
                    totalLevels = jobs.size,
                    finishedLevels = finished,
                    failedLevels = failed,
                    currentMission = current?.target?.displayName.orEmpty(),
                    currentLevel = current?.let(::levelLabel).orEmpty(),
                    currentPriority = priority?.wireName.orEmpty(),
                    currentDetail = if (sameCurrent) previous.first.currentDetail else "",
                    currentProgressCompleted = if (sameCurrent) previous.first.currentProgressCompleted else 0,
                    currentProgressTotal = if (sameCurrent) previous.first.currentProgressTotal else 0,
                    phase =
                        when {
                            current != null -> "analyzing"
                            jobs.isNotEmpty() && finished == jobs.size -> "complete"
                            else -> "idle"
                        },
                    statusMessage = "",
                    updatedAtMs = System.currentTimeMillis(),
                ),
            loggedMissions = previous.second,
        )
    }

    fun analysisProgress(
        job: RouteMetadataPrecomputeJob,
        priority: RouteMetadataPriority,
        progress: LevelMetadataAnalysisProgress,
    ) = synchronized(FILE_LOCK) {
        val state = readState()
        val estimated = progress.estimatedLevel ?: progress.currentLevel ?: progress.overall
        writeState(
            state.first.copy(
                currentMission = job.target.displayName,
                currentLevel = levelLabel(job),
                currentPriority = priority.wireName,
                currentDetail = progress.currentLevel?.label ?: progress.overall.label,
                currentProgressCompleted = estimated.completed,
                currentProgressTotal = estimated.total,
                phase = "analyzing",
                statusMessage = "",
                updatedAtMs = System.currentTimeMillis(),
            ),
            state.second,
        )
    }

    fun launchHandoff(
        status: String,
        elapsedMs: Long = 0L,
        terminatedProcesses: Int = 0,
    ) = synchronized(FILE_LOCK) {
        val state = readState()
        writeState(
            state.first.copy(
                currentMission = "",
                currentLevel = "",
                currentPriority = "",
                currentDetail = "",
                currentProgressCompleted = 0,
                currentProgressTotal = 0,
                phase = if (status == "started") "pausing_for_game" else "paused_for_game",
                statusMessage = if (status == "timeout") "Metadata shutdown timed out; game launch continued" else "",
                updatedAtMs = System.currentTimeMillis(),
            ),
            state.second,
        )
        append("GAME_HANDOFF status=$status elapsed_ms=$elapsedMs terminated_processes=$terminatedProcesses")
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

    fun coordinatorEvent(
        status: String,
        reason: String,
        mission: String = "",
    ) = synchronized(FILE_LOCK) {
        append(
            "STATE status=$status reason=${singleLine(reason)}" +
                mission.takeIf { it.isNotBlank() }?.let { " mission=${singleLine(it)}" }.orEmpty(),
        )
    }

    fun retryDeferred(job: RouteMetadataPrecomputeJob) =
        synchronized(FILE_LOCK) {
            append("RETRY mission=${job.target.displayName} level=${levelLabel(job)} action=deferred")
        }

    fun heartbeat(
        job: RouteMetadataPrecomputeJob,
        priority: RouteMetadataPriority,
        detail: String,
    ) = synchronized(FILE_LOCK) {
        append(
            "HEARTBEAT mission=${job.target.displayName} level=${levelLabel(job)} " +
                "priority=${priority.wireName} detail=${singleLine(detail)}",
        )
    }

    fun inGameLevelFinished(
        mission: String,
        levelNum: Int,
        levelFile: String,
        status: RouteMetadataLedgerStatus,
        priority: RouteMetadataPriority,
        elapsedMs: Long,
    ) = synchronized(FILE_LOCK) {
        append(
            "LEVEL mission=${singleLine(mission)} level=$levelNum:$levelFile " +
                "status=${status.wireName} duration_ms=$elapsedMs context=in_game " +
                "priority=${priority.wireName} cpu_duty_percent=${priority.cpuDutyPercent}",
        )
    }

    fun inGameMissionFinished(
        mission: String,
        levelCount: Int,
    ) = synchronized(FILE_LOCK) {
        append(
            "MISSION mission=${singleLine(mission)} levels=$levelCount status=idle context=in_game cpu_duty_percent=0",
        )
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
                currentDetail = root.optString("current_detail"),
                currentProgressCompleted = root.optInt("current_progress_completed"),
                currentProgressTotal = root.optInt("current_progress_total"),
                phase =
                    root.optString("phase").ifBlank {
                        if (root.optInt("total_levels") == 0) "discovering" else "idle"
                    },
                statusMessage = root.optString("status_message"),
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
                .put("current_detail", snapshot.currentDetail)
                .put("current_progress_completed", snapshot.currentProgressCompleted)
                .put("current_progress_total", snapshot.currentProgressTotal)
                .put("phase", snapshot.phase)
                .put("status_message", snapshot.statusMessage)
                .put("updated_at_ms", snapshot.updatedAtMs)
                .put("logged_missions", JSONArray(loggedMissions.sorted()))
                .toString(2) + "\n",
        )
    }

    private fun levelLabel(job: RouteMetadataPrecomputeJob): String =
        "${job.target.levelNum}:${job.target.levelFile.orEmpty()}"

    private fun singleLine(value: String): String = value.replace(Regex("[\\r\\n]+"), " ").take(500)

    private companion object {
        val FILE_LOCK = Any()
        val TERMINAL_STATUSES = setOf(RouteMetadataLedgerStatus.COMPLETE, RouteMetadataLedgerStatus.FAILED)
    }
}

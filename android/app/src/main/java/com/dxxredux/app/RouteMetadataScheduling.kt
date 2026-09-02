package com.dxxredux.app

import android.util.Log
import org.json.JSONObject
import java.io.File
import java.io.RandomAccessFile
import java.util.Locale

// Keep in sync with ROUTE_ANALYSIS_CACHE_GENERATION in route_analysis_cache.h.
internal const val ROUTE_METADATA_CACHE_GENERATION = 23

internal object RouteMetadataDiagnostics {
    fun log(message: String) {
        DebugLog.log(DebugLogCategory.PROFILING, message)
        Log.i("DXX-RouteMetadata", message)
    }
}

internal enum class RouteMetadataPriority(
    val wireName: String,
    val threadPriority: Int,
    val cpuDutyPercent: Int,
    val fviLimit: Int,
) {
    ACTIVE("active", 10, 10, 250_000),
    NEXT("next", 15, 10, 100_000),
    FILL("fill", 19, 2, 25_000),
}

internal object RouteMetadataCpuPolicy {
    const val LAUNCHER_VISIBLE_DUTY_PERCENT = 20
    const val COMPUTE_FASTER_DUTY_PERCENT = 100

    fun launcherDutyPercent(computeFaster: Boolean): Int =
        if (computeFaster) COMPUTE_FASTER_DUTY_PERCENT else LAUNCHER_VISIBLE_DUTY_PERCENT
}

internal object RouteMetadataInGameCpuPolicy {
    private const val CONTROL_FILENAME = "route_metadata_ingame_cpu_duty"
    const val AUTOMAP_DUTY_PERCENT = 100

    fun dutyPercent(
        currentLevelCalculating: Boolean,
        automapOpen: Boolean,
    ): Int =
        if (currentLevelCalculating && automapOpen) {
            AUTOMAP_DUTY_PERCENT
        } else {
            RouteMetadataPriority.ACTIVE.cpuDutyPercent
        }

    fun controlFile(filesDir: File): File = File(filesDir, CONTROL_FILENAME)

    fun publish(
        filesDir: File,
        dutyPercent: Int,
    ) {
        AtomicFilePublication.writeUtf8(controlFile(filesDir), "${dutyPercent.coerceIn(1, 100)}\n")
    }
}

internal object RouteMetadataPreemption {
    fun shouldPreempt(
        running: RouteMetadataPriority?,
        incoming: RouteMetadataPriority,
        replacesFocus: Boolean = false,
    ): Boolean {
        running ?: return false
        return incoming.ordinal < running.ordinal ||
            (replacesFocus && incoming.ordinal == running.ordinal)
    }
}

internal data class RouteMetadataMissionLevel(
    val levelNum: Int,
    val levelFile: String,
    val priority: RouteMetadataPriority,
)

internal data class RouteMetadataCurrentDisposition(
    val skip: Boolean,
    val ready: Boolean,
    val priority: RouteMetadataPriority,
)

internal object RouteMetadataCurrentWork {
    fun forReadiness(readiness: String): RouteMetadataCurrentDisposition =
        when (readiness) {
            "complete" -> {
                RouteMetadataCurrentDisposition(
                    skip = true,
                    ready = true,
                    priority = RouteMetadataPriority.NEXT,
                )
            }

            "next_ready" -> {
                RouteMetadataCurrentDisposition(
                    skip = false,
                    ready = true,
                    priority = RouteMetadataPriority.NEXT,
                )
            }

            else -> {
                RouteMetadataCurrentDisposition(
                    skip = false,
                    ready = false,
                    priority = RouteMetadataPriority.ACTIVE,
                )
            }
        }
}

internal object RouteMetadataMissionOrdering {
    fun order(
        currentLevelNum: Int,
        currentLevelFile: String,
        normalLevelFiles: List<String>,
        secretLevelFiles: List<String>,
        secretEntryLevels: List<Int>,
    ): List<RouteMetadataMissionLevel> {
        val normal = normalLevelFiles.mapIndexed { index, file -> index + 1 to file }
        val secret = secretLevelFiles.mapIndexed { index, file -> -(index + 1) to file }
        val ordered = mutableListOf<Pair<Int, String>>()
        val nextLevelNums = mutableSetOf<Int>()

        ordered += currentLevelNum to currentLevelFile
        if (currentLevelNum > 0) {
            normal.firstOrNull { it.first == currentLevelNum + 1 }?.let {
                ordered += it
                nextLevelNums += it.first
            }
            secret.forEachIndexed { index, level ->
                if (secretEntryLevels.getOrNull(index) == currentLevelNum) {
                    ordered += level
                    nextLevelNums += level.first
                }
            }
            ordered += normal.filter { it.first > currentLevelNum }
            ordered += normal.filter { it.first < currentLevelNum }
        } else {
            val entryLevel = secretEntryLevels.getOrNull(-currentLevelNum - 1) ?: 0
            normal.firstOrNull { it.first == entryLevel + 1 }?.let { nextLevelNums += it.first }
            ordered += normal.filter { it.first > entryLevel }
            ordered += normal.filter { it.first <= entryLevel }
        }
        ordered += secret

        val seen = mutableSetOf<String>()
        return ordered
            .filter { (levelNum, file) ->
                file.isNotBlank() && seen.add("$levelNum|${file.lowercase(Locale.US)}")
            }.mapIndexed { index, (levelNum, file) ->
                RouteMetadataMissionLevel(
                    levelNum,
                    file,
                    if (index == 0) {
                        RouteMetadataPriority.ACTIVE
                    } else if (levelNum in nextLevelNums) {
                        RouteMetadataPriority.NEXT
                    } else {
                        RouteMetadataPriority.FILL
                    },
                )
            }
    }
}

internal enum class RouteMetadataLedgerStatus(
    val wireName: String,
) {
    PARTIAL("partial"),
    COMPLETE("complete"),
    FAILED("failed"),
}

internal data class RouteMetadataLedgerEntry(
    val status: RouteMetadataLedgerStatus,
    val cacheFile: String = "",
    val progressToken: String = "",
    val failureKind: String = "",
    val failureFingerprint: String = "",
    val failureCount: Int = 0,
    val updatedAtMs: Long = 0L,
)

internal class RouteMetadataLedger(
    filesDir: File,
) {
    private val stateFile = File(filesDir, "route_metadata_precompute.json")
    private val lockFile = File(filesDir, "route_metadata_precompute.lock")

    fun read(jobId: String): RouteMetadataLedgerEntry? = withEntries { it[jobId] }

    fun entries(): Map<String, RouteMetadataLedgerEntry> = withEntries { it.toMap() }

    fun update(
        jobId: String,
        transform: (RouteMetadataLedgerEntry?) -> RouteMetadataLedgerEntry?,
    ): RouteMetadataLedgerEntry? =
        withLockedEntries { entries ->
            val updated = transform(entries[jobId])
            if (updated == null) entries.remove(jobId) else entries[jobId] = updated
            updated
        }

    fun completedCache(jobId: String): String? =
        read(jobId)?.takeIf { it.status == RouteMetadataLedgerStatus.COMPLETE }?.cacheFile

    fun record(
        jobId: String,
        assessment: RouteMetadataAttemptAssessment,
        nowMs: Long = System.currentTimeMillis(),
    ): RouteMetadataLedgerEntry =
        checkNotNull(
            update(jobId) { previous ->
                val failureCount =
                    if (assessment.failureFingerprint.isBlank()) {
                        0
                    } else if (previous?.failureFingerprint == assessment.failureFingerprint) {
                        previous.failureCount + 1
                    } else {
                        1
                    }
                RouteMetadataLedgerEntry(
                    status = assessment.status,
                    cacheFile = assessment.cacheFile,
                    progressToken = assessment.progressToken,
                    failureKind = assessment.failureKind,
                    failureFingerprint = assessment.failureFingerprint,
                    failureCount = failureCount,
                    updatedAtMs = nowMs,
                )
            },
        )

    private fun <T> withEntries(block: (Map<String, RouteMetadataLedgerEntry>) -> T): T {
        lockFile.parentFile?.mkdirs()
        return RandomAccessFile(lockFile, "rw").use { lock ->
            lock.channel.lock().use { block(loadEntries()) }
        }
    }

    private fun <T> withLockedEntries(block: (MutableMap<String, RouteMetadataLedgerEntry>) -> T): T {
        lockFile.parentFile?.mkdirs()
        return RandomAccessFile(lockFile, "rw").use { lock ->
            lock.channel.lock().use {
                val entries = loadEntries().toMutableMap()
                val result = block(entries)
                saveEntries(entries)
                result
            }
        }
    }

    private fun loadEntries(): Map<String, RouteMetadataLedgerEntry> =
        runCatching {
            val root = JSONObject(stateFile.readText(Charsets.UTF_8))
            val values = root.optJSONObject("jobs") ?: root.optJSONObject("completed") ?: JSONObject()
            buildMap {
                values.keys().forEach { id ->
                    val value = values.opt(id)
                    if (value is String) {
                        put(id, RouteMetadataLedgerEntry(RouteMetadataLedgerStatus.COMPLETE, cacheFile = value))
                    } else if (value is JSONObject) {
                        val status =
                            RouteMetadataLedgerStatus.entries.firstOrNull {
                                it.wireName == value.optString("status")
                            } ?: return@forEach
                        put(
                            id,
                            RouteMetadataLedgerEntry(
                                status = status,
                                cacheFile = value.optString("cache_file"),
                                progressToken = value.optString("progress_token"),
                                failureKind = value.optString("failure_kind"),
                                failureFingerprint = value.optString("failure_fingerprint"),
                                failureCount = value.optInt("failure_count"),
                                updatedAtMs = value.optLong("updated_at_ms"),
                            ),
                        )
                    }
                }
            }
        }.getOrDefault(emptyMap())

    private fun saveEntries(entries: Map<String, RouteMetadataLedgerEntry>) {
        val jobs = JSONObject()
        entries.toSortedMap().forEach { (id, entry) ->
            jobs.put(
                id,
                JSONObject()
                    .put("status", entry.status.wireName)
                    .put("cache_file", entry.cacheFile)
                    .put("progress_token", entry.progressToken)
                    .put("failure_kind", entry.failureKind)
                    .put("failure_fingerprint", entry.failureFingerprint)
                    .put("failure_count", entry.failureCount)
                    .put("updated_at_ms", entry.updatedAtMs),
            )
        }
        AtomicFilePublication.writeUtf8(
            stateFile,
            JSONObject()
                .put("schema", "dxx-route-precompute-v2")
                .put("cache_generation", ROUTE_METADATA_CACHE_GENERATION)
                .put("jobs", jobs)
                .toString(2) + "\n",
        )
    }
}

internal data class RouteMetadataAttemptAssessment(
    val status: RouteMetadataLedgerStatus,
    val cacheFile: String,
    val progressToken: String,
    val failureKind: String,
    val failureFingerprint: String,
    val shouldRetry: Boolean,
)

internal object RouteMetadataAttemptClassifier {
    fun classify(
        result: LevelMetadataResult,
        cachePublished: Boolean,
        previous: RouteMetadataLedgerEntry?,
    ): RouteMetadataAttemptAssessment {
        val level = result.levels.singleOrNull()
        val cacheFile = level?.routeCacheFile.orEmpty()
        val progressToken =
            listOf(
                cacheFile,
                level?.routeReadiness.orEmpty(),
                level?.routeSteps?.size ?: 0,
                level?.visibilityEntries ?: 0,
                level?.visibilityCheckpointSequence ?: 0,
            ).joinToString("|")
        if (result.status == "ok" && level?.routeStatus == "ok" && cachePublished) {
            return RouteMetadataAttemptAssessment(
                RouteMetadataLedgerStatus.COMPLETE,
                cacheFile,
                progressToken,
                "",
                "",
                false,
            )
        }
        val meaningfulProgress =
            cacheFile.isNotBlank() ||
                (level?.routeSteps?.isNotEmpty() == true) ||
                (level?.visibilityEntries ?: 0) > 0 ||
                (level?.visibilityCheckpointSequence ?: 0) > 0
        if ((cachePublished && previous == null) ||
            (meaningfulProgress && progressToken != previous?.progressToken)
        ) {
            return RouteMetadataAttemptAssessment(
                RouteMetadataLedgerStatus.PARTIAL,
                cacheFile,
                progressToken,
                "",
                "",
                true,
            )
        }
        val failureKind =
            level
                ?.failureKind
                .orEmpty()
                .ifBlank { result.failureKind }
                .ifBlank { "analysis_failed" }
        if (failureKind == "busy" || failureKind == "preempted") {
            return RouteMetadataAttemptAssessment(
                RouteMetadataLedgerStatus.PARTIAL,
                cacheFile,
                progressToken,
                "",
                "",
                true,
            )
        }
        val fingerprint = "$failureKind|${level?.routeProblem.orEmpty()}|${result.status}"
        val deterministic = failureKind in setOf("missing", "invalid_input", "unroutable")
        val repeated = previous?.failureFingerprint == fingerprint
        val failures = if (repeated) previous.failureCount + 1 else 1
        return RouteMetadataAttemptAssessment(
            status =
                if (deterministic ||
                    failures >= 2
                ) {
                    RouteMetadataLedgerStatus.FAILED
                } else {
                    RouteMetadataLedgerStatus.PARTIAL
                },
            cacheFile = cacheFile,
            progressToken = progressToken,
            failureKind = failureKind,
            failureFingerprint = fingerprint,
            shouldRetry = !deterministic && failures < 2,
        )
    }
}

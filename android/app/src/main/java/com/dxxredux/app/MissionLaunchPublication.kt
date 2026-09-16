package com.dxxredux.app

import android.system.Os
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.security.MessageDigest

/** Native contract: android_mission_assets.cpp reads schema 1 and these exact field names */
internal data class MissionLaunchPublication(
    val discoveryDir: File,
    val manifest: String,
)

internal fun missionLaunchHash(text: String): String =
    MessageDigest.getInstance("SHA-256").digest(text.toByteArray(Charsets.UTF_8)).joinToString("") {
        "%02x".format(it.toInt() and 255)
    }

/** Publish immutable mission views; only the descriptor view belongs on the session search path */
internal fun MissionLaunchCatalog.publish(
    gameDir: File,
    globalRevision: String,
    activationErrors: Map<MissionLaunchKey, String> = emptyMap(),
    generateOverrides: (MissionLaunchKey, File) -> Unit = { _, _ -> },
): MissionLaunchPublication {
    fun activationError(mission: MissionLaunchEntry): String =
        listOf(mission.activationError, activationErrors[mission.key].orEmpty())
            .filter { it.isNotBlank() }
            .joinToString("\n")

    val identity = JSONArray()
    for (mission in missions) {
        identity.put(
            JSONObject()
                .put(
                    "key",
                    mission.key.toJson(),
                ).put(
                    "revision",
                    revisionFor(mission.key),
                ).put("activation_error", activationError(mission))
                .put(
                    "resources",
                    JSONArray(resourcesFor(mission.key).map { "${it.virtualPath}\u0000${it.sha256}" }),
                ),
        )
    }
    val generation = missionLaunchHash(globalRevision + "\n" + identity.toString())
    val destination = File(gameDir, ".mission_assets/$generation")
    val discovery = File(destination, "discovery")
    val entries = JSONArray()
    // Construct the expected manifest even when this verified generation is already staged
    for (mission in missions) {
        val token = missionLaunchHash(mission.key.toJson().toString())
        val context = File(destination, "contexts/$token")
        val alias = "missions/_packs/$token/${mission.key.descriptor.removePrefix("missions/")}"
        val mounts = mutableListOf(context.absolutePath)
        mounts +=
            resourcesFor(mission.key)
                .filter {
                    GameFileFormats.isDxa(it.virtualPath)
                }.map { File(context, it.virtualPath).absolutePath }
        entries.put(
            JSONObject()
                .put("alias", alias)
                .put("key", token)
                .put("descriptor", mission.key.descriptor)
                .put("owner", mission.key.owner)
                .put("revision", revisionFor(mission.key))
                .put(
                    "game",
                    mission.key.game,
                ).put("activation_error", activationError(mission))
                .put("mounts", JSONArray(mounts)),
        )
    }
    val manifest =
        JSONObject()
            .put("schema", 1)
            .put("generation", generation)
            .put("entries", entries)
            .toString(2) + "\n"
    val complete = File(destination, "catalog.json")
    if (complete.isFile && complete.readText() == manifest &&
        missions.withIndex().all { (index, mission) ->
            val entry = entries.getJSONObject(index)
            val context = File(entry.getJSONArray("mounts").getString(0))
            File(discovery, entry.getString("alias")).isFile &&
                resourcesFor(mission.key).all { resource ->
                    val staged = File(context, resource.virtualPath)
                    staged.isFile && staged.length() == resource.source.length()
                }
        }
    ) {
        retainMissionGenerations(destination)
        return MissionLaunchPublication(discovery, manifest)
    }

    val temporary = AtomicFilePublication.uniqueSibling(destination, "tmp")
    check(temporary.mkdirs()) { "Cannot stage mission assets" }
    try {
        check(File(temporary, "discovery").mkdirs())
        for ((index, mission) in missions.withIndex()) {
            val entry = entries.getJSONObject(index)
            val token = missionLaunchHash(mission.key.toJson().toString())
            val context = File(temporary, "contexts/$token")
            for (resource in resourcesFor(mission.key)) {
                val output = File(context, resource.virtualPath)
                output.parentFile?.mkdirs()
                linkMissionResource(resource.source, output)
            }
            val descriptor = resourcesFor(mission.key).single { it.virtualPath == mission.key.descriptor }
            val alias = File(temporary, "discovery/${entry.getString("alias")}")
            alias.parentFile?.mkdirs()
            // Copy descriptors: catalog reads must not retain handles to a mission payload
            descriptor.source.copyTo(alias)
            generateOverrides(mission.key, context)
        }
        File(temporary, "catalog.json").writeText(manifest)
        AtomicFilePublication.publishDirectory(temporary, destination)
    } finally {
        if (temporary.exists()) temporary.deleteRecursively()
    }
    retainMissionGenerations(destination)
    return MissionLaunchPublication(discovery, manifest)
}

/** Called before launching the engine; retain two preceding complete generations */
private fun retainMissionGenerations(current: File) {
    current.setLastModified(System.currentTimeMillis())
    current.parentFile
        ?.listFiles()
        .orEmpty()
        .filter { it != current && it.isDirectory && it.name.matches(Regex("[0-9a-f]{64}")) }
        .sortedByDescending { it.lastModified() }
        .drop(2)
        .forEach { it.deleteRecursively() }
}

private fun MissionLaunchKey.toJson(): JSONObject =
    JSONObject().put("owner", owner).put("descriptor", descriptor).put("game", game)

private fun linkMissionResource(
    source: File,
    destination: File,
) {
    try {
        Os.link(source.absolutePath, destination.absolutePath)
    } catch (_: Exception) {
        // JVM tests and filesystems without hard links use a private copy
        source.copyTo(destination)
    }
    check(source.length() == destination.length()) { "Mission resource staging failed: ${source.name}" }
}

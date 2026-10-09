package com.dxxredux.app

import android.app.Instrumentation
import android.net.Uri
import kotlinx.coroutines.runBlocking
import java.io.File
import java.util.zip.ZipFile

/** Full downloaded wrapper through the launcher's stream probe and URI importer */
internal class EnemyWithinImportChecks(
    private val instrumentation: Instrumentation,
) {
    fun run() =
        runBlocking {
            val context = instrumentation.targetContext
            val source = File(context.filesDir, "enemy-within-import-fixture.zip")
            check(source.length() == 936110524L) { "Expected the full Enemy Within wrapper" }
            check(
                AssetManifest.computeSha256(source) ==
                    "d79a110669b9bc2d4439dddc41a129dd3b359a1bcbe4346b80f02f54b24656da",
            ) {
                "Wrong Enemy Within wrapper bytes"
            }
            val root = File(context.cacheDir, "enemy-within-import-checks")
            check(root.mkdirs() || root.isDirectory)
            try {
                val uri = Uri.fromFile(source)
                check(context.contentResolver.openInputStream(uri)!!.use { MissionZip.isImportCandidate(it, root) }) {
                    "Launcher did not recognize the full wrapper as a mission import"
                }
                check(root.listFiles().orEmpty().isEmpty()) { "Probe leaked staged ZIP" }
                val manager = ModManager(root, context)
                val imported =
                    checkNotNull(manager.importMissionZip(uri, "ewithin-versions.zip", context.contentResolver))
                check(imported.filename == "ewithin-rebirth.zip") { "Wrong mission variant selected" }
                check(manager.listMods().map { it.filename } == listOf("ewithin-rebirth.zip"))
                val child = manager.modFile(imported.filename)
                val childHash = "bf8e9f58ef6993f44df5b94f38efd0235bb407485267948a4d0eaaa0a6aa8c84"
                check(AssetManifest.computeSha256(child) == childHash) {
                    "Imported Rebirth archive differs from original"
                }
                val extracted = File(root, "mods/.extracted_mission_zips/ewithin-rebirth.zip")
                ZipFile(child).use { zip ->
                    for (path in listOf("missions/ewithin.mn2", "missions/ewithin.hog", "ewithin.dxa")) {
                        val entry = checkNotNull(zip.getEntry(path))
                        val output = File(extracted, path)
                        check(output.length() == entry.size) { "Incomplete extracted asset: $path" }
                        val digest = java.security.MessageDigest.getInstance("SHA-256")
                        zip.getInputStream(entry).use { input ->
                            val buffer = ByteArray(64 * 1024)
                            while (true) {
                                val count = input.read(buffer)
                                if (count < 0) break
                                digest.update(buffer, 0, count)
                            }
                        }
                        val expected = digest.digest().joinToString("") { "%02x".format(it) }
                        check(AssetManifest.computeSha256(output) == expected) { "Extracted bytes differ: $path" }
                    }
                }
                val catalog = manager.buildMissionLaunchCatalog("d2")
                val mission = catalog.missions.first { it.key.owner == "mod/ewithin-rebirth.zip" }
                check(catalog.resourcesFor(mission.key).any { it.virtualPath == "ewithin.dxa" })
                check(
                    root.walkTopDown().none {
                        it.name == "ewithin-xl.zip" ||
                            it.name.startsWith(
                                "mission_zip_import_",
                            )
                    },
                ) {
                    "Unused XL variant or temporary wrapper retained"
                }
            } finally {
                root.deleteRecursively()
            }
        }
}

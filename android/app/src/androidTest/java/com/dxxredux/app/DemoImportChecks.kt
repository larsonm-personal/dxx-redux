package com.dxxredux.app

import android.app.Instrumentation
import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/** Original downloaded archives through the exact operation called by Download and install */
internal class DemoImportChecks(
    private val instrumentation: Instrumentation,
) {
    fun run() =
        runBlocking {
            val context = instrumentation.targetContext
            KnownVersions.init(context)
            val fixtures = File(context.filesDir, "demo-import-fixtures")
            val archives = JSONObject(File(fixtures, "oracles.json").readText()).getJSONArray("archives")
            val names = (0 until archives.length()).map { archives.getJSONObject(it).getString("archive") }.toSet()
            check(
                names ==
                    DemoInstallerPackages.knownPackages
                        .map {
                            it.filename
                        }.toSet(),
            ) { "Demo corpus does not cover the package catalog" }
            val root = File(context.cacheDir, "demo-import-checks")
            val runtimeRoot = File(context.filesDir, "demo-import-runtime")
            runtimeRoot.deleteRecursively()
            check(runtimeRoot.mkdirs())
            root.deleteRecursively()
            check(root.mkdirs())
            try {
                for (i in 0 until archives.length()) {
                    val oracle = archives.getJSONObject(i)
                    val name = oracle.getString("archive")
                    val archive = File(fixtures, "archive-$i")
                    check(
                        AssetManifest.computeSha256(archive) == oracle.getString("sha256"),
                    ) { "Wrong source archive: $name" }
                    val setDir = File(root, "set-$i")
                    val expected = oracle.getJSONArray("files")
                    val installed = installDemoArchive(context, archive, name, File(root, "tmp-$i"), setDir)
                    check(installed == expected.length()) { "Wrong installed count for $name: $installed" }
                    val manifest = AssetManifest(setDir)
                    val expectedNames = mutableSetOf("assets.json")
                    for (j in 0 until expected.length()) {
                        val entry = expected.getJSONObject(j)
                        val filename = entry.getString("file")
                        expectedNames.add(filename)
                        val file = File(setDir, filename)
                        check(
                            file.isFile && file.length() == entry.getLong("size") &&
                                AssetManifest.computeSha256(file) == entry.getString("sha256"),
                        ) { "Incorrect installed bytes: $name / $filename" }
                        val recorded = checkNotNull(manifest.getEntry(filename))
                        check(recorded.sha256 == entry.getString("sha256") && recorded.sizeBytes == file.length()) {
                            "Incorrect manifest: $name / $filename"
                        }
                    }
                    check(
                        setDir.listFiles()!!.map { it.name }.toSet() == expectedNames,
                    ) { "Unexpected installed paths for $name" }
                    val game = checkNotNull(DemoInstallerPackages.matchByName(name)).game
                    check(
                        checkFiles(setDir, if (game == "d1") D1_FILES else D2_DEMO_FILES, manifest)
                            .filter { it.info.required }
                            .all { it.found },
                    ) { "Launcher is not ready for $name" }
                    check(launchDataReadyForGame(game, setDir, manifest, SafManifest.forDir(setDir))) {
                        "Launcher launch readiness rejected $name"
                    }
                    val readiness = oracle.getJSONObject("launch_ready")
                    for (target in listOf("d1", "d2", "d1-in-d2")) {
                        check(
                            launchDataReadyForGame(target, setDir, manifest, SafManifest.forDir(setDir)) ==
                                readiness.getBoolean(target),
                        ) { "Unexpected $target readiness for $name" }
                    }
                    if (!oracle.isNull("runtime_suite")) {
                        check(oracle.getString("runtime_suite") == "d1-in-d2-shareware")
                        check(readiness.getBoolean("d1-in-d2"))
                        // The gameplay runner consumes these exact production-import outputs on the device
                        check(setDir.copyRecursively(File(runtimeRoot, "set-$i")))
                    }
                    instrumentation.sendStatus(0, android.os.Bundle().apply { putString("stream", "Verified $name\n") })
                }
                // A known package with only one valid game file must not publish or change a prior installation
                val broken = File(root, "incomplete.zip")
                ZipOutputStream(broken.outputStream()).use { zip ->
                    zip.putNextEntry(ZipEntry("descent.hog"))
                    zip.write("incomplete fixture".toByteArray())
                    zip.closeEntry()
                }
                val existing = File(root, "set-0")
                val before = existing.listFiles()!!.associate { it.name to AssetManifest.computeSha256(it) }
                val failed =
                    runCatching {
                        installDemoArchive(context, broken, "descent 1 demo 1-4.zip", File(root, "failure"), existing)
                    }
                check(failed.isFailure) { "Incomplete demo reported success" }
                check(
                    existing.listFiles()!!.associate {
                        it.name to AssetManifest.computeSha256(it)
                    } == before,
                ) { "Failed import changed installed files" }
                val missingVolume = File(root, "missing-volume.zip")
                openZipInputStreamSkippingPreamble(File(fixtures, "archive-1").inputStream(), root).use { input ->
                    ZipOutputStream(missingVolume.outputStream()).use { output ->
                        var entry = input.nextEntry
                        while (entry != null) {
                            if (entry.name.endsWith("DESCENT1.SOW", ignoreCase = true)) {
                                output.putNextEntry(ZipEntry(entry.name))
                                input.copyTo(output)
                                output.closeEntry()
                            }
                            input.closeEntry()
                            entry = input.nextEntry
                        }
                    }
                }
                check(
                    runCatching {
                        installDemoArchive(
                            context,
                            missingVolume,
                            "descent 1 demo 1-4.zip",
                            File(root, "missing"),
                            existing,
                        )
                    }.isFailure,
                ) { "Missing SOW volume reported success" }
                check(
                    existing.listFiles()!!.associate {
                        it.name to AssetManifest.computeSha256(it)
                    } == before,
                ) { "Missing-volume import changed installed files" }
            } finally {
                root.deleteRecursively()
            }
        }
}

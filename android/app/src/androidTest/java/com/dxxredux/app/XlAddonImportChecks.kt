package com.dxxredux.app

import android.app.Instrumentation
import android.net.Uri
import android.view.View
import kotlinx.coroutines.runBlocking
import org.apache.commons.compress.archivers.sevenz.SevenZOutputFile
import java.io.File

/** Exercise normal resource-mod import using isolated mod storage */
internal class XlAddonImportChecks(
    private val instrumentation: Instrumentation,
) {
    fun run() =
        runBlocking {
            val context = instrumentation.targetContext
            val root = File(context.cacheDir, "xl-addon-import-checks")
            check(!root.exists()) { "Previous addon check directory remains" }
            check(root.mkdirs())
            val source = File(context.filesDir, "saf_provider_source/xl-addon-check.7z")
            check(!source.exists())
            source.parentFile!!.mkdirs()
            val prefs = context.getSharedPreferences("dxx_prefs", 0)
            val hadPreference = prefs.contains(PREF_SHOW_PROGRESS_BARS)
            val oldPreference = prefs.getBoolean(PREF_SHOW_PROGRESS_BARS, true)
            try {
                val fixtures =
                    linkedMapOf(
                        "D2X-XL\\data\\d2x-xl.hog" to "camera bytes",
                        "other/d2x.ham" to "vertigo bytes",
                        "other/hoard.ham" to "not requested",
                    )
                SevenZOutputFile(source).use { archive ->
                    // Exercise the portable fallback; the genuine distribution covers native LZMA2
                    archive.setContentCompression(org.apache.commons.compress.archivers.sevenz.SevenZMethod.COPY)
                    for ((name, text) in fixtures) {
                        val file = File(root, "input").apply { writeText(text) }
                        val entry = archive.createArchiveEntry(file, name)
                        archive.putArchiveEntry(entry)
                        archive.write(text.toByteArray())
                        archive.closeArchiveEntry()
                    }
                }
                val manager = ModManager(root)

                fun checkPack() {
                    check(manager.listMods().size == 1)
                    val resources = File(root, "mods/xl-resources")
                    check(resources.isDirectory)
                    check(File(resources, "d2x-xl.hog").readText() == "camera bytes")
                    check(File(resources, "d2x.ham").readText() == "vertigo bytes")
                    check(!File(resources, "hoard.ham").exists())
                    check(root.walkTopDown().none { it.extension == "dxa" })
                    check(D2_FILES.none { it.filename == "d2x-xl.hog" || it.filename == "d2x.ham" })
                }
                for (provider in listOf("seekable", "pipe")) {
                    val progress = mutableListOf<String>()
                    val mod =
                        importResourceMod(
                            context,
                            Uri.parse("content://${context.packageName}.saf-test/$provider/${source.name}"),
                            source.name,
                            manager,
                        ) { label, _, _ -> progress += label }
                    check(mod?.filename == "xl-resources")
                    check(progress.first() == "Opening ${source.name}")
                    check(progress.any { it.startsWith("Reading ") } == (provider == "pipe"))
                    checkPack()
                }
                val existing = manager.listMods().single()
                manager.setEnabled(existing.filename, false)
                val raw = File(root, "d2x.ham").apply { writeText("vertigo bytes") }
                check(importResourceMod(context, Uri.fromFile(raw), raw.name, manager)?.enabled == false)
                checkPack()
                check(manager.listMods().single().order == existing.order)
                val zipSource = File(root, "addons.zip")
                java.util.zip.ZipOutputStream(zipSource.outputStream()).use { zip ->
                    for ((name, text) in fixtures) {
                        zip.putNextEntry(java.util.zip.ZipEntry(name))
                        zip.write(text.toByteArray())
                        zip.closeEntry()
                    }
                }
                check(importResourceMod(context, Uri.fromFile(zipSource), zipSource.name, manager) != null)
                checkPack()
                // Resource recognition must not consume mixed retail or mission packages
                for (other in listOf("descent2.ham", "mission.mn2")) {
                    java.util.zip.ZipOutputStream(zipSource.outputStream()).use { zip ->
                        for ((name, text) in mapOf("d2x-xl.hog" to "replacement", other to "other")) {
                            zip.putNextEntry(java.util.zip.ZipEntry(name))
                            zip.write(text.toByteArray())
                            zip.closeEntry()
                        }
                    }
                    check(importResourceMod(context, Uri.fromFile(zipSource), zipSource.name, manager) == null)
                    val camera = File(root, "mods/xl-resources/d2x-xl.hog")
                    check(camera.readText() == if (other == "descent2.ham") "replacement" else "camera bytes")
                    importResourceMod(
                        context,
                        Uri.parse("content://${context.packageName}.saf-test/seekable/${source.name}"),
                        source.name,
                        manager,
                    )
                }
                XlAddonPickerChecks(instrumentation).run(source)
                instrumentation.runOnMainSync {
                    val overlay = LoadingProgressOverlayView(context)
                    prefs.edit().remove(PREF_SHOW_PROGRESS_BARS).commit()
                    overlay.showProgress("XL resources", "texture", 10)
                    check(overlay.visibility == View.VISIBLE)
                    prefs.edit().putBoolean(PREF_SHOW_PROGRESS_BARS, false).commit()
                    overlay.showProgress("XL resources", "texture", 20)
                    check(overlay.visibility == View.GONE)
                    prefs.edit().putBoolean(PREF_SHOW_PROGRESS_BARS, true).commit()
                    overlay.showProgress("XL resources", "texture", 30)
                    check(overlay.visibility == View.VISIBLE)
                }
                check(
                    GameSettingsPreset.entries.all { preset ->
                        preset.settings.any {
                            it.label == "Show progress bars" &&
                                it.enabled == true
                        }
                    },
                )
                checkMissionToggle(root)
            } finally {
                val editor = prefs.edit()
                if (hadPreference) {
                    editor.putBoolean(
                        PREF_SHOW_PROGRESS_BARS,
                        oldPreference,
                    )
                } else {
                    editor.remove(PREF_SHOW_PROGRESS_BARS)
                }
                editor.commit()
                root.deleteRecursively()
                source.delete()
            }
        }

    private fun checkMissionToggle(root: File) {
        // Admission fixture only: this HOG is not a playable mine
        val hog =
            java.io
                .ByteArrayOutputStream()
                .apply {
                    write("DHF".toByteArray(Charsets.US_ASCII))
                    write(ByteArray(13).apply { "level01.rl2".toByteArray().copyInto(this) })
                    write(byteArrayOf(1, 0, 0, 0, 0))
                }.toByteArray()
        val source = File(root, "mission-toggle.7z")
        SevenZOutputFile(source).use { archive ->
            for ((name, bytes) in mapOf(
                "toggle.hog" to hog,
                "toggle.mn2" to "name = Toggle fixture\nnum_levels = 1\nlevel01.rl2\n".toByteArray(),
            )) {
                val input = File(root, "mission-toggle-input").apply { writeBytes(bytes) }
                archive.putArchiveEntry(archive.createArchiveEntry(input, name))
                archive.write(bytes)
                archive.closeArchiveEntry()
            }
        }
        val profile = File(root, "mission-toggle-profile")
        val manager = ModManager(profile)
        val imported = checkNotNull(manager.importMissionZipFile(source, source.name))
        manager.setEnabled(imported.filename, false)
        val reloaded = ModManager(profile)
        check(!reloaded.listMods().single().enabled)
        reloaded.setEnabled(imported.filename, true)
        check(ModManager(profile).listMods().single().enabled)

        reloaded.setEnabled(imported.filename, false)
        reloaded.modFile(imported.filename).writeText("damaged archive")
        ModManager(profile).setEnabled(imported.filename, true)
        check(!ModManager(profile).listMods().single().enabled) { "Damaged mission archive was enabled" }
        check(ModManager(profile).buildMissionLaunchCatalog("d2").missions.isEmpty())
    }
}

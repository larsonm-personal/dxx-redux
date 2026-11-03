package com.dxxredux.app

import android.app.Instrumentation
import android.net.Uri
import android.view.View
import kotlinx.coroutines.runBlocking
import org.apache.commons.compress.archivers.sevenz.SevenZOutputFile
import java.io.File

/** Exercise the same raw/7z operations used by the optional addon picker */
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
            source.parentFile.mkdirs()
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
                    for ((name, text) in fixtures) {
                        val file = File(root, "input").apply { writeText(text) }
                        val entry = archive.createArchiveEntry(file, name)
                        archive.putArchiveEntry(entry)
                        archive.write(text.toByteArray())
                        archive.closeArchiveEntry()
                    }
                }
                for (provider in listOf("seekable", "pipe")) {
                    val output = File(root, provider).apply { mkdirs() }
                    val progress = mutableListOf<String>()
                    val result =
                        extract7zContents(
                            context,
                            Uri.parse("content://${context.packageName}.saf-test/$provider/${source.name}"),
                            output,
                            XL_ADDON_FILENAMES,
                        ) { name, _, _ ->
                            progress +=
                                name
                        }
                    check(result.error == null) { result.error.orEmpty() }
                    check(result.files.map { it.name }.toSet() == XL_ADDON_FILENAMES)
                    check(
                        ("Copying archive" in progress) == (provider == "pipe"),
                    ) { "Wrong seekable/streaming strategy" }
                    val set = File(root, "$provider-set").apply { mkdirs() }
                    val player = File(root, "player.sg0").apply { writeText("preserved") }
                    for (file in result.files) {
                        check(importFile(context, FoundFile(file.name, Uri.fromFile(file.tmpFile)), set))
                    }
                    FileSetContentManager(set).reconcile()
                    check(
                        checkFiles(set, D2_FILES).filter { it.info.filename in XL_ADDON_FILENAMES }.all {
                            it.found &&
                                !it.info.required
                        },
                    )
                    check(File(set, "d2x-xl.hog").readText() == "camera bytes")
                    check(File(set, "d2x.ham").readText() == "vertigo bytes")
                    check(!File(set, "hoard.ham").exists())
                    check(player.readText() == "preserved")
                    check(!File(output, ".tmp_7z_import").exists())
                }
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
}

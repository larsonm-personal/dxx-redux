package com.dxxredux.app

import android.app.Instrumentation
import android.content.Intent
import android.os.SystemClock
import org.json.JSONObject
import java.io.File

/** Original archive through the real picker callback; the protected host owns launch/restoration */
internal class PumoUiImportChecks(
    private val instrumentation: Instrumentation,
) {
    fun run() {
        val context = instrumentation.targetContext
        val sets = FileSetManager(context.filesDir)
        val active = sets.getActive()
        val name = "default"
        check(active == name) { "Pumo UI fixture requires the protected default retail set" }
        check(ModManager.forActiveSet(context.filesDir, context).listMods().isEmpty()) {
            "Pumo UI fixture requires a retail-only disposable baseline"
        }
        val source = File(context.filesDir, "saf_provider_source")
        val archive = File(source, "pmines_v11.7z")
        check(
            MissionContentIdentity.compute(archive).sha256 ==
                "82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf",
        )
        val raw =
            listOf(
                "d2x-xl.hog",
                "smokingfire.tga",
                "smoke.tga",
                "bubble.tga",
                "corona.tga",
                "glare.tga",
                "lightning.wav",
                "airbubbles.wav",
            ).map {
                File(source, it)
            }
        check(raw.all { it.isFile }) { "Original raw addon inputs are missing" }
        val target = sets.getSetDir(name)
        check(target.canonicalPath.startsWith(File(context.filesDir, "imported/sets").canonicalPath + "/")) {
            "Pumo UI fixture requires internal disposable import storage"
        }
        val launcher =
            instrumentation.startActivitySync(
                Intent(
                    context,
                    SetupActivity::class.java,
                ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK),
            ) as SetupActivity
        try {
            val picker = XlAddonPickerChecks(instrumentation)
            picker.pickFiles(raw) {
                val deadline = SystemClock.uptimeMillis() + 30000

                fun ready(): Boolean {
                    val manager = ModManager.forActiveSet(context.filesDir, context)
                    return manager.listMods().any { it.filename == "xl-resources" && it.enabled } &&
                        File(manager.modFile("xl-resources"), "sounds/d2x-xl/airbubbles.wav").isFile
                }
                while (!ready() && SystemClock.uptimeMillis() < deadline) SystemClock.sleep(100)
                check(ready()) { "Raw addon UI import did not finish" }
                picker.reveal("Imported mod: XL resources")
            }
            picker.pickFiles(listOf(archive)) {
                val deadline = SystemClock.uptimeMillis() + 300000

                fun imported(): ModManager.ModInfo? =
                    ModManager.forActiveSet(context.filesDir, context).listMods().singleOrNull {
                        it.kind ==
                            ModManager.MOD_KIND_MISSION_ZIP
                    }
                while (imported() == null && SystemClock.uptimeMillis() < deadline) SystemClock.sleep(200)
                val mod = checkNotNull(imported()) { "Original Pumo UI import did not finish" }
                check(
                    mod.enabled && mod.importMode == "extracted_bundle",
                ) { "Original Pumo lacks enabled extracted cache: $mod" }
                check(mod.archiveSha256 == "82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf")
                picker.reveal("Extracted level pack: ${mod.displayName} (cached for faster launches)")
                val manager = ModManager.forActiveSet(context.filesDir, context)
                val pack = manager.modFile("xl-resources")
                check(
                    MissionContentIdentity.compute(File(pack, "d2x-xl.hog")).sha256 ==
                        "e7351d66d534c3fdb400f03b91b771ad2d0c0d2b0100cf48718d504008ada7e6",
                )
                check(!File(pack, "d2x.ham").exists())
                File(context.getExternalFilesDir(null), "pumo-ui-import-proof.json").writeText(
                    JSONObject()
                        .put(
                            "set",
                            name,
                        ).put("set_dir", target.canonicalPath)
                        .put("mission_archive", mod.filename)
                        .put("archive_sha256", mod.archiveSha256)
                        .put("import_mode", mod.importMode)
                        .put("enabled", mod.enabled)
                        .put("protected_retail_fixture", true)
                        .toString(2) + "\n",
                )
                instrumentation.waitForIdleSync()
                val image = checkNotNull(instrumentation.uiAutomation.takeScreenshot())
                try {
                    File(context.getExternalFilesDir(null), "pumo-ui-import.png").outputStream().use {
                        check(image.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, it))
                    }
                } finally {
                    image.recycle()
                }
            }
        } finally {
            instrumentation.runOnMainSync { launcher.finish() }
            instrumentation.waitForIdleSync()
        }
    }
}

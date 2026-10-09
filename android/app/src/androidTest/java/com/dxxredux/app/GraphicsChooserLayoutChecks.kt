package com.dxxredux.app

import android.app.Instrumentation
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.Canvas
import android.util.Log
import android.view.View
import android.view.ViewGroup
import android.widget.ScrollView
import android.widget.TextView
import org.json.JSONObject
import java.io.File

/** Measure and render the production chooser, including text below its cycling controls */
internal class GraphicsChooserLayoutChecks(
    private val instrumentation: Instrumentation,
) {
    fun run() {
        var result: Result<Unit>? = null
        instrumentation.runOnMainSync { result = runCatching { checkLayouts() } }
        result!!.getOrThrow()
    }

    private fun checkLayouts() {
        val base = instrumentation.targetContext
        for (fontScale in listOf(1f, 1.3f, 2f)) {
            val config =
                Configuration(base.resources.configuration).apply {
                    densityDpi = 160
                    this.fontScale = fontScale
                }
            val context = base.createConfigurationContext(config)
            for ((width, height) in listOf(640 to 360, 640 to 320, 360 to 640)) {
                val state =
                    JSONObject(
                        """{"phase":"editing","trial_id":1,
                    "candidate":{"TexFilt":2,"AnisoLevel":16,"MsaaLevel":4},
                    "current":{"TexFilt":2,"AnisoLevel":16,"MsaaLevel":4},
                    "capabilities":{"schema":1,"aniso_max":16,"msaa_max":4,"msaa_2":2,"msaa_4":4,
                    "aniso_reason":"","msaa_reason":"","renderer":"Layout test"}}""",
                    )
                val view =
                    GraphicsConfirmationOverlay(
                        context,
                        { true },
                        { _, _, _ -> 1 },
                        { state.toString() },
                        { true },
                        { true },
                        { _, _, _ -> true },
                        {},
                        {},
                        { error(it) },
                    )
                try {
                    view.update(state.toString())
                    view.measure(
                        View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                        View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY),
                    )
                    view.layout(0, 0, width, height)
                    val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
                    try {
                        view.draw(Canvas(bitmap))
                        File(
                            base.getExternalFilesDir(null),
                            "chooser-$width-$height-$fontScale.png",
                        ).outputStream().use {
                            bitmap.compress(Bitmap.CompressFormat.PNG, 100, it)
                        }
                    } finally {
                        bitmap.recycle()
                    }
                    val texts = descendants(view).filterIsInstance<TextView>().toList()
                    for (text in texts) {
                        Log.i(
                            "DXX-ChooserLayout",
                            "$width x $height font=$fontScale text=${text.text} bounds=${text.left},${text.top},${text.right},${text.bottom} layout=${text.layout?.height}",
                        )
                    }
                    check(texts.any { it.text.toString() == "Choose graphics options" })
                    for (text in texts) {
                        check(
                            text.layout.height <= text.height - text.compoundPaddingTop - text.compoundPaddingBottom,
                        ) {
                            "Clipped chooser text at $width x $height font=$fontScale: ${text.text}"
                        }
                    }
                    val panel = view.getChildAt(0)
                    check(panel.top >= view.paddingTop && panel.bottom <= height - view.paddingBottom)
                    val footer = texts.single { it.text.startsWith("Change later in") }
                    check(
                        footer.parent === panel,
                    ) { "Chooser explanation must stay visible outside the scrolling options" }
                    val scroll = descendants(view).filterIsInstance<ScrollView>().single()
                    val content = scroll.getChildAt(0)
                    check(
                        scroll.height >= 48,
                    ) { "No usable chooser scrolling area at $width x $height font=$fontScale" }
                    for (child in (0 until (panel as ViewGroup).childCount).map(panel::getChildAt)) {
                        check(child.top >= panel.paddingTop && child.bottom <= panel.height - panel.paddingBottom) {
                            "Chooser element extends outside panel at $width x $height font=$fontScale"
                        }
                    }
                    if (fontScale == 1f) {
                        check(
                            content.height <= scroll.height,
                        ) { "Default chooser should fit without scrolling at $width x $height" }
                    }
                    scroll.scrollTo(0, content.height)
                    check(
                        content.height - scroll.scrollY <= scroll.height,
                    ) { "Last chooser option cannot be scrolled into view" }
                } finally {
                    view.update("{\"phase\":\"idle\"}")
                }
            }
        }
    }

    private fun descendants(view: View): Sequence<View> =
        sequence {
            yield(view)
            if (view is ViewGroup) {
                for (index in 0 until view.childCount) yieldAll(descendants(view.getChildAt(index)))
            }
        }
}

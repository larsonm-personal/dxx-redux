package com.dxxredux.app

import android.Manifest
import android.app.Instrumentation
import android.app.UiAutomation
import android.content.Intent
import android.media.session.MediaSession
import android.media.session.MediaSessionManager
import android.os.Build
import kotlinx.coroutines.runBlocking

/** Observe platform routing before injecting global keys; never target the session directly */
internal class PreviewMediaControlsChecks(
    private val instrumentation: Instrumentation,
) {
    fun run(audioPath: String) {
        check(Build.VERSION.SDK_INT >= 33)
        val context = instrumentation.targetContext
        val manager = context.getSystemService(MediaSessionManager::class.java)
        val automation = instrumentation.getUiAutomation(UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES)
        val activity =
            instrumentation.startActivitySync(
                Intent(context, SetupActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            )
        val sessionField =
            LauncherPreviewMediaSession::class.java.getDeclaredField("session").apply {
                isAccessible = true
            }
        val observed = mutableMapOf<LauncherPreviewMediaSession, Boolean>()
        try {
            runBlocking {
                testPreviewMediaControls(context, audioPath) { media ->
                    val expected = (sessionField.get(media) as MediaSession).sessionToken
                    // Adopt only the read API's permission, and restore identity before dispatch.
                    val ready =
                        try {
                            automation.adoptShellPermissionIdentity(Manifest.permission.MEDIA_CONTENT_CONTROL)
                            manager.mediaKeyEventSession == expected
                        } finally {
                            automation.dropShellPermissionIdentity()
                        }
                    if (observed[media] != ready) {
                        LauncherDebugLog.log(
                            "preview-media-test platform routing ready=$ready session=${expected.hashCode()}",
                        )
                        observed[media] = ready
                    }
                    ready
                }
            }
        } finally {
            instrumentation.runOnMainSync { activity.finish() }
        }
    }
}

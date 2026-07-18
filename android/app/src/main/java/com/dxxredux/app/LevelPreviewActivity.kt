package com.dxxredux.app

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.graphics.Color
import android.os.Bundle
import android.util.Log
import android.view.KeyEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat

@Suppress("DEPRECATION")
abstract class LevelPreviewActivity :
    Activity(),
    SurfaceHolder.Callback {
    protected abstract val previewGame: String

    private external fun startLevelPreview(
        requestPath: String,
        dataDir: String,
    )

    private external fun nativeSetSurface(surface: Surface?)

    private external fun nativeSetSurfaceSize(
        width: Int,
        height: Int,
    )

    private external fun nativeKeyEvent(
        action: Int,
        androidKeyCode: Int,
        unicodeChar: Int,
    )

    private external fun nativeJoystickAxis(
        axis: Int,
        value: Float,
        touchActive: Boolean,
    )

    private external fun nativeJoystickAxes(
        axes: IntArray,
        values: FloatArray,
        touchActive: BooleanArray,
    )

    private external fun nativeJoystickButton(
        button: Int,
        pressed: Int,
    )

    private external fun nativeAutomapCenter()

    private external fun nativeSecretAreaRevealActive(): Boolean

    private external fun nativeToggleSecretAreaReveal()

    private external fun nativeObjectiveOverlayMode(): Int

    private external fun nativeCycleObjectiveOverlay()

    private lateinit var runtimeRequest: LevelPreviewRuntimeRequest
    private lateinit var inputMixer: InputMixer
    private lateinit var touchOverlay: TouchOverlayView
    private var nativeStarted = false
    private var nativeFinished = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val requestPath = intent.getStringExtra(EXTRA_REQUEST_PATH).orEmpty()
        runtimeRequest =
            try {
                LevelPreviewRequestStore.validateForLaunch(cacheDir, requestPath, previewGame)
            } catch (e: Exception) {
                finishWithResult(e.message ?: "Invalid preview request")
                return
            }

        val library = if (previewGame == GameFileFormats.GAME_D1) "dxx-redux-d1" else "dxx-redux-d2"
        try {
            System.loadLibrary(library)
            CrashLog.installNativeHandler(this)
        } catch (e: Throwable) {
            finishWithResult(e.message ?: "Could not load preview engine")
            return
        }

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        val orientation = getSharedPreferences("dxx_prefs", MODE_PRIVATE).getString("game_orientation", "landscape")
        requestedOrientation =
            if (orientation == "portrait") {
                ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
            } else {
                ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            }
        WindowCompat.setDecorFitsSystemWindows(window, false)

        val surface =
            SurfaceView(this).apply {
                setBackgroundColor(Color.BLACK)
                holder.addCallback(this@LevelPreviewActivity)
                isFocusable = true
                isFocusableInTouchMode = true
                requestFocus()
            }
        touchOverlay = TouchOverlayView(this)
        touchOverlay.setLayout(TouchLayoutRepository.load(this))
        inputMixer =
            InputMixer(
                buttonCallback = { button, pressed ->
                    nativeJoystickButton(TouchBindings.MIXER_BTN_BASE + button, pressed)
                },
                axisCallback = { axis, value, touchActive -> nativeJoystickAxis(axis, value, touchActive) },
                axisBatchCallback = { axes, values, touchActive ->
                    nativeJoystickAxes(axes, values, touchActive)
                },
            )
        touchOverlay.inputMixer = inputMixer
        touchOverlay.axisCallback = { axis, value -> inputMixer.setAxis(axis, "touch", value) }
        touchOverlay.keyCallback = { action, keyCode, unicode -> nativeKeyEvent(action, keyCode, unicode) }
        touchOverlay.gameVariant = previewGame
        touchOverlay.previewMode = true
        touchOverlay.automapActive = true
        touchOverlay.isActive = true
        touchOverlay.automapActionsProvider = { automapTouchActions(includeMarkers = false, previewMode = true) }
        touchOverlay.mapButtonCallback = ::closePreview
        touchOverlay.secretAreaRevealProvider = {
            runCatching { nativeSecretAreaRevealActive() }.getOrDefault(false)
        }
        touchOverlay.adminTrayObjectiveModeProvider = {
            runCatching { nativeObjectiveOverlayMode() }.getOrDefault(OBJECTIVE_MODE_OFF)
        }
        touchOverlay.adminTrayToggleStateProvider = { action ->
            action == TouchOverlayView.ADMIN_AUTOMAP_SECRET_REVEAL &&
                runCatching { nativeSecretAreaRevealActive() }.getOrDefault(false)
        }
        touchOverlay.adminTrayCallback = { action ->
            when (action) {
                TouchOverlayView.ADMIN_AUTOMAP -> closePreview()
                TouchOverlayView.ADMIN_AUTOMAP_RECENTER -> nativeAutomapCenter()
                TouchOverlayView.ADMIN_AUTOMAP_SECRET_REVEAL -> nativeToggleSecretAreaReveal()
                TouchOverlayView.ADMIN_AUTOMAP_OBJECTIVES -> nativeCycleObjectiveOverlay()
            }
        }

        setContentView(
            FrameLayout(this).apply {
                addView(surface, matchParentLayoutParams())
                addView(touchOverlay, matchParentLayoutParams())
            },
        )
        hideSystemBars()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        nativeSetSurfaceSize(holder.surfaceFrame.width(), holder.surfaceFrame.height())
        nativeSetSurface(holder.surface)
        if (nativeStarted) return
        nativeStarted = true
        Thread {
            try {
                startLevelPreview(runtimeRequest.requestFile.absolutePath, runtimeRequest.dataDir.absolutePath)
            } catch (e: Throwable) {
                Log.e(TAG, "Native level preview failed", e)
                onNativePreviewFinished(e.message ?: "Native level preview failed")
            }
        }.start()
    }

    override fun surfaceChanged(
        holder: SurfaceHolder,
        format: Int,
        width: Int,
        height: Int,
    ) {
        nativeSetSurfaceSize(width, height)
        nativeSetSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        if (nativeStarted && !nativeFinished) closePreview()
        nativeSetSurfaceSize(0, 0)
        nativeSetSurface(null)
    }

    @Deprecated("Android framework callback")
    override fun onBackPressed() {
        closePreview()
    }

    override fun onStop() {
        if (::inputMixer.isInitialized) inputMixer.releaseAll()
        super.onStop()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
    }

    @Suppress("unused")
    fun onNativePreviewFinished(error: String?) {
        if (nativeFinished) return
        nativeFinished = true
        finishWithResult(error.orEmpty())
    }

    private fun closePreview() {
        if (!nativeStarted) {
            finishWithResult("")
            return
        }
        runCatching {
            nativeKeyEvent(0, KeyEvent.KEYCODE_ESCAPE, 0)
            nativeKeyEvent(1, KeyEvent.KEYCODE_ESCAPE, 0)
        }
    }

    private fun finishWithResult(error: String) {
        if (::runtimeRequest.isInitialized) {
            LevelPreviewRequestStore.delete(cacheDir, runtimeRequest.requestFile.absolutePath)
        }
        val result = Intent()
        if (error.isNotBlank()) result.putExtra(EXTRA_ERROR, error)
        setResult(if (error.isBlank()) RESULT_OK else RESULT_CANCELED, result)
        finish()
        overridePendingTransition(0, 0)
    }

    private fun hideSystemBars() {
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    private fun matchParentLayoutParams() =
        FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT,
        )

    companion object {
        const val EXTRA_REQUEST_PATH = "level_preview_request_path"
        const val EXTRA_ERROR = "level_preview_error"
        private const val TAG = "DXX-LevelPreview"

        internal fun createIntent(
            context: Context,
            request: LevelPreviewLaunchRequest,
        ): Intent {
            val activity =
                if (request.game == GameFileFormats.GAME_D1) {
                    LevelPreviewD1Activity::class.java
                } else {
                    LevelPreviewD2Activity::class.java
                }
            return Intent(context, activity)
                .putExtra(EXTRA_REQUEST_PATH, request.requestFile.absolutePath)
                .addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION)
        }
    }
}

class LevelPreviewD1Activity : LevelPreviewActivity() {
    override val previewGame = GameFileFormats.GAME_D1
}

class LevelPreviewD2Activity : LevelPreviewActivity() {
    override val previewGame = GameFileFormats.GAME_D2
}

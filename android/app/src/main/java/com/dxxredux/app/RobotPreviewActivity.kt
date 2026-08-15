package com.dxxredux.app

import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.ActivityInfo
import android.graphics.Color
import android.os.Bundle
import android.util.Log
import android.view.Gravity
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import java.io.File

@Suppress("DEPRECATION")
abstract class RobotPreviewActivity :
    Activity(),
    SurfaceHolder.Callback {
    protected abstract val previewGame: String

    private external fun startRobotPreview(
        requestPath: String,
        dataDir: String,
    )

    private external fun nativeSetSurface(surface: Surface?)

    private external fun nativeSetSurfaceSize(
        width: Int,
        height: Int,
    )

    private external fun nativeRotateRobotPreview(
        headingDelta: Int,
        pitchDelta: Int,
    )

    private external fun nativeResetRobotPreview()

    private external fun nativeCloseRobotPreview()

    private external fun nativeRequestIntrospect()

    private external fun nativeSetIntrospectPath(path: String)

    private lateinit var runtimeRequest: RobotPreviewRuntimeRequest
    private lateinit var loadingProgressOverlay: LoadingProgressOverlayView
    private var nativeStarted = false
    private var nativeFinished = false
    private var closeRequested = false
    private var previousTouchX = 0f
    private var previousTouchY = 0f
    private var debugReceiverRegistered = false

    private val debugReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(
                context: Context,
                intent: Intent,
            ) {
                when (intent.action) {
                    ACTION_INTROSPECT -> nativeRequestIntrospect()
                    ACTION_COMMAND -> handleDebugCommand(intent)
                }
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        runtimeRequest =
            try {
                RobotPreviewRequestStore.validateForLaunch(
                    cacheDir,
                    intent.getStringExtra(EXTRA_REQUEST_PATH).orEmpty(),
                    previewGame,
                )
            } catch (e: Exception) {
                finishWithResult(e.message ?: "Invalid robot preview request")
                return
            }
        try {
            System.loadLibrary(if (previewGame == GameFileFormats.GAME_D1) "dxx-redux-d1" else "dxx-redux-d2")
            CrashLog.installNativeHandler(this)
        } catch (e: Throwable) {
            finishWithResult(e.message ?: "Could not load robot preview engine")
            return
        }
        if (BuildConfig.DEBUG) {
            val introspection = File(filesDir, INTROSPECTION_FILE)
            introspection.delete()
            File(filesDir, "$INTROSPECTION_FILE.tmp").delete()
            nativeSetIntrospectPath(introspection.absolutePath)
        }

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        requestedOrientation =
            if (getSharedPreferences("dxx_prefs", MODE_PRIVATE).getString("game_orientation", "landscape") ==
                "portrait"
            ) {
                ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
            } else {
                ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            }
        WindowCompat.setDecorFitsSystemWindows(window, false)

        val surface =
            SurfaceView(this).apply {
                holder.addCallback(this@RobotPreviewActivity)
                setOnTouchListener { _, event -> handleTouch(event) }
            }
        loadingProgressOverlay = LoadingProgressOverlayView(this)
        val controls = createControls()
        setContentView(
            FrameLayout(this).apply {
                addView(surface, matchParentLayoutParams())
                addView(controls, matchParentLayoutParams())
                addView(loadingProgressOverlay, matchParentLayoutParams())
            },
        )
        loadingProgressOverlay.showProgress("Preparing Preview", "Loading robot", 0)
        hideSystemBars()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        nativeSetSurfaceSize(holder.surfaceFrame.width(), holder.surfaceFrame.height())
        nativeSetSurface(holder.surface)
        if (nativeStarted) return
        nativeStarted = true
        Thread {
            try {
                startRobotPreview(runtimeRequest.requestFile.absolutePath, runtimeRequest.dataDir.absolutePath)
            } catch (e: Throwable) {
                Log.e(TAG, "Native robot preview failed", e)
                onNativeRobotPreviewFinished(e.message ?: "Native robot preview failed")
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
        if (nativeStarted && !nativeFinished) nativeCloseRobotPreview()
        nativeSetSurfaceSize(0, 0)
        nativeSetSurface(null)
    }

    @Deprecated("Android framework callback")
    override fun onBackPressed() = closePreview()

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
    }

    override fun onStart() {
        super.onStart()
        if (!BuildConfig.DEBUG || debugReceiverRegistered) return
        debugReceiverRegistered =
            DynamicReceiverPolicy.registerDebugExternal(
                this,
                debugReceiver,
                IntentFilter().apply {
                    addAction(ACTION_INTROSPECT)
                    addAction(ACTION_COMMAND)
                },
            )
    }

    override fun onStop() {
        if (debugReceiverRegistered) {
            runCatching { unregisterReceiver(debugReceiver) }
            debugReceiverRegistered = false
        }
        super.onStop()
    }

    @Suppress("unused")
    fun onNativeRobotPreviewFinished(error: String?) {
        if (nativeFinished) return
        nativeFinished = true
        if (!closeRequested) runOnUiThread { finishWithResult(error.orEmpty()) }
    }

    @Suppress("unused")
    fun showLoadingProgress(
        phase: String,
        item: String,
        percent: Int,
    ) = runOnUiThread { loadingProgressOverlay.showProgress(phase, item, percent) }

    @Suppress("unused")
    fun hideLoadingProgress() = runOnUiThread { loadingProgressOverlay.hideProgress() }

    @Suppress("unused")
    fun reportNativeFatalError(message: String) {
        if (message.isNotBlank()) Log.e(TAG, message)
    }

    @Suppress("unused")
    fun debugLogFromNative(
        category: Int,
        message: String,
    ) = DebugLog.log(category, message)

    @Suppress("unused")
    fun debugLogForcedFromNative(
        category: Int,
        message: String,
    ) = DebugLog.logForced(this, category, message)

    @Suppress("unused")
    fun debugLogBatchFromNative(
        category: Int,
        payload: String,
    ) = DebugLog.logBatch(category, payload)

    @Suppress("unused")
    fun debugLogBatchForcedFromNative(
        category: Int,
        payload: String,
    ) = DebugLog.logBatchForcedAsync(this, category, payload)

    @Suppress("ClickableViewAccessibility")
    private fun handleTouch(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                previousTouchX = event.x
                previousTouchY = event.y
            }

            MotionEvent.ACTION_MOVE -> {
                val heading = ((event.x - previousTouchX) * ROTATION_PER_PIXEL).toInt()
                val pitch = ((event.y - previousTouchY) * ROTATION_PER_PIXEL).toInt()
                previousTouchX = event.x
                previousTouchY = event.y
                if (heading != 0 || pitch != 0) nativeRotateRobotPreview(heading, pitch)
            }
        }
        return true
    }

    private fun createControls(): ViewGroup =
        FrameLayout(this).apply {
            addView(
                TextView(this@RobotPreviewActivity).apply {
                    text = runtimeRequest.robotLabel
                    setTextColor(Color.WHITE)
                    textSize = 18f
                    setShadowLayer(3f, 1f, 1f, Color.BLACK)
                    setPadding(dp(16), dp(12), dp(16), dp(12))
                },
                FrameLayout
                    .LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    ).apply {
                        gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
                    },
            )
            addView(
                TextView(this@RobotPreviewActivity).apply {
                    text = "Drag to rotate"
                    setTextColor(Color.LTGRAY)
                    textSize = 13f
                    setShadowLayer(3f, 1f, 1f, Color.BLACK)
                    setPadding(dp(16), dp(8), dp(16), dp(8))
                },
                FrameLayout
                    .LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    ).apply {
                        gravity = Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
                        bottomMargin = dp(8)
                    },
            )
            addView(
                LinearLayout(this@RobotPreviewActivity).apply {
                    orientation = LinearLayout.VERTICAL
                    gravity = Gravity.CENTER
                    addView(
                        Button(context).apply {
                            text = "Reset"
                            setOnClickListener { nativeResetRobotPreview() }
                        },
                    )
                    addView(
                        Button(context).apply {
                            text = "Close"
                            setOnClickListener { closePreview() }
                        },
                    )
                },
                FrameLayout
                    .LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    ).apply {
                        gravity = Gravity.TOP or Gravity.END
                        topMargin = dp(12)
                        marginEnd = dp(12)
                    },
            )
        }

    private fun closePreview() {
        if (closeRequested) return
        closeRequested = true
        finishWithResult("")
        if (nativeStarted) nativeCloseRobotPreview()
    }

    private fun handleDebugCommand(intent: Intent) {
        val command = intent.getStringExtra(EXTRA_COMMAND).orEmpty()
        Log.i(TAG, "Robot preview debug command: $command")
        when (command) {
            COMMAND_ROTATE -> {
                nativeRotateRobotPreview(
                    intent.getIntExtra(EXTRA_HEADING, 0),
                    intent.getIntExtra(EXTRA_PITCH, 0),
                )
            }

            COMMAND_RESET -> {
                nativeResetRobotPreview()
            }

            COMMAND_CLOSE -> {
                closePreview()
            }
        }
    }

    private fun finishWithResult(error: String) {
        if (::runtimeRequest.isInitialized) {
            RobotPreviewRequestStore.delete(cacheDir, runtimeRequest.requestFile.absolutePath)
        }
        setResult(
            if (error.isBlank()) RESULT_OK else RESULT_CANCELED,
            Intent().apply { if (error.isNotBlank()) putExtra(EXTRA_ERROR, error) },
        )
        finish()
        overridePendingTransition(0, 0)
    }

    private fun hideSystemBars() {
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    private fun matchParentLayoutParams() =
        FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)

    companion object {
        const val EXTRA_REQUEST_PATH = "robot_preview_request_path"
        const val EXTRA_ERROR = "robot_preview_error"
        const val ACTION_INTROSPECT = "com.dxxredux.ROBOT_PREVIEW_INTROSPECT"
        const val ACTION_COMMAND = "com.dxxredux.ROBOT_PREVIEW_COMMAND"
        const val INTROSPECTION_FILE = "robot_preview_introspect.json"
        private const val EXTRA_COMMAND = "command"
        private const val EXTRA_HEADING = "heading"
        private const val EXTRA_PITCH = "pitch"
        private const val COMMAND_ROTATE = "rotate"
        private const val COMMAND_RESET = "reset"
        private const val COMMAND_CLOSE = "close"
        private const val ROTATION_PER_PIXEL = 80f
        private const val TAG = "DXX-RobotPreview"

        internal fun createIntent(
            context: Context,
            request: RobotPreviewLaunchRequest,
        ): Intent =
            Intent(
                context,
                if (request.game == GameFileFormats.GAME_D1) {
                    RobotPreviewD1Activity::class.java
                } else {
                    RobotPreviewD2Activity::class.java
                },
            ).putExtra(EXTRA_REQUEST_PATH, request.requestFile.absolutePath)
                .addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION)
    }
}

class RobotPreviewD1Activity : RobotPreviewActivity() {
    override val previewGame = GameFileFormats.GAME_D1
}

class RobotPreviewD2Activity : RobotPreviewActivity() {
    override val previewGame = GameFileFormats.GAME_D2
}

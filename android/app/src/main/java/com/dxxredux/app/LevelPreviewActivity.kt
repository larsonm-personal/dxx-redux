package com.dxxredux.app

import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.ActivityInfo
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.KeyEvent
import android.view.PixelCopy
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.json.JSONObject
import java.io.File

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

    private external fun nativeRequestClose()

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

    private external fun nativeRequestIntrospect()

    private external fun nativeSetIntrospectPath(path: String)

    private lateinit var runtimeRequest: LevelPreviewRuntimeRequest
    private lateinit var inputMixer: InputMixer
    private lateinit var touchOverlay: TouchOverlayView
    private lateinit var previewSurfaceView: SurfaceView
    private var nativeStarted = false
    private var nativeFinished = false
    private var debugReceiverRegistered = false

    private val debugReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(
                context: Context,
                intent: Intent,
            ) {
                when (intent.action) {
                    ACTION_INTROSPECT -> {
                        nativeRequestIntrospect()
                        requestPresentationProbe()
                    }

                    ACTION_COMMAND -> {
                        handleDebugCommand(intent)
                    }
                }
            }
        }

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
        if (BuildConfig.DEBUG) {
            File(filesDir, INTROSPECTION_FILE).delete()
            File(filesDir, "$INTROSPECTION_FILE.tmp").delete()
            File(filesDir, PRESENTATION_PROBE_FILE).delete()
            File(filesDir, "$PRESENTATION_PROBE_FILE.tmp").delete()
            File(filesDir, COMPOSITE_PROBE_FILE).delete()
            File(filesDir, "$COMPOSITE_PROBE_FILE.tmp").delete()
            nativeSetIntrospectPath(File(filesDir, INTROSPECTION_FILE).absolutePath)
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

        previewSurfaceView =
            SurfaceView(this).apply {
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
                addView(previewSurfaceView, matchParentLayoutParams())
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
        unregisterDebugReceiver()
        super.onStop()
    }

    override fun onStart() {
        super.onStart()
        registerDebugReceiver()
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
        runCatching { nativeRequestClose() }
    }

    private fun handleDebugCommand(intent: Intent) {
        if (!nativeStarted || nativeFinished) return
        when (intent.getStringExtra(EXTRA_COMMAND).orEmpty()) {
            COMMAND_AXIS -> {
                val axis = intent.getIntExtra(EXTRA_AXIS, -1)
                val value = intent.getFloatExtra(EXTRA_VALUE, 0f)
                val active = intent.getBooleanExtra(EXTRA_ACTIVE, value != 0f)
                if (axis in 0 until AUTOMATION_AXIS_COUNT && value.isFinite()) {
                    nativeJoystickAxis(axis, value.coerceIn(-1f, 1f), active)
                }
            }

            COMMAND_INTROSPECT -> {
                nativeRequestIntrospect()
                requestPresentationProbe()
            }

            COMMAND_CENTER -> {
                nativeAutomapCenter()
            }

            COMMAND_CLOSE -> {
                closePreview()
            }
        }
    }

    private fun registerDebugReceiver() {
        if (!BuildConfig.DEBUG || debugReceiverRegistered) return
        val filter =
            IntentFilter().apply {
                addAction(ACTION_INTROSPECT)
                addAction(ACTION_COMMAND)
            }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(debugReceiver, filter, RECEIVER_EXPORTED)
        } else {
            @Suppress("UnspecifiedRegisterReceiverFlag")
            registerReceiver(debugReceiver, filter)
        }
        debugReceiverRegistered = true
    }

    private fun requestPresentationProbe() {
        if (!BuildConfig.DEBUG || Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return
        val width = previewSurfaceView.width
        val height = previewSurfaceView.height
        if (width <= 0 || height <= 0 || !previewSurfaceView.holder.surface.isValid) return
        val surfaceBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        PixelCopy.request(
            previewSurfaceView.holder.surface,
            surfaceBitmap,
            { result ->
                Thread {
                    writePresentationProbe(surfaceBitmap, result, PRESENTATION_PROBE_FILE)
                    surfaceBitmap.recycle()
                }.start()
            },
            Handler(Looper.getMainLooper()),
        )
        val compositeBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        PixelCopy.request(
            window,
            compositeBitmap,
            { result ->
                Thread {
                    writePresentationProbe(compositeBitmap, result, COMPOSITE_PROBE_FILE)
                    compositeBitmap.recycle()
                }.start()
            },
            Handler(Looper.getMainLooper()),
        )
    }

    private fun writePresentationProbe(
        bitmap: Bitmap,
        result: Int,
        filename: String,
    ) {
        val width = bitmap.width
        val height = bitmap.height
        val pixels = IntArray(width * height)
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height)
        var nonblackPixels = 0L
        var visiblePixels = 0L
        var rgbSum = 0L
        var maxChannel = 0
        var minX = -1
        var minY = -1
        var maxX = -1
        var maxY = -1
        var mapVisiblePixels = 0L
        var mapRgbSum = 0L
        var mapMaxChannel = 0
        val mapMinX = width / 23
        val mapMaxX = mapMinX + (width / 1.1).toInt()
        val mapMinY = height / 6
        val mapMaxY = mapMinY + (height / 1.45).toInt()
        pixels.forEachIndexed { index, pixel ->
            val red = Color.red(pixel)
            val green = Color.green(pixel)
            val blue = Color.blue(pixel)
            val brightest = maxOf(red, green, blue)
            rgbSum += red + green + blue
            maxChannel = maxOf(maxChannel, brightest)
            val x = index % width
            val y = index / width
            if (x in mapMinX until mapMaxX && y in mapMinY until mapMaxY) {
                mapRgbSum += red + green + blue
                mapMaxChannel = maxOf(mapMaxChannel, brightest)
                if (brightest >= 8) mapVisiblePixels++
            }
            if (brightest != 0) nonblackPixels++
            if (brightest >= 8) {
                visiblePixels++
                if (minX < 0 || x < minX) minX = x
                if (minY < 0 || y < minY) minY = y
                maxX = maxOf(maxX, x)
                maxY = maxOf(maxY, y)
            }
        }
        val json =
            JSONObject()
                .put("schema", "dxx-level-preview-presentation-v1")
                .put("pixel_copy_result", result)
                .put("width", width)
                .put("height", height)
                .put("pixel_count", pixels.size)
                .put("nonblack_pixels", nonblackPixels)
                .put("visible_pixels", visiblePixels)
                .put("rgb_sum", rgbSum)
                .put("max_channel", maxChannel)
                .put("map_visible_pixels", mapVisiblePixels)
                .put("map_rgb_sum", mapRgbSum)
                .put("map_max_channel", mapMaxChannel)
                .put("visible_min_x", minX)
                .put("visible_min_y", minY)
                .put("visible_max_x", maxX)
                .put("visible_max_y", maxY)
                .toString(2)
        val output = File(filesDir, filename)
        val temporary = File(filesDir, "$filename.tmp")
        temporary.writeText("$json\n")
        if (!temporary.renameTo(output)) {
            output.writeText("$json\n")
            temporary.delete()
        }
    }

    private fun unregisterDebugReceiver() {
        if (!debugReceiverRegistered) return
        runCatching { unregisterReceiver(debugReceiver) }
        debugReceiverRegistered = false
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
        const val ACTION_INTROSPECT = "com.dxxredux.LEVEL_PREVIEW_INTROSPECT"
        const val ACTION_COMMAND = "com.dxxredux.LEVEL_PREVIEW_COMMAND"
        const val INTROSPECTION_FILE = "level_preview_introspect.json"
        const val PRESENTATION_PROBE_FILE = "level_preview_presented_probe.json"
        const val COMPOSITE_PROBE_FILE = "level_preview_composite_probe.json"
        private const val EXTRA_COMMAND = "command"
        private const val EXTRA_AXIS = "axis"
        private const val EXTRA_VALUE = "value"
        private const val EXTRA_ACTIVE = "active"
        private const val COMMAND_AXIS = "axis"
        private const val COMMAND_INTROSPECT = "introspect"
        private const val COMMAND_CENTER = "center"
        private const val COMMAND_CLOSE = "close"
        private const val AUTOMATION_AXIS_COUNT = 8
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

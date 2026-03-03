package com.dxxredux.app

import android.app.Activity
import android.os.Bundle
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager

class MainActivity : Activity(), SurfaceHolder.Callback {

    companion object {
        init {
            System.loadLibrary("d2x-redux")
        }
        /** Game canvas resolution (must match gr_set_mode in the engine) */
        const val GAME_W = 640
        const val GAME_H = 480
    }

    // ── JNI declarations ────────────────────────────────────
    external fun helloFromNative(): String
    external fun startGame()
    external fun nativeSetSurface(surface: Surface?)
    external fun nativeTouchEvent(action: Int, gameX: Int, gameY: Int)
    external fun nativeKeyEvent(action: Int, androidKeyCode: Int, unicodeChar: Int)

    private var gameStarted = false
    private var surfaceW = 1
    private var surfaceH = 1

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Keep screen on while the game is running
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val sv = SurfaceView(this)
        sv.holder.addCallback(this)
        sv.isFocusable = true
        sv.isFocusableInTouchMode = true
        sv.requestFocus()
        setContentView(sv)
    }

    // ── SurfaceHolder.Callback ──────────────────────────────
    override fun surfaceCreated(holder: SurfaceHolder) {
        nativeSetSurface(holder.surface)

        // Start the engine only once, after the surface is ready
        if (!gameStarted) {
            gameStarted = true
            Thread {
                startGame()
            }.start()
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        surfaceW = width
        surfaceH = height
        nativeSetSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        nativeSetSurface(null)
    }

    // ── Touch → Mouse ───────────────────────────────────────
    override fun onTouchEvent(event: MotionEvent): Boolean {
        // Map touch coordinates from SurfaceView pixels to 640×480 canvas.
        // Letterbox: fit 4:3 canvas inside the surface, centred.
        val surfaceAspect = surfaceW.toFloat() / surfaceH.toFloat()
        val gameAspect    = GAME_W.toFloat() / GAME_H.toFloat()

        val drawW: Float
        val drawH: Float
        val offsetX: Float
        val offsetY: Float

        if (surfaceAspect > gameAspect) {
            // Surface is wider than 4:3 → pillarbox
            drawH = surfaceH.toFloat()
            drawW = drawH * gameAspect
            offsetX = (surfaceW - drawW) / 2f
            offsetY = 0f
        } else {
            // Surface is taller than 4:3 → letterbox
            drawW = surfaceW.toFloat()
            drawH = drawW / gameAspect
            offsetX = 0f
            offsetY = (surfaceH - drawH) / 2f
        }

        val gameX = ((event.x - offsetX) / drawW * GAME_W).toInt().coerceIn(0, GAME_W - 1)
        val gameY = ((event.y - offsetY) / drawH * GAME_H).toInt().coerceIn(0, GAME_H - 1)

        val action = when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> 0
            MotionEvent.ACTION_MOVE -> 1
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> 2
            else -> return super.onTouchEvent(event)
        }

        nativeTouchEvent(action, gameX, gameY)
        return true
    }

    // ── Keyboard ────────────────────────────────────────────
    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        // Let the system handle volume keys
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN)
            return super.onKeyDown(keyCode, event)

        nativeKeyEvent(0, keyCode, event.unicodeChar)
        return true
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN)
            return super.onKeyUp(keyCode, event)

        nativeKeyEvent(1, keyCode, 0)
        return true
    }
}

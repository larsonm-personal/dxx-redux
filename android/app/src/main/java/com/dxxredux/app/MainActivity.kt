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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Keep screen on while the game is running
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val sv = SurfaceView(this)
        sv.holder.addCallback(this)
        sv.isFocusable = true
        sv.isFocusableInTouchMode = true
        sv.requestFocus()

        // Handle touch on the SurfaceView so coordinates are view-relative
        sv.setOnTouchListener { view, event ->
            handleTouch(view, event)
        }

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
        nativeSetSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        nativeSetSurface(null)
    }

    // ── Touch → Mouse ───────────────────────────────────────
    private fun handleTouch(view: android.view.View, event: MotionEvent): Boolean {
        // The engine renders 640×480 into ANativeWindow which the compositor
        // stretches to fill the entire SurfaceView.  Map proportionally.
        val viewW = view.width.toFloat()
        val viewH = view.height.toFloat()
        if (viewW <= 0f || viewH <= 0f) return false

        val gameX = (event.x / viewW * GAME_W).toInt().coerceIn(0, GAME_W - 1)
        val gameY = (event.y / viewH * GAME_H).toInt().coerceIn(0, GAME_H - 1)

        val action = when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> 0
            MotionEvent.ACTION_MOVE -> 1
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> 2
            else -> return false
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

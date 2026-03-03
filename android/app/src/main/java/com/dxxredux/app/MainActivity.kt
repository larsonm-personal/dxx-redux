package com.dxxredux.app

import android.app.Activity
import android.os.Bundle
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager

class MainActivity : Activity(), SurfaceHolder.Callback {

    companion object {
        init {
            System.loadLibrary("d2x-redux")
        }
    }

    external fun helloFromNative(): String
    external fun startGame()
    external fun nativeSetSurface(surface: Surface?)

    private var gameStarted = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Keep screen on while the game is running
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val sv = SurfaceView(this)
        sv.holder.addCallback(this)
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
        // Surface geometry changed — update native side
        nativeSetSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        nativeSetSurface(null)
    }
}

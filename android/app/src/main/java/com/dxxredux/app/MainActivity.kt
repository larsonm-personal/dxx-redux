package com.dxxredux.app

import android.app.Activity
import android.content.Context
import android.os.Bundle
import android.text.InputType
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowManager
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputMethodManager

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
    external fun nativeTextInput(unicodeChar: Int)
    external fun nativeOnPause()

    private var gameStarted = false
    private lateinit var gameSurfaceView: GameSurfaceView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Keep screen on while the game is running
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        gameSurfaceView = GameSurfaceView(this)
        gameSurfaceView.holder.addCallback(this)
        gameSurfaceView.isFocusable = true
        gameSurfaceView.isFocusableInTouchMode = true
        gameSurfaceView.requestFocus()

        // Handle touch on the SurfaceView so coordinates are view-relative
        gameSurfaceView.setOnTouchListener { view, event ->
            handleTouch(view, event)
        }

        setContentView(gameSurfaceView)
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

    // ── Lifecycle ────────────────────────────────────────────
    override fun onStop() {
        super.onStop()
        // Inject Escape so the engine opens its pause / game menu.
        // This pauses a single-player game while the app is in the background.
        if (gameStarted) {
            nativeOnPause()
        }
    }

    // ── Touch → Mouse ───────────────────────────────────────
    private fun handleTouch(view: View, event: MotionEvent): Boolean {
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

    // ── Soft keyboard show/hide (called from JNI) ───────────
    @Suppress("unused")   // Called from native code
    fun showKeyboard(inputType: Int) {
        runOnUiThread {
            gameSurfaceView.currentInputType = when (inputType) {
                2    -> InputType.TYPE_CLASS_NUMBER
                else -> InputType.TYPE_CLASS_TEXT
            }
            gameSurfaceView.keyboardActive = true
            gameSurfaceView.requestFocus()
            val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
            imm.restartInput(gameSurfaceView)
            imm.showSoftInput(gameSurfaceView, InputMethodManager.SHOW_IMPLICIT)
        }
    }

    @Suppress("unused")   // Called from native code
    fun hideKeyboard() {
        runOnUiThread {
            gameSurfaceView.keyboardActive = false
            val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
            imm.hideSoftInputFromWindow(gameSurfaceView.windowToken, 0)
        }
    }

    // ── GameSurfaceView with InputConnection for soft keyboard ──
    private inner class GameSurfaceView(context: Context) : SurfaceView(context) {
        var currentInputType = InputType.TYPE_CLASS_TEXT
        var keyboardActive = false

        override fun onCheckIsTextEditor(): Boolean = keyboardActive

        override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
            // Disable word prediction / autocorrect so each keystroke arrives
            // immediately via commitText instead of being buffered in composition.
            outAttrs.inputType = currentInputType or
                    InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS or
                    InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
            outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE or
                    EditorInfo.IME_FLAG_NO_EXTRACT_UI
            return GameInputConnection(this)
        }
    }

    /**
     * Routes soft-keyboard text input into the engine via JNI.
     * commitText → nativeTextInput (one SDL key pair per character)
     * performEditorAction(DONE) → Enter key
     * deleteSurroundingText → Backspace key(s)
     */
    private inner class GameInputConnection(view: View) : BaseInputConnection(view, false) {

        override fun setComposingText(text: CharSequence, newCursorPosition: Int): Boolean {
            // Some IMEs still compose even with NO_SUGGESTIONS.
            // Finish composition immediately and commit the text so each
            // character appears in the game without waiting for a space.
            finishComposingText()
            return commitText(text, newCursorPosition)
        }

        override fun commitText(text: CharSequence, newCursorPosition: Int): Boolean {
            for (c in text) {
                nativeTextInput(c.code)
            }
            return true
        }

        override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
            // Each "before" character = one Backspace press
            repeat(beforeLength) {
                nativeKeyEvent(0, KeyEvent.KEYCODE_DEL, 0)
                nativeKeyEvent(1, KeyEvent.KEYCODE_DEL, 0)
            }
            return true
        }

        override fun performEditorAction(actionCode: Int): Boolean {
            // "Done" / Enter on the soft keyboard → inject Enter key
            nativeKeyEvent(0, KeyEvent.KEYCODE_ENTER, '\r'.code)
            nativeKeyEvent(1, KeyEvent.KEYCODE_ENTER, 0)
            return true
        }
    }
}

package com.dxxredux.app

import android.app.Activity
import android.os.Bundle
import android.widget.TextView

class MainActivity : Activity() {

    companion object {
        init {
            System.loadLibrary("d2x-redux")
        }
    }

    external fun helloFromNative(): String
    external fun startGame()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val tv = TextView(this)
        tv.text = helloFromNative()
        tv.textSize = 24f
        setContentView(tv)

        // Launch engine init on a background thread so we can observe logcat
        Thread {
            startGame()
        }.start()
    }
}

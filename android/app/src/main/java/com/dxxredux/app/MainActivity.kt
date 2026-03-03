package com.dxxredux.app

import android.app.Activity
import android.os.Bundle
import android.widget.TextView

class MainActivity : Activity() {

    companion object {
        init {
            System.loadLibrary("dxx-redux")
        }
    }

    external fun helloFromNative(): String

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val tv = TextView(this)
        tv.text = helloFromNative()
        tv.textSize = 24f
        setContentView(tv)
    }
}

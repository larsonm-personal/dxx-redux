package com.dxxredux.app.multiplayer

import java.util.concurrent.CopyOnWriteArrayList

// Same exported-log entry point as the app, captured without Android storage
object NetLog {
    val entries = CopyOnWriteArrayList<String>()

    fun log(
        category: String,
        message: String,
    ) {
        entries.add("[$category] $message")
    }
}

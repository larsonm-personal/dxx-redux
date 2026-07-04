package com.dxxredux.app.multiplayer

import com.dxxredux.app.DebugLog
import com.dxxredux.app.DebugLogCategory

object CoopDesyncLog {
    fun log(message: String) {
        DebugLog.log(DebugLogCategory.COOP_DESYNC, message)
    }
}

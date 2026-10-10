package com.dxxredux.app.multiplayer

import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.os.Process
import com.dxxredux.app.DebugLog
import com.dxxredux.app.DebugLogCategory

/**
 * Keeps the native multiplayer engine runnable while MainActivity is hidden
 * The transport foreground service lives in a different process, so it cannot
 * by itself prevent Android from caching and freezing :game
 */
class MultiplayerGameService : Service() {
    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(
        intent: Intent?,
        flags: Int,
        startId: Int,
    ): Int {
        MultiplayerSessionNotification.promote(this, gameActive = true)
        DebugLog.log(
            DebugLogCategory.NETWORK,
            "Multiplayer engine foreground protection active: pid=${Process.myPid()}",
        )
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        DebugLog.log(DebugLogCategory.NETWORK, "Multiplayer engine protection released: pid=${Process.myPid()}")
        super.onDestroy()
    }

    companion object {
        fun start(context: Context) {
            val intent = Intent(context, MultiplayerGameService::class.java)
            if (Build.VERSION.SDK_INT >=
                Build.VERSION_CODES.O
            ) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }

        fun stop(context: Context) {
            context.stopService(Intent(context, MultiplayerGameService::class.java))
        }
    }
}

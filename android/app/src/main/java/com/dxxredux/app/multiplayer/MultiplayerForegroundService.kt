package com.dxxredux.app.multiplayer

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Messenger
import android.os.RemoteException
import android.util.Log

/**
 * Foreground service that keeps the main process alive during multiplayer.
 * The main process hosts MatchmakingService and the UDP relay proxy; without
 * a FGS, Android's process freezer halts it ~80s after the game activity
 * (in the :game process) takes the foreground.
 *
 * Start with [start] when entering a multiplayer game, stop with [stop]
 * when leaving.
 */
class MultiplayerForegroundService : Service() {
    private val runtimeSession = RuntimeIpcSession()
    private val messenger =
        Messenger(
            Handler(Looper.getMainLooper()) { message ->
                handleRuntimeMessage(message)
                true
            },
        )

    override fun onBind(intent: Intent?): IBinder = messenger.binder

    override fun onUnbind(intent: Intent?): Boolean {
        disconnectGameProcess()
        return false
    }

    override fun onStartCommand(
        intent: Intent?,
        flags: Int,
        startId: Int,
    ): Int {
        ensureChannel()
        val notification =
            Notification
                .Builder(this, CHANNEL_ID)
                .setContentTitle("DXX-Redux Multiplayer")
                .setContentText("Multiplayer game in progress")
                .setSmallIcon(android.R.drawable.ic_menu_compass)
                .setOngoing(true)
                .build()
        startForeground(NOTIFICATION_ID, notification)
        return START_NOT_STICKY
    }

    private fun handleRuntimeMessage(message: Message) {
        if (message.sendingUid != applicationInfo.uid) {
            Log.w(TAG, "Rejected runtime IPC from UID ${message.sendingUid}")
            return
        }
        when (message.what) {
            RUNTIME_IPC_REGISTER -> {
                runtimeSession.register(message.data.getBoolean(RUNTIME_IPC_KEY_HOST))
                sendDiagnostics(message)
            }

            RUNTIME_IPC_GAME_STATE -> {
                if (runtimeSession.acceptsGameState()) {
                    message.data.getIntArray(RUNTIME_IPC_KEY_STATE)?.let {
                        MatchmakingService.publishRuntimeGameState(it)
                    }
                }
            }

            RUNTIME_IPC_GET_DIAGNOSTICS -> {
                sendDiagnostics(message)
            }

            RUNTIME_IPC_GAME_STOPPED -> {
                disconnectGameProcess()
            }
        }
    }

    private fun sendDiagnostics(request: Message) {
        val reply = request.replyTo ?: return
        val encoded =
            RuntimeDiagnosticsCodec.encode(
                MatchmakingService.getProxyStats(),
                MatchmakingStateHolder.state.value.connectionInfo,
            )
        try {
            reply.send(
                Message.obtain(null, RUNTIME_IPC_DIAGNOSTICS).apply {
                    data = Bundle().apply { putString(RUNTIME_IPC_KEY_DIAGNOSTICS, encoded) }
                },
            )
        } catch (e: RemoteException) {
            Log.w(TAG, "Could not return runtime diagnostic snapshot", e)
        }
    }

    private fun disconnectGameProcess() {
        MatchmakingService.runtimeGameProcessDisconnected(runtimeSession.disconnect())
    }

    private fun ensureChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val nm = getSystemService(NotificationManager::class.java) ?: return
            if (nm.getNotificationChannel(CHANNEL_ID) != null) return
            val channel =
                NotificationChannel(
                    CHANNEL_ID,
                    "Multiplayer Session",
                    NotificationManager.IMPORTANCE_LOW,
                )
            channel.description = "Keeps the game alive during multiplayer"
            nm.createNotificationChannel(channel)
        }
    }

    companion object {
        private const val TAG = "MultiplayerForeground"
        private const val CHANNEL_ID = "dxx_multiplayer_fg"
        private const val NOTIFICATION_ID = 1001

        fun start(context: Context) {
            val intent = Intent(context, MultiplayerForegroundService::class.java)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }

        fun stop(context: Context) {
            context.stopService(Intent(context, MultiplayerForegroundService::class.java))
        }
    }
}

package com.dxxredux.app.multiplayer

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.os.Build
import com.dxxredux.app.R

/** Both multiplayer processes share one notification for the live session */
internal object MultiplayerSessionNotification {
    private const val CHANNEL_ID = "dxx_multiplayer_fg"
    private const val NOTIFICATION_ID = 1001

    fun promote(
        service: Service,
        gameActive: Boolean,
    ) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val manager = service.getSystemService(NotificationManager::class.java)
            if (manager != null && manager.getNotificationChannel(CHANNEL_ID) == null) {
                val channel = NotificationChannel(CHANNEL_ID, "Multiplayer Session", NotificationManager.IMPORTANCE_LOW)
                channel.description = "Keeps the game alive during multiplayer"
                manager.createNotificationChannel(channel)
            }
        }
        val builder =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                Notification.Builder(service, CHANNEL_ID)
            } else {
                @Suppress("DEPRECATION")
                Notification.Builder(service)
            }
        val notification =
            builder
                .setContentTitle("${service.getString(R.string.app_brand_name)} Multiplayer")
                .setContentText(if (gameActive) "Multiplayer game in progress" else "LAN lobby active")
                .setSmallIcon(android.R.drawable.ic_menu_compass)
                .setOngoing(true)
                .build()
        // Android retains a shared notification until its last foreground service stops
        service.startForeground(NOTIFICATION_ID, notification)
    }
}

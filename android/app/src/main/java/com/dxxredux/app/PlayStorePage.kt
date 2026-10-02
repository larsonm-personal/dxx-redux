package com.dxxredux.app

import android.content.Context
import android.content.Intent
import android.net.Uri

/** All distributions link to the published Play Store package */
fun openPlayStorePage(context: Context) {
    val pkg = "com.dxxredux.app"
    try {
        context.startActivity(
            Intent(Intent.ACTION_VIEW, Uri.parse("market://details?id=$pkg")),
        )
    } catch (_: Exception) {
        context.startActivity(
            Intent(
                Intent.ACTION_VIEW,
                Uri.parse("https://play.google.com/store/apps/details?id=$pkg"),
            ),
        )
    }
}

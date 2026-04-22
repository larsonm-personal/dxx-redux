package com.dxxredux.app

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.Log
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.google.android.play.core.appupdate.AppUpdateManagerFactory
import com.google.android.play.core.install.model.UpdateAvailability

private const val TAG = "UpdateChecker"
private const val PREFS_NAME = "dxx_prefs"
private const val KEY_DISMISSED_VERSION = "dismissed_update_version"

private fun getDismissedVersion(context: Context): Int =
    context
        .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        .getInt(KEY_DISMISSED_VERSION, 0)

private fun setDismissedVersion(
    context: Context,
    versionCode: Int,
) {
    context
        .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        .edit()
        .putInt(KEY_DISMISSED_VERSION, versionCode)
        .apply()
}

private fun getInstalledVersionCode(context: Context): Int =
    try {
        val info = context.packageManager.getPackageInfo(context.packageName, 0)
        @Suppress("DEPRECATION")
        info.versionCode
    } catch (_: Exception) {
        0
    }

fun openPlayStorePage(context: Context) {
    val pkg = context.packageName
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

/**
 * Banner shown at the top of the launcher when a Play Store update is available.
 * Dismissing persists the version code so the banner won't reappear for that
 * version (or older), but will reappear for a newer release.
 */
@Composable
fun UpdateBanner() {
    val context = LocalContext.current
    var availableVersion by remember { mutableIntStateOf(0) }
    var dismissed by remember { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        val manager = AppUpdateManagerFactory.create(context)
        manager.appUpdateInfo.addOnSuccessListener { info ->
            if (info.updateAvailability() == UpdateAvailability.UPDATE_AVAILABLE) {
                availableVersion = info.availableVersionCode()
                Log.d(TAG, "Update available: version $availableVersion")
            }
        }
        manager.appUpdateInfo.addOnFailureListener { e ->
            Log.d(TAG, "Update check failed: ${e.message}")
        }
    }

    if (availableVersion <= 0 || dismissed) return

    val installed = remember { getInstalledVersionCode(context) }
    val dismissedVersion = remember { getDismissedVersion(context) }

    if (availableVersion <= installed || availableVersion <= dismissedVersion) return

    Card(
        modifier = Modifier.fillMaxWidth().padding(bottom = 8.dp),
        colors =
            CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer,
            ),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 4.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                "Update available",
                color = MaterialTheme.colorScheme.onPrimaryContainer,
            )
            Row {
                TextButton(onClick = { openPlayStorePage(context) }) {
                    Text("Update")
                }
                TextButton(onClick = {
                    setDismissedVersion(context, availableVersion)
                    dismissed = true
                }) {
                    Text("Dismiss")
                }
            }
        }
    }
}

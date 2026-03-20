package com.dxxredux.app.multiplayer

import android.content.Context
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

/**
 * Caches the most recent addresses (IPs or URLs) in SharedPreferences.
 * Each connection type gets its own key so lists stay separate.
 */
class RecentAddressPrefs(
    private val key: String,
) {
    companion object {
        private const val PREFS_NAME = "dxx_prefs"
        private const val MAX_ENTRIES = 5

        /** LAN Join-by-IP and Join-Lobby-by-IP dialogs */
        val LAN_IPS = RecentAddressPrefs("recent_lan_ips")

        /** Matchmaking server URLs */
        val SERVER_URLS = RecentAddressPrefs("recent_server_urls")
    }

    fun load(context: Context): List<String> {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val raw = prefs.getString(key, null) ?: return emptyList()
        return raw.split(",").filter { it.isNotBlank() }
    }

    fun add(
        context: Context,
        address: String,
    ) {
        val current = load(context).toMutableList()
        current.remove(address)
        current.add(0, address)
        val trimmed = current.take(MAX_ENTRIES)
        context
            .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putString(key, trimmed.joinToString(","))
            .apply()
    }
}

/** Clickable list of recent addresses for use in address-entry UIs. */
@Composable
fun RecentSuggestions(
    entries: List<String>,
    onSelect: (String) -> Unit,
) {
    if (entries.isNotEmpty()) {
        Text("Recent:", style = MaterialTheme.typography.bodySmall)
        for (entry in entries) {
            Text(
                entry,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.primary,
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .clickable { onSelect(entry) }
                        .padding(vertical = 4.dp),
            )
        }
    }
}

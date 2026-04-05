package com.dxxredux.app.multiplayer

import android.content.Context
import java.util.UUID

/**
 * Persistent installation identity for multiplayer client matching.
 *
 * Generates a UUID on first launch and stores it in SharedPreferences.
 * Used to re-identify players across sessions even if their callsign
 * changes. The GPGS player_id is preferred when available; this UUID
 * is the fallback for offline / dev-mode play.
 *
 * Shared constant: COOP_CLIENT_ID_LEN = 36 (duplicated in coop_save.h)
 */
object ClientIdentity {
    private const val PREFS_NAME = "client_identity"
    private const val KEY_INSTALLATION_ID = "installation_id"
    const val CLIENT_ID_LEN = 36 // UUID string length, no null

    private var cachedId: String? = null

    /** Returns the 36-char UUID string, creating one on first call. */
    fun getInstallationId(context: Context): String {
        cachedId?.let { return it }
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        var id = prefs.getString(KEY_INSTALLATION_ID, null)
        if (id == null) {
            id = UUID.randomUUID().toString()
            prefs.edit().putString(KEY_INSTALLATION_ID, id).apply()
        }
        cachedId = id
        return id
    }
}

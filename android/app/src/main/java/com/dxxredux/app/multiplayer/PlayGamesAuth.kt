package com.dxxredux.app.multiplayer

import android.app.Activity
import android.util.Log
import com.dxxredux.app.BuildConfig
import com.google.android.gms.games.GamesSignInClient
import com.google.android.gms.games.PlayGames
import com.google.android.gms.games.PlayGamesSdk
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlin.coroutines.resume

private const val TAG = "PlayGamesAuth"

/**
 * Thin wrapper around Google Play Games Services v2 sign-in.
 * Provides coroutine-friendly helpers for checking auth status and
 * requesting a server auth code for our matchmaking server.
 *
 * Call [initialize] once from SetupActivity.onCreate().
 * Call [getServerAuthCode] when connecting to the matchmaking server.
 * Returns null on any failure (no Play Services, user not signed in, etc.)
 * so the caller can fall back to dev token auth.
 */
object PlayGamesAuth {
    private var initialized = false

    /** The web client ID from auth_config.json5 (injected via BuildConfig). */
    private val serverClientId: String = BuildConfig.GPGS_SERVER_CLIENT_ID

    /** True if a real client ID is configured (not empty/placeholder). */
    val isConfigured: Boolean
        get() = serverClientId.isNotEmpty() && !serverClientId.startsWith("YOUR_")

    /** Call once in SetupActivity.onCreate(). Safe to call multiple times. */
    fun initialize(activity: Activity) {
        if (initialized) return
        if (!isConfigured) {
            Log.i(TAG, "GPGS not configured (no server_client_id) -- dev token fallback")
            return
        }
        try {
            PlayGamesSdk.initialize(activity)
            initialized = true
            Log.i(TAG, "PlayGamesSdk initialized")
        } catch (e: Exception) {
            Log.w(TAG, "PlayGamesSdk.initialize() failed -- Play Services unavailable?", e)
        }
    }

    /**
     * Check if the user is currently signed into Play Games.
     * Returns true if authenticated, false otherwise.
     */
    suspend fun isAuthenticated(activity: Activity): Boolean {
        if (!initialized) return false
        return suspendCancellableCoroutine { cont ->
            try {
                val client: GamesSignInClient = PlayGames.getGamesSignInClient(activity)
                client.isAuthenticated.addOnCompleteListener { task ->
                    val result = task.isSuccessful && task.result.isAuthenticated
                    Log.i(TAG, "isAuthenticated: $result")
                    cont.resume(result)
                }
            } catch (e: Exception) {
                Log.w(TAG, "isAuthenticated check failed", e)
                cont.resume(false)
            }
        }
    }

    /**
     * Request a one-time server auth code for our matchmaking server.
     * The server exchanges this code with Google's OAuth2 endpoint to get
     * the stable GPGS player ID.
     *
     * Returns the auth code string, or null if GPGS is unavailable/not signed in.
     * The auth code expires within minutes -- send it immediately.
     */
    suspend fun getServerAuthCode(
        activity: Activity,
        forceRefresh: Boolean = false,
    ): String? {
        if (!initialized || !isConfigured) return null
        return suspendCancellableCoroutine { cont ->
            try {
                val client = PlayGames.getGamesSignInClient(activity)
                client
                    .requestServerSideAccess(serverClientId, forceRefresh)
                    .addOnSuccessListener { authCode ->
                        Log.i(TAG, "Got server auth code (${authCode.length} chars)")
                        cont.resume(authCode)
                    }.addOnFailureListener { e ->
                        Log.w(TAG, "requestServerSideAccess failed", e)
                        cont.resume(null)
                    }
            } catch (e: Exception) {
                Log.w(TAG, "requestServerSideAccess threw", e)
                cont.resume(null)
            }
        }
    }
}

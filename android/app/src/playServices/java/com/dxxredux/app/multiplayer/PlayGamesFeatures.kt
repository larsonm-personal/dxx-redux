package com.dxxredux.app.multiplayer

import android.app.Activity
import android.content.Context
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import com.dxxredux.app.BuildConfig
import com.google.android.gms.games.PlayGames
import com.google.android.gms.games.PlayGamesSdk
import com.google.android.gms.tasks.Task
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

/** Optional feature boundary, deliberately unused until there is a usable platform feature UI */
object PlayGamesFeatures {
    private const val PREFS = "play_games_features"
    private var gate: PlayGamesFeatureGate? = null
    private var initialized = false
    private val available: Boolean
        get() = BuildConfig.PLAY_GAMES_APP_ID.let { id -> id.any { it in '1'..'9' } && id.all { it.isDigit() } }

    private fun gate(context: Context): PlayGamesFeatureGate =
        gate ?: PlayGamesFeatureGate(
            available,
            context.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getBoolean("enabled", false),
        ).also { gate = it }

    /** Reading availability/opt-in never initializes the SDK or opens UI */
    suspend fun state(context: Context): PlayGamesFeatureState =
        withContext(Dispatchers.Main.immediate) { gate(context).state }

    /** Call only from an explicit Enable Play Games or foreground feature-recovery action */
    suspend fun enable(activity: Activity): PlayGamesFeatureState =
        withContext(Dispatchers.Main.immediate) {
            val owner = gate(activity)
            if (!available) return@withContext owner.state
            check(!activity.isFinishing && !activity.isDestroyed)
            check((activity as? LifecycleOwner)?.lifecycle?.currentState?.isAtLeast(Lifecycle.State.RESUMED) == true)
            activity.applicationContext
                .getSharedPreferences(
                    PREFS,
                    Context.MODE_PRIVATE,
                ).edit()
                .putBoolean("enabled", true)
                .apply()
            val generation = owner.begin()
            try {
                if (!initialized) {
                    PlayGamesSdk.initialize(activity.applicationContext)
                    initialized = true
                }
                val signIn = PlayGames.getGamesSignInClient(activity)
                val authenticated = signIn.isAuthenticated.awaitResult().isAuthenticated
                if (!owner.isCurrent(generation)) return@withContext owner.state
                if (!authenticated &&
                    (
                        activity.isFinishing || activity.isDestroyed ||
                            (activity as? LifecycleOwner)?.lifecycle?.currentState?.isAtLeast(
                                Lifecycle.State.RESUMED,
                            ) !=
                            true
                    )
                ) {
                    return@withContext owner.state
                }
                if (authenticated || signIn.signIn().awaitResult().isAuthenticated) {
                    if (!owner.isCurrent(generation)) return@withContext owner.state
                    val profile = PlayGames.getPlayersClient(activity).currentPlayer.awaitResult()
                    currentCoroutineContext().ensureActive()
                    owner.publish(generation, profile.playerId)
                }
            } catch (cancelled: CancellationException) {
                owner.publish(generation, null)
                throw cancelled
            } catch (_: Exception) {
                owner.publish(generation, null)
            }
            owner.state
        }

    /** No manual sign-in and no initialization from a background refresh */
    suspend fun refresh(activity: Activity): PlayGamesFeatureState =
        withContext(Dispatchers.Main.immediate) {
            val owner = gate(activity)
            if (!owner.state.enabled || !initialized) return@withContext owner.state
            val generation = owner.begin()
            try {
                if (PlayGames
                        .getGamesSignInClient(activity)
                        .isAuthenticated
                        .awaitResult()
                        .isAuthenticated
                ) {
                    if (!owner.isCurrent(generation)) return@withContext owner.state
                    val profile = PlayGames.getPlayersClient(activity).currentPlayer.awaitResult()
                    currentCoroutineContext().ensureActive()
                    owner.publish(generation, profile.playerId)
                }
            } catch (cancelled: CancellationException) {
                owner.publish(generation, null)
                throw cancelled
            } catch (_: Exception) {
                owner.publish(generation, null)
            }
            owner.state
        }

    /** Stops our integration, not the Google platform account or an already initialized SDK */
    suspend fun disable(context: Context) =
        withContext(Dispatchers.Main.immediate) {
            gate(context).disable()
            context.applicationContext
                .getSharedPreferences(
                    PREFS,
                    Context.MODE_PRIVATE,
                ).edit()
                .putBoolean("enabled", false)
                .apply()
        }

    private suspend fun <T> Task<T>.awaitResult(): T =
        suspendCancellableCoroutine { continuation ->
            addOnCompleteListener { completed ->
                if (continuation.isActive) {
                    if (completed.isSuccessful) {
                        continuation.resume(completed.result)
                    } else {
                        continuation.resumeWithException(IllegalStateException("Play Games feature unavailable"))
                    }
                }
            }
        }
}

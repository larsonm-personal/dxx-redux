package com.dxxredux.app.multiplayer

import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.dxxredux.app.LauncherDebugLog
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

internal data class MissionLoadState<T>(
    val value: T? = null,
    val loading: Boolean = true,
    val error: String? = null,
)

/** Reset immediately on selection changes and never publish results from a dismissed or replaced request */
@Composable
internal fun <T> rememberMissionLoad(
    vararg inputs: Any?,
    failureMessage: String,
    load: suspend () -> T,
): MissionLoadState<T> {
    val lifecycle = LocalLifecycleOwner.current.lifecycle
    val resumeVersion = remember { mutableIntStateOf(0) }
    DisposableEffect(lifecycle) {
        var wasPaused = !lifecycle.currentState.isAtLeast(Lifecycle.State.RESUMED)
        val observer =
            LifecycleEventObserver { _, event ->
                if (event == Lifecycle.Event.ON_PAUSE) wasPaused = true
                if (event == Lifecycle.Event.ON_RESUME && wasPaused) {
                    wasPaused = false
                    resumeVersion.intValue++
                }
            }
        lifecycle.addObserver(observer)
        onDispose { lifecycle.removeObserver(observer) }
    }
    return key(inputs.toList(), resumeVersion.intValue) {
        produceState(MissionLoadState<T>()) {
            value =
                try {
                    MissionLoadState(value = withContext(Dispatchers.IO) { load() }, loading = false)
                } catch (e: CancellationException) {
                    throw e
                } catch (e: Exception) {
                    LauncherDebugLog.log("$failureMessage: ${e.javaClass.simpleName}: ${e.message}")
                    MissionLoadState(loading = false, error = failureMessage)
                }
        }.value
    }
}

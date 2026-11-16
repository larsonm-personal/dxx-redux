package com.dxxredux.app

import android.content.Context
import android.content.ContextWrapper
import androidx.activity.ComponentActivity
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider

/** In-memory state only: process recreation starts with saved data, never a partial draft */
internal class SettingsRetainedState : ViewModel() {
    val values = SettingsDraftState()
}

/** Keep only data and Compose state here, never an Activity, callback, job or native resource */
internal class SettingsDraftState {
    private val values = mutableMapOf<String, Any>()

    @Suppress("UNCHECKED_CAST")
    fun <T : Any> state(
        key: String,
        factory: () -> T,
    ): T = values.getOrPut(key, factory) as T

    fun remove(key: String) {
        values.remove(key)
    }
}

private fun Context.settingsActivity(): ComponentActivity =
    when (this) {
        is ComponentActivity -> this
        is ContextWrapper -> baseContext.settingsActivity()
        else -> error("Settings state requires a ComponentActivity")
    }

@Composable
internal fun <T : Any> rememberSettingsRetainedState(
    key: String,
    factory: () -> T,
): T {
    val activity = LocalContext.current.settingsActivity()
    val retained = remember(activity) { ViewModelProvider(activity)[SettingsRetainedState::class.java] }
    return retained.values.state(key, factory)
}

/** A draft lives for this editor visit and survives Activity configuration recreation */
@Composable
internal fun rememberSettingsEditorDraft(key: String): SettingsDraftState {
    val activity = LocalContext.current.settingsActivity()
    val retained = remember(activity) { ViewModelProvider(activity)[SettingsRetainedState::class.java] }
    val draft = retained.values.state(key) { SettingsDraftState() }
    DisposableEffect(activity, draft) {
        onDispose {
            if (!activity.isChangingConfigurations) retained.values.remove(key)
        }
    }
    return draft
}

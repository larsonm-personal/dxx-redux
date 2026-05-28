package com.dxxredux.app.multiplayer

import androidx.activity.compose.BackHandler
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusManager
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.input.InputMode
import androidx.compose.ui.input.InputModeManager
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalInputModeManager
import kotlinx.coroutines.delay

internal enum class MultiplayerBrowserInitialFocusTarget {
    LAN,
    CANCEL_CONNECT,
    REFRESH_LOBBIES,
}

internal enum class LanDiscoveryInitialFocusTarget {
    PERMISSION_ACTION,
    PRIMARY_ACTION,
}

internal fun multiplayerBrowserInitialFocusTarget(status: ConnectionStatus): MultiplayerBrowserInitialFocusTarget =
    when (status) {
        ConnectionStatus.DISCONNECTED -> MultiplayerBrowserInitialFocusTarget.LAN

        ConnectionStatus.CONNECTING,
        ConnectionStatus.AUTHENTICATING,
        ConnectionStatus.RECONNECTING,
        -> MultiplayerBrowserInitialFocusTarget.CANCEL_CONNECT

        ConnectionStatus.CONNECTED -> MultiplayerBrowserInitialFocusTarget.REFRESH_LOBBIES
    }

internal fun lanDiscoveryInitialFocusTarget(permissionGranted: Boolean): LanDiscoveryInitialFocusTarget =
    if (permissionGranted) {
        LanDiscoveryInitialFocusTarget.PRIMARY_ACTION
    } else {
        LanDiscoveryInitialFocusTarget.PERMISSION_ACTION
    }

internal fun controllerBackShouldExitTextEntry(textEntryActive: Boolean): Boolean = textEntryActive

@Composable
internal fun RequestControllerInitialFocus(
    focusRequester: FocusRequester,
    key: Any? = Unit,
) {
    val inputModeManager = LocalInputModeManager.current
    LaunchedEffect(focusRequester, key) {
        inputModeManager.requestInputMode(InputMode.Keyboard)
        withFrameNanos { }
        focusRequester.requestFocusSafely()
        delay(300)
        inputModeManager.requestInputMode(InputMode.Keyboard)
        withFrameNanos { }
        focusRequester.requestFocusSafely()
    }
}

@Composable
internal fun ControllerTextEntryBackHandler(
    textEntryActive: Boolean,
    fallbackFocusRequester: FocusRequester,
    onTextEntryActiveChange: (Boolean) -> Unit,
) {
    val focusManager = LocalFocusManager.current
    val inputModeManager = LocalInputModeManager.current
    BackHandler(enabled = textEntryActive) {
        endControllerTextEntry(focusManager, inputModeManager, fallbackFocusRequester, onTextEntryActiveChange)
    }
}

@Composable
internal fun rememberControllerTextEntryDismiss(
    textEntryActive: Boolean,
    fallbackFocusRequester: FocusRequester,
    onTextEntryActiveChange: (Boolean) -> Unit,
    onDismiss: () -> Unit,
): () -> Unit {
    val focusManager = LocalFocusManager.current
    val inputModeManager = LocalInputModeManager.current
    return remember(
        textEntryActive,
        focusManager,
        inputModeManager,
        fallbackFocusRequester,
        onTextEntryActiveChange,
        onDismiss,
    ) {
        {
            if (controllerBackShouldExitTextEntry(textEntryActive)) {
                endControllerTextEntry(
                    focusManager,
                    inputModeManager,
                    fallbackFocusRequester,
                    onTextEntryActiveChange,
                )
            } else {
                onDismiss()
            }
        }
    }
}

internal fun Modifier.controllerTextEntryFocus(onTextEntryActiveChange: (Boolean) -> Unit): Modifier =
    onFocusChanged { onTextEntryActiveChange(it.isFocused) }

private fun endControllerTextEntry(
    focusManager: FocusManager,
    inputModeManager: InputModeManager,
    fallbackFocusRequester: FocusRequester,
    onTextEntryActiveChange: (Boolean) -> Unit,
) {
    focusManager.clearFocus(force = true)
    onTextEntryActiveChange(false)
    inputModeManager.requestInputMode(InputMode.Keyboard)
    fallbackFocusRequester.requestFocusSafely()
}

private fun FocusRequester.requestFocusSafely() {
    runCatching { requestFocus() }
}

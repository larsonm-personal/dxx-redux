package com.dxxredux.app.multiplayer

import androidx.activity.compose.BackHandler
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusDirection
import androidx.compose.ui.focus.FocusManager
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.focus.focusTarget
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.input.InputMode
import androidx.compose.ui.input.InputModeManager
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalInputModeManager
import com.dxxredux.app.dpadTextFieldNavigation
import kotlinx.coroutines.delay

internal enum class MultiplayerBrowserInitialFocusTarget {
    CONNECT,
    CANCEL_CONNECT,
    REFRESH_LOBBIES,
}

internal enum class LanDiscoveryInitialFocusTarget {
    PERMISSION_ACTION,
    PRIMARY_ACTION,
}

internal fun multiplayerBrowserInitialFocusTarget(status: ConnectionStatus): MultiplayerBrowserInitialFocusTarget =
    when (status) {
        ConnectionStatus.DISCONNECTED -> MultiplayerBrowserInitialFocusTarget.CONNECT

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

internal enum class CoopSaveFocusTarget {
    RESTORE,
    START_FRESH,
}

internal fun selectedCoopSaveFocusTarget(useRestore: Boolean): CoopSaveFocusTarget =
    if (useRestore) CoopSaveFocusTarget.RESTORE else CoopSaveFocusTarget.START_FRESH

@Composable
internal fun RequestControllerInitialFocus(
    focusRequester: FocusRequester,
    key: Any? = Unit,
    revealFocusOnRequest: Boolean = true,
) {
    val inputModeManager = LocalInputModeManager.current
    LaunchedEffect(focusRequester, key, revealFocusOnRequest) {
        if (!revealFocusOnRequest) return@LaunchedEffect
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
internal fun Modifier.showControllerFocusOnDpad(
    initialFocusRequester: FocusRequester,
    key: Any? = Unit,
): Modifier {
    val pageFocusRequester = remember { FocusRequester() }
    val focusManager = LocalFocusManager.current
    val inputModeManager = LocalInputModeManager.current
    var pageTargetFocused by remember { mutableStateOf(false) }
    LaunchedEffect(pageFocusRequester, key) {
        inputModeManager.requestInputMode(InputMode.Keyboard)
        withFrameNanos { }
        pageFocusRequester.requestFocusSafely()
    }
    return focusRequester(pageFocusRequester)
        .onFocusChanged { pageTargetFocused = it.isFocused }
        .focusTarget()
        .onPreviewKeyEvent {
            if (pageTargetFocused && it.type == KeyEventType.KeyDown) {
                val direction = it.key.controllerFocusDirection()
                if (direction != null) {
                    inputModeManager.requestInputMode(InputMode.Keyboard)
                    initialFocusRequester.requestFocusSafely()
                    focusManager.moveFocus(direction)
                    return@onPreviewKeyEvent true
                }
            }
            false
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

internal fun Modifier.controllerTextFieldDpadExit(
    up: FocusRequester? = null,
    down: FocusRequester? = null,
): Modifier = dpadTextFieldNavigation(up = up, down = down)

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

internal fun FocusRequester.requestFocusSafely() {
    runCatching { requestFocus() }
}

private fun Key.controllerFocusDirection(): FocusDirection? =
    when (this) {
        Key.DirectionLeft -> FocusDirection.Left
        Key.DirectionRight -> FocusDirection.Right
        Key.DirectionUp -> FocusDirection.Up
        Key.DirectionDown -> FocusDirection.Down
        else -> null
    }

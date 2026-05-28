package com.dxxredux.app.multiplayer

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/** Chat area with message list and input field, shared between LAN and matchmaking lobbies. */
@Composable
internal fun ChatArea(
    messages: List<ChatMessage>,
    onSend: (String) -> Unit,
    modifier: Modifier = Modifier,
    textEntryFallbackFocusRequester: FocusRequester? = null,
) {
    var text by remember { mutableStateOf("") }
    var textEntryActive by remember { mutableStateOf(false) }
    val sendFocus = remember { FocusRequester() }
    val fallbackFocusRequester = textEntryFallbackFocusRequester ?: sendFocus

    ControllerTextEntryBackHandler(textEntryActive, fallbackFocusRequester) { textEntryActive = it }

    Column(modifier = modifier) {
        Text("Chat", style = MaterialTheme.typography.titleSmall)
        val chatListState = rememberLazyListState()
        LaunchedEffect(messages.size) {
            if (messages.isNotEmpty()) {
                chatListState.animateScrollToItem(messages.size - 1)
            }
        }
        LazyColumn(
            state = chatListState,
            modifier = Modifier.weight(1f).fillMaxWidth(),
        ) {
            items(messages) { msg ->
                val prefix = if (msg.isMe) "You" else msg.fromCallsign
                Text(
                    "$prefix: ${msg.text}",
                    fontSize = 12.sp,
                    color =
                        if (msg.isMe) {
                            MaterialTheme.colorScheme.primary
                        } else {
                            MaterialTheme.colorScheme.onSurface
                        },
                )
            }
        }
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth(),
        ) {
            OutlinedTextField(
                value = text,
                onValueChange = { text = it },
                placeholder = { Text("Message...") },
                singleLine = true,
                modifier =
                    Modifier
                        .weight(1f)
                        .controllerTextEntryFocus { textEntryActive = it },
            )
            Spacer(Modifier.width(4.dp))
            Button(
                onClick = {
                    if (text.isNotBlank()) {
                        onSend(text.trim())
                        text = ""
                    }
                },
                enabled = text.isNotBlank(),
                modifier = Modifier.focusRequester(sendFocus),
            ) {
                Text("Send")
            }
        }
    }
}

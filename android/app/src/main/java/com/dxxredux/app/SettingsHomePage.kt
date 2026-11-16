package com.dxxredux.app

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusProperties
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/** State belongs to the Settings visit, above child-page switching */
@Composable
fun SettingsHomePage(
    query: String,
    onQueryChange: (String) -> Unit,
    listState: LazyListState,
    focusedEntryId: String?,
    onFocusedEntryChange: (String) -> Unit,
    controllerFocusActive: Boolean = true,
    onOpen: (SettingsSearchEntry) -> Unit,
    onBack: () -> Unit,
) {
    BackHandler(onBack = onBack)
    val results = remember(query) { SettingsCatalog.search(query) }
    val searchFocus = remember { FocusRequester() }
    val rowFocus = remember(results) { results.associate { it.id to FocusRequester() } }
    val focusManager = LocalFocusManager.current
    val keyboard = LocalSoftwareKeyboardController.current
    val scope = rememberCoroutineScope()
    // Returning from a child restores the invoking row; typing must never steal search focus
    LaunchedEffect(controllerFocusActive) {
        if (controllerFocusActive) {
            val index = results.indexOfFirst { it.id == focusedEntryId }.takeIf { it >= 0 } ?: 0
            val entry = results.getOrNull(index)
            if (entry != null) {
                if (listState.layoutInfo.visibleItemsInfo.none { it.key == entry.id }) listState.scrollToItem(index)
                withFrameNanos { }
                rowFocus[entry.id]?.requestFocus()
            }
        }
    }
    // Debounce spoken result counts so TalkBack does not chatter for every keystroke
    var resultCount by remember { mutableIntStateOf(results.size) }
    LaunchedEffect(results.size) {
        delay(500)
        resultCount = results.size
    }
    Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
        Column(
            Modifier
                .fillMaxSize()
                .safeDrawingPadding()
                .imePadding()
                .padding(16.dp)
                .repeatVerticalDpadFocus(),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Column(Modifier.widthIn(max = 720.dp).fillMaxSize()) {
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    TextButton(onClick = onBack, modifier = Modifier.heightIn(min = 48.dp).tvFocusBorder()) {
                        Text("< Back")
                    }
                    Text(
                        "Settings",
                        style = MaterialTheme.typography.headlineSmall,
                        modifier = Modifier.semantics { heading() },
                    )
                }
                OutlinedTextField(
                    value = query,
                    onValueChange = {
                        onQueryChange(it)
                        scope.launch { listState.scrollToItem(0) }
                    },
                    label = { Text("Search settings") },
                    singleLine = true,
                    trailingIcon = {
                        if (query.isNotEmpty()) {
                            TextButton(
                                onClick = {
                                    onQueryChange("")
                                    scope.launch { listState.scrollToItem(0) }
                                },
                                modifier = Modifier.sizeIn(minWidth = 48.dp, minHeight = 48.dp).tvFocusBorder(),
                            ) { Text("Clear") }
                        }
                    },
                    modifier = Modifier.fillMaxWidth().focusRequester(searchFocus),
                )
                Spacer(Modifier.height(8.dp))
                if (query.isNotBlank()) {
                    Text(
                        if (results.isEmpty()) {
                            "No settings found. Try another name."
                        } else {
                            "$resultCount settings found"
                        },
                        style = MaterialTheme.typography.bodyMedium,
                        modifier = Modifier.padding(vertical = 8.dp).semantics { liveRegion = LiveRegionMode.Polite },
                    )
                }
                LazyColumn(
                    state = listState,
                    modifier = Modifier.fillMaxWidth().weight(1f),
                    verticalArrangement = Arrangement.spacedBy(6.dp),
                ) {
                    itemsIndexed(results, key = { _, entry -> entry.id }) { index, entry ->
                        OutlinedButton(
                            onClick = {
                                onFocusedEntryChange(entry.id)
                                keyboard?.hide()
                                focusManager.clearFocus()
                                onOpen(entry)
                            },
                            shape = MaterialTheme.shapes.medium,
                            contentPadding = PaddingValues(12.dp),
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .heightIn(min = 56.dp)
                                    .focusRequester(rowFocus.getValue(entry.id))
                                    .focusProperties { if (index == 0) up = searchFocus }
                                    .onFocusChanged { if (it.isFocused) onFocusedEntryChange(entry.id) }
                                    .tvFocusBorder(),
                        ) {
                            Column(Modifier.weight(1f)) {
                                Text(entry.title, fontWeight = FontWeight.SemiBold)
                                if (query.isNotBlank()) {
                                    Text(
                                        entry.path,
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    )
                                }
                            }
                            Text(">", modifier = Modifier.padding(start = 8.dp).clearAndSetSemantics { })
                        }
                    }
                }
            }
        }
    }
}

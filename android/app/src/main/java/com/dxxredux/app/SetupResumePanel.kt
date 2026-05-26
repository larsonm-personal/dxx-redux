package com.dxxredux.app

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

private fun resumeGameDisplayName(game: String): String = if (game == "d1") "Descent 1" else "Descent 2"

private fun resumeSaveKindLabel(saveKind: String): String =
    when (saveKind) {
        "auto_minimize" -> "Auto-save on minimize"
        "auto_exit" -> "Auto-save on exit"
        "auto_progress" -> "Highest progress save"
        else -> "Manual save"
    }

private fun formatResumeSaveTime(unixSeconds: Long): String {
    if (unixSeconds <= 0L) return "Unknown"
    val format = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US)
    return format.format(Date(unixSeconds * 1000L))
}

private fun formatResumeDuration(totalSeconds: Long): String {
    if (totalSeconds < 0L) return "Unknown"
    val hours = totalSeconds / 3600L
    val minutes = (totalSeconds % 3600L) / 60L
    val seconds = totalSeconds % 60L
    return if (hours > 0L) {
        String.format(Locale.US, "%d:%02d:%02d", hours, minutes, seconds)
    } else {
        String.format(Locale.US, "%d:%02d", minutes, seconds)
    }
}

internal fun decodeResumeSaveThumbnail(candidate: ResumeSaveBridge.ResumeSaveCandidate): Bitmap? {
    val rgb = candidate.thumbnailRgb6 ?: return null
    val width = candidate.thumbnailWidth
    val height = candidate.thumbnailHeight
    val pixelCount = width * height
    val expectedBytes = pixelCount * 3
    var hasVisiblePixel = false

    if (width <= 0 || height <= 0 || rgb.size < expectedBytes) return null
    for (index in 0 until expectedBytes) {
        if (rgb[index].toInt() != 0) {
            hasVisiblePixel = true
            break
        }
    }
    if (!hasVisiblePixel) return null

    val pixels = IntArray(pixelCount)
    var src = 0
    for (index in 0 until pixelCount) {
        val red = rgb[src++].toInt() and 0xFF
        val green = rgb[src++].toInt() and 0xFF
        val blue = rgb[src++].toInt() and 0xFF
        pixels[index] = (0xFF shl 24) or (red shl 16) or (green shl 8) or blue
    }
    return Bitmap.createBitmap(pixels, width, height, Bitmap.Config.ARGB_8888)
}

@Composable
internal fun ResumeSavePanel(
    candidate: ResumeSaveBridge.ResumeSaveCandidate,
    options: ResumeSaveBridge.ResumeSaveOptions?,
    thumbnail: Bitmap?,
    onLoad: () -> Unit,
    onLoadCandidate: (ResumeSaveBridge.ResumeSaveCandidate) -> Unit,
    onHide: () -> Unit,
    onStopShowing: () -> Unit,
) {
    val panelColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.94f)
    var showChooser by remember { mutableStateOf(false) }
    ElevatedCard(
        modifier = Modifier.fillMaxWidth(),
        colors =
            CardDefaults.elevatedCardColors(
                containerColor = panelColor,
            ),
        elevation = CardDefaults.elevatedCardElevation(defaultElevation = 4.dp),
    ) {
        Column(
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 6.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Surface(
                    modifier = Modifier.size(width = 54.dp, height = 27.dp),
                    shape = RoundedCornerShape(8.dp),
                    color = MaterialTheme.colorScheme.surface,
                ) {
                    if (thumbnail != null) {
                        Image(
                            bitmap = thumbnail.asImageBitmap(),
                            contentDescription = "Save thumbnail",
                            modifier = Modifier.fillMaxSize(),
                            contentScale = ContentScale.FillBounds,
                        )
                    } else {
                        Box(
                            modifier = Modifier.fillMaxSize(),
                            contentAlignment = Alignment.Center,
                        ) {
                            Text(
                                "No thumbnail",
                                fontSize = 6.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }
                Spacer(modifier = Modifier.width(8.dp))
                TextButton(
                    onClick = onStopShowing,
                    contentPadding = PaddingValues(horizontal = 4.dp, vertical = 2.dp),
                ) {
                    Text("Stop Showing This", fontSize = 7.sp, maxLines = 1)
                }
                Text(
                    "Resume Recent Save",
                    modifier = Modifier.weight(1f),
                    fontSize = 10.sp,
                    fontWeight = FontWeight.SemiBold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                IconButton(
                    onClick = onHide,
                    modifier = Modifier.size(24.dp),
                ) {
                    Icon(
                        imageVector = Icons.Filled.KeyboardArrowUp,
                        contentDescription = "Hide resume panel",
                    )
                }
            }

            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(1.dp),
            ) {
                Text(
                    resumePanelPrimaryLine(candidate),
                    fontSize = 7.sp,
                    fontWeight = FontWeight.SemiBold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                Text(
                    resumePanelSecondaryLine(candidate),
                    fontSize = 7.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Button(
                    onClick = onLoad,
                    modifier = Modifier.weight(1f),
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 6.dp),
                ) {
                    Text("Load Last Save", fontSize = 9.sp, maxLines = 1)
                }
                Button(
                    onClick = { showChooser = true },
                    modifier = Modifier.weight(1f),
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 6.dp),
                    enabled = options.hasChooseCandidates(),
                ) {
                    Text("Choose Save", fontSize = 9.sp, maxLines = 1)
                }
            }
        }
    }

    if (showChooser && options != null) {
        ResumeSaveChoiceDialog(
            options = options,
            onDismiss = { showChooser = false },
            onLoadCandidate = {
                showChooser = false
                onLoadCandidate(it)
            },
        )
    }
}

private fun ResumeSaveBridge.ResumeSaveOptions?.hasChooseCandidates(): Boolean =
    this?.let { it.highestProgress != null || it.lastExit != null || it.lastMinimize != null } == true

@Composable
private fun ResumeSaveChoiceDialog(
    options: ResumeSaveBridge.ResumeSaveOptions,
    onDismiss: () -> Unit,
    onLoadCandidate: (ResumeSaveBridge.ResumeSaveCandidate) -> Unit,
) {
    val choices =
        listOfNotNull(
            options.highestProgress?.let { "Highest Progress" to it },
            options.lastExit?.let { "Last Exit Save" to it },
            options.lastMinimize?.let { "Last Minimize Save" to it },
        )

    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel")
            }
        },
        title = { Text("Choose Save") },
        text = {
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                choices.forEach { (label, choice) ->
                    Button(
                        onClick = { onLoadCandidate(choice) },
                        modifier = Modifier.fillMaxWidth(),
                        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 10.dp),
                    ) {
                        Column(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalAlignment = Alignment.Start,
                        ) {
                            Text(label, fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
                            Text(
                                resumeChoiceLine(choice),
                                fontSize = 9.sp,
                                maxLines = 2,
                                overflow = TextOverflow.Ellipsis,
                            )
                        }
                    }
                }
            }
        },
    )
}

internal fun resolveResumeSaveLaunchPath(
    filesDir: File,
    candidate: ResumeSaveBridge.ResumeSaveCandidate,
): String? {
    candidate.relativePath.takeIf { it.isNotBlank() }?.let { return it }
    val filesPrefix = filesDir.absolutePath.replace('\\', '/') + "/"
    val absolutePath = candidate.path.replace('\\', '/')
    absolutePath.takeIf { it.startsWith(filesPrefix) }?.removePrefix(filesPrefix)?.let { return it }
    return relativeSavePathFromGameAnchor(absolutePath)
}

internal fun resolveResumeSaveLaunchCallsign(candidate: ResumeSaveBridge.ResumeSaveCandidate): String? {
    candidate.callsign.takeIf { it.isNotBlank() }?.let { return it }
    return callsignFromSavePath(candidate.relativePath)
        ?: callsignFromSavePath(candidate.path)
}

private fun relativeSavePathFromGameAnchor(path: String): String? {
    val normalized = path.replace('\\', '/')
    listOf("/d1x-redux/", "/d2x-redux/").forEach { anchor ->
        val index = normalized.indexOf(anchor)
        if (index >= 0) return normalized.substring(index + 1)
    }
    return null
}

private fun callsignFromSavePath(path: String): String? {
    val normalized = path.replace('\\', '/')
    val fileName = normalized.substringAfterLast('/')
    val dotIndex = fileName.lastIndexOf('.')
    if (dotIndex <= 0) return null
    return fileName.substring(0, dotIndex).takeIf { it.isNotBlank() }
}

private fun resumePanelPrimaryLine(candidate: ResumeSaveBridge.ResumeSaveCandidate): String {
    val callsign = resolveResumeSaveLaunchCallsign(candidate) ?: "unknown pilot"
    return buildString {
        append(candidate.description.ifBlank { "AUTO SAVE" })
        append(" | ")
        append(resumeGameDisplayName(candidate.game))
        append(" | ")
        append(callsign)
    }
}

private fun resumePanelSecondaryLine(candidate: ResumeSaveBridge.ResumeSaveCandidate): String =
    buildString {
        append(resumeSaveKindLabel(candidate.saveKind))
        append(" | ")
        append(("${candidate.levelNum} ${candidate.levelName}").trim())
        append(" | ")
        append(formatResumeSaveTime(candidate.saveTimeUnixSeconds))
        append(" | ")
        append(formatResumeDuration(candidate.levelSeconds))
        append(" lvl")
        append(" | ")
        append(formatResumeDuration(candidate.totalSeconds))
        append(" total")
    }

private fun resumeChoiceLine(candidate: ResumeSaveBridge.ResumeSaveCandidate): String {
    val callsign = resolveResumeSaveLaunchCallsign(candidate) ?: "unknown pilot"
    return buildString {
        append(resumeGameDisplayName(candidate.game))
        append(" | ")
        append(callsign)
        append(" | ")
        append(("${candidate.levelNum} ${candidate.levelName}").trim())
        append(" | ")
        append(formatResumeSaveTime(candidate.saveTimeUnixSeconds))
    }
}

internal fun resumeCandidateLogSummary(candidate: ResumeSaveBridge.ResumeSaveCandidate?): String {
    if (candidate == null) return "none"
    val launchPath = candidate.relativePath.ifBlank { candidate.path }
    val callsign = candidate.callsign.ifBlank { "-" }
    return "game=${candidate.game} kind=${candidate.saveKind} slot=${candidate.slot} " +
        "path=$launchPath callsign=$callsign meta=${candidate.metadataBacked}"
}

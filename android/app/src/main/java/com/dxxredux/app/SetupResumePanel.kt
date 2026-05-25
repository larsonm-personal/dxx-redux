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
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
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
    thumbnail: Bitmap?,
    onLoad: () -> Unit,
    onHide: () -> Unit,
    onStopShowing: () -> Unit,
) {
    val panelColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.94f)
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
                    onClick = onStopShowing,
                    modifier = Modifier.weight(1f),
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 6.dp),
                    colors =
                        ButtonDefaults.buttonColors(
                            containerColor = panelColor,
                            contentColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        ),
                ) {
                    Text("Stop Showing This", fontSize = 9.sp, maxLines = 1)
                }
                Button(
                    onClick = onLoad,
                    modifier = Modifier.weight(1f),
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 6.dp),
                ) {
                    Text("Load Last Save", fontSize = 9.sp, maxLines = 1)
                }
            }
        }
    }
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

internal fun resumeCandidateLogSummary(candidate: ResumeSaveBridge.ResumeSaveCandidate?): String {
    if (candidate == null) return "none"
    val launchPath = candidate.relativePath.ifBlank { candidate.path }
    val callsign = candidate.callsign.ifBlank { "-" }
    return "game=${candidate.game} kind=${candidate.saveKind} slot=${candidate.slot} " +
        "path=$launchPath callsign=$callsign meta=${candidate.metadataBacked}"
}

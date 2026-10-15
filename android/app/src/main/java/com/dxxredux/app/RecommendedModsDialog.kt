package com.dxxredux.app

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import java.util.Locale

internal data class RecommendedMod(
    val filename: String,
    val displayName: String,
    val description: String,
    val game: String,
    val sizeBytes: Long,
) {
    val downloadUrl: String
        get() = "https://github.com/larsonm-personal/dxx-redux/releases/download/texture-packs-v1/$filename"
}

// Download sizes match the release's mod-packs.json (decimal MB in the UI)
internal val RECOMMENDED_MODS =
    listOf(
        RecommendedMod(
            filename = "uud1tp-textures.dxa",
            displayName = "UUD1 Texture Fixes",
            description =
                "Fixes original-resolution textures, selected level surfaces and door timing " +
                    "for D1 PC retail and D1-in-D2. " +
                    "Place above high-res packs to retain its corrected artwork " +
                    "(higher mods in the list mask matching assets in lower ones)",
            game = "d1",
            sizeBytes = 473_084L,
        ),
        RecommendedMod(
            filename = "uud2tp-textures.dxa",
            displayName = "UUD2 Texture Fixes and Extras",
            description =
                "Fixes original-resolution textures and restores extra doors, lava and surfaces " +
                    "for D2 custom missions. " +
                    "Place above high-res packs to retain its corrected artwork " +
                    "(higher mods in the list mask matching assets in lower ones)",
            game = "d2",
            sizeBytes = 3_243_334L,
        ),
        RecommendedMod(
            filename = "uud2sp-sounds.dxa",
            displayName = "UUD2SP Sound Fixes and Restorations",
            description =
                "Restores robot and door sounds in D2 and fixes effects' playback rates. " +
                    "Place above D2 High-Res Sounds to retain these effects " +
                    "(higher mods in the list mask matching assets in lower ones)",
            game = "d2",
            sizeBytes = 6_962_917L,
        ),
        RecommendedMod(
            filename = "d1-hires-256-textures-ktx2.dxa",
            displayName = "D1 High-Res Level Textures (256px)",
            description =
                "Sharper wall and object textures for D1 and D1-in-D2. " +
                    "This is the recommended balance of detail and memory use; enable only one D1 texture pack",
            game = "d1",
            sizeBytes = 61_767_022L,
        ),
        RecommendedMod(
            filename = "d2-hires-256-textures-ktx2.dxa",
            displayName = "D2 High-Res Level Textures (256px)",
            description =
                "Sharper wall and object textures for D2. " +
                    "This is the recommended balance of detail and memory use; enable only one D2 texture pack",
            game = "d2",
            sizeBytes = 118_746_528L,
        ),
        RecommendedMod(
            filename = "d1-hires-sounds.dxa",
            displayName = "D1 High-Res Sounds",
            description =
                "Higher-quality 22 kHz sound effects for D1 and D1-in-D2. " +
                    "Can be combined with the texture and menu packs",
            game = "d1",
            sizeBytes = 1_859_837L,
        ),
        RecommendedMod(
            filename = "d2-hires-sounds.dxa",
            displayName = "D2 High-Res Sounds",
            description = "Higher-quality 22 kHz sound effects for D2. Can be combined with the D2 texture pack",
            game = "d2",
            sizeBytes = 3_669_899L,
        ),
        RecommendedMod(
            filename = "d1xr-hires.dxa",
            displayName = "D1 High-Res Menus and Fonts",
            description =
                "Higher-resolution fonts and 640x480 menu and briefing images for D1 and D1-in-D2. " +
                    "Does not replace level textures",
            game = "d1",
            sizeBytes = 1_426_884L,
        ),
        RecommendedMod(
            filename = "d1xr-mac-demo-sounds.dxa",
            displayName = "Mac D1 Missing Sounds",
            description =
                "Restores missing sound effects when using Mac D1 game data in the D1 engine. " +
                    "Not needed for PC game data and does not support Mac data in D1-in-D2",
            game = "d1",
            sizeBytes = 1_008_096L,
        ),
    )

@Composable
internal fun RecommendedModsDialog(
    installedNames: Set<String>,
    downloadProgress: Map<String, Int>,
    onDownload: (RecommendedMod) -> Unit,
    onDismiss: () -> Unit,
    loadDocuments: suspend (
        java.io.File,
        RecommendedMod,
    ) -> RecommendedModDocuments.Loaded = ::loadRecommendedModDocuments,
) {
    var documentsTarget by remember { mutableStateOf<RecommendedMod?>(null) }
    documentsTarget?.let { mod ->
        RecommendedModDocumentsDialog(mod, onDismiss = { documentsTarget = null }, loadDocuments = loadDocuments)
    }
    val scrollState = rememberScrollState()
    NavigationAlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Recommended mods") },
        confirmButton = { TextButton(onClick = onDismiss) { Text("Close") } },
        text = {
            Box(modifier = Modifier.fillMaxWidth().heightIn(max = 480.dp)) {
                Column(
                    modifier = Modifier.verticalScroll(scrollState).padding(bottom = 16.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    Text(
                        "Download packs here, then manage them in Mods/Levels. " +
                            "To update an installed pack, remove it and download it again. " +
                            "Start a fresh game session to use changes.",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                    RECOMMENDED_MODS.forEach { rec ->
                        HorizontalDivider()
                        RecommendedModRow(
                            rec = rec,
                            installed = portableGameFilenameIdentity(rec.filename) in installedNames,
                            progress = downloadProgress[rec.filename],
                            onDownload = { onDownload(rec) },
                            onViewDocs = { documentsTarget = rec },
                        )
                    }
                }
                SetupScrollArrows(scrollState)
            }
        },
    )
}

@Composable
private fun RecommendedModRow(
    rec: RecommendedMod,
    installed: Boolean,
    progress: Int?,
    onDownload: () -> Unit,
    onViewDocs: () -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text(rec.displayName, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.SemiBold)
        Text(rec.description, style = MaterialTheme.typography.bodyMedium)
        Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(
                String.format(Locale.getDefault(), "%.1f MB", rec.sizeBytes / 1_000_000.0),
                modifier = Modifier.weight(1f),
                style = MaterialTheme.typography.labelLarge,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            when {
                installed -> Text("Installed", color = MaterialTheme.colorScheme.primary)
                progress != null && progress in 0..100 -> Text("Downloading $progress%")
                else -> TextButton(onClick = onDownload) { Text(if (progress == -1) "Retry" else "Download") }
            }
        }
        TextButton(onClick = onViewDocs) { Text("View docs") }
        if (!installed && progress == -1) {
            Text("Download failed", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error)
        }
    }
}

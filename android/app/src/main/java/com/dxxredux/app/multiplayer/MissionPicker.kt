package com.dxxredux.app.multiplayer

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.unit.dp
import com.dxxredux.app.AssetManifest
import com.dxxredux.app.GameFileFormats
import com.dxxredux.app.MissionZip
import com.dxxredux.app.ModManager
import com.dxxredux.app.SafManifest
import com.dxxredux.app.dpadTextFieldNavigation
import com.dxxredux.app.launchDataReadyForGame
import java.io.File
import java.util.Locale

/**
 * Scans game data directories for available missions.
 *
 * Duplicated constants from C (keep in sync):
 *   d2/main/mission.h: FULL_MISSION_FILENAME="d2", D1_MISSION_FILENAME="descent",
 *                       SHAREWARE_MISSION_FILENAME="d2demo", MISSION_DIR="missions/"
 *   d1/main/mission.h: D1_MISSION_FILENAME="" (empty for D1 engine), MISSION_DIR="missions/"
 * Display names mirror D1_MISSION_NAME, OEM_MISSION_NAME, etc. from those headers.
 */
object MissionScanner {
    data class MissionInfo(
        val filename: String, // passed to nativeSetAutoHost (e.g. "d2", "descent", "mymod")
        val displayName: String,
        val levelCount: Int = 0, // 0 = unknown
        val anarchyOnly: Boolean = false,
        val isBuiltin: Boolean = false,
        val descriptorPath: String? = null,
        val wrapperFilename: String? = null,
        val archiveSizeBytes: Long? = null,
        val archiveSha256: String? = null,
        val archiveChunkSizeBytes: Int = 0,
        val archiveChunkSha256: List<String> = emptyList(),
    ) {
        val transferable: Boolean
            get() =
                !isBuiltin &&
                    wrapperFilename != null &&
                    archiveSha256 != null &&
                    archiveSizeBytes != null &&
                    archiveSizeBytes in 1..MISSION_TRANSFER_MAX_BYTES
    }

    fun requirement(
        game: String,
        mission: MissionInfo,
        offerDownload: Boolean,
    ): MissionRequirement {
        val kind =
            when {
                mission.isBuiltin -> MissionRequirement.KIND_BUILTIN
                mission.wrapperFilename != null -> MissionRequirement.KIND_WRAPPER
                else -> MissionRequirement.KIND_LOOSE
            }
        val revision =
            if (mission.archiveSha256 != null && mission.archiveSizeBytes != null) {
                "${mission.archiveSha256}:${mission.archiveSizeBytes}:$game:${mission.filename}"
            } else {
                "$kind:$game:${mission.filename}"
            }
        return MissionRequirement(
            revision = revision,
            game = game,
            missionKey = mission.filename,
            displayName = mission.displayName,
            kind = kind,
            descriptorPath = mission.descriptorPath,
            wrapperFilename = mission.wrapperFilename,
            sizeBytes = mission.archiveSizeBytes,
            sha256 = mission.archiveSha256,
            offerAvailable = offerDownload && mission.transferable,
        )
    }

    // -- mirrors d2/main/mission.h --
    private const val D2_SHAREWARE_HOG_SIZE = 2_292_566L
    private const val D2_MAC_SHAREWARE_HOG_SIZE = 4_292_746L
    private const val D2_OEM_HOG_SIZE = 6_132_957L

    private val D1_SHAREWARE_HOG_SIZES = setOf(2_339_773L, 2_365_676L)
    private val D1_MAC_SHAREWARE_HOG_SIZES = setOf(3_370_339L, 3_387_843L)
    private val D1_OEM_HOG_SIZES = setOf(4_492_107L, 4_494_862L)

    internal fun builtins(
        setDir: File,
        game: String,
        includeD1ForD2: Boolean = true,
    ): List<MissionInfo> {
        val d1 = d1Builtin(setDir, if (game == "d2") "descent" else "")
        if (game != "d2") return listOf(d1)

        val full = File(setDir, "descent2.hog")
        val demo = File(setDir, "d2demo.hog")
        val d2 =
            when {
                full.isFile && full.length() == D2_OEM_HOG_SIZE -> {
                    MissionInfo("d2", "D2 Destination: Quartzon", levelCount = 8, isBuiltin = true)
                }

                full.isFile -> {
                    MissionInfo("d2", "Descent 2: Counterstrike!", levelCount = 24, isBuiltin = true)
                }

                demo.isFile && demo.length() == D2_MAC_SHAREWARE_HOG_SIZE -> {
                    MissionInfo("d2demo", "Descent 2 Demo", levelCount = 4, isBuiltin = true)
                }

                demo.isFile && demo.length() == D2_SHAREWARE_HOG_SIZE -> {
                    MissionInfo("d2demo", "Descent 2 Demo", levelCount = 3, isBuiltin = true)
                }

                else -> {
                    MissionInfo("d2", "Descent 2: Counterstrike!", levelCount = 24, isBuiltin = true)
                }
            }
        return if (includeD1ForD2) listOf(d2, d1) else listOf(d2)
    }

    private fun d1Builtin(
        setDir: File,
        filename: String,
    ): MissionInfo {
        val size = File(setDir, "descent.hog").takeIf(File::isFile)?.length()
        val (name, levels) =
            when (size) {
                in D1_SHAREWARE_HOG_SIZES -> "Descent Demo" to 7
                in D1_MAC_SHAREWARE_HOG_SIZES -> "Descent Demo" to 3
                in D1_OEM_HOG_SIZES -> "Destination Saturn" to 15
                else -> "Descent: First Strike" to 27
            }
        return MissionInfo(filename, name, levelCount = levels, isBuiltin = true)
    }

    fun scan(
        filesDir: File,
        setDir: File,
        game: String,
        mode: String,
    ): List<MissionInfo> {
        val manifest = AssetManifest(setDir)
        val safManifest = SafManifest.forDir(setDir)
        val includeD1ForD2 =
            game != "d2" ||
                (
                    launchDataReadyForGame("d1", setDir, manifest, safManifest) &&
                        launchDataReadyForGame("d2", setDir, manifest, safManifest)
                )
        val builtins = builtins(setDir, game, includeD1ForD2)
        val missions = builtins.toMutableList()
        val seen = builtins.map { it.filename.lowercase(Locale.ROOT) }.toMutableSet()

        val modManager = ModManager(filesDir, setDir = setDir)
        val missionMods =
            modManager
                .listMods()
                .filter { mod ->
                    mod.enabled &&
                        mod.kind == ModManager.MOD_KIND_MISSION_ZIP &&
                        (
                            mod.game == game ||
                                mod.game == "both" ||
                                (game == "d2" && includeD1ForD2 && mod.game == "d1")
                        )
                }.sortedBy { it.order }
        for (mod in missionMods) {
            val wrapper = modManager.modFile(mod.filename)
            val scan = runCatching { MissionZip.inspect(wrapper) }.getOrNull() ?: continue
            val identity = runCatching { modManager.ensureMissionContentIdentity(mod.filename) }.getOrNull()
            for (missionSet in scan.effectiveMissionSets) {
                val descriptor = missionSet.mission
                val compatible =
                    descriptor.game == game ||
                        (game == "d2" && includeD1ForD2 && descriptor.game == "d1")
                if (!compatible) continue
                val anarchyOnly = descriptor.type.equals("anarchy", ignoreCase = true)
                if (mode == "coop" && anarchyOnly) continue
                val basename = File(descriptor.path.replace('/', File.separatorChar)).nameWithoutExtension
                if (!seen.add(basename.lowercase(Locale.ROOT))) continue
                missions +=
                    MissionInfo(
                        filename = basename,
                        displayName = descriptor.displayName,
                        levelCount = descriptor.declaredLevelCount ?: descriptor.levelNames.size,
                        anarchyOnly = anarchyOnly,
                        descriptorPath = descriptor.path,
                        wrapperFilename = mod.filename,
                        archiveSizeBytes = identity?.sizeBytes,
                        archiveSha256 = identity?.sha256,
                        archiveChunkSizeBytes = identity?.chunkSizeBytes ?: 0,
                        archiveChunkSha256 = identity?.chunkSha256.orEmpty(),
                    )
            }
        }

        val dirs = listOf(setDir, File(setDir, "missions"))

        for (dir in dirs) {
            val files = dir.listFiles() ?: continue
            for (file in files) {
                if (!file.isFile) continue
                val descriptorGame = GameFileFormats.gameForDescriptor(file.name) ?: continue
                val compatible =
                    descriptorGame == game ||
                        (game == "d2" && includeD1ForD2 && descriptorGame == "d1")
                if (!compatible) continue
                val basename = file.nameWithoutExtension
                if (basename.lowercase(Locale.ROOT) in seen) continue
                val info = parseMissionFile(file, basename)
                if (info != null && (mode != "coop" || !info.anarchyOnly)) {
                    seen.add(basename.lowercase(Locale.ROOT))
                    missions.add(info)
                }
            }
        }

        return missions.sortedWith(compareBy({ !it.isBuiltin }, { it.displayName.lowercase(Locale.ROOT) }))
    }

    private fun parseMissionFile(
        file: File,
        basename: String,
    ): MissionInfo? =
        try {
            val descriptor = MissionZip.parseMissionDescriptor(file.name, file.readBytes())
            MissionInfo(
                filename = basename,
                displayName = descriptor.displayName,
                levelCount = descriptor.declaredLevelCount ?: descriptor.levelNames.size,
                anarchyOnly = descriptor.type.equals("anarchy", ignoreCase = true),
            )
        } catch (_: Exception) {
            MissionInfo(filename = basename, displayName = basename)
        }
}

/**
 * A composable that shows the currently-selected mission and opens a picker dialog on tap.
 * Drop-in replacement for the OutlinedTextField that both host dialogs previously used.
 */
@Composable
fun MissionPickerField(
    selectedFilename: String?,
    missions: List<MissionScanner.MissionInfo>,
    onSelect: (MissionScanner.MissionInfo) -> Unit,
    modifier: Modifier = Modifier,
) {
    val displayText =
        remember(selectedFilename, missions) {
            if (selectedFilename == null) {
                ""
            } else {
                resolveMissionSelection(missions, selectedFilename)?.displayName ?: selectedFilename
            }
        }

    var showPicker by remember { mutableStateOf(false) }

    OutlinedButton(
        onClick = { showPicker = true },
        modifier = modifier.fillMaxWidth(),
    ) {
        Column(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(2.dp),
        ) {
            Text("Mission", style = MaterialTheme.typography.labelSmall)
            Text(displayText.ifBlank { "Tap to select" }, style = MaterialTheme.typography.bodyMedium)
        }
    }

    if (showPicker) {
        MissionPickerDialog(
            missions = missions,
            onSelect = {
                onSelect(it)
                showPicker = false
            },
            onDismiss = { showPicker = false },
        )
    }
}

@Composable
private fun MissionPickerDialog(
    missions: List<MissionScanner.MissionInfo>,
    onSelect: (MissionScanner.MissionInfo) -> Unit,
    onDismiss: () -> Unit,
) {
    var filter by remember { mutableStateOf("") }
    var textEntryActive by remember { mutableStateOf(false) }
    val dismissFocus = remember { FocusRequester() }
    val dismissOrEndTextEntry =
        rememberControllerTextEntryDismiss(textEntryActive, dismissFocus, { textEntryActive = it }, onDismiss)

    RequestControllerInitialFocus(dismissFocus)

    val filtered =
        remember(filter, missions) {
            if (filter.isBlank()) {
                missions
            } else {
                missions.filter {
                    it.displayName.contains(filter, ignoreCase = true) ||
                        it.filename.contains(filter, ignoreCase = true)
                }
            }
        }

    AlertDialog(
        onDismissRequest = dismissOrEndTextEntry,
        title = { Text("Select Mission") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                if (missions.size > 5) {
                    OutlinedTextField(
                        value = filter,
                        onValueChange = { filter = it },
                        placeholder = { Text("Search...") },
                        singleLine = true,
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .dpadTextFieldNavigation(up = dismissFocus)
                                .controllerTextEntryFocus { textEntryActive = it },
                    )
                }
                LazyColumn(modifier = Modifier.heightIn(max = 320.dp)) {
                    items(filtered) { m ->
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .clickable { onSelect(m) }
                                    .padding(vertical = 8.dp, horizontal = 4.dp),
                        ) {
                            Column {
                                Text(m.displayName, style = MaterialTheme.typography.bodyLarge)
                                if (m.filename.isNotEmpty() && m.filename != m.displayName) {
                                    Text(
                                        m.filename,
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    )
                                }
                                if (m.anarchyOnly) {
                                    Text(
                                        "Anarchy only",
                                        style = MaterialTheme.typography.labelSmall,
                                        color = MaterialTheme.colorScheme.error,
                                    )
                                }
                            }
                        }
                        HorizontalDivider()
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss, modifier = Modifier.focusRequester(dismissFocus)) { Text("Cancel") }
        },
    )
}

internal fun resolveMissionSelection(
    missions: List<MissionScanner.MissionInfo>,
    selectedFilename: String?,
): MissionScanner.MissionInfo? =
    selectedFilename?.let { selected ->
        missions.singleOrNull { it.filename.equals(selected, ignoreCase = true) }
    }

internal fun selectedMissionLevelIsValid(
    mission: MissionScanner.MissionInfo?,
    level: Int?,
): Boolean = mission != null && mission.levelCount > 0 && level != null && level in 1..mission.levelCount

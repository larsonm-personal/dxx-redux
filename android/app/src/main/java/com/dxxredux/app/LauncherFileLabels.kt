package com.dxxredux.app

import java.io.File

internal fun launcherExtensionOf(filename: String): String = GameFileFormats.extensionOf(filename)

internal fun isLauncherDxaFilename(filename: String): Boolean = GameFileFormats.isDxa(filename)

internal fun stripLauncherDxaSuffix(filename: String): String = GameFileFormats.stripDxaSuffix(filename)

internal fun launcherFileTypeLabel(filename: String): String = GameFileFormats.typeLabel(filename)

internal fun launcherExtensionDescription(filename: String): String = GameFileFormats.extensionDescription(filename)

internal fun launcherStorageFilePurpose(
    file: File,
    relativePath: String,
    importedRootFile: Boolean,
): String = GameFileFormats.storagePurpose(file, relativePath, importedRootFile)

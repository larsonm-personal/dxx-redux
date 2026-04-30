package com.dxxredux.app

data class ImportChooserConfig(
    val directPickLabel: String,
    val helpText: String,
)

fun importChooserConfigForDevice(isAndroidTv: Boolean): ImportChooserConfig =
    if (isAndroidTv) {
        ImportChooserConfig(
            directPickLabel = "Pick Single File",
            helpText = "Pick a single file or a folder containing the files to import",
        )
    } else {
        ImportChooserConfig(
            directPickLabel = "Pick Multiple Files",
            helpText = "Pick multiple files or a folder containing the files to import",
        )
    }

package com.dxxredux.app

import android.content.ClipData
import android.content.Intent
import android.net.Uri

internal fun Intent.attachReadGrant(uris: List<Uri>) {
    require(uris.isNotEmpty()) { "A share intent needs at least one URI" }
    clipData =
        ClipData.newRawUri("shared file", uris.first()).also { clip ->
            uris.drop(1).forEach { clip.addItem(ClipData.Item(it)) }
        }
    addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
}

internal fun Intent.attachReadGrant(uri: Uri) = attachReadGrant(listOf(uri))

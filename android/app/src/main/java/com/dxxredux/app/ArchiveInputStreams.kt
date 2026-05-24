package com.dxxredux.app

import java.io.BufferedInputStream
import java.io.InputStream
import java.io.PushbackInputStream
import java.util.zip.ZipException
import java.util.zip.ZipInputStream

private val ZIP_LOCAL_FILE_HEADER = byteArrayOf(0x50, 0x4b, 0x03, 0x04)

internal fun openZipInputStreamSkippingPreamble(input: InputStream): ZipInputStream {
    val stream = PushbackInputStream(BufferedInputStream(input), ZIP_LOCAL_FILE_HEADER.size)
    var matched = 0
    while (true) {
        val next = stream.read()
        if (next < 0) throw ZipException("ZIP local file header not found")
        val expected = ZIP_LOCAL_FILE_HEADER[matched].toInt() and 0xff
        if (next == expected) {
            matched++
            if (matched == ZIP_LOCAL_FILE_HEADER.size) {
                stream.unread(ZIP_LOCAL_FILE_HEADER)
                return ZipInputStream(stream)
            }
        } else {
            matched = if (next == (ZIP_LOCAL_FILE_HEADER[0].toInt() and 0xff)) 1 else 0
        }
    }
}

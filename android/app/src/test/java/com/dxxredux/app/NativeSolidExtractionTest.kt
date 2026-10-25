package com.dxxredux.app

import net.sf.sevenzipjbinding.ExtractAskMode
import net.sf.sevenzipjbinding.ExtractOperationResult
import net.sf.sevenzipjbinding.IArchiveExtractCallback
import net.sf.sevenzipjbinding.IInArchive
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import java.io.IOException
import java.lang.reflect.Proxy
import java.nio.file.Files

class NativeSolidExtractionTest {
    private fun exercise(mode: String) {
        val root = Files.createTempDirectory("solid-extraction").toFile()
        val source =
            object : RarCatalogSource {
                override val itemCount = 2

                override fun path(index: Int) = "sounds/$index.wav"

                override fun isDirectory(index: Int) = false

                override fun size(index: Int) = 2L

                override fun packedSize(index: Int) = 2L
            }
        var calls = 0
        val archive =
            Proxy.newProxyInstance(
                IInArchive::class.java.classLoader,
                arrayOf(IInArchive::class.java),
            ) { _, method, args ->
                check(method.name == "extract")
                calls++
                assertArrayEquals(intArrayOf(0, 1), args[0] as IntArray)
                val callback = args[2] as IArchiveExtractCallback
                for (index in 0..1) {
                    if (mode == "missing" && index == 1) break
                    val stream = callback.getStream(index, ExtractAskMode.EXTRACT)!!
                    callback.prepareOperation(ExtractAskMode.EXTRACT)
                    val bytes =
                        when (mode) {
                            "short" -> byteArrayOf(1)
                            "long" -> byteArrayOf(1, 2, 3)
                            else -> byteArrayOf(1, 2)
                        }
                    stream.write(bytes)
                    callback.setOperationResult(
                        if (mode == "crc") ExtractOperationResult.CRCERROR else ExtractOperationResult.OK,
                    )
                }
                null
            } as IInArchive
        try {
            enumerateRarCatalog(source).use { extractSolidCatalog(archive, it, root.canonicalFile) }
            assertEquals(1, calls)
            for (index in 0..1) assertArrayEquals(byteArrayOf(1, 2), root.resolve("sounds/$index.wav").readBytes())
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun extractsAdmittedEntriesInOneNativeCall() = exercise("ok")

    @Test
    fun rejectsIncompleteOversizedCorruptAndMissingEntries() {
        for (mode in listOf("short", "long", "crc", "missing")) {
            val failure = assertThrows(Exception::class.java) { exercise(mode) }
            assertEquals(IOException::class.java, generateSequence<Throwable>(failure) { it.cause }.last().javaClass)
        }
    }
}

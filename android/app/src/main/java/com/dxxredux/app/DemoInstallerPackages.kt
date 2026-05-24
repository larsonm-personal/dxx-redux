package com.dxxredux.app

object DemoInstallerPackages {
    data class PackageInfo(
        val filename: String,
        val sha256: String,
        val label: String,
        val game: String,
        val extraction: String,
        val expectedFiles: List<String>,
    )

    val knownPackages =
        listOf(
            PackageInfo(
                filename = "desc14sw.exe",
                sha256 = "3dadb7fbc01efce2904d0908c55d9a9cf1f402e83bf771970552efaca15efcb0",
                label = "D1 Demo v1.4",
                game = "d1",
                extraction = "self-extracting ZIP with SOW archives",
                expectedFiles = listOf("descent.hog", "descent.pig"),
            ),
            PackageInfo(
                filename = "descent 1 demo 1-4.zip",
                sha256 = "64741386ad88d7a60a9529383affb4d2415e11d907ea6dbab8a8a66e1c20b745",
                label = "D1 Demo v1.4",
                game = "d1",
                extraction = "ZIP with SOW archives",
                expectedFiles = listOf("descent.hog", "descent.pig"),
            ),
            PackageInfo(
                filename = "descent 1 demo mac.zip",
                sha256 = "622ab2d5328f5c5dd9f804e59b8a8769ed85fc25f92428a3cfb5ff53aed95d07",
                label = "D1 Demo (Mac)",
                game = "d1",
                extraction = "ZIP with direct game data files",
                expectedFiles = listOf("descent.hog", "descent.pig"),
            ),
            PackageInfo(
                filename = "Descent Shareware.sit",
                sha256 = "f45c338df4bc4ceda38e6541f14b8dc93b543fd07d90a2c5d5118d2001c12ad2",
                label = "D1 Demo (Mac)",
                game = "d1",
                extraction = "StuffIt archive with direct game data files",
                expectedFiles = listOf("descent.hog", "descent.pig"),
            ),
            PackageInfo(
                filename = "descent 2 demo 1-0.zip",
                sha256 = "a7c31eae6dfd22e1f6a4c0b9fb2dfb2e25197831bc43c3e9d65734c7fa446c4d",
                label = "D2 Demo v1.0",
                game = "d2",
                extraction = "ZIP with SOW archives",
                expectedFiles = listOf("d2demo.hog", "d2demo.ham", "d2demo.pig", "d2demo.dem"),
            ),
            PackageInfo(
                filename = "d2demo10.zip",
                sha256 = "f8d005670fe5cd17e07ca9bf4022f1045aed436639c37f1e83dd647e14fcec1f",
                label = "D2 Demo v1.0",
                game = "d2",
                extraction = "ZIP with SOW archives",
                expectedFiles = listOf("d2demo.hog", "d2demo.ham", "d2demo.pig", "d2demo.dem"),
            ),
            PackageInfo(
                filename = "Descent II Preview.sit",
                sha256 = "4b5b7739b9da59472bcdca92f23957f90247bedd84ef8bded57d37d5d229f6d6",
                label = "D2 Demo (Mac)",
                game = "d2",
                extraction = "StuffIt archive with nested STi installer",
                expectedFiles = listOf("d2demo.hog", "d2demo.ham", "d2demo.pig", "descent2.s11", "exit.ham"),
            ),
        )

    fun matchByName(filename: String): PackageInfo? =
        knownPackages.firstOrNull { it.filename.equals(filename, ignoreCase = true) }

    fun matchBySha256(sha256: String): PackageInfo? =
        knownPackages.firstOrNull { it.sha256.equals(sha256, ignoreCase = true) }

    fun isKnownArchiveName(filename: String): Boolean = matchByName(filename) != null
}

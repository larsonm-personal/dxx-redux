package com.dxxredux.app

/**
 * Hard-coded SHA-256 → version-name table for identifying known game asset versions.
 *
 * Organized by distribution package (e.g. "D2 v1.2 PC GOG", "D2 Demo v1.04a").
 * Filenames are lowercase. Each filename maps to a list of (sha256, versionName) pairs
 * because the same logical file can exist in multiple retail/patch/demo versions.
 *
 * To add new entries: compute `sha256sum <file>` and add to the appropriate package section.
 */
object KnownVersions {

    data class VersionEntry(val sha256: String, val versionName: String)

    /** filename (lowercase) → list of known versions */
    private val table: Map<String, List<VersionEntry>> = buildTable()

    private fun buildTable(): Map<String, MutableList<VersionEntry>> {
        val t = mutableMapOf<String, MutableList<VersionEntry>>()
        fun add(file: String, sha256: String, version: String) {
            t.getOrPut(file) { mutableListOf() }.add(VersionEntry(sha256, version))
        }

        // ── D2 v1.2 PC (GOG) ──────────────────────────────────────────
        add("descent2.hog", "f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703", "D2 v1.2 PC (GOG)")
        add("descent2.ham", "5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d", "D2 v1.2 PC (GOG)")
        add("groupa.pig",   "facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b", "D2 v1.2 PC (GOG)")
        add("descent2.s22", "4f10632dd4efcbffe532c35b6763edd22817135442bbcc4171381706f3893728", "D2 v1.2 PC (GOG)")
        add("descent2.s11", "d444c6f93476f8941936164d2981387a26b0a25e3f9d5e930ef96bfbb86c1e68", "D2 v1.2 PC (GOG)")
        add("alien1.pig",   "811fc58caa3e2a72cdfa07d7530b2bb0ca71836a6a2d8a3cb401e4284949c233", "D2 v1.2 PC (GOG)")
        add("alien2.pig",   "75ef8fa0cba03410c61ad1b58f57dcb1481f1f302985828aab0af90639926055", "D2 v1.2 PC (GOG)")
        add("fire.pig",     "26a5a5f4e91456abf31f79578d0922e7bc3348b6aa92489a84033de83f358156", "D2 v1.2 PC (GOG)")
        add("ice.pig",      "ae6152ef69502b00e51a98d8f04b21f2855a332cd2988ecceb3b909a49fa26a1", "D2 v1.2 PC (GOG)")
        add("water.pig",    "de88ead87dcb32f16936b3e2a08b81a2248440f29e6f8be0c4c3a5f9fe4b63c1", "D2 v1.2 PC (GOG)")
        add("intro-h.mvl",  "b40a60bccbb4e2eea5dad222f85fd63abd29f36a48b5bd60174e10820c174b84", "D2 v1.2 PC (GOG)")
        add("other-h.mvl",  "e00a016f6064cbb96b791597d9bd1fe70b3cf2e573778cc3136e3814634fddd0", "D2 v1.2 PC (GOG)")
        add("robots-h.mvl", "f491f078308a310b53bb46477b916f9de4cce9358a77b733e31b1bce86135b0a", "D2 v1.2 PC (GOG)")
        add("robots-l.mvl", "601c0374f3f2c86c77621315bdd371c0c86abf956b5e50bf6673747df71990f1", "D2 v1.2 PC (GOG)")

        // ── D2 GOG CD audio ────────────────────────────────────────
        add("descent_ii.gog",  "a09a7bab7cad3c1e08169e269c6c8f1ed63d9891dc9687924b9ee70de496c87f", "D2 GOG CD image")
        add("descent_ii.inst", "839d528316009f104cf864cf09d8954943d206ee3b527266da4a7661da190dcb", "D2 GOG cue sheet")

        // ── D2 Vertigo Series ──────────────────────────────────────
        // descent2.hog and descent2.ham are identical to D2 v1.2 PC (GOG), already listed above
        add("d2x-h.mvl",  "2b5c1f1d3c20cebbbe3d109c0ab3f3bc916c70d5b45ec4292c1d5bda54deaa42", "D2 Vertigo Series")
        add("d2x-l.mvl",  "8701b8878b7d72a642199c52d73dfffda095f81e2efdaab0baa98403cabe583e", "D2 Vertigo Series")
        add("hoard.ham",  "dcdc777daaa02a56cbba885c18d242414525e900644f7b9bca990ce0f2b734ae", "D2 Vertigo Series")
        add("d2x.hog",    "97a4433833ec1f2fbdd2becc144a1e9726ca78b1210a71a49ddbec6182665fe6", "D2 Vertigo Series")
        add("d2x.mn2",    "42d1c04c96f4dad7e8d03a6fdf342ac9ccd22672c0db5886f965638bd325764d", "D2 Vertigo Series")
        add("panic.hog",  "04864cbeefcf304a89dff96232e768400b9426bad7dc0e08e7bdb971780a3cee", "D2 Vertigo Series")
        add("panic.mn2",  "059c02d1632c3b706b464b1a14e5a2d7b7711e18c1be579e884158703a25ab88", "D2 Vertigo Series")

        // ── D2 Demo 1.0 (PC) ──────────────────────────────────────────
        add("d2demo.hog", "b6bf5514b7f2c25ff516c46e9d49eef5862b10667a95365631e7a64a10adc47e", "D2 Demo 1.0 (PC)")
        add("d2demo.pig", "368f9ea56fe8eb8b6e4636ab5eba60bfffdf692fe10100d604fedf654d7d8989", "D2 Demo 1.0 (PC)")
        add("d2demo.ham", "747ccf2494916892061e13601cd8695c35e46f2a99062fff3e3f298da94b9be6", "D2 Demo 1.0 (PC)")
        add("d2demo.dem", "8c6e2d43ba88166d17759d90e3817edd0c3ef0a33861ef35a51a8cd4db89c892", "D2 Demo 1.0 (PC)")

        // ── D2 Demo (Mac) ──────────────────────────────────────────
        // descent2.s11 is identical to D2 v1.2 PC (GOG), already listed above
        add("d2demo.hog", "e39285e4346f3066cf4ad745abcf3dc4bdf142df7c0395a42b26ae291282696b", "D2 Demo (Mac)")
        add("d2demo.pig", "88e834d13f15bfe502e32570a44302326e6486f685cb95e12b3b81d0a14b8642", "D2 Demo (Mac)")
        add("d2demo.ham", "b3d94652282859e188f9530b63d77b37289ac973bce402025d10021eaffc7a92", "D2 Demo (Mac)")
        add("exit.ham",   "c2f1fbc0e39a53d1d92336c45e59e8d79c50bb36c008a4c2bf9bf80f235226b7", "D2 Demo (Mac)")

        // ── D1 v1.4a PC (GOG) ─────────────────────────────────────────
        add("descent.hog", "83d76ff0c46bb2e7348a49bdd287ad764abeda0d851bfb16b42c1ede93b21052", "D1 v1.4a PC (GOG)")
        add("descent.pig", "093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe", "D1 v1.4a PC (GOG)")

        // ── D1 Demo 1.4 (PC) ──────────────────────────────────────────
        add("descent.hog", "26d1e31e7709dfe6dddf17ccd37f5c82e866dce49a0faf07e90ba3213b288eab", "D1 Demo 1.4 (PC)")
        add("descent.pig", "710f1c1bafc4c2fcb9623ebe701e2fff34c21b5d3d3e0fe164c1162615971a54", "D1 Demo 1.4 (PC)")

        // ── D1 Demo (Mac) ──────────────────────────────────────────
        add("descent.hog", "b70528d0c9daeb8137f05a5a699d0bf884058398a6ab4a97307807a1c0cee9be", "D1 Demo (Mac)")
        add("descent.pig", "b4608a1d0e6191ac6f07410d9714c591c77605a84bccdb882c2611bd885a2905", "D1 Demo (Mac)")

        return t
    }

    /** All known distribution package names. */
    val KNOWN_PACKAGES = listOf(
        "D2 v1.2 PC (GOG)",
        "D2 Vertigo Series",
        "D2 Demo 1.0 (PC)",
        "D2 Demo (Mac)",
        "D1 v1.4a PC (GOG)",
        "D1 Demo 1.4 (PC)",
        "D1 Demo (Mac)",
    )

    /**
     * Look up a version name for a file by its SHA-256 hash.
     * Returns null if the hash is not in the known table.
     */
    fun lookup(filename: String, sha256: String): String? {
        val entries = table[filename.lowercase()] ?: return null
        return entries.firstOrNull { it.sha256 == sha256.lowercase() }?.versionName
    }

    /**
     * Given a set of filename → sha256 pairs, identify the distribution package.
     * Returns the most common version name among matched files, or null if none match.
     */
    fun identifyPackage(files: Map<String, String>): String? {
        val votes = mutableMapOf<String, Int>()
        for ((filename, sha256) in files) {
            val version = lookup(filename, sha256)
            if (version != null) {
                votes[version] = (votes[version] ?: 0) + 1
            }
        }
        return votes.maxByOrNull { it.value }?.key
    }

    /**
     * Return short display hash (last 8 hex chars) for an unknown version.
     */
    fun shortHash(sha256: String): String = sha256.takeLast(8)
}

package com.dxxredux.app

/**
 * Hard-coded SHA-256 → version-name table for identifying known game asset versions.
 *
 * Organized by distribution package (e.g. "D2 v1.2 GOG", "D2 Demo v1.04a").
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

        // ── D2 v1.2 (GOG) ──────────────────────────────────────────
        add("descent2.hog", "f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703", "D2 v1.2 (GOG)")
        add("descent2.ham", "5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d", "D2 v1.2 (GOG)")
        add("groupa.pig",   "facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b", "D2 v1.2 (GOG)")
        add("descent2.s22", "4f10632dd4efcbffe532c35b6763edd22817135442bbcc4171381706f3893728", "D2 v1.2 (GOG)")
        add("descent2.s11", "d444c6f93476f8941936164d2981387a26b0a25e3f9d5e930ef96bfbb86c1e68", "D2 v1.2 (GOG)")
        add("alien1.pig",   "811fc58caa3e2a72cdfa07d7530b2bb0ca71836a6a2d8a3cb401e4284949c233", "D2 v1.2 (GOG)")
        add("alien2.pig",   "75ef8fa0cba03410c61ad1b58f57dcb1481f1f302985828aab0af90639926055", "D2 v1.2 (GOG)")
        add("fire.pig",     "26a5a5f4e91456abf31f79578d0922e7bc3348b6aa92489a84033de83f358156", "D2 v1.2 (GOG)")
        add("ice.pig",      "ae6152ef69502b00e51a98d8f04b21f2855a332cd2988ecceb3b909a49fa26a1", "D2 v1.2 (GOG)")
        add("water.pig",    "de88ead87dcb32f16936b3e2a08b81a2248440f29e6f8be0c4c3a5f9fe4b63c1", "D2 v1.2 (GOG)")
        add("intro-h.mvl",  "b40a60bccbb4e2eea5dad222f85fd63abd29f36a48b5bd60174e10820c174b84", "D2 v1.2 (GOG)")
        add("other-h.mvl",  "e00a016f6064cbb96b791597d9bd1fe70b3cf2e573778cc3136e3814634fddd0", "D2 v1.2 (GOG)")
        add("robots-h.mvl", "f491f078308a310b53bb46477b916f9de4cce9358a77b733e31b1bce86135b0a", "D2 v1.2 (GOG)")
        add("robots-l.mvl", "601c0374f3f2c86c77621315bdd371c0c86abf956b5e50bf6673747df71990f1", "D2 v1.2 (GOG)")

        // ── D2 GOG CD audio ────────────────────────────────────────
        add("descent_ii.gog",  "a09a7bab7cad3c1e08169e269c6c8f1ed63d9891dc9687924b9ee70de496c87f", "D2 GOG CD image")
        add("descent_ii.inst", "839d528316009f104cf864cf09d8954943d206ee3b527266da4a7661da190dcb", "D2 GOG cue sheet")

        // ── D2 Demo v1.04a ─────────────────────────────────────────
        // Placeholder hashes — fill in when demo files are available
        // add("d2demo.hog", "<sha256>", "D2 Demo v1.04a")
        // add("d2demo.ham", "<sha256>", "D2 Demo v1.04a")
        // add("d2demo.pig", "<sha256>", "D2 Demo v1.04a")

        // ── D1 v1.5 (GOG) ─────────────────────────────────────────
        add("descent.hog", "83d76ff0c46bb2e7348a49bdd287ad764abeda0d851bfb16b42c1ede93b21052", "D1 v1.5 (GOG)")
        add("descent.pig", "093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe", "D1 v1.5 (GOG)")

        return t
    }

    /** All known distribution package names. */
    val KNOWN_PACKAGES = listOf(
        "D2 v1.2 (GOG)",
        "D2 Demo v1.04a",
        "D1 v1.5 (GOG)",
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

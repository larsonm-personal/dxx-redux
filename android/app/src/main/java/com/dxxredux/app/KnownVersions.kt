package com.dxxredux.app

/**
 * Hard-coded SHA-256 → version-name table for identifying known game asset versions.
 *
 * Filenames are lowercase. Each filename maps to a list of (sha256, versionName) pairs
 * because the same logical file can exist in multiple retail/patch/demo versions.
 *
 * To add new entries: compute `sha256sum <file>` and add to the appropriate list.
 */
object KnownVersions {

    data class VersionEntry(val sha256: String, val versionName: String)

    /** filename (lowercase) → list of known versions */
    private val table: Map<String, List<VersionEntry>> = mapOf(
        // ── Descent 2 required ──────────────────────────────────────
        "descent2.hog" to listOf(
            VersionEntry("f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703", "D2 v1.2 (GOG)"),
        ),
        "descent2.ham" to listOf(
            VersionEntry("5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d", "D2 v1.2 (GOG)"),
        ),
        "groupa.pig" to listOf(
            VersionEntry("facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b", "D2 v1.2 (GOG)"),
        ),
        "descent2.s22" to listOf(
            VersionEntry("4f10632dd4efcbffe532c35b6763edd22817135442bbcc4171381706f3893728", "D2 v1.2 (GOG)"),
        ),
        "descent2.s11" to listOf(
            VersionEntry("d444c6f93476f8941936164d2981387a26b0a25e3f9d5e930ef96bfbb86c1e68", "D2 v1.2 (GOG)"),
        ),
        "alien1.pig" to listOf(
            VersionEntry("811fc58caa3e2a72cdfa07d7530b2bb0ca71836a6a2d8a3cb401e4284949c233", "D2 v1.2 (GOG)"),
        ),
        "alien2.pig" to listOf(
            VersionEntry("75ef8fa0cba03410c61ad1b58f57dcb1481f1f302985828aab0af90639926055", "D2 v1.2 (GOG)"),
        ),
        "fire.pig" to listOf(
            VersionEntry("26a5a5f4e91456abf31f79578d0922e7bc3348b6aa92489a84033de83f358156", "D2 v1.2 (GOG)"),
        ),
        "ice.pig" to listOf(
            VersionEntry("ae6152ef69502b00e51a98d8f04b21f2855a332cd2988ecceb3b909a49fa26a1", "D2 v1.2 (GOG)"),
        ),
        "water.pig" to listOf(
            VersionEntry("de88ead87dcb32f16936b3e2a08b81a2248440f29e6f8be0c4c3a5f9fe4b63c1", "D2 v1.2 (GOG)"),
        ),

        // ── Descent 2 optional ──────────────────────────────────────
        "intro-h.mvl" to listOf(
            VersionEntry("b40a60bccbb4e2eea5dad222f85fd63abd29f36a48b5bd60174e10820c174b84", "D2 v1.2 (GOG)"),
        ),
        "other-h.mvl" to listOf(
            VersionEntry("e00a016f6064cbb96b791597d9bd1fe70b3cf2e573778cc3136e3814634fddd0", "D2 v1.2 (GOG)"),
        ),
        "robots-h.mvl" to listOf(
            VersionEntry("f491f078308a310b53bb46477b916f9de4cce9358a77b733e31b1bce86135b0a", "D2 v1.2 (GOG)"),
        ),
        "robots-l.mvl" to listOf(
            VersionEntry("601c0374f3f2c86c77621315bdd371c0c86abf956b5e50bf6673747df71990f1", "D2 v1.2 (GOG)"),
        ),
        "descent_ii.gog" to listOf(
            VersionEntry("a09a7bab7cad3c1e08169e269c6c8f1ed63d9891dc9687924b9ee70de496c87f", "D2 GOG CD image"),
        ),
        "descent_ii.inst" to listOf(
            VersionEntry("839d528316009f104cf864cf09d8954943d206ee3b527266da4a7661da190dcb", "D2 GOG cue sheet"),
        ),

        // ── Descent 1 ──────────────────────────────────────────────
        "descent.hog" to listOf(
            VersionEntry("83d76ff0c46bb2e7348a49bdd287ad764abeda0d851bfb16b42c1ede93b21052", "D1 v1.5 (GOG)"),
        ),
        "descent.pig" to listOf(
            VersionEntry("093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe", "D1 v1.5 (GOG)"),
        ),
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
     * Return short display hash (last 8 hex chars) for an unknown version.
     */
    fun shortHash(sha256: String): String = sha256.takeLast(8)
}

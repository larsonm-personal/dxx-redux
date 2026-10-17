# Song-list mount priority

- Reproduction: a mission HOG containing `Descent.sng` loses to a lower-priority stock HOG containing `descent.sng`; the legacy case resolver intentionally prefers exact case across the complete virtual filesystem
- Scope: resolve root song-list names by existing PhysicsFS search-path order before D1/D2 `songs_init` opens them; keep exact-case preference within one mount, D1 presentation namespaces, and `dxx-r.sng` preference
- Do not alter the general case resolver, mount ownership, path permissions, authored files, or checked-in soundtrack baselines
- Extend the existing upstream compatibility integration owner with actual `songs_init` checks for mixed-case priority, exact archive/member/bytes, repeated mission-stock-mission activation, and same-mount exact-case ties
- Qualify both native games and original private U3AAH metadata without loose aliases; keep private assets and generated metadata out of this change
- Android U3AAH save/cold resume remains a separate device verification gap until the change is available in the existing trusted asset lane; do not weaken its secret-admission policy
- Run allowed native checks locally; repository PowerShell formatting/catalog commands require the hosted lane because local PowerShell execution is unavailable for this task

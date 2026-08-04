# BR-0515 immutable FileProvider URIs

## Plan

- [x] Read repository instructions, BR-0515, and related findings
- [x] Trace FileProvider URI publication, grant lifetime, and backing-file mutation
- [x] Implement immutable granted artifacts with focused regression coverage
- [x] Run scoped code quality, focused tests, and Android build verification
- [x] Archive BR-0515 with exact validation evidence

## Result

Every FileProvider publication now stages complete bytes into a fresh UUID generation before granting its URI. Mission documents, config exports, debug logs, crash reports, file views, and input demos share the same generation store. Generations remain untouched for 24 hours; only expired generations are pruned, and a new publication is rejected when the 64 MiB per-root budget cannot be met without deleting an unexpired grant. Focused collision, repeat, containment, budget, expiry, and failure tests, existing config-export and mission ZIP tests, scoped code quality, the three-ABI debug APK build, and `git diff --check` passed.

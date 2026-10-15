# Chunk0426 Play and build identity continuation, 2026-10-14

Conditional implementation plan only. No code/test/helper edits or execution during this diagnosis tranche. Keep existing BR0546/0547/0556/0559 ownership and archivedBR0548FIXED;do not add duplicate roots or claim closure from source checks.

Preserve current repairs

- Exact GUID-named output path flows from orchestrator through builder to deployer;direct deploy refuses missing AabPath,hashes selected file and rechecks actual loaded bytes before upload
- Used version codes fail instead of promoting unverified remote bytes;track-query errors propagate instead of becoming zero. Preserve completed BR0548
- Child BuildType ValidateSet1/2/3 and missing Release/Internal signing-config guard reject before compilation;current APK signature/package/version/threeABI-both-engine checks must survive cleanup
- Shared Java initializer reads configured JDK_MAJOR and rejects absent java executable;do not restore lexicographic directory selection
- Separate Play AAB/direct-install APK IDs,explicit Gradle distribution properties,native content-stable identity header,and clean/unclean GitHub selection remain intentional

Complete existing identity and admission contracts

1. Under BR0556 derive one invocation record for revision/version/variant/distribution and artifact paths/digests/embedded identity. Generate Kotlin identity in variant-specific Gradle output rather than shared source BuildInfo/skipBuildInfo handshake;retain native commit/version marker. Wrapper reports immutable artifact identity instead of rereading mutable source timestamps
2. Under BR0546 validate full pinned JDK release/executable identity and explicit JAVA_HOME override. Current major-only resolution is a partial repair. Validate before build mutations and arrange caller preflight so invalid tools/variants do not perform avoidable authentication/query work
3. Under BR0547 give the top-level wrapper the same closed BuildType contract as its child and declare which options apply to each Action. Preserve child rejection rather than claim current invalid values still reach signed Gradle builds
4. Under BR0559 require complete signing configuration and intended certificate for direct Gradle Play Release/Internal as well as wrappers. Preserve current GitHub/wrapper missing-file guards and APK signature checks;cryptographic validity alone does not prove expected upload/distribution certificate
5. Select the exact produced AAB from a declared build output record and bind embedded package/version/variant/commit to the selected bytes. Current first-file selection in variant output and sequential shared build directories need actual concurrent/stale-output acceptance. This completes invocation identity without reopening archivedBR0548's repaired newest-deploy/reused-code promotion path
6. Make temporary query/edit ownership explicit with cleanup on read/upload/update/commit failures while preserving the original error. Require commit failures to remain failures even if an HTTP exception lacks ErrorDetails text;this is a future negative-control requirement,not a new canonical finding from this partial review
7. Return actual destination outcome,including committed draft versus completed and promotion warning. Keep checked upload/track/commit paths and never report complete publication from a child exit code alone. Source currently prints final draft status;do not say draft state is hidden everywhere

Later validation (not run now)

- Stub remote/auth boundaries;do not mutate live Play/GitHub during local negative controls. Cover all Actions/default/explicit/BuildOnly and BuildType domains,missing/unreadable/mismatching JDK/signing input before mutations
- Cover full/shallow/divergent/same-count revisions and concurrent direct/scripted builds;inspect each final artifact and require Kotlin/native/package/variant/digest agreement and unchanged source files
- Seed stale/multiple outputs,change source artifact during load,reused-code responses and failed/malformed track reads;require exact intended bytes or failure before track mutation
- Cover upload/track/commit/cleanup failures,draft fallback,promotion warning and success,retry errors with and without body text. Require truthful structured outcome and retained original failure
- Preserve deterministic unchanged metadata and artifact reports;regeneration must keep existing bytes/time when equivalent. Never remove handmade guidance or desktop/game identity contracts

GitHub release's actual caller/build/signature/identity/remote replacement paths and prior release plans remain to qualify before final0426disposition. No newroot/remediation/status/rating/runtime/inheritedsaving from this plan.

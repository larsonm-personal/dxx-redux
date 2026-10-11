# Proposed GQR-0280: captured Setup source actions, 2026-10-11

Diagnosis and implementation plan only. Proposed GQF-0294/GQR-0280 require final canonical admission with chunk0362; no product changes or runtime acceptance here.

Provisional impact rating: 44 (H/M/B/C/R = 23/0/4/10/7); proposed owner: GQR-0280. Focused Setup action correctness, exact cross-owner mechanism, moderate composed validation, zero original-file saving.

## Concrete witness

MusicInfoSection2664-2669 keys audioSrcManager/audioSources by setDir/refresh but pendingAudioSourceRemoval and busy state remain unkeyed. Confirmation2899-2906 reads the current manager and removes the remembered old source ID. If the owning set changes and its new manager loads while the confirmation remains composed, the title still describes setA's source while removal addresses setB. Equal source IDs make this a wrong-owner deletion rather than merely unavailable selection. SetupActivity3543-3547 refreshes activeSetName from persisted state and4630-4634 switches the set; AudioSourceManager253-263 resolves deletion under its own registry. Use actual recomposition/refresh routing as the oracle, not a helper-only predicate.

ContentEntryDetailsDialog2563-2580 and its parent retained contentDetailTarget have the same missing owner-set identity. Existing availableTarget may retain the old consistent native target after a switch; do not claim every switch creates mixed native inputs. The demonstrated defect is unretired old target/action state. directFile747-806 binds dataDir to passed setDir, so rebuilt targets must capture coherent entry/file/set context.

## Implementation boundary

Allowed product paths: SetupSections.kt and only a minimal private captured target/attempt helper if needed; retain existing manager transactions and native target builder semantics. Allowed tests: focused composed Setup owner/action integration and relevant existing audio/content fixture; required test-catalog wiring. Update canonical ledgers and this plan when implemented.

Capture source selection and canonical owning set/registry generation together. Confirmation must either use that exact retained owner after revalidation or retire the pending action on set/source replacement. Never pair an old displayed source ID with the newly active manager. Key content detail/result/effect state by complete owner, not entry ID alone. Retire stale target/action state and keep cancellation-owned effects.

Use one small private mutation-attempt owner for admission, captured manager/ordered IDs, progress/error and terminal busy release. Rethrow cancellation; contain ordinary mutation failure with truthful recoverable outcome; clear busy in finally only if still current. Do not publish an old manager's result into the newly active set. Preserve separate registry/byte/grant commit contracts under BR0472 and immutable source acceptance under GQR0102.

## Acceptance and simplification

Build two sets with equal source IDs but different labels/bytes/grants. Open removal inA, switch persisted/current set toB through actual refresh/recomposition and confirm: either action is retired or only explicitly capturedA is changed;B must remain byte/registry/grant identical. Repeat unequal IDs, refreshed source replacement, delayed manager load, blocked mutation, dismissal, cancellation and late completion. Fault setEnabled/reorder/remove and require truthful error, busy release/retry and no old finally clearing a newer action. Cover content details with same IDs across sets, target refresh and late discovery; title/files/native dataDir must follow one owner.

Share action attempt wiring privately without broad manager or native dedup. Remove unreachable music detailStatus declaration/read/null-dismiss block only after current/base three-occurrence census; it is baseline dead state, not introduced by RecommendedModRow removal. Preserve full robot variant/source identity and separate preview preparation paths.

When implementation is authorized, run scoped quality, relevant JVM fixtures and the focused composed Android route. No tests or product edits during diagnosis.

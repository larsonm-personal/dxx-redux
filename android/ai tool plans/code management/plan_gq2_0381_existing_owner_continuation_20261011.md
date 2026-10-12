# GQ2 chunk0381 existing-owner continuation, 2026-10-11

Diagnosis and implementation planning only. No source changes, tests, media generation, devices or fault probes were run. Existing owner statuses remain unchanged. Canonical publication and final consumer assessment are still pending.

## Capture ownership and cleanup

Continue BR-0539, BR-0543 and BR-0173 rather than creating another device/process/lifetime finding. Capture already verifies the dedicated DxxStoreAssets AVD, selects its serial, uses bounded checked ADB calls and package-scoped automation, and binds durable automation results to run IDs. Preserve these controls.

Move operation cleanup ownership to the first acquisition: audio can start before screen-record acquisition and before the caller's try/finally. Register each acquired resource immediately. On failure, stop every acquired recorder and audio session independently, preserve the primary exception, and report secondary cleanup failures. Do not let failed video stop/pull prevent audio stop or let manifest publication prevent scoped rotation restoration. Release only owned child processes and paths; do not kill unrelated applications or emulators.

Opening and boss operations need independent restoration attempts for each field they temporarily modify. The dedicated preparation operation intentionally configures its capture AVD; preserve that documented provisioning role rather than requiring arbitrary-device restoration. For boss/featured capture, restore the actual pre-operation display/config state and report failed restoration instead of silently assuming success.

Future actual-entry acceptance: substitute bounded ADB and recorder processes at the Python entry points; fail before and after audio start, video acquisition, capture, stop, pull and manifest publication. Require finite completion, primary-error preservation, all acquired resources released, unrelated-process sentinels intact, and each temporary state independently restored. Cover both games and ordinary successful capture. No malformed-media or resource-pressure probes are authorized in this tranche.

## Checked correlated output publication

Continue BR-0233. Native PCM export currently ignores RIFF/header write and close results, opens final WAV/JSON paths directly, and short-circuits the second WAV after first failure. Metadata-open failure also returns before buffer release. Preserve the locked callback detach and preallocated callback buffers; do export I/O outside the callback.

Check every header, payload, flush and close result. Stage invocation-owned outputs, validate rate/channels/frame count and paired music/mix alignment, and publish a correlated generation only after all requested members succeed. Always release buffers after stop, including invalid capture and output failures. Define reader-visible completion through a validated manifest or equivalent existing ownership boundary; per-file rename alone is not a multi-file transaction.

The summarizer similarly writes final JSON before CSV. Zero rows leave an earlier CSV untouched. Stage both outputs, serialize and validate them before replacement, and define the zero-row CSV policy explicitly so stale rows cannot appear to belong to the new summary. Preserve previous complete output on failed generation and distinguish an intentionally empty survey from missing required inputs. Keep the existing source tree/filter and duplicate mission/version counting policy explicit.

Future actual-entry acceptance: preseed complete correlated outputs and cover controlled output-open, short-write, header, flush/close, replacement and interruption failures. Require nonzero failure, prior-generation preservation, no partial completion claim, owned residue cleanup and exact JSON/CSV or WAV/metadata correspondence. Exercise zero-row and normal summary input through the real CLI using isolated tiny inputs. Fault execution remains future implementation acceptance.

## Corpus, stripping and dependency provisioning

BR-0607 owns clean-checkout, integrity-checked regression-demo acquisition and required/optional accounting. Include each retained store recipe input with game, role, size, SHA-256 and source generation. Do not substitute historical local replay claims for a provisioned corpus or change intentionally supplementary mismatch reporting into exact-replay failure policy. The primary replay path retains its exact result requirements.

BR-0585 owns transactional stripping. Parse and validate every selected input before committing changes, retain unchanged-byte behavior and ShouldProcess, and publish each replacement through checked owned staging. Distinguish candidate counts under WhatIf from committed changes. Reconcile the missing historical planned fixture with a maintained actual-entry test rather than treating the old plan as execution evidence.

GQF-0297/GQR-0283 already owns boss dependency provisioning. Apply the existing small shared basic/video environment profiles and exact pins before Python entry. Cover fresh basic environment plus ComposeOnly/ReuseCapture, complete environment and installer failure; preserve old outputs when setup fails. Do not create a generic dependency framework or a duplicate finding.

## Preserve media and engine contracts

Keep graphics capability validation and real safety-overlay confirmation. Both music backends tap after output volume scaling and before effects mixing; SDL delivers aligned stem/mix buffers under the mixer lock. Preserve those controls and the dedicated AVD guard.

Boss composition checks archived input hashes and exact audio section sample counts. Its PCM-correlation check after AAC re-encoding and combined video's identical decoded-audio check after stream copy serve distinct contracts. Do not merge them into an apparently simpler weaker oracle. Finite/nonempty correlation acceptance remains an unadmitted question; no new failure reproduction is claimed.

Headless and JNI use the same native flyout collector, including explicit level_not_loaded records. Retain engine ownership of MVE, END/TXB, route traversal, asset selection and timing. The summarizer audits the published projection; it should not duplicate engine format knowledge. Structured-input type assumptions and sorting after reported invalid durations remain acceptance questions pending schema-owner reconciliation, not a newly admitted finding.

## Simplification and inherited boundary

Retain all four assigned helpers: media planning/audio/provenance, combined video/motion validation, demo diagnostic-state stripping, and corpus timing survey are distinct live roles. Shared dependency setup and small scoped ownership changes can simplify the existing remediations; broad wrapper or validation frameworks are not justified by this review.

All four assigned paths are absent from the 1996-original tree. No inherited saving is proposed, and no justified assigned authored deletion was found. Preserve desktop parity and both games; no broad D1/D2 deduplication is proposed.

## Evidence and remaining gates

Producer completion checkpoint fe118c110be88b382140a7f5833e034c1876ee702d60423d0a745b61adce2337; independent static audit PASS85 e0af034442114fa92fc61e377c95d2518693f25171c4b0f9f32c509ec7a23743. Six fresh contexts650 lines complete the exact qualified 1040-line capture and 439-line boss producer bindings and reconcile the admitted dependency plan. Earlier audio/metadata PASS68 and capture PASS67 remain qualified static evidence, not runtime acceptance.

Remaining: finalize schema/corpus notes and full named-consumer checkpoint, reconcile maximum existing-owner rating, then publish canonical report/snapshot/dispositions and independently audit publication before marking 0381 DONE. Broader campaign coverage and implementation remain separate.

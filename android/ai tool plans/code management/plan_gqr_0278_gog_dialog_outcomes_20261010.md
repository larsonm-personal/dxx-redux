# GQR-0278: truthful GOG dialog outcomes and attached action focus

Diagnosis and implementation plan only. No product/test edit or runtime execution in this tranche.

## Confirmed problem and ownership

GQF-0292, P2/high, priority44 (23/0/4/10/7). SetupDialogs.kt currently writes native/count-validation failure (-1) to extractedCount at736, clears processing at772, displays No files extracted, and hides primary/secondary extraction actions requiring extractedCount==0 at873/926. The effect at401-413 nevertheless selects importFocus whenever fileList remains nonempty. That requester has no attached primary button after failure. The error text also requires extractedCount==0. The user must close/reopen instead of getting a truthful retryable failure.

This is the dialog's inconsistent terminal state/action policy, separate from FIXED BR0020 native partial-success handling, FIXED BR0511 filename collisions, BR0473 delayed focus retries, GQR0040 source generation and GQR0277 ordered automation acknowledgement. Do not reopen those fixed roots or substitute a command-only test for the actual dialog.

The inspected cached Compose UI1.12.1 FocusRequester class logs and returns false for an empty requester-node list; it does not throw on that path. The existing generated dependency model names this same version, but no fresh dependency resolution or app execution ran. Do not claim a reproduced crash. Preserve this evidence limit if dependencies change.

## Minimal implementation

- [ ] Replace the overloaded count-as-state decisions with one private terminal outcome for this dialog: ready, working, succeeded(count), failed(reason), cancelled. Keep displayed file counts as counts. Preserve native negative/cancelled meanings; do not coerce errors into successful empty imports.
- [ ] Derive primary/secondary visibility, enabled state, status/error message and focus target from the same outcome. Failure/cancellation exposes an explicit retry or close action; successful Done remains available only after publication. Request focus only on the action actually composed for that state, after attachment, and do not override intervening user focus.
- [ ] Keep the existing local extractInstaller helper, synchronous processing admission and finally reset. Retain combined game-plus-audio and checkbox-specific actions, original native parsing, exact count checking, staging, content publication, descriptor use and audio registration.
- [ ] Integrate with BR0456 attempt-owned lifetime and BR0472 captured-target publication without building a generic importer framework. Preserve the native failure reason where available; use a specific extraction-failed fallback when only a negative result exists.

## Acceptance

- [ ] Exercise the actual composed GogImportDialog with the real action handler and a controlled native/import boundary returning failure, count mismatch, cancellation, zero and success. Advance the frame clock after each terminal result. A failed attempt must visibly report failure, keep a reachable retry/close action, and never request an absent target or call onImported.
- [ ] Retry from the same dialog after a negative result and then succeed; verify one admitted extraction per activation, correct combined/checkbox audio choice, matching target set and exactly one success callback. Include keyboard/controller/touch transitions and dismissal while the actual worker is still retiring.
- [ ] Retain the existing native/JVM publication and collision checks, plus D1 and D2 GOG import/launch/audio integrations. Those automation-command tests supplement the dialog test; they do not replace it.
- [ ] Run scoped quality and appropriate Android builds/integration only in the later implementation phase. Register any new top-level runner and timeout through maintained suite ownership.

No inherited D1/D2 changes or 1996-line savings are proposed.

## Related ISO outcome projection, GQ2-CHUNK-0360

The same terminal-outcome policy also applies to IsoImportDialog, with a different visible consequence. extractIsoDiscContent returns a negative primary result without publication; the UI at1923-1945 coerces it to zero and reports No supported game files found in ISO image. Genuine zero matches, extraction failure and native cancellation therefore share an empty-result message. ISO keeps its retry action attached at1867 and does not reproduce the GOG absent-action focus defect. Preserve that distinction; no new root, severity or priority is allocated.

- [ ] Derive the ISO status from the actual primary and nested outcomes before deriving a nonnegative display count. Handle EXTRACT_CANCELLED separately, report negative primary extraction as failure, preserve a genuine zero-match result, and keep negative nested extraction distinct. Done/onChanged require successful publication; retry remains reachable for failure, cancellation and zero matches.
- [ ] Keep the current ISO staging/publish/finally helper and native format reader. Coordinate one shared primary/nested attempt through existing GQR0048, actual worker retirement through BR0456/0021, and captured destination through BR0472. Give ISO synchronous admission and a finally reset without introducing a generic importer framework.
- [ ] Exercise the actual IsoImportDialog action with negative primary, EXTRACT_CANCELLED, zero matches, nested failure and successful publication. Advance frames and retry successfully from the same dialog; require truthful distinct messages, an attached reachable action, no false onChanged/onImported, preserved prior content and no stale progress after dismissal. Existing setup-command extraction coverage supplements this dialog fixture.

GQF0292/GQR0278 remains P2/high priority44 because the original GOG retry-loss witness determines its rating. The ISO projection broadens the same outcome policy; it does not prove a reproduced crash or an executed cancellation result. Diagnosis and plans only.

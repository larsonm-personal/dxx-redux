# Archive stored-date fixture continuation, 2026-10-09

## Ownership and retained contract

Diagnosis and future acceptance only. GQ2-CHUNK-0062 covers complete53-line ArchiveEntryDatesTest; existing0033 owns helper/CLI/host producer diagnosis. Continue TODO GQR0212/GQF0227 admission and existing GQR0102/0099 source-generation/qualified-member policy; no duplicate finding or new status. Retain shared Kotlin stored-date helpers and native provenance interpretation. Do not infer vintage from extracted filesystem dates or move mission-format interpretation into Kotlin.

The existing test writes one ZIP member with local calendar date and one7z member at a UTC day boundary, comparing catalog modified values to explicit1998-06-26 and shared helper. Preserve that smoke baseline. It does not call Android archive facade, persistent CLI, PowerShell projection/staging or native provenance worker; equal helper results do not establish full production-path parity.

## Future ordinary acceptance

- [ ] Use scoped/restored timezone controls to establish intended ZIP local-calendar and7z UTC semantics around day boundaries; avoid changing process-global timezone across concurrent tests
- [ ] Cover missing timestamps, directories, unsupported formats and backslash paths; assert exact stored file/modified/kind shape and absence without staging-date inference
- [ ] Define and test exact duplicate, normalized-path and case/NFC collision behavior through Kotlin JSON and PowerShell projection, using the actual selected archive member and staged payload identity
- [ ] Carry one admitted source generation through extraction and date acquisition; a small controlled source replacement must reject or produce one coherent generation, with prior output preserved
- [ ] Use small injected count/name/work/retained-byte limits including skipped/undated entries; prove typed rejection before further visits/serialization and no partial provenance publication
- [ ] Exercise actual persistent CLI/host transport with bounded response and diagnostics, complete finite request deadline/cancellation and worker retirement; later native scan timeout does not cover earlier archive_dates request
- [ ] Preserve existing native mission-provenance integration's original/conflicting/invalid/declared/missing/unrelated/shadowed descriptor controls; add actual Android-facade/host selected-member parity without duplicating native date-estimate policy

No code/test changes, build/JVM/CLI/native/device execution or malformed/security/resource-pressure probes in this tranche. Source inspection and retained historical evidence do not close acceptance.

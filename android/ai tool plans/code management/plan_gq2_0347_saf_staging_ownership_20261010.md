# SAF staging acquisition ownership, 2026-10-10

Diagnosis and implementation plan only. Proposed distinct P2 resource-lifetime finding, not yet admitted to the canonical queue. Do not implement in the current diagnosis tranche

## Source evidence

SafDescriptorStager.kt56-67 creates/checks the staging directory, removes leftovers, checks available space and creates the temporary file before entering try/finally67-112. The source descriptor is already owned by this call. A directory failure, zero staging budget or temporary-file creation failure exits without deterministic source.close(). The ordinary streaming and post-open failures inside the try are already cleaned up; retain that behavior

MainActivity.openSafFile876-894 opens a provider descriptor and passes it directly to detachSeekable. Its outer catch logs and returns -1 without closing that descriptor. The production disc path is independently reachable: PreparedDiscImages.openSeekable removes its source from the retained map before calling the stager, so PreparedDiscImages.close cannot recover it after the early exception. openSeekableDiscImage similarly opens and forwards without an enclosing source cleanup

The evidence establishes a missing deterministic release, not a measured permanent leak or reproduced descriptor exhaustion. Runtime finalization is not the ownership contract. Repeated failed attempts can retain provider descriptors until later reclamation. No provider, resource-pressure or fault probe was executed

Whole114-line stager is identical at review base, frozen head and current worktree: SHA256 d7df2d5f2bf2c73a7743a85f28223496c78b60b262274a31c32c18a25d8f3e4c. It is absent from1996. This is a pre-existing branch helper defect discovered while tracing chunk0347's caller, not a newly introduced MainActivity hunk regression

## Ownership and prior work

Keep archived BR0085 FIXED: it repairs pipe/seekability/length/seek contracts and its provider success cases do not exercise staging setup failure. GQR0086/GQF0099 owns mutable seekable-source identity, GQR0170 covers native JNI acquisition/publication, BR0087 owns the native manifest I/O object, and BR0506 owns coherent launch preparation. None supplies deterministic Kotlin descriptor cleanup before stageAndOpen enters its try. GQ1-0482's statement that staging cleans its descriptors applies to the protected body and needs this early-acquisition qualification

Before canonical admission, retain this exact boundary and recheck the current root definitions and available IDs. Proposed priority44 MEDIUM (H/M/B/C/R23/0/4/10/7); no existing status/rating is changed here

## Implementation plan

- [ ] Make source ownership explicit from entry to openSeekable. Preserve successful regular/seekable pass-through ownership; the caller still owns the returned descriptor
- [ ] Enclose every staging acquisition, including directory setup, budget rejection and temporary-file creation, within the source cleanup boundary. Track temporary-file ownership separately so cleanup is valid before a file exists
- [ ] On staged success, return the independently opened read-only descriptor while closing the original source. Preserve sync, unlink-before-return, byte/free-space/zero-read bounds, original failure propagation and partial-file cleanup
- [ ] Check both disc helpers and MainActivity against that transfer contract. Avoid caller-side unconditional close that invalidates a successful pass-through descriptor, and avoid duplicate independent owners for one source
- [ ] Add focused Android ownership coverage through the actual helper and actual disc handoff, then run relevant validation in a later implementation tranche

## Future acceptance

Use owned provider/pipe descriptors and a controlled staging-directory failure (for example a regular file at the directory path), a deterministic budget refusal seam, and temporary-file creation failure. Assert the source becomes closed before the helper returns failure, no staged descriptor or partial file remains, and an immediate valid retry succeeds. Exercise a source removed from PreparedDiscImages and the direct openSeekableDiscImage path, as well as MainActivity's -1 error mapping

Keep ordinary seekable pass-through as a control: the returned original descriptor stays open and readable until its caller closes it. Keep ordinary pipe staging, exact bytes/length, sync/unlink behavior and caller-owned staged descriptor as controls. Do not rely on GC, whole-process FD counts alone, a mock that reimplements cleanup, or existing successful SAF mount assertions to prove the early-failure boundary. New tests must follow the repository catalog and timeout rules when implemented

No inherited file changes or removable1996-line savings are needed. Canonical finding/remediation admission and chunk0347 publication remain pending


## Canonical admission, 2026-10-10

Admitted as GQF-0287 OPEN/GQR-0273 TODO through chunk0347; P2/medium priority44(23/0/4/10/7). Publication audit PASS3553 SHA291d924f9d8c806d071f6adcf379b6540e549bcea88527735e0ae7c129b86ad3. Earlier proposed/unadmitted wording records the prepublication checkpoint. Implementation and acceptance boxes remain open; diagnosis only, no runtime reproduction or existing-owner status changes. Snapshot/audit bind the plan before this admission append

# Chunk0338 shared managed audio artifact ownership, 2026-10-10

Diagnosis and remediation plan only. No implementation, fixture execution, runtime reproduction or canonical finding/status/rating admission.

## Evidence and causation

AudioSourceManager frozen hunk3 adds .inst/.gog cleanup admission beneath private filesDir or the active import root. The base classifier admitted only .cue/.bin. The generic private-CUE sharing witness therefore predates this branch and cannot establish a new branch regression by itself.

The actual add_audio_source case accepts caller-selected distinct IDs and a single local BIN path, parses the CUE content regardless of its filename, and stores that local path in binContentUris. For a valid private .inst/.gog pair, two distinct accepted IDs can retain the same exact files. addSource deduplicates ID only; removeSource calls releaseSourceResources before removing/saving the row. releaseSourceResources deletes every classified file without checking retained file owners. Removing either row can delete both files still referenced by the other row. This is source-derived evidence, not an executed reproduction. Ordinary buildGogAudioSource uses a stable basename-derived ID, so repeated normal registration within one set replaces that ID; do not describe it as independently creating duplicate rows.

GogAudioSourceRegistrationTest covers filename preservation, generated known-D2 CUE and case ambiguity. The GOG removal case in AudioSourceManagerArtifactPathsTest installs one owner and requires deletion. Neither covers retained shared pairs. AudioSourceManager is per-set but artifact resolution permits private/global paths; FileSetManager tracks retained SAF URIs across sets, not canonical managed-file owners.

## Existing-root boundaries

BR-0453 FIXED confines external-file cleanup and revalidates canonical containment. Preserve that repair; a retained private file is a distinct ownership schedule from the old external custom-name deletion trigger. BR-0459 FIXED preserves shared SAF grants after durable reduced-registry publication; it does not supply retained file identities. BR-0472 OPEN owns corruption recovery, ignored deletion failures and registry-to-artifact consistency. Keep those requirements attached to BR-0472 instead of manufacturing another transaction finding. Final new-versus-existing attribution and cross-set owner census remain pending canonical publication.

## Proposed implementation boundary

Before destructive cleanup, derive canonical artifact references for every retained source in each relevant file-set registry and any actual shared helper owner. Separate referenced bytes from exclusively created staging. A source ID is not proof of exclusive file ownership. Retain any file with another owner; delete only the final exclusively owned managed artifact, rechecking containment immediately before deletion. Keep one transaction/recovery policy with BR-0472 for truthful deletion failure and reduced-registry publication; do not silently forget failed cleanup ownership.

Apply the same retained-owner decision to single removal, replacement, orphan pruning and clear-one versus global clear. Preserve intentional disposable launcher formats, current shared SAF-grant cleanup and external-media confinement. Confirm actual cross-set/global owner producers before adding any registry abstraction.

## Acceptance to implement later

- Two accepted IDs sharing a valid .inst/.gog pair; removing either retains exact bytes and a usable remaining row after reload, final-owner removal cleans exclusive staging
- Shared CUE with distinct images, shared image with distinct CUEs, canonical aliases, private and dedicated import roots, actual cross-set references and source replacement
- Clear-one preserves other owners; global clear follows its documented ownership boundary; orphan pruning does not delete a referenced file
- External sentinels stay byte-identical; denied delete and failed registry publication retain accurate retry ownership under BR-0472
- Retained SAF trees remain readable under BR-0459; no compatibility readers or migrations are introduced

Chunk0338 remains TODO until consumer/minimization closure, final report, canonical publication and independent audit.


## Actual owner-consumer census addendum

Production managed-artifact helper references occur in AudioSourceManager local discovery/pruning, active-set SAF helper classification for the storage scan, and MusicPicker selected-source deletion summary. The MusicPicker confirmation directly invokes removeSource with mandatory Context; it does not compare retained file owners. FileSetManager enumerates known sets and collects per-set tracked SAF URIs, but those grant references are not canonical file retention. Advanced Settings global clear intentionally clears every file set and the root audio registry; preserve that documented global scope. Orphan pruning scans .cue/.bin helpers against the current manager only and does not itself scan .inst/.gog. These sources establish the consumer boundary; broader abstraction/action aliases and final canonical finding attribution remain to reconcile. No runtime acceptance or new finding admission.


## Final root disposition for chunk0338

EXTENDS existing BR-0472 OPEN registry-to-artifact ownership consistency. That owner already requires exact registry-to-artifact ownership and complete prior/new removal across source and file-set registries. Successful removal deleting bytes referenced by a retained row is additional evidence for that required invariant. No new GQF/GQR, duplicate transaction framework or priority change. Keep the explicit source-derived GOG witness and private-CUE prebase distinction above. Preserve BR-0453 FIXED confinement and BR-0459 FIXED retained grants; neither repair is reopened. The proposed retained canonical-file decision and full acceptance matrix belong to this existing BR-0472 remediation plan.

# Export RNG Trace From Advanced Page

## Goal

Make the Advanced Settings recorded-demo exporter expose `.rngtrace.jsonl` sidecars alongside their `.dximdemo` files.

## Planned Steps

- [x] Confirm how the Advanced page currently lists and exports recorded demos.
- [x] Extend the recorded-demo manager to track sibling RNG trace files.
- [x] Update Advanced page save/share text and actions to include the sidecar when present.
- [x] Add or extend a focused launcher unit test for paired demo and trace handling.
- [x] Run focused validation for the launcher-side change.

## Notes

- The current exporter only operates on `demo.file`, so traces are unreachable from the launcher even when they exist on disk.
- The paired file naming scheme is `<demo>.rngtrace.jsonl`.
- Validation: `:app:testDebugUnitTest --tests com.dxxredux.app.InputDemoManagerTest` passed after the manager and Advanced page changes.
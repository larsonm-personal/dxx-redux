# Outer Reaches mission name truncation investigation

- [x] Inspect the generated metadata and the mission descriptor source for `outerrch11.json`.
- [x] Trace the metadata generator/parser path that chooses `mission_name`.
- [x] Decide whether the JSON should preserve the full descriptor title and note any code change needed.
- [x] If a small safe fix is clear, implement it and run focused verification.

Result: the `.msn` contains `name = The Outer Reaches`, but the metadata request used `outerrch` as the engine load key and the native result copied that key into `mission_name`. The fix passes a separate descriptor display name and uses it in the exported result while preserving the short load key internally.

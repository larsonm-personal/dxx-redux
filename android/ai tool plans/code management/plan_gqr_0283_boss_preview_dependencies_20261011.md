# GQR-0283: provision boss preview dependencies explicitly

Diagnosis-only plan for GQ2-CHUNK-0368. Admitted GQF-0297 / GQR-0283, NEW; canonical admission audited 2026-10-11. P3/high, score36 (12/0/4/10/10). No implementation or runtime reproduction.

## Source evidence and ownership

generate-store-assets.ps1 always installs imageio-ffmpeg==0.6.0 and Pillow==11.3.0 in its private venv. NumPy==2.2.6 and opencv-python-headless==4.12.0.88 are installed only in combine-video/audio paths. generate-store-boss-preview.ps1 checks only whether the private Python executable exists, including before ComposeOnly.

store_asset_boss_video.py imports numpy before argument handling and imports store_asset_video, which imports cv2 and numpy unconditionally. A newly provisioned basic environment plus existing offline boss output can therefore fail before ComposeOnly performs its work. Existing artifacts can originate elsewhere; the documented offline composition command does not require running CombineVideo first on this machine. This import/dependency witness holds at the frozen review boundary. No venv, installation, import, media or emulator execution was performed.

Canonical definition census found no dedicated admitted boss dependency root. Existing pending store helper/document/test assignments remain future review, not defect closure. Capture's Python constructor validates DxxStoreAssets identity; do not attach the rejected wrong-AVD observation to this finding. Existing BR-0539 bounded target-selection acceptance remains separate.

## Implementation scope

- Use a small shared environment setup helper called by the two store wrappers, preserving the private venv location and exact existing dependency pins
- Keep the basic imageio-ffmpeg/Pillow profile; explicitly provision NumPy/OpenCV for boss and combine operations before Python entry
- Make ComposeOnly and ReuseCapture work from a fresh basic environment with existing inputs, without requiring unrelated capture or CombineVideo first
- Fail dependency setup before capture or output replacement, with a useful diagnostic and the existing install log
- Avoid a generic dependency framework. Lazy imports alone do not provision missing dependencies needed by the requested operation
- Preserve emulator identity checks, dedicated capture state, media composition semantics and existing artifacts

## Future acceptance

Invoke the actual PowerShell wrapper and Python entry points using a clean private environment, existing tiny offline media inputs and controlled capture substitution. Use pinned offline wheels where available; do not depend on a developer's preinstalled global NumPy/OpenCV.

Cover basic provisioning followed by boss ComposeOnly and ReuseCapture, direct boss setup, combine profile, an already complete environment, missing packages and controlled installer failure. Require import readiness before operation entry, successful offline operation where its fixture inputs permit it, no capture for ComposeOnly, preserved old output on setup failure, and unchanged basic asset behavior. Reuse the maintained store pipeline fixture where practical and register any new top-level runner.

Run scoped quality and actual-entry acceptance after implementation. No real emulator, large media generation or resource-pressure probe is required for this source diagnosis.

## Gates

- [x] Provisioning/import witness and focused remediation prepared
- [x] Canonical finding/remediation admission and final chunk publication
- [ ] Implement shared explicit dependency profiles
- [ ] Run actual-entry acceptance and scoped quality

No inherited source changes or 1996-original saving are proposed.


## Admission checkpoint, 2026-10-11

Canonical GQR-0283 remains TODO; diagnosis publication does not implement the remediation. Chunk0368 publication audit PASS3022 SHA277779920f8e5a948be53876b7568b274a5fa920c402eee9831f1d45f1054b9c. Snapshot binds the earlier proposal bytes before this admission/gate update. No runtime acceptance.

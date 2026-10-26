# Native keyboard regression recovery

Base: `cmake` at `a6036eacf4725dfaaab56630baca813f80c99dba`.
Source idea: `test-overnight-config-fuzz` at `9273bbc011f8e66f65b50f4dcf706acd6389bcfc`.

This independent slice recovers binding cancellation, old/new input behavior and restart persistence without restoring the old observation bridge. It changes no production code.

`test_keyboard_rebind` invokes the actual D1/D2 engine fixture in four separate processes: prepare/rebind, read rebound, restore, read restored. Events enter the production keyboard configuration window through `window_send_event`; production close handling publishes the binding. Native `kconfig_read_controls` must produce pitch for the assigned key, no pitch for the previous key, and clear pitch after release. Production player-file writers/readers persist and recover the full standard keyboard/joystick/mouse maps. Escape during capture must leave those maps unchanged.

Two extra fresh-process negative controls deliberately expect the opposite persisted map and require exit 1 with the exact map assertion. A crash, timeout or unrelated failure does not satisfy these controls.

The asset-free fixture supplies localized placeholder labels and canvas/font geometry; it does not draw or claim visual menu correctness. It uses native window dispatch rather than OS keyboard injection, and checks control output rather than actual ship movement. Android input, joystick/mouse rebinding, extended weapon binding edits and profile migration are outside this slice.

Validation uses Debug D1 and D2 `test_upstream_compat` builds with SDL dummy drivers. Run `ctest -R '^(test_keyboard_rebind|test_upstream_compat)$' --output-on-failure` in each build. The test owns a per-build profile directory; concurrent invocations sharing that build directory are unsupported.

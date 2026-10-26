# Mission-owned opaque wall animation

Child branch xl-opaque-wall-animation from fully qualified PR130 head cc996b8b3817b520f35763e4bf206e24724a152a. All eight parent workflows passed before implementation began. No simulation timing changes, new mission admission, ordinal directories, object/vclip animation, partial alpha, or door adapter in this slice.

## Authored witness

BoilingPoint force01#0.tga is a 128x768 strip with six opaque engine-decoded frames. Retail effect78 changes texture420 using eight frames of 6144 fixed-point time units (49152 total). redgoal#0 has six opaque frames while effect95 uses four frames of8192 (32768 total). Different source/stock counts are intentional test cases. The original control/control2 geometry each contains96 matching material sides. Subzero geometry also references these materials but its unresolved object parser makes it unsuitable for gameplay qualification. The source archive, HOG, level and image hashes remain pinned in the retained manifests. No original pixels enter the repository or public artifacts.

## Runtime adapter

Observe the authoritative phase when effect_apply_bitmap_state actually maps a wall bitmap. A later critical clip can advance after its parent has applied the previous critical frame, so reading its latest clock at draw time is incorrect. Keep a bounded renderer-only phase observation per parent effect, bound to package generation and HOG identity; verify it still matches the mapped bitmap. Preserve this observation through GPU-only resets so paused walls retain their applied phase. An explicit stop_effect call observes the parent's first frame even after reactor destruction. Restore has an inherited distinction: effect_apply_bitmap_state can map a critical frame while the parent remains stopped. Observe and freeze that actually applied phase too, rather than changing restore semantics or synthesizing a contradictory parent-first mapping. Reject mismatched or malformed observations and preserve the existing strict-negative timer boundary. Derive a bounded integer phase from frame_count and time_left; do not write Effects, Textures, WallAnims, GameTime, FrameTime or RNG. Use frame_time times the stock frame count as the actual period rather than inventing timing from rounded metadata.

Restrict substitution to bracketed wall draw context under the sealed D2 mission texture capability, selected owned archive and active package generation. Do not animate sprites, models, UI or classic missions from replacement filenames. Retain custom/POG priority. Exact frame files precede #0 fallback; malformed or unsupported authored sources must not be relabelled supported.

## Resource path

Reuse owner/case validation and direct source decode. Validate square-frame vertical strips before fitting: power-of-two square width of at least2 pixels, integral frame count, at most128 frames, bounded dimensions, compressed bytes, decoded bytes and total resident texture budget. Reject source alpha other than255 and every purple key before any resize. Fit each frame independently to prevent atlas/mipmap bleed. Keep immutable source metadata and per-frame renderer bitmaps separate from gameplay bitmaps. Decode once per retained source, never per draw.

Use the existing upload, filtering and composition paths. Exclude frame replacements from static merged-result caching unless its key includes selected source frame and generation. Reset source records and GPU ownership together on mission release, failed committed load, replacement toggle and GL recreation. Preflight rollback must retain only the genuinely current mission. Avoid texture-pool exhaustion: allocation failure must fall back without leaking partially created frame handles.

## Tests

- Synthetic strips at1/2/128 frames and reject129, non-integral heights, dimension/byte overflow, partial alpha, later-frame purple keys and non-owned/case-colliding sources.
- Original pinned import/publication/native resolution and engine-decoded frame contents for force01#0 and redgoal#0, without requiring full level admission.
- Timing at zero, exact boundaries, final tick, wrap, stopped/restarted and critical state. Compare different replacement counts over the same authoritative cycle.
- One-shot side changes, pause, native save/restore and RNG remain identical with sampling enabled/disabled and at different rendering rates.
- Production wall drawing with base/overlay and rotations, per-frame color assertions, nearest/linear/mips, no adjacent-frame bleed, exact-frame precedence and POG override.
- Classic with matching mods data remains unchanged; XL A/B/stock/A, failed load, generation change, GL reset and texture-slot reuse cannot retain stale frames.

Controlled injected renderer fixtures remain distinct from full mission-load rendering. Original-artwork GPU rendering is a separate evidence claim. Door/wclip timing needs a separate per-door-state adapter; the measured door53#0 includes partial alpha and cannot qualify the opaque-door artwork path.

The two original strips share the same six normalized frame hashes but differ in encoding: force01 is24-bit RGB,295973 bytes,SHA256 ce0ed3f3874c1e01fb27c46a2eb2cfba0034559fc3464052e40fa2fa4ac73dac; redgoal is32-bit RGBA,393234 bytes,SHA256 73f9547cfd47b59d2d5f7abea0482631b8e96e40f1069fefd9befcbc57971407. Test both channel layouts without interpreting the files as distinct visual designs. Their different underlying stock clip durations/counts remain valuable independent timing controls.

## Initial implementation evidence

The first build keeps the adapter in shared C renderer code. Source admission accepts only opaque power-of-two square frames of at least2 pixels, with1 through128 frames. It retains at most32 source records,512 bounded name resolutions,128 uploaded frames and32 MiB of conservatively accounted GPU storage. Negative lookup/decode results are retained until ordinary texture invalidation. Each frame is fitted independently to a power-of-two device limit, and custom/POG ownership wins before lookup. Multiplayer and imported-D1 paths cannot activate this D2 adapter.

The initial host phase/path contract and six selected existing contracts passed. All three original-resource native integrations passed, including native water save/restore with identical presentation phase samples and unchanged SIM/FX random counts. All18 focused JVM tests passed without skips, including exact original force01#0 and redgoal#0 import/publication/native byte resolution. Both Android engines and the extended D2 GLES fixture compile. GPU assertions, final CI and independent review are pending; none of these compilation results proves rendered animation fidelity.

The controlled renderer adds six-phase color checks through production standalone and merged wall drawing, nearest/linear/mip filters, classic/XL identity, owner A/B/stock/A, critical-effect state, native one-shot/stopped behavior, native POG ownership and source1/2/128/129-frame bounds. Its existing injected identity/reset spy still does not constitute full production mission-loading GPU evidence. Original-artwork GPU rendering remains unverified.

References: [pinned XL effect duration adjustment](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/main/effects.cpp#L28), [existing Redux effect loop](https://github.com/larsonm-personal/dxx-redux/blob/cc996b8b3817b520f35763e4bf206e24724a152a/d2/main/effects.c#L65), and the [measured original-source manifest](xl_texture_followon_evidence_20261025.json). This adapter preserves Redux's authoritative cycle rather than copying XL's mutation of animation timing tables.

## Critical update-order review correction

The review exposed an actual ordering gap in the initial renderer, not a simulation defect. The focused regression now advances a later critical effect through real do_special_effects calls with different frames and timing, then renders the bitmap phase applied before that advancement. It also renders stopped parents with distinct and shared critical bitmaps, one-shot frames before/after the native side change, and an authored synthetic exact #1 file competing with #0. These checks remain controlled GLES evidence pending execution at the corrected head; host contracts and compilation do not replace that qualification.

The stopped-plus-critical restore regression serializes native effect records into the existing rewind memory buffer and calls mod_save_prepare/mod_save_read_effects. Distinct and shared first-bitmap cases assert the inherited critical mapping, frozen replacement phase while the critical clock advances, and explicit stop returning to the parent first frame. This exercises the production effect restore reader inside the controlled renderer, not an entire mission save/load campaign. The core stop hook only observes its already-applied mapping; it does not alter any effect state, saved bytes, timers, or update order.

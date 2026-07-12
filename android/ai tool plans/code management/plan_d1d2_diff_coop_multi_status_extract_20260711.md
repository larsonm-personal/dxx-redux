# D1/D2 coop multi-status extraction plan

## Goal

Move the byte-identical cooperative kill statistics, peer-status packet, rejoin
spew cleanup, and inventory-restore packet implementation out of the inherited
`d1/main/multi.c` and `d2/main/multi.c` files into one shared source while
preserving desktop statistics behavior and Android packet bytes.

## Scope

- Add `shared/coop/coop_multi_status.c` and `coop_multi_status.h`.
- Build the shared source unconditionally in both D1 and D2 main targets.
- Replace the duplicated `multi.h` declaration blocks with one shared include.
- Remove the duplicated `multi.c` implementation blocks exactly.
- Do not change packet identifiers, lengths, field offsets, guards, callers,
  reconnect policy, or validation behavior.

## Work plan

- [x] Record the live duplicate boundaries and baseline upstream metrics.
- [x] Add the shared declaration header and exact-move implementation source.
- [x] Wire the source into both unconditional main source lists.
- [x] Replace both duplicated declaration blocks and implementation blocks.
- [x] Verify the moved implementation remains byte-equivalent and the Android
  wire layout remains 8 bytes for peer status and 78 bytes for inventory restore.
- [x] Run scoped static checks.
- [x] Run practical D1/D2 build validation after the concurrent OGL validation
  releases the shared build trees.
- [x] Record final upstream metrics, validation results, and any environment
  limitations.

## Expected payoff

- Remove 179 upstream-diff additions from each `multi.c`, including the separator
  blank before the old end-of-file block.
- Replace 30 additions in each `multi.h` with one include, saving 58 additions.
- Add one CMake source entry per game.
- Expected net reduction in inherited-file additions: 414 lines.

## Baseline

- Source block: D1 lines 6140-6317 and D2 lines 8005-8182, 178 physical
  lines and 5,692 characters each, SHA-256
  `ea68b2b6de440134fef565929e79809bdb5f528085cb166c3d2f8abce1a7e5e4`.
- Header declaration block: 30 lines per game, SHA-256
  `22b8740fa6622a0bc7c863043befb2eaba0cd77cdac740c5db061420ec0a7eb4`.
- Upstream numstat before extraction:
  - `d1/main/multi.c`: `+669/-27`
  - `d2/main/multi.c`: `+730/-30`
  - `d1/main/multi.h`: `+79/-2`
  - `d2/main/multi.h`: `+82/-3`
  - `d1/main/CMakeLists.txt`: `+161/-35`
  - `d2/main/CMakeLists.txt`: `+226/-33`

## Result

- The normalized 178-line implementation body in `coop_multi_status.c` compares
  exactly with the former D1 body from the marker through end of file.
- The normalized 29-line public declaration body in `coop_multi_status.h`
  compares exactly with the former D1 and D2 declaration body.
- Packet command definitions, lengths, call sites, guards, field offsets, send
  priorities, identity lookup, and cleanup ordering were not changed.
- Upstream numstat after extraction:
  - `d1/main/multi.c`: `+490/-27`, down 179 additions
  - `d2/main/multi.c`: `+551/-30`, down 179 additions
  - `d1/main/multi.h`: `+50/-2`, down 29 additions
  - `d2/main/multi.h`: `+53/-3`, down 29 additions
  - `d1/main/CMakeLists.txt`: `+162/-35`, up 1 addition
  - `d2/main/CMakeLists.txt`: `+227/-33`, up 1 addition
- Net inherited-file reduction: 414 additions.
- Shared implementation and header add 231 branch-only lines, for a net code
  reduction of 183 lines before counting this plan document.
- `git diff --check` passed with only the repository's existing CRLF conversion
  warnings for the inherited files.
- New files are ASCII without UTF-8 BOMs.
- Scoped code quality was attempted in check-only mode. The managed sandbox
  denied execution of `C:\local\clang-format-20\clang-format.exe`, so formatter
  validation remains outstanding.
- CMake regeneration initially attempted a network update of SDL_mixer. The
  three existing Android build trees were regenerated with
  `FETCHCONTENT_UPDATES_DISCONNECTED=ON`, using their cached dependency sources.
- D1 and D2 then compiled and linked successfully for arm64-v8a,
  armeabi-v7a, and x86_64. The new shared source produced a distinct object in
  all six game/ABI targets and emitted no new warnings.
- A Windows configure was attempted with vcpkg manifest installation disabled,
  but another uncached FetchContent dependency (`nlohmann_json`) still required
  sandboxed network access. This is an environment validation limitation, not
  a source compile failure.

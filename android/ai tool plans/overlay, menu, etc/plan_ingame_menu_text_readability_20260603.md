# In-Game Menu Text Readability Plan

Status: implementation in progress

Created: 2026-06-03

## Request

Improve Android readability for in-game menus, especially:

- F1 "KEYS" help menus while in-level
- Keyboard, joystick, mouse, and weapon control editing views
- "Find LAN/Online Games" and related netgame list/detail screens

User priorities:

- First look for an intermediate text rendering resolution fix
- Then improve text scale and wrapping/reflow
- Consider menu panels up to about 85 percent of screen width, not only 85 percent of height
- Consider taller/wider menus that slide or scroll from below the screen if needed

## Current Code Shape

- `d1/main/newmenu.c` and `d2/main/newmenu.c` already include Android menu scaling.
- `android/app/src/main/cpp/shared/android_menu_scale.c` computes a target fill of `0.85f`, with `compute_cropped()` and `compute_kconfig()`.
- `newmenu_draw()` and `listbox_draw()` render the menu into a full `SWIDTH` by `SHEIGHT` 8-bit scratch bitmap, crop it, scale it into another 8-bit bitmap, then blit/upload it.
- `d1/main/kconfig.c` and `d2/main/kconfig.c` use a similar Android scaled path around the fixed 320x200-style controls table.
- `show_help()` and `show_netgame_help()` in `d1/main/game.c` and `d2/main/game.c` use `newmenu_dotiny(... TXT_KEYS ...)`, which sets `tiny_mode` and draws body text with `GAME_FONT`.
- "Find LAN/Online Games" is also a `newmenu_dotiny("NETGAMES", ..., TabsFlag=1, ...)` path in `net_udp_list_join_game()`, with 74-character tabbed rows.
- Netgame rules/details use custom fixed 320x200 drawing in `net_udp_show_game_rules()`.
- Font selection in `gamefont_choose_game_font()` uses 320x200 or 640x480 font assets, then integer scales glyph bitmaps through `FNTScaleX/Y`.
- Default Android render resolution is intentionally below full device resolution. Introspection exposes `resolution.render_width`, `resolution.render_height`, `resolution.display_width`, and `resolution.display_height`.
- Existing automation covers basic menu scale introspection in `android/game_scripts/test_menu_scale_d2.json5`.

## Working Hypotheses

1. The biggest "intermediate low-res" issue is likely the current scaled menu path:
   - Text is rasterized into the game render canvas
   - The crop is CPU-scaled in 8-bit indexed space
   - The whole game canvas may itself be below physical display resolution
   - The phone display can be high DPI, but the text has already lost detail

2. The current `0.85f` target fill is height-limited for tall menus:
   - The help menu has many rows, so height becomes the limiting dimension
   - Width may be underused even on landscape phones
   - The result can still be physically small

3. `newmenu_dotiny()` is the wrong final presentation for help text on phones:
   - `GAME_FONT` plus dense one-line rows is not legible
   - Long rows like `Alt-Shift-F11/F12` and multiplayer commands need wrapping
   - Existing tab rendering assumes single-line key/action rows

4. Kconfig needs a different strategy than help text:
   - It is an old fixed coordinate grid with many selectable fields
   - Reflowing it is higher risk
   - A whole-panel zoom and better render source is the best first fix

5. Netgame listing needs both better source resolution and row redesign:
   - Rows are 74-character tabbed strings
   - On phone, columns are hard to parse even if scaled
   - Reflow into stacked rows would be better than trying to preserve desktop columns

## Phase 1: Measure and Confirm

- Add temporary introspection or debug fields for active menu rendering:
  - menu type: newmenu, tiny newmenu, listbox, kconfig, custom netgame rules
  - source box and destination box
  - scale factor
  - render resolution and display resolution
  - active font name or family where practical
  - whether the menu was CPU-scaled or drawn directly
- Extend existing `test_menu_scale_d2.json5` or add focused scripts:
  - open an in-level F1 "KEYS" menu
  - open "FIND LAN/ONLINE GAMES"
  - open keyboard/joystick/mouse controls
  - assert `menu_scale.active`, source/destination sizes, render/display ratio, and title/menu type
- Do not rely on screenshots as the primary test signal. Use introspection first.

## Phase 2: Fix Intermediate Render Quality First

Candidate A, preferred first experiment:

- When Android scaled menus are active, render the menu scratch bitmap at a higher virtual resolution than `SWIDTH` by `SHEIGHT`.
- Scale coordinates/fonts for the scratch render, then downsample or blit to the final destination.
- Keep the published input mapping in logical source coordinates so touch behavior remains stable.
- Cap scratch size by display size and `ogl_max_texture_size`.
- Target examples:
  - source logical box remains engine coordinates
  - scratch scale is 2x when render canvas is low relative to display
  - destination still uses 85 percent fill initially

Candidate B:

- For OGL builds, bypass the intermediate CPU `gr_bitmap_scale_to()` for menu scaling.
- Upload the cropped 8-bit bitmap once and let GL scale it with nearest or linear sampling.
- This may reduce one CPU rescale pass, but it does not solve low source resolution by itself.

Candidate C:

- Raise default Android render resolution for menus only.
- This is conceptually clean but dangerous if it requires live `gr_set_mode()` changes while in-game.
- Keep as fallback or long-term option.

Phase 2 acceptance:

- F1 help, kconfig, listbox, and netgame list all show improved glyph detail at the same physical size.
- Touch hit testing still maps correctly through `android_menu_scale_publish()`.
- Existing menu scale test still passes after updating expected fields if needed.

## Phase 3: Improve Fill Strategy

- Split fill rules by menu class:
  - generic compact newmenu: keep current cropped 85 percent behavior
  - tiny/help menus: prefer width fill up to 85 percent, allow vertical scroll
  - kconfig: allow up to 85 percent width and height, maybe retain `k_kconfig_max_scale`
  - listbox: allow more width for long rows
  - custom netgame rules: treat as phone detail panel
- Change `android_menu_scale_compute_cropped()` to support strategy flags instead of one global `k_target_fill`.
- Consider new compute functions:
  - `android_menu_scale_compute_help()`
  - `android_menu_scale_compute_table()`
  - `android_menu_scale_compute_kconfig()`
- Preserve the existing function for lower-risk menus.

## Phase 4: Help Text Reflow

- Add an Android-only helper menu path for `newmenu_dotiny()` text-only menus:
  - detect all or mostly `NM_TYPE_TEXT`
  - split key/action rows on tab
  - draw key label on first line
  - draw action text wrapped on subsequent indented lines
  - support explicit blank rows and headings
- Start with `show_help()`, `show_netgame_help()`, and `show_newdemo_help()`.
- Keep desktop unchanged.
- For multiplayer command help, wrap examples as text blocks instead of preserving one-line tabbed rows.
- Add vertical scrolling with touch drag and controller/keyboard navigation.

## Phase 5: Kconfig and Controls Editing

Lower-risk first step:

- Keep the current fixed grid
- Improve its scaled render source quality
- Increase width-based fill to use more of landscape phones
- Keep `k_kconfig_max_scale` or tune it after measuring

Possible later redesign:

- Replace fixed table on Android with a scrollable list:
  - action name
  - current binding chips/fields
  - selected row highlight
  - tap row to bind
  - launcher-managed joystick controls remain read-only
- This is larger and should wait until the quality/zoom fix lands.

## Phase 6: Netgame Menus

For "Find LAN/Online Games":

- Keep discovery logic unchanged.
- Replace the 74-character tabbed row presentation with Android-specific stacked rows:
  - game name
  - mode and player count
  - mission and level
  - status
- Preserve the backing `newmenu_item` indexes so selection and callback logic remain low risk.
- Consider fewer rows per page with larger row height.

For "Netgame Info" rules:

- Either route it through the same scalable table/panel helper
- Or add an Android-specific scrollable details panel with sections:
  - match settings
  - special flags
  - allowed objects

## Phase 7: Verification

- Extend introspection for:
  - menu scale strategy
  - high-res scratch factor
  - wrapped row count if help reflow is active
  - kconfig selected row bounds after scaling
- Add or extend automation scripts:
  - F1 keys menu in-level, D1 and D2
  - netgame help menu in multiplayer context if easy
  - kconfig keyboard controls
  - joystick/mouse controls
  - find netgames menu, with no games and with synthetic/autonet rows if available
- Run Android focused tests through `android/helpers/run_test.ps1`.
- Run host build checks after touching shared `d1/` and `d2/` C code.

## Open Design Questions

- Should the help menu preserve the old `newmenu` look, or can Android use a more readable phone-native panel while still rendered by game C?
- Is the intended cap exactly 85 percent of physical display width, or 85 percent of engine render width after aspect scaling?
- Should in-game menus slide in from below only for touch presentation, or should they simply appear centered but scrollable?
- Do we want menu text to be crisp pixel art via nearest scaling, or smoother via linear scaling when the source becomes high resolution?

## Implementation Progress

- 2026-06-03: Started Phase 2 intermediate render quality work.
- Added menu scale introspection fields for direct render state, render scale, and render bitmap size.
- Prototyped Android direct scaled drawing for `newmenu`, `listbox`, and `kconfig` in both D1 and D2 so glyphs render at the destination scale instead of being enlarged from the low-resolution source crop.
- Extended `test_menu_scale_d2.json5` expectations to require direct rendering metadata.
- Ran targeted code quality with `android/run-code-quality.ps1 -Fix -Paths ...`; all checks passed.
- Ran `./android/gradlew.bat -p android :app:assembleDebug` with JDK 21 after formatting; build passed.
- Could not run the emulator automation script in this shell because no Android device was attached and repo instructions say not to start the emulator from PowerShell.
- 2026-06-03: Added Android-only wrapping for tiny menus whose items are all `NM_TYPE_TEXT`.
- Wrapped help rows become private menu-owned text rows, with tabbed key/action rows rendered as a first line plus indented continuation lines as needed.
- Kept the original caller-owned item array available to close callbacks so existing `free_help()` ownership still works.
- Reduced Android wrapped tiny text panels to 12 visible rows so the scaler can zoom the panel more and let extra help lines scroll.
- Added `menu.android_wrapped_text` and `menu.android_original_num_items` introspection fields for screenshot-free verification.
- Ran scoped code quality and a post-format Android debug build with JDK 21; both passed.
- 2026-06-03: Added Android-specific `NETGAMES` browser formatting.
- Kept one selectable menu row per netgame so join selection indexes still map to the original game slots.
- Replaced the desktop tabbed multi-column rows with compact Android summaries and shorter control header text.
- Capped Android `NETGAMES` tiny panels at 12 visible rows so the scaler can zoom more while preserving scroll behavior.
- Ran scoped code quality and a post-format Android debug build with JDK 21; both passed.

## Recommended Next Implementation Slice

1. Add introspection fields for current menu scale source/destination/render/display ratios.
2. Prototype high-resolution scratch rendering for Android scaled `newmenu`, `listbox`, and `kconfig`.
3. Update `test_menu_scale_d2.json5` to assert the new scratch/source quality fields.
4. Only after quality improves, implement Android-only text wrapping for `newmenu_dotiny()` help menus.

# Autoselect Editor Refactor Plan

## Goal
Refactor android_autoselect.cpp (~650 lines) to reuse existing game code
from playsave.c, weapon.c, and text.h instead of duplicating file format
parsing, weapon names, and default orderings.

Follows the pattern established by android_gamepad_config.cpp which calls
plr_patch_keysettings() in playsave.c rather than reimplementing the
binary format.

## Changes

### d2/main/playsave.c
Add two #ifdef ANDROID functions:
- plr_read_weapon_order(path, primary, prim_len, secondary, sec_len)
  Opens .plr, validates header, computes ks_base offset, reads 22
  interleaved bytes (primary[i], secondary[i]) at ks_base + 8*MC + 3.
- plr_patch_weapon_order(path, primary, prim_len, secondary, sec_len)
  Same offset calc, writes interleaved bytes. Mirrors plr_patch_keysettings.

### d1/main/playsave.c
Add two #ifdef ANDROID functions:
- plx_read_weapon_order(path, primary, prim_len, secondary, sec_len)
  Opens .plx text file, parses [weapon reorder] section.
- plx_write_weapon_order(path, primary, prim_len, secondary, sec_len)
  Reads existing .plx, replaces [weapon reorder] section, writes back.

### d2/main/playsave.h, d1/main/playsave.h
Add declarations for the new functions.

### d2/main/weapon.c, d1/main/weapon.c
Remove `static` from DefaultPrimaryOrder[] and DefaultSecondaryOrder[].

### d2/main/weapon.h, d1/main/weapon.h
Add extern declarations for DefaultPrimaryOrder[] and DefaultSecondaryOrder[].

### android_autoselect.cpp
- Remove all file format parsing code (~200 lines)
- Remove duplicated default orderings (~10 lines)
- Remove hardcoded weapon name tables (~50 lines)
- Add #include "text.h" with USE_BUILTIN_ENGLISH_TEXT_STRINGS for weapon names
- Add #include "dxxerror.h" (needed by text.h for Int3 macro)
- Use DefaultPrimaryOrder/DefaultSecondaryOrder from weapon.c
- Call plr_read/patch_weapon_order (D2) or plx_read/write_weapon_order (D1)
- Keep pilot file scanning (find_first_pilot, for_each_pilot)
- Keep JNI entry points with same interface

### No changes needed
- NativeAutoselectPatcher.kt (same JNI interface)
- AutoselectEditorPage.kt (same UI)
- CMakeLists.txt (same compilation targets)
- Tests should continue to pass

## Estimated line reduction
~650 -> ~250 lines in android_autoselect.cpp
~200 lines of new code in playsave.c (where it belongs)

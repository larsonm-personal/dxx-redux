# Menu checkbox controller activate fix

Status legend: `[ ]` not started, `[~]` in progress, `[x]` done.

## Scope

- Fix Android controller `A` and TV remote center so `newmenu` checkboxes and radio items can be toggled in submenus.
- Keep normal `Enter` selection for standard menu items intact.

## Plan

- [x] Confirm the activation path: Android routes controller confirm to `KEY_ENTER`, while `newmenu_key_command()` only toggles `NM_TYPE_CHECK` / `NM_TYPE_RADIO` on `KEY_SPACEBAR`.
- [x] Update `d1/main/newmenu.c` and `d2/main/newmenu.c` so `KEY_ENTER` / `KEY_PADENTER` toggle checkboxes and radio items instead of closing the menu when one of those items is selected.
- [x] Validate with a Windows host build and an Android native build.

## Result

- `newmenu_key_command()` in both `d1` and `d2` now treats `KEY_ENTER` / `KEY_PADENTER` like a toggle when the selected item is `NM_TYPE_CHECK` or `NM_TYPE_RADIO`.
- Normal `Enter` behavior for regular menu items and input-menu activation is unchanged.
- Validation passed:
	- `run-windows-build.ps1 -Target both -Compiler vs2022-community`
	- `android\gradlew.bat :app:externalNativeBuildDebug :app:testDebugUnitTest`
	- `android\run-code-quality.ps1 -Fix`

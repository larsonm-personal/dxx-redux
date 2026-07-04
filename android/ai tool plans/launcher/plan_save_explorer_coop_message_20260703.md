# Save Explorer coop unloadable message

Goal: Improve the Save Explorer message shown for saves marked `not_loadable_from_launcher`, especially co-op saves

## Steps

- [x] Locate the Save Explorer UI and status key mapping
- [x] Trace how co-op saves are detected in launcher metadata
- [x] Update the visible message to explain that co-op saves must be loaded through multiplayer flow
- [x] Add or update focused tests where practical
- [x] Run scoped formatting or tests for touched launcher files

Completed:

- `not_loadable_from_launcher` is still kept as the native reason key
- Save Explorer now maps that key to `Co-op save: use Multiplayer > Create Game > Restore from save` for co-op saves
- The row warning and details Status field use the same formatter
- Focused JVM coverage was added in `SaveExplorerTest`

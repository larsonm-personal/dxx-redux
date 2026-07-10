# Save Explorer Live Pager Tabs

Goal: Make Save Explorer tabs and page content move together as a live horizontal pager instead of snapping selection before animating the tab row.

- [x] Confirm available Compose pager APIs and current Save Explorer state flow.
- [x] Replace separate mode state and tab centering with a shared pager-driven page index.
- [x] Keep controller left/right, tap selection, tab centering, and scroll indicators synchronized with the pager.
- [x] Run scoped formatting and focused tests/build checks.

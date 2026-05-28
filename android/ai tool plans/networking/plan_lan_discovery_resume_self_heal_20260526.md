# LAN Discovery Resume Self-Healing Plan

## Goal
Make LAN lobby scanning recover cleanly after the Android app is minimized and restored, especially when broadcast sends temporarily fail while the app is backgrounded or network power state changes.

## Steps
- [x] Trace LAN discovery broadcast failure handling and Android resume lifecycle hooks
- [x] Add a focused recovery path that refreshes sockets or diagnostics on resume without disturbing active lobbies
- [x] Add or update tests around resume recovery and permission/broadcast warning behavior
- [x] Run focused validation and code quality checks for touched Android files

## Notes
- Keep this in launcher networking code where possible
- Do not edit android/outstanding_bugs.md
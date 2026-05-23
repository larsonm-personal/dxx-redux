# Guidebot Menu and Game Menu Follow-up Plan

## Goals
- Route the GuideBot controller menu through the shared Android native menu scale helper so it fills the usual target height
- Make the native ESC/game menu cancel reliably with controller B

## Plan
1. [completed] Inspect the shared native menu scale helper and the native menu B/ESC routing path
2. [completed] Patch the GuideBot menu draw path to reuse the shared Android menu scale helper
3. [completed] Patch Android controller routing so controller B cancels the native ESC/game menu
4. [completed] Re-run focused compile and regression validation
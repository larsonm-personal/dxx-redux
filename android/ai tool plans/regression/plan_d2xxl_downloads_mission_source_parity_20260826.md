# D2XXL Downloads Mission Source Parity

## Goal

Treat `game_data/mission_files/d2xxl_downloads` as an appendage of the main
mission archive directory in random `T` samples and all equivalent regression
data workflows.

## Plan

1. [Completed] Trace `T` sampling and enumerate mission archive source
   discovery across regression, metadata, and fingerprint workflows.
2. [Completed] Identify omissions and choose the smallest shared source-directory
   definition that works in PowerShell 5.1 and 7.
3. [Completed] Update all affected workflows while preserving output locations
   and special D2XXL handling.
4. [Completed] Add contract/integration coverage for main-directory and appendage
   parity, including random sampling and missing-only modes.
5. [Completed] Run scoped quality checks and relevant PowerShell/Python tests in
   both supported PowerShell versions.

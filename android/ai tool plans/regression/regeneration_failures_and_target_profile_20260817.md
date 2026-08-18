# Regeneration failures and target-profile audit

## Goal

Diagnose and fix the fingerprint and CD regression failures from the latest run, determine whether the 45-minute `T` profile was selected, and correct targeted-profile behavior if its estimate or selection was misleading.

## Plan

- [completed] Inspect the latest summary, stage logs, timing history, category, sample seed, and selected-stage evidence
- [completed] Diagnose and fix fingerprint regeneration failures
- [completed] Diagnose and fix CD regression failures
- [completed] Correct and test 45-minute profile selection or estimation behavior as needed
- [completed] Run focused validation and scoped code quality, then record results here

## Findings

- The run summary recorded `category: Target45`, confirming that `T` was selected
- Failed short runs were accepted as timing history, so the selector incorrectly estimated that all three coarse stages fit in 45 minutes
- Targeted runs were also incorrectly allowed to write full-run timing history
- Three long OGG tracks exceeded the shared 128 MiB decoded-audio safety limit in the host fingerprint tool
- The CD tests staged roughly 700 MiB per affected disc while retaining a reproducible mission-music cache and performing duplicate audio work

## Changes

- Target selection now uses successful timings only and targeted runs no longer record full-run history
- The host fingerprint executable has a 1 GiB decoded-audio limit while Android keeps its 128 MiB safety limit
- Direct CD extraction tests clear their owned mission-music cache and omit separate CD-audio registration work

## Validation

- Both failed mission ZIP fingerprint cases passed with all three long tracks
- All three failed CD cases passed import, expected-file checks, and launch verification
- PowerShell targeted-sampling, master-regeneration, and extraction-workflow tests passed
- Host CMake build passed and all 42 CTest tests passed
- Scoped code quality passed

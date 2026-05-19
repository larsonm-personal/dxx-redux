# Plan: DXA conversion parallel speed survey

## Status: PHASE 0.5 COMPLETE, PARALLEL WORK NOT STARTED

## Guardrails
- Keep texture contents, naming fixups, strip splitting, mask generation, mip generation, and DXA entry names unchanged unless a later validation pass proves equivalence
- Do not benchmark or rebuild in this tranche because the conversion scripts are currently running
- Treat correctness as the constraint, not just wall clock time

## Current findings
- convert_all.ps1 runs the six texture conversions serially, then runs the merge pass serially per game
- convert_d2xxl_textures.ps1 spends most of its time in the per-texture conversion loop, which serially invokes ImageMagick and etc2tool and then writes into one open ZIP
- etc2tool already compresses with a fixed 16-thread encode path internally, so the hottest step is not actually single-threaded today
- etc2tool exposes a --no-mips flag, but that changes the output mip chain and is not acceptable for a correctness-preserving speed pass
- 7-Zip multithread switches are not a meaningful first target here because these scripts are extracting .7z inputs, not compressing them
- ImageMagick thread control is mainly a coordination knob to prevent oversubscription once outer parallelism exists; it is not a free speedup on its own
- There are two real observability gaps today:
  - archive extraction only gets a start banner from the script, then mostly 7-Zip summary output
  - post-extraction work can stay silent for a long time because rename, strip splitting, target setup, and the first several texture encodes do not emit regular script-owned progress lines

## Progress-reporting study
- The D1 256 extraction excerpt is consistent with the current extraction call: the script prints one line, then 7-Zip prints archive metadata and its final summary, but there is no script-side elapsed time or per-stage follow-up
- The D1 512 "stalls forever" point is most likely not still extraction. After the 7-Zip summary, the script moves into one of these silent stages:
  - name fixups with no output if the rename count is zero
  - strip splitting, which can spend noticeable time in ImageMagick identify and crop calls before the first summary line
  - the main conversion loop or merge conversion loop, which only prints every 10 completed textures today
- In the merge path, the gap is even harder to read because extraction is followed by strip splitting and per-target conversion work without a clear "now processing target X" banner until 10 textures complete
- Script-owned progress is more reliable than depending on child tool chatter. etc2tool output may vary by build or buffering behavior, and 7-Zip's default progress rendering is not durable when terminals rewrite lines

## Phase 0: Survey and constraints
- [x] Confirm the serial control flow in convert_all.ps1
- [x] Confirm the serial per-texture loop and ZIP write coupling in convert_d2xxl_textures.ps1
- [x] Confirm etc2tool thread behavior and CLI flags
- [x] Reject unsafe shortcut flags that would change output correctness

## Phase 0.5: Add console progress telemetry
- [x] Add a small stopwatch helper and emit start and finish lines with elapsed time for extraction, name fixups, strip splitting, file inventory, conversion, packaging, and merge targets
- [x] Add stage banners before rename, strip-split scan, and conversion target setup even when the resulting work count is zero
- [x] Replace the current every-10-items-only reporting with a hybrid heartbeat:
  - print the first item immediately
  - print again after a few early items
  - then print every N items or every 30 to 60 seconds, whichever comes first
- [x] Include current file name, completed count, total count, elapsed time, average seconds per item, and ETA in the durable console lines
- [x] Mirror the same heartbeat logic in Convert-AndAdd so the merge pass stops looking frozen after extraction
- [x] For 7-Zip extraction, prefer durable log output first:
  - evaluate -bb1 to print processed file names during extraction
  - optionally add -bsp1 or -bsp2 for interactive progress, but do not rely on progress-bar-only output as the primary signal
- [x] Treat Write-Progress as supplemental only. Keep Write-Host or equivalent durable lines so output remains useful in redirected logs and VS Code terminals

Implementation notes:
- Added durable extraction output in the texture, sound, and merge paths
- Added elapsed-time stage banners around extraction, fixups, strip splitting, inventory, conversion, and archive finalization
- Added per-item start lines plus periodic summary lines with ok count, error count, average time, and ETA
- Added archive update progress inside the merge DXA update path
- Extracted the shared progress and extraction helpers into game_data/mods/d2x-xl/convert_progress_helpers.ps1 and dot-sourced it from all three conversion scripts

Estimated impact:
- No meaningful speedup by itself
- High diagnostic value because it turns the current 10-plus-minute silent windows into named stages with elapsed time and ETA
- Reduces the risk of misclassifying slow ETC2 or strip-splitting work as a hard hang

## Phase 1: Refactor for safe parallelism
- [ ] Make convert_d2xxl_textures.ps1 use a unique temp directory per invocation instead of one fixed directory per game
- [ ] Split the texture pipeline into two stages:
  - stage A: produce .ktx2 and optional _mask.png files into a staging directory
  - stage B: package staged outputs into the DXA in one deterministic serial pass
- [ ] Keep output ordering deterministic so later validation is easy

Estimated speedup:
- None by itself
- Required to unlock safe parallel work without corrupting ZIP output or causing temp-dir collisions

## Phase 2: Make etc2tool thread count configurable
- [ ] Add an optional CLI flag such as --threads N to etc2tool and thread it through both scripts
- [ ] Default to the current behavior so existing one-process runs stay unchanged
- [ ] When outer parallelism is enabled, divide logical cores across workers instead of letting every worker use the full hard-coded thread budget

Estimated speedup:
- None by itself
- Prevents regressions and makes later parallel phases tunable instead of guesswork

## Phase 3: Add bounded per-file parallelism inside convert_d2xxl_textures.ps1
- [ ] Add a worker-count parameter, defaulting conservatively
- [ ] Run independent texture conversions in parallel only through the staging directory, not against a shared ZIP handle
- [ ] Set ImageMagick thread limits per worker when downscaling is active
- [ ] Set etc2tool threads per worker based on worker count and logical cores
- [ ] Start with a small default such as 2 workers until measured

Estimated speedup:
- About 1.2x to 1.8x on a typical 8 to 16 logical-core machine after thread budgeting
- Potentially near 2.0x on higher-core machines
- Could be neutral or slower if outer parallelism is added before etc2tool thread budgeting

## Phase 4: Add top-level parallelism in convert_all.ps1
- [ ] Do not launch all six texture jobs at once
- [ ] First parallelize D1 and D2 work for the same size tier after Phase 1 and Phase 2 land
- [ ] Keep merge work gated until the prerequisite DXAs for that game exist
- [ ] Consider overlapping the sound conversion only after texture tuning is measured

Estimated speedup:
- Additional 1.1x to 1.3x beyond Phase 3 on machines with spare cores
- Near zero on machines already saturated by the per-file worker pool

## Phase 5: Parallelize the merge pass conservatively
- [ ] Keep DXA updates serial per archive
- [ ] Parallelize only the per-texture conversion work in Convert-AndAdd
- [ ] Reuse the same worker and thread budgeting from Phase 3

Estimated speedup:
- About 1.1x to 1.25x on the merge phase itself
- Usually around 3 percent to 10 percent overall, depending on how much time the merge pass currently consumes

## Phase 6: Low-priority micro-optimizations
- [ ] Replace ReadAllBytes plus Write with stream copy when inserting files into ZIPs
- [ ] Reduce repeated filesystem probes where they are clearly redundant
- [ ] Revisit strip-splitting only after the ETC2 path is measured, since it is unlikely to be the top bottleneck

Estimated speedup:
- Usually below 5 percent overall

## Flag review
- 7-Zip multithreading:
  - Not a priority for these scripts because they spend archive time on extraction of .7z inputs, and the larger bottleneck is later per-texture processing
- ImageMagick thread flags:
  - Use MAGICK_THREAD_LIMIT or -limit thread only as part of a worker-budget strategy
  - Do not expect a meaningful standalone win from adding them to the current serial script
- etc2tool --no-mips:
  - Theoretical encoder-side win because it avoids compressing the mip chain
  - Rejected for this task because it changes the generated output and risks visible correctness regressions

## Combined outcome estimate
- If only simple flags are changed and the pipeline structure stays the same, expect little to no real improvement
- If Phases 1 through 4 are implemented carefully, a realistic target is about 1.3x to 1.8x faster wall clock time on common developer hardware, with higher upside on larger CPUs after tuning

## Validation plan for the later implementation tranche
- Compare entry counts and entry names for all six texture DXAs against a baseline run
- Verify README.md presence and mask entry counts
- Spot-check sample output hashes or file sizes where exact byte identity is expected
- Run the existing build and test steps only after the current conversion jobs finish
- Keep the benchmark harness separate from the functional validation so regressions are easy to localize
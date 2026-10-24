# XL mission list prefix

- Prefix display names with XL: when native descriptor/level-header inspection
  identifies XL content; retain original mission IDs, names, sorting and saves
- Classify independently of the optional runtime. Keep format logic in XL_compat
  and resolve Android package aliases without activating their resource mounts
- Check explicit XL descriptors, standard and extended HOGs, referenced loose
  levels, secret levels, unrelated archive members and malformed inputs
- Build opt-in/default-off Windows and Android; exercise the prefixed mission
  selection in the existing Android runtime test

## Result

- Detection and prefix rendering are independent of optional runtime admission
- Android aliases resolve to their owned physical descriptor without mounting
- Replaced the old list-entry/runtime struct cast with explicit field copies so
  list-only classification cannot change runtime mission metadata
- Scoped formatting/lint and whitespace checks pass
- Native classifier test covers title-only false positives, unused archive
  members, referenced secret/loose levels, case differences, truncated HOGs,
  extended HOG signatures and d2x-name descriptors
- Opt-in and default-off Windows builds pass. The initial default-off build
  exposed its C++11 default; detection now has an isolated C++17 object target
- Classifier and host lifecycle CTests pass (4.06 seconds together)
- Final Android x86_64 pilot build passes (1m11s)
- Android runtime test passes 36/36 in 17958 ms, run
  4119b7cc1da44d2d984ea1d3754c1fae. Log explicitly finds and confirms
  XL: D2-XL: Counterstrike! in the mission list before testing save/restore
  and switching to stock. Evidence: temp/xl-label-runtime.log and
  temp/xl-label-runtime-result.json
- Tested app stopped after evidence capture; changes remain uncommitted

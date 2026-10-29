# True-color POG admission boundary

The final Pumo resource audit found 26 true-color replacements in five POGs.
Their headers use BM_FLAG_TGA (128), width 1024 and height 1. The pinned XL
loader interprets those as 1024-square RGBA frames, while Redux's current
installer uses an indexed 1024-by-1 bitmap. Generic bounds validation alone
does not establish support for this format.

1. Add native valid-RGBA controls to the existing resource admission test and
   demonstrate that the old preflight accepts them incorrectly.
2. Investigate the impact of a useful preparation-time rejection before
   deciding whether it is an independent publishable correction.
3. Complete true-color ownership/application and the associated admission,
   live-output, persistence and Redux controls before claiming support.
4. Record original counts, source semantics and mission INI requirements in the
   Pumo checkpoint. Continue to budget full resource/application qualification;
   a level-only requirement inventory is incomplete.

The native regression reproduces misadmission. A trial guard also rejects all
19 original Robotech Arena replacements at startup, preventing the existing
mechanics test from running. Neptune/HXM controls and normal Android compilation
pass with the trial, but they do not resolve that failure. Preserve the local
prototype patch and publish this investigation only; do not ship a partially
validated guard or weaken the failing original requirement. Prioritize the
actual true-color runtime in the next implementation run.

This does not implement true-color POGs or qualify Pumo. Keep the earlier source,
native and Android evidence scoped. Stop this continuing run at
2026-10-29 04:54:16 UTC and report remaining CI honestly.

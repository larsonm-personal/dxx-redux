# Owned XL level messages

## Scope and source

Wall and nonplayer object action 29 now uses a separate XL message runtime.
Positive fixed-point IDs select mission-owned level MSG text. Nonnegative times
are display durations in seconds, not scheduler delays; negative times open a
modal that pauses and resumes simulation. Timed messages replace the previous
message, fade in over 250 ms, fade out over the final 500 ms and expire. Empty
durations expire immediately. Redux retains its existing HUD/message behavior.

The reader and action oracle compile unchanged bodies from XL commit
2576a98b5de310e3ac7c9777df2677d1627b8bae. Consumed CRLF files are pinned:

| File              | SHA-256                                                          |
| ----------------- | ---------------------------------------------------------------- |
| main/textdata.cpp | d6172ffd9c3e94162358228ce168dd005d03feba231f45165b97f9b4ff1c0c5c |
| main/trigger.cpp  | 74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852 |
| include/descent.h | f8c34f569c51541dc4d1e69ae3ac04972401b4c6cad99ac450bbc57a245f83e4 |

The comparison extracts the text records, FreeTextData, QSortTextData,
LoadTextData, FindTextData, ShowGameMessage, DoShowMessage and IsDelayed.
It checks exact IDs, text, line counts and lookups, plus 52 fade/expiry cases
and three negative-duration modal cases. The modal adapter checks that source
display runs inside its pause/resume pair. This is source behavior evidence,
not a screenshot comparison with the original XL renderer.

The bounded decoder preserves the source's unusual one-separator and whitespace
rules and escaped newlines. It deliberately rejects embedded NULs, overflowing
IDs, duplicate IDs, digits-only end-of-file and resources over 1 MiB. Empty and
ignored tables are safe; CR-only separators do not reproduce the source's
unchecked pointer behavior. The runtime requires positive action IDs. Negative
text IDs can be inspected but are not admitted for display.

## Ownership, rendering and persistence

Resource preparation resolves the level's MSG name through the selected mission
and checks the archive that actually supplied it. Missing, malformed, shadowed
or incomplete text rejects admission before changing live resources. Every
authored message ID must resolve. Successful preparation installs owned text;
mission changes clear it. The browser counts wall and object messages and says
that MSG resources still require load-time checks.

The normal OpenGL renderer draws the authored text with a fading dark background.
The port uses Redux fonts/menu styling; pixel-identical XL typography is outside
this increment. Modal text uses an owned string and the menu API directly,
avoiding the classic fixed-size printf message-box buffer.

Development schema 15 replaces 14. Active state stores the exact MSG-byte hash,
message ID, authored duration and relative elapsed milliseconds. Restore checks
types, widths, identity, authored ID/duration and phase before applying state.
Fourteen malformed payload variants reject without altering live state; the
subsequent valid payload recovers. A changed MSG table cannot inherit a save even
when the level and trigger bytes are unchanged.

An Android checkpoint test exposed an initial implementation error: sampling
wall time inside serialization produced two different saves of the same frame.
The runtime now samples the display clock during frame processing, as XL does,
and serializers read that owned frame state. Native coverage deliberately waits
20 ms between two same-frame writes and requires byte equality. The existing
Android detached/synchronous full-save comparison remains exact. Display time
uses a monotonic 64-bit clock, avoiding the reference's signed tick wrap; restore
rebases the saved elapsed phase instead of counting file-loading time.

## Controlled runtime evidence

The derived fixture retains unchanged Neptune geometry and owned companions,
with canonical 23/40 and 27/40 levels. It exercises actual wall dispatch, master
children, one-shot, disabled, permanent and autoplay actions, long durations,
replacement, expiry and actual removal of an object owning a message action.
Source text/action checks and runtime dispatch are separate tests.

Native controls also cover missing/duplicate/incomplete MSG tables, unsupported
sound action 30, a verified loose-file ownership shadow, foreign MSG bytes,
malformed saves, disk restoration through Redux, stock isolation and recovery.
The shadow control first asserts the resolved owner so an ineffective override
cannot grant a false pass. The complete native message integration passes in
8.03 seconds with exact elapsed-phase assertions after the clock correction.

The registered Android owner is `test_xl_level_messages.ps1`. Its 80 steps check
actual text drawing and opacity, expiry, modal pause/resume, object removal,
disk and memory restoration, Redux transitions and the 23-to-27 level change.
It preserves protected player, configuration and mission files and checks their
hashes after cleanup. All 80 steps pass in 180271 ms, run
2524a2e4b96c43f5b592a7db9696097a. The captured frame visibly contains both message
lines; introspection records full draw opacity. Protected-file hashes match.
The tested APK hash is
f764fcbab240c3ae8087d973327f791a977a9114ce28b31a1fc92acfd73d59a3.
The subsequent browser-summary update also builds on Android and passes the
native metadata contracts; its APK hash is
7cb024f3c2f92d840a5effdb048b287aca4489c7c6a12aa678ca942ffb6e25ed.

All 38 public Windows contracts pass in 19.89 seconds. The complete private
Linux owner passes 94 required CTests, sixteen original-format outcomes and
three modern source comparisons; its test/inspection phase takes 317.377
seconds on the local Windows-mounted filesystem. This includes all 39 Linux
public cases and the normal Redux loader controls. The nineteen owner tests,
both automation catalogs and scoped formatting pass. Catalog membership is
421 top-level entries and 478 support scripts. These controls do not establish
an original campaign playthrough.

| Input                | SHA-256                                                          |
| -------------------- | ---------------------------------------------------------------- |
| Original Neptune HOG | 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303 |
| Derived xmessage.hog | 54b1d6a89c3d43df371639c9c811790e10a46328e59fa4313ac24818a51c8a80 |
| Derived xmessage.mn2 | 2ee0ea78bdfc3165daacbf1986791079656e327877b8171a3b97965bc765b157 |

## Original Pumo evidence and unfinished work

The unchanged 196505200-byte Pumo HOG has SHA-256
6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043.
Its five MSG files contain 20 rows; twelve SND files contain 35 rows. All 17
tables match the independently compiled source reader. Combined with generated
cases, the oracle passes 2078 tables and 6214 exact row/lookup comparisons.
All fourteen original levels are inspected and all 21 message actions resolve
to their own level's MSG rows, on Windows and Linux. They use positive IDs and
durations of two through five seconds. The negative-duration modal checks use
the derived fixture.

The shared decoder establishes the text format of SND tables only. Sound action
30, ambient sound objects, particles, lightning and effect activation remain
blocked. Original Pumo still has independent event, environment, wall and
resource requirements; no original level gains gameplay qualification here.
Full campaign progression requires human playtesting after these dependencies
are implemented. Automated controls use direct events and controlled poses,
not autonomous navigation.

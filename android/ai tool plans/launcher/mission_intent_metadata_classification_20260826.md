# Mission Intent Metadata Classification 2026-08-26

Goal: calculate a deterministic mission-wide single-player/cooperative versus anarchy classification during metadata analysis, show the bottom-line result as a tappable launcher metadata row, explain the classification inputs and rule path in a dialog, and include the mission-wide result in checked-in regression JSON.

## Plan

- [x] Trace native metadata serialization, launcher parsing and display, host regression projection, and focused tests
- [x] Define one shared classification schema with raw aggregate inputs, result, confidence, and rule explanation
- [x] Add per-level object/start counts and mission-wide classification to D1/D2 native metadata analysis
- [x] Parse and display the classification in the launcher metadata viewer
- [x] Add a tappable summary row and details dialog showing inputs and the applied rule
- [x] Project the mission-wide classification into generated regression JSON and regenerate checked-in mission metadata
- [x] Add focused native/Kotlin/integration coverage and run scoped builds, tests, and code quality

## Results

- Shared native rules classify explicit campaign plus competitive descriptor modes as `single_player_or_coop_and_multiplayer_anarchy`, anarchy-only or competitive declarations as `multiplayer_anarchy`, levels with campaign actors as `single_player_or_coop`, and actor-free multi-start level sets as `multiplayer_anarchy`.
- The launcher shows a tappable `Intended play` row and an evidence dialog with declarations, confidence, reason, applied rule, ordinary-level categories, start ranges, and actor/object totals.
- All 367 checked-in mission entries have a mission-wide `mission_intent`; the corpus contains 310 `single_player_or_coop`, 30 `single_player_or_coop_and_multiplayer_anarchy`, and 27 `multiplayer_anarchy` results, with no missing or ambiguous values.
- Spot checks: Headband is `single_player_or_coop_and_multiplayer_anarchy`; Insanity, WaP2, and Yuhclean are `multiplayer_anarchy`.
- Verification passed: focused Kotlin tests, Android debug APK/native ABI build, D1/D2 Windows builds, full host metadata regeneration (132 files, 367 mission entries), four mission-metadata regression scripts, and scoped code quality.

## Constraints

- Do not interpret README or other free-form prose
- Treat standard `type = anarchy` as a hard machine-readable override when available
- Use mission-level aggregation so isolated training, finale, secret, or arena-like levels do not override a campaign mission
- Do not infer `single_player_or_coop_and_multiplayer_anarchy` from ordinary eight-player plus cooperative spawn layouts; require explicit machine-readable mode declarations
- Keep classification rules in native/shared code as the source of truth; Kotlin displays serialized results and explanations

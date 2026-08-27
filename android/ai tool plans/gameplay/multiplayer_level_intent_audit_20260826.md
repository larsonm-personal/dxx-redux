# Multiplayer Level Intent Audit 2026-08-26

Goal: study the four level sets in `game_data/multiplayer_levels` and determine whether each is intended for single-player/cooperative play, multiplayer anarchy, or both.

- [x] Inventory each set and identify its game and mission descriptors
- [x] Extract comparable level signals, including robots, hostages, reactors, exits, keys, player starts, cooperative starts, and competitive item placement
- [x] Classify each set with confidence and define a reusable categorization heuristic
- [x] Record conclusions and limitations

## Conclusions

- `headband.zip`: both campaign/cooperative and competitive multiplayer, high confidence. Both levels contain 26 robots, 8 competitive player starts, 3 additional cooperative starts, and a reactor-to-exit completion route. The descriptor explicitly enables normal, cooperative, anarchy, robot anarchy, capture-the-flag, and hoard modes.
- `insanity.zip`: competitive anarchy, definitive. Its readme explicitly calls the five maps 2-player anarchy/dogfighting levels. Every level has 8 player starts, no robots, no hostages, no cooperative starts, and an item-heavy layout.
- `WaP2.zip`: competitive anarchy, definitive. Its readme calls it a small 1v1 dogfight level and records 8 starts with no reactor or exit. Native analysis confirms zero robots and hostages and no completion route.
- `Yuhclean.zip`: competitive anarchy, high confidence. The undocumented level contains exactly 8 player starts and 34 powerups, with no robots, hostages, or cooperative starts.

The strongest general rule is to prefer explicit descriptor/readme declarations, then combine typed start objects with campaign content. Robots alone are only a clue because robot-anarchy maps exist. Eight normal player starts plus no robots/hostages/cooperative starts and a powerup-dominated object list is a strong competitive signature. Cooperative starts plus meaningful enemies and an achievable reactor/exit route is a strong campaign/cooperative signature. A set containing both signatures should be classified as both.

Reactor and exit presence should not be used alone because competitive D1 maps can retain default campaign scaffolding. `Total Insanity` demonstrates this: all five levels have a reactor/exit route despite the author's definitive anarchy-only description.

## Deterministic On-Device Classifier Follow-Up

- [x] Audit machine-readable mission mode declarations and loaded-level object signals already available to the analyzer
- [x] Define a deterministic classification metric that does not inspect free-form readmes
- [x] Apply the metric to all four sets and identify ambiguous cases explicitly
- [x] Record recommended native metadata fields and thresholds

The primary discriminator should be typed start objects. Count `OBJ_PLAYER` and `OBJ_GHOST` as normal/competitive starts and count `OBJ_COOP` separately as additional cooperative starts. The existing `coop_starts` field intentionally counts the first normal start plus cooperative-only starts, so it loses the competitive start count needed for classification.

Recommended per-level metadata additions:

- `player_start_count`: `OBJ_PLAYER` plus `OBJ_GHOST`
- `coop_start_count`: `OBJ_COOP` only
- `powerup_count`: `OBJ_POWERUP`
- `reactor_count`: `OBJ_CNTRLCEN`
- `exit_count`: exit triggers or exit route steps
- Optional stronger campaign evidence: boss count, required key count, guidebot count, and gated route-step count

Use these structural predicates:

- `competitive_spawns = player_start_count >= 2`
- `coop_spawns = coop_start_count >= 1`
- `campaign_payload = robots + hostages + matcens + guidebots > 0`
- A reactor/exit pair is supporting evidence only, not campaign payload, because arena levels often retain it

Classification:

- Competitive spawns, no coop spawns, and no campaign payload: `anarchy`
- Competitive spawns, coop spawns, and campaign payload: `both`
- No competitive spawn set and campaign payload or coop spawns: `single_player_or_coop`
- Competitive spawns plus campaign payload but no coop spawns: normally `anarchy`/robot-anarchy, but report low confidence or `ambiguous` when there is strong gated campaign progression
- Any remaining combination: `ambiguous`

Mission aggregation should classify every normal level first. Uniform results become the mission result; mixed campaign and anarchy levels, or any level classified `both`, produce `both`. Preserve an `ambiguous` result rather than forcing a guess.

Applied without readmes or free-form text:

| Set | Normal starts | Coop-only starts | Campaign payload | Other objects | Result |
| --- | ---: | ---: | --- | --- | --- |
| HEADBAND, each level | 8 | 3 | 26 robots | 62 or 12 powerups, reactor | both |
| Total Insanity, each level | 8 | 0 | none | 20-26 powerups, reactor | anarchy |
| Whack-a-Pyro | 8 | 0 | none | 13 powerups | anarchy |
| yuh clean | 8 | 0 | none | 34 powerups | anarchy |

The standard engine mission parser supplies one useful deterministic override: `type = anarchy` sets `anarchy_only_flag`. It ignores HEADBAND's extra per-mode keys, so those nonstandard keys should not be necessary for the structural classifier.

## Existing Mission Corpus Validation

- [x] Identify the large existing single-player/cooperative corpus and reusable host-analysis artifacts
- [x] Collect typed start, powerup, and campaign-payload counts for every available level
- [x] Apply the deterministic classifier and inspect false anarchy/both/ambiguous results
- [x] Refine the rule from corpus evidence and record coverage and limitations

The probe parsed 805 unique RDL/RL2 payloads from 88 ZIP archives in `game_data/mission_files`. Its simple filename summary recognized 245 D1 and 545 D2 names; 15 legacy HOG filename fields contained trailing control bytes after the apparent extension but their payloads still parsed. Ten archives over the focused 10 MiB probe limit and one HOG over the 32 MiB uncompressed-entry limit were reported as exclusions. The checked-in corpus is larger, so these figures validate the heuristic rather than claiming complete corpus coverage.

The start-based rule failed as an intent classifier:

- 526 of 805 levels have eight ordinary `OBJ_PLAYER`/`OBJ_GHOST` starts.
- 617 have three `OBJ_COOP` starts.
- 780 levels (96.9%) contain robots or hostages.
- 528 of those 780 campaign-payload levels have multiple ordinary player starts, including 515 with exactly eight.
- The earlier rule therefore labeled 487 known campaign levels `both`, even though the spawn layout is simply the normal campaign authoring pattern in this corpus.

Powerup density also does not rescue the rule. HEADBAND's two levels have 12 and 62 powerups, approximately the 12th and 66th percentiles of the campaign corpus. Neither is distinctive enough to infer competitive intent.

Only 25 parsed levels lacked both robots and hostages. Twelve had multiple player starts and looked arena-like; thirteen had one player start and were mostly training, noncombat, finale, or utility levels. Important examples include robot-free training stages in `trainng.zip`, a robot-free final level in `tu.zip`, and a robot-free secret level in `phenomia.zip`. Isolated level classification must therefore retain ambiguity.

Mission/archive aggregation was much stronger. Treat a level as campaign-like when it has robots/hostages or only one ordinary player start, and arena-like only when it has multiple ordinary starts and no campaign actors. All 88 parsed campaign archives aggregated to campaign: 84 contained no arena-like exception and four contained arena-like levels alongside campaign levels. The four mixed archives were `magma.zip`, `descent_maximum_fixed.zip`, `phenomia.zip`, and `Extra_Missions.zip`; some bundle multiple descriptors, reinforcing that the production classifier must aggregate per mission descriptor rather than per archive.

## Refined Recommendation

Use mission-level evidence in this order:

1. Parse explicit machine-readable descriptor declarations. `type = anarchy` is a hard anarchy-only result. If the analyzer elects to support explicit `normal = yes`, `coop = yes`, and `anarchy = yes` extension keys, these can deterministically establish `both` without readme interpretation.
2. For each normal level, set `campaign_actors` from robots, hostages, matcens, bosses, and Guide-Bots. Set `arena_like` only when there are at least two ordinary player starts and no campaign actors.
3. Aggregate within each mission descriptor. A mission whose normal levels are all arena-like is an anarchy candidate. A mission with campaign actors in any substantial portion of its ordered levels is single-player/cooperative; isolated robot-free training, finale, or secret levels do not change the mission category.
4. Use `ambiguous` for a one-level mission with one start and no campaign actors, or for a genuinely mixed mission without explicit mode declarations.

Typed start counts remain useful metadata for player capacity and diagnosing malformed coop support, but the corpus demonstrates that they do not encode competitive intent. Robots and related campaign actors are the primary structural signal. `Both` cannot be inferred reliably from level structure because normal campaign levels and deliberately dual-purpose levels share the same eight-player-plus-coop spawn pattern; it requires an explicit descriptor declaration or should remain unknown.

Probe artifacts are `temp/audit_mission_intent.ps1`, `temp/mission_intent_corpus.tsv`, and `temp/mission_intent_corpus.tsv.errors.tsv`.

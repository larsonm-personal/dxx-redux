# XL target-definition window, November 12, 2026

Started approximately 21:07 PST; bounded to 12 hours, with the final three hours
reserved for validation, CI repair and handoff. Work is sequential, without agents.
The invoked objective and the next-window plan govern this work.

## Refreshed baseline

- Clean checkout at `bf40de9bc3b85c2279fe8163c621bb6c052dec12`, matching
  `origin/cmake`. PRs 464, 465, 467 and 468 are now in the fork's trunk.
- PR 468 was merged externally at 21:10 PST. Its final published head was
  `2edc3875f534d2c2dc12247b6e9f4abf05c7d8cc`. No merge was performed here.
- New branch: `xl_gameplay_20261112_04_target_definitions`, parent `origin/cmake`.
  A merged PR cannot receive a new layer; publish a new non-draft fork PR against
  this immediate parent, then stack subsequent layers on their actual parent.
- Private controller run 43357326772 used controller
  `b3bb12cbeaa64cb17b87fbaf70cd06efaa25bcf0` and tested candidate `2edc3875`.
  The host receipt records 113 required XL tests passed, 16 format witnesses,
  three modern source levels. Its broad master coverage is explicitly incomplete;
  that does not mean all master owners ran. Public selected jobs passed.
- Sampled readiness remains failed: four loaded/ticked classic/imported cases and
  four load failures. The preserved frozen baseline is separate from regression CI
  and does not establish playability. Receipt lacks root-cause fingerprints.
- Baseline receipts: `temp/xl-g2-window-20261112/ci-baseline/`. Three existing
  stashes retained, no tracked/untracked modifications at start, 187 GiB free.

## Execution

1. Recount the immutable catalog; join evidence only with matching full identities,
   retaining unknown eligibility and all independent qualification dimensions.
2. Rebuild and reproduce `birth-atomic`; retain the failure and map every restore
   owner before editing production code.
3. Bind the existing native HAM reader to explicit destination storage. Cover all
   written fields and model ownership, versions, patch hooks and initializer state.
   Integrate a genuine independently prepared target through existing owners.
4. Publish a coherent tested partial milestone early. Full G2 stays open until
   whole-save preparation/publication, pure observation and cold continuation pass.
5. Run affected Windows/Linux Debug and runtime owners, ordinary developer-OFF
   playback, scoped formatting, then rebuild/retest any material formatting changes.
6. Track final pushed-candidate CI to terminal states. Produce the dated report,
   next-window plan and replacement kickoff prompt, commit/push them, and check CI.

## Restore ownership map before implementation

| Category                     | Existing live writes / retained candidate                                                                     | Preparation dependencies and fallible work                                                               | Publication and discard gap                                                                                                     |
| ---------------------------- | ------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| Save and mission source      | Open save; retained native descriptor and isolated archive scope exist                                        | Descriptor parse, origin checks, handles and archive identity; retain actual reads                       | `mission_commit_prepared` still calls mutating selection; busy handles forbid discard                                           |
| HAM definitions and models   | `bm_read_all_internal` writes global counts/tables, ship, reactors, gauges, cockpit, exits and model payloads | Native typed readers, version defaults, checked counts, model allocation/alignment/endian initialization | No independently decoded target; `g3_init_polygon_model` writes shared scratch; metadata/Android hooks must stay at publication |
| Bitmap/sample/material banks | `MissionDefinitions` captures live tables; resource preparation retains some HXM/POG data                     | Live mission lookup, installed sample/bitmap banks and paging, target ordinals                           | POG install can allocate; retained copied tables alone cannot establish atomic resource publication                             |
| Geometry/world/player        | `load_mission_by_name`, clock assignment, player initialization and `StartNewLevelSub` precede late checks    | Target geometry, topology, counts/signatures, allocator/free/segment lists and saved epoch               | Native object/player context decoders exist; enclosing level/world still mutates before rejection                               |
| AI and native runtime        | AI candidate is retained; runtime validator follows native publication                                        | Candidate geometry/epoch/capacities, Guide-Bot async work and caches                                     | Stage all owners, retain decoded data, then publish without another unchecked decode                                            |
| XL and SIM                   | `xl_wall_trigger_read` follows native restore; events/producers/weapons/time/birth validation is late         | Target definitions/world/signatures/epoch, source identity and pending allocations                       | Must retain all validated owners and publish clocks/RNG together; no rollback or live-table swapping                            |
| Lifecycle/Android            | Secret return and platform adaptation/metadata follow native load                                             | Existing stock/rewind/multiplayer contracts and Android state owners                                     | Account for recoverable post-read failures before whole-save acceptance                                                         |
| Observation                  | Named audio snapshot can maintain/destroy owners                                                              | Pure state read and future-behavior control required                                                     | Move maintenance to runtime boundary before accepting unchanged-state proof                                                     |

Native edits are justified only for the single existing native codec or its state
owner. XL selection, resource policy and candidate ownership belong in
`android/XL_compat`. No native parser duplication, private-global exports or hidden
implementation includes. Authentic inputs, pinned reference, frozen readiness,
required private membership and timeouts remain unchanged.

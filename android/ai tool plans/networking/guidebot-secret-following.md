# Guide-Bot following through co-op secret travel

Scope: Android D2 co-op secret entry, return, revisit and advancement after a
destroyed base, using the existing frozen travel transaction

- Capture the host's released/live or docked companion, owner and shields in
  the immutable source checkpoint and prepared campaign envelope
- Apply after destination players are placed, before arrival acknowledgement
  Replace destination companions rather than duplicate them, preserve docking,
  use the current mine's robot definition and reset mine-specific navigation
- An unreleased or destroyed source companion does not follow; discard a stale
  released destination copy so returning does not resurrect it
- On a first visit with nobody following, retain any companion authored into
  the destination mine, including custom mines with an uncaged companion
- Retain an eligible owner, otherwise choose a remaining player; observers
  cannot own the companion
- Normalize allocation and include the companion's network object identity in
  arrival agreement; disagreement or allocation failure uses normal rollback
- Use the existing rollback/full-save transaction and bump the Android D2 protocol
- Extend the two-peer LAN regression for client ownership, entry/return/revisit,
  docking, singleton count and usable navigation; run scoped formatting,
  Android and Windows builds and live tests

Additional issues found by the live regression:

- Co-op save restoration rechecked cage walls after clearing the saved release
  flag. A deployed guidebot near an intact cage became unreleased on rollback
  Preserve the saved co-op release flag instead
- Generic enemy robot collision arbitration could release a redeployed
  companion's network slot and set its hands-off timer, despite explicit
  guidebot ownership. Companion bumps/hits now leave ownership to the escort
  protocol
- World-state fixtures now count enemies separately from the travelling
  companion, whose robot/ghost type changes when docked
- The final redeployment check uses reachable unexplored segment 354 in
  Counterstrike level 8. The inventory fixture resets keys before every leg,
  and the end-of-level planner correctly reports "red key unreachable" from
  the secret return area. Released entry/return still check the default route

Validation complete:

- Android x86_64 and Windows D1/D2 builds pass, including the collision guard
  The final first-visit guard Android rebuild also passes
- Nine selected campaign, transition, world-visit, gameplay-fence and escort
  ownership CTests pass across D1 and D2
- The complete two-peer `GuidebotTravel + SecretRollback` run passes:
  failed-load rollback, released entry/return, docked revisit/return and
  redeployment with a usable exploration path. Every leg verifies one
  companion, client ownership and retained health
  Evidence: `temp/guidebot-travel-complete-live.log`
- The existing two-peer `GuidebotSpawn` control run passes on the final APK:
  an unreleased base companion stays behind, then client deployment, repeated
  deployment, docking and redeployment in the secret mine work
  Evidence: `temp/guidebot-travel-unreleased-live.log`
- Scoped formatting/lint and `git diff --check` pass
- The ABI-specific Gradle invocation writes a test-only APK at
  `android/app/build/intermediates/apk/debug/app-debug.apk`; install with `-t`
  The older APK in `outputs/apk` correctly failed the initial carryover check

Reusable regression:

```powershell
.\android\tests\test_lan.ps1 -Game d2 -InitialLevel 8 -GuidebotTravel -SecretRollback -AllowSecretWarps -NoCoopQol -SkipBuild -TimeoutSeconds 180
.\android\tests\test_lan.ps1 -Game d2 -InitialLevel 8 -GuidebotSpawn -AllowSecretWarps -NoCoopQol -SkipBuild -TimeoutSeconds 180
```

Owner departure, allocation exhaustion, destroyed-companion cleanup and
secret-to-next-normal-level advancement are handled in the shared arrival
path but have not received dedicated live scenarios in this change. Custom
mines with their own uncaged companions have only been reviewed in code
Both network peers need the updated Android D2 protocol (30075)

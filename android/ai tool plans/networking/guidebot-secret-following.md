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
- Retain an eligible owner, otherwise choose a remaining player; observers
  cannot own the companion
- Normalize allocation and include the companion's network object identity in
  arrival agreement; disagreement or allocation failure uses normal rollback
- Keep rollback/full-save restore unchanged and bump the Android D2 protocol
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

Validation in progress:

- Android x86_64 and Windows D1/D2 builds pass; final collision guard rebuild
  and complete live rerun pending
- Nine selected campaign, transition, world-visit, gameplay-fence and escort
  ownership CTests pass across D1 and D2
- The rollback fix, released entry/return and docked revisit/return passed on
  both peers in `temp/guidebot-travel-verified-live.log`; the final redeploy
  assertion exposed the collision issue above
- The ABI-specific Gradle invocation writes a test-only APK at
  `android/app/build/intermediates/apk/debug/app-debug.apk`; install with `-t`
  The older APK in `outputs/apk` correctly failed the initial carryover check

Reusable regression:

```powershell
.\android\tests\test_lan.ps1 -Game d2 -InitialLevel 8 -GuidebotTravel -SecretRollback -AllowSecretWarps -NoCoopQol -SkipBuild -TimeoutSeconds 180
```

Owner departure, allocation exhaustion, destroyed-companion cleanup and
secret-to-next-normal-level advancement are handled in the shared arrival
path but have not received dedicated live scenarios in this change

# Original Sophodo native progression

The opt-in xl_pumo_sophodo_player CTest fixture follows Iquaxel's actual earned
level6 save. Its144-action main plan earns the original robot-carried blue key,
uses the blue gates and original hub switch bank to reach gold, then gold-gated
red access. Ordinary fire/thrust kills guards151,187,185,137 and normal pickups earn
all keys. It checks16 cyclic respawn slots from original selectors33/36 and
save/load. Type28 configures respawn points, not current-player teleportation.

The route collects three original shield boosts and the original Gauss pickup,
checks owned weapon32's actual30-ammo/14-second interval, and fires one shot.
It then earns the original cloak82 and fourth shield29, uses the owned laser to
damage reactor203 normally, and follows all16 boosts/master11/countdown30 through
exit27 to level7. Destination save/load is exercised. A second native process
restores the real reactor checkpoint with exact health, energy, inventory, cloak
and complete16-slot position/orientation table, then repeats the11-action exit.

Combat qualification uses fixed local positions and explicit scripted view aiming
every200ms while firing at the reactor. This changes only view orientation;
position, velocity, enemy simulation, damage and ammunition remain active. Every
view adjustment is traced and counted in the receipt. This is NOT an unaided
combat/human-route pass. The fresh-process boost exit uses no view aiming. No
health, ammunition, keys, trigger, exit or ending is granted or forced. The cloak
is acquired by a normal authored pickup, not a granted assist or invulnerability.

Probe additions expose live powerup identities, exact respawn tables, door flags,
owned ship muzzles and actual weapon/ammo/timing data. Resource acquisition checks
object consumption rather than net shields: combat can reduce health during a
legitimate pickup. Sideways thrust uses normal Controls; axis values are bounded.
All production engine and original mission assets are unchanged.

The original owned ship108 gun points0/1/6 are below the classic muzzle-distance
limit. That guard was not changed. Early weapon assumptions were wrong: owned
weapon32 has fire_wait917504(14seconds), ammo_usage30, strength3538944(54),
damage_radius720896(11), speed13107200(200). It is not stock rapid-fire Gauss.
The route verifies this profile instead of changing it to make a combat test pass.

Configure XL_PUMO_PLAYER_ROOT, XL_PREVIEW_LIFECYCLE and XL_RETAIL_DATA, then run
ctest -R xl_pumo_sophodo_player --output-on-failure. Earlier earned fixtures are
automatically included. With an existing earned6 save, invoke pumo_sophodo.cmake
with ENGINE, DATA, RESOURCES, ENTRY and OUTPUT. Use a new owned OUTPUT/profile,
shared read-only original resources and owned retail data, never a user's pilot.

Private evidence is retained under temp/xl-overnight-20261104/native-sophodo-player,
including blue14/gold26/red31/armed23/reactor19/exit10 phases and combined137 prototype PASS (its stronger wrapper exposed the timer defect).
Static firing, incorrect selector-teleport assumptions, health-threshold pickup
errors and combat deaths remain retained and do not count as passes. The exact
prototype binaries and source diffs are retained separately; final qualification
requires the committed native candidate. No APK builds or phone/device writes.

Native simulation is unrendered with real GL admission and skipped presentation.
Earlier initiallevel3 is setup, not hub-origin proof. UI/import, rendered models,
briefing/audio, phone, connected human traversal, remaining main7..10 and Coldrom14
reactor/exit remain unqualified. The complete14-level campaign is incomplete.

The manual-frame probe must refresh timer_update as well as advancing GameTime64:
timer_query returns a cached SDL timer. Before the fix, a robot-first master11
activation left the later player attempt at age0 forever, suppressing countdown30.
Developer-only entry/child logging proved this; production trigger code is
unchanged. This gate opts into real-time frame pacing, clears the original
escape guard137 through normal fire, waits for the ordinary source750ms debounce,
and starts the local escape approach while not boosted. The full authored
master/countdown99 check remains mandatory. Earlier90-second exits are partial
progression evidence, not a pass of that stronger gate. Temporary logging was
removed after diagnosis; its exact binary/diff/log remain private evidence.

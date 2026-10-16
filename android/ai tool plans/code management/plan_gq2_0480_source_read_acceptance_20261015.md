# Chunk 0480 source read acceptance, 2026-10-15

Read eight whole frozen scripts848lines via896physical addition patch rows1-320/321-620/621-896, all eight whole current bodies865lines and complete later17add0del2hunks. Eight whole addition hunks848add0del, all paths absent base/1996original;7unchanged/1changed. No original removal or extra enclosing additions. No runtime/device/network/payload work.

All eight declare gamesD1/D2, nonstandalone and owner test_lan. They require an already established cooperative session; no independent launcher/payload/session setup in these bodies. Preserve original source/owner metadata and paired host/client orchestration rather than treating them as standalone acceptance.

Countdown seed/save are18line native-debug command plus2s wait each. Verify23lines invokes verify, waits3s, then clock. No JSON state assertions in these scripts; real checks/errors lie in coop_countdown_save_test/native helper and test_lan result interpretation. Fixed waits do not by themselves prove native save publication, same save generation or paused/countdown invariants.

Countdown wait34frozen/51current first observes transferbusytrue then false with ingame/unpaused, waits1.5s and invokes verify_pause. Current adds progress0/message0 then strictlyintermediate0<percent<100/message contains, requires percent-1 when busyfalse. Preserve this actual UI progress lifecycle coverage. Fast transfer may skip an observation; aggregate engine-native return/status and peer state require consumer tracing. Busyfalse/progressclear alone not successful restore or saved payload equality.

Near client171 waits for reactor destruction,1.5s, poses authored level1 exit approach, holds throttle until endlevel then releases. Host202 clears robots, poses reactor, holds fire until destruction, releases, then exit pose/throttle. Host alone asserts endlevel2 plus peer1active/inside/visible/labelvisible/age0..6000. Preserve closelyspaced tunnel case distinct from staggered exterior.

Players client171 instead delays20s after destruction; host211 requires localfinished/localstate3/peer notexited, then peer1active/outside/visible/labelvisible/age1000..14000 while localfinished. These host assertions are meaningful state diagnostics, not pixel equality or proof of exact packet/session identity without producer review. Clientdoesnot assert peer model directly.

All flyout variants require2connected players/level1/front/ingame/unpaused, waitpostlevelready, submit currentgeneration advance, requirelevel2cooperativebriefingphase6/presenting/ready, queue skip, then requirebriefinginactive/gamefront/unpaused/2connected/control4/movement1. They exercise normal level travel after real throttle exit rather than trigger_endlevel synthetic helper.

Posttravel all holdthrottle and requirethrottle_time>0, holdfire and requireenergy<100, then verify Escape opensnewmenu and secondEscape restoresfront/unpaused/alive. Preserve this input/simulation/menu ownership evidence. Energythreshold is not per-actiondelta baseline and can already be low; throttlefield meaning requires native consumer. Hostreactorfire may affect later energy control.

NEXT test_lan pairing/staging/target/session/run checks/timeout/cleanup; native debug seed/save/verify/clock/pause helper and file generation policy; cooperative transferprogress/result producer; paired D1/D2 flyout state/packet age/model/label draw and briefing/travel/window/input ownership. Whole material existing owners/priorplans/direct disposition and publication remain. No new finding/remediation/status/rating/runtime result/product change/inherited saving.

# Chunk 0487 native movie input and transition checkpoint, 2026-10-15

Diagnosis only. Sixteen freshly read selected contexts. Source closed; consumer/owner/direct/publication gates remain open.

Ordinary touch maps normalized coordinates through engine canvas dimensions, clamps to actual screen, adjusts keyboard offset and calls the existing enlarged-menu remapper before pushing events. Intro preference hit handling is separate: inside pill DOWN is retained and matching UP persists preference and emits Escape; ordinary intro screen taps fall through. The assigned script does not exercise preference persistence.

android_push_touch_action checks transition suppression on DOWN. Suppressed DOWN records position/active ownership and hit count without queuing; MOVE is ignored, and matching UP clears suppression/active state and releases held gate without queuing. Unsuppressed DOWN queues absolute mouse motion then left-button down; UP queues final position then left-button up. SDL_PushEvent returns are unchecked here; nominal injection does not guarantee admitted delivery. Native movie outcome assertions supply a separate behavioral observation.

Joystick tracking similarly records held buttons, suppresses guarded press and its matching release, and queues ordinary JNI joystick edges. Engine automation joystick injection directly invokes joy_button_handler after the same tracking predicate. This script's screen_touch uses Activity surface delivery; do not relabel it as joystick coverage.

Movie pause key is mapped to KEY_PAUSE and STEP_KEY can dispatch directly to MovieHandler when that callback is front, rather than through Android keyboard hardware. It falls back to queued SDL taps for unsupported front handlers. Current dispatcher checks callback/data and invokes selected native handlers; retain the distinction between direct native automation and physical delivery.

PlayMovie preserves native filename/extension and media/audio ownership, invokes RunMovie, reinitializes ordinary audio as applicable and resets screen mode. In co-op presentation, aborted ordinary movies leave following briefing pages available; flyout abort has a separate skip rule. The single-player script is not co-op synchronization or audio concurrency acceptance.

show_titles holds intro_active across pre-intro/intro/titles/logos/OEM sequence, checks skip preference between phases, and clears it at done. A real intro.mve aborted result still marks intro_played; missing intro can fall back to title movies and logos. The assigned current-name assertions require actual intro media and cannot be satisfied by fallback alone.

Native level-intro table maps level1 to pla; full builtin retail ShowLevelIntro plays its mapped movie then do_briefing_screens. Shareware/OEM/addon branches differ. Briefing startup flushes input and begins a new BRIEFING screen generation around nested event processing, then ends it. Movie aborted -> briefing kind/frame-1 -> in_game are meaningful forward-transition checks; final in_game alone would be weaker.

skip_intro repeatedly injects a configured joystick button or default engine mouse tap until intro_active clears, with step timeout. skip_briefing attempts Escape on current native menu/window, falls back to mouse/key, and treats Game_wind with no front or Game_wind front as complete. Its elapsed timer restarts when front-window/game-presence changes, so per-step timeout is not a complete attempt budget. Do not infer intended page identity or all transient cleanup from that generic helper.

Whole prior relaunch-fix plan records a different two-phase intro regression whose launcher continuation needed force-stop/relaunch. That historical successful run does not execute this single-launch real-movie scenario, and no new relaunch work is justified here.

Owner locator caution: broad movie-named terminal GQ2-CHUNK-0191 references GQR-0243, but the actual current remediation is DONE for shared reactor/matcen packet extraction. It is unrelated to skip admission and its score71 must not be inherited or its status reopened. BR-0206 is cooperative restore ownership, not a movie input root despite a historical context mention. A combined unrelated GQR-0212/BR-0206 read clipped; no whole-record credit is claimed. Native movie audio BR-0364 was read whole as an existing OPEN historical concurrency owner; its current implementation/fault coverage still needs explicit tracing before disposition, and it is not silently fixed by input regression.

NEXT whole existing screen/input/thread/gesture, request/budget/outcome/retirement and actual media-source owners, current quality rows and relevant prior scope; exact reuse harness/assertion/UI invocation only after current-byte proof. Preserve native media/title/briefing authority and Android guards. No new finding/status/rating/runtime acceptance, inherited saving, product edits/execution or canonical append.

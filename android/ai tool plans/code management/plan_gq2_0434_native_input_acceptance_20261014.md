# Chunk0434 native input continuation, 2026-10-14

Diagnosis and plans only. Preserve shared Android policy and narrow paired D1/D2 hooks. No product changes or runtime acceptance in this checkpoint.

Current axis mailbox uses a mutex, coherent batch generations, latest production values, automation ownership and a separate deque of effective axis-button region transitions. Publishing records region changes before ordinary latest-value coalescing. Transition consumption is bounded by the snapshot generation. Flush deliberately leaves current axis samples pending so a racing release is not discarded. Preserve these repairs; do not replace the mailbox with an unsynchronized latest-value array or an SDL-flush-dependent release.

Paired event_poll drains axes before SDL events and routes admitted joystick events to paired joy_button_handler. Android identity mapping preserves virtual button values; desktop mapping remains separate. Paired kconfig handling updates state bits on down/up and increments count fields on down. The D2 Afterburner row has a state pointer and no count pointer. Static delivery of both events does not prove that a gameplay frame sampled the held state. A 50ms UI delay is not an engine-frame acknowledgement.

The JNI joystick button publisher updates cutscene tracking before enqueue and discards SDL_PushEvent's return. Whole BR-0245 already includes direct joystick enqueue failure; do not allocate a duplicate transport root. Its atomic admission/retry/release acceptance must cover this caller. This does not claim that every historical BR-0245 location remains unchanged.

Whole BR-0530 records radial state-only pulse visibility and delayed-release teardown acceptance. Use its existing owner when comparing pulse consumers; do not infer closure from a wall-clock delay or source-only tests. Current radial producer and actual frame consumption still require fresh tracing before a final disposition. The mouse-mode Extreme Action capability candidate remains unresolved with no canonical ID; this native review does not define missing mouse geometry or establish intended capability.

Next: complete Activity/controller/action-table mapping, current native axis drain and pulse/frame consumers, original extreme intent and historical duplicate reconciliation. Then author the direct disposition and publish/replay the complete chunk with mounted Drive verification. Primary consumer/owner/disposition/publication gates remain open.

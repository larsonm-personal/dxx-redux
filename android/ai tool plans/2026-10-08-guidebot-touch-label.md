# Guidebot touch label after client deployment

- [x] Compare the rendered label with native release state in a two-peer game
- [x] Refresh guidebot presentation when asynchronous native state changes
- [x] Run scoped code quality, Android build, and the client deployment regression

The overlay polls weapon changes every 100 ms but only reads guidebot state during drawing
Client deployment can finish after the touch release redraw, leaving the old label visible

Before the fix, First Strike level 1 in D2 reproduced the failure on two emulators:
native state on both peers had released=true and owner=1, but both rendered labels
remained Locked through the 10-second assertion window

The existing 100 ms overlay poll now compares the rendered label with current native
state and invalidates only when it changes, sharing the label calculation with drawing
Native spawning and networking are unchanged

Scoped quality and both automation catalog checks passed
Android assembleDebug passed for both engines and all three configured ABIs
All seven GuidebotLockedWheelTest tests passed
The first baseline run exceeded the client startup timeout during mod-path preparation;
the completed baseline and fixed runs use the existing -TimeoutSeconds 300 option

The fixed First Strike co-op run passed: both labels initially showed Locked,
the client gained control after one deployment, and the labels changed to Guide
on the client and LanJoin on the host without another command
Evidence: temp/guidebot_label_before_lan.log and temp/guidebot_label_after_lan.log

# Re-enable ETC2 on emulator + launch button investigation

## Status: IN PROGRESS

## ETC2 re-enable
The emulator ETC2 disable was added when black textures appeared on both emulator
and real phone, misattributing the problem to the GLES translator. The user wants
it removed since both platforms had the same issue.

### Changes
- d2/arch/ogl/gr.c: Remove the emulator detection block that sets ogl_etc2_broken=1
- d1/arch/ogl/gr.c: Same
- d2/arch/ogl/ogl.c: Keep ogl_etc2_broken global (might be wanted as a settings hook later)
  but change comment to reflect it's a manual override, not auto-detected
- d1/arch/ogl/ogl.c: Same

## Launch button double-tap
The hog file check I added is only in the broadcast receiver handler, NOT in the
UI button's onClick lambda. The UI button's double-tap issue appears to be caused
by the startup file hashing (LaunchedEffect) which sets isHashing=true, disabling
the button until hashing completes. This predates my change.

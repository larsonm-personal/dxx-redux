# bugs
[ ] when loading a mod and close to device storage capacity, this can cause a crash (with no crash dump). 1. need kotlin crash dumps working in this case and 2. check for free space when extracting things into game data
[ ] android TV shows a generic controller name as the detected controller rather than the name of the first bluetooth gamepad. usb gamepads on regular android show their names currently
[ ] d1 load game preview snapshots are corrupted. probably d2 as well. could be some kind of dynamic pallette thing that needs a special -> bitmap procedure
[ ] controller on android TV, with the default "B" to fire secondary binding, is sticking and firing all missiles in one go when "B" is pressed. this may be stale - on my last test, the "B" button didn't fire anything
[ ] in the android TV dxx launcher, only the d-pad can move the selected button around. the convention in other apps and in the system menus is for both d-pad and left analog stick to move around menus
[ ] in-level pause and game menu (save/load/quit) have filtering applied even when the graphics settings have filtering off for menus
[fixed] on android TV, and maybe on non-TV, multiple debug logs are now being produced for a single app run. at one point they were combined into a single file per run. this is as of 22 May and seems to have changed in the last couple days
[fixed] android TV keyboard doesn't shift game menus up to keep the text input field visible like we have for regular android on screen keyboard
[fixed] level select game menu should be cancellable with back button, controller "B" button, etc. same as other menus
[fixed] android TV doesn't use the gamepad for the on-screen keyboard. only "a" works as a button press to select the current character, d-pad or sticks don't move around chars

# nice to haves
[ ] "prepare for descent" loading screen takes potentially up to ~5 seconds with high res textures. it would be nice to show a texture loading progress bar and maybe the text name of the most recently loaded texture or major step, or one text/bar update every 300ms max to limit drawing overhead
[ ] need some fixes for android TV to transition all remaining touch overlay bits to controller interfaces. the immediate need is for the in-game settings menu (the overlay one) to be the thing opened by the select button, rather than the in-game save/load/quit menu (its current function). there was a task at one point to move extra items into the settings menu when there was no touch interface: that needs to be rechecked
[ ] more launcher menus need highlighting. the sliders need green outlines when selected, for example

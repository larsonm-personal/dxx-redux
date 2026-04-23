# bugs
[ ] level select game menu should be cancellable with back button, controller "B" button, etc. same as other menus
[ ] when loading a mod and close to device storage capacity, this can cause a crash (with no crash dump). 1. need kotlin crash dumps working in this case and 2. check for free space when extracting things into game data
[ ] android TV doesn't use the gamepad for the on-screen keyboard. only "a" works as a button press to select the current character, d-pad or sticks don't move around chars
[ ] android TV shows a generic controller name as the detected controller rather than the name of the first bluetooth gamepad. usb gamepads on regular android show their names currently
[ ] d1 load game preview snapshots are corrupted. probably d2 as well. could be some kind of dynamic pallette thing that needs a special -> bitmap procedure
[ ] controller on android TV, with the default "B" to fire secondary binding, is sticking and firing all missiles in one go when "B" is pressed
[ ] on android TV, and maybe on non-TV, multiple debug logs are now being produced for a single app run. at one point they were combined into a single file per run. this is as of 22 May and seems to have changed in the last couple days
[ ] in the android TV dxx launcher, only the d-pad can move the selected button around. the convention in other apps and in the system menus is for both d-pad and left analog stick to move around menus
[fixed] android TV keyboard doesn't shift game menus up to keep the text input field visible like we have for regular android on screen keyboard

# nice to haves
[ ] "prepare for descent" loading screen takes potentially up to ~5 seconds with high res textures. it would be nice to show a texture loading progress bar and maybe the text name of the most recently loaded texture or major step, or one text/bar update every 300ms max to limit drawing overhead

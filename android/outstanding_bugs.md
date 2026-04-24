# bugs
[ ] controller on android TV, with the default "B" to fire secondary binding, is sticking and firing all missiles in one go when "B" is pressed. this may be stale - on my last test, the "B" button didn't fire anything. a recent code change changed berhavior to mostly ignore the "B" button (never fire)
[ ] in - game “b” button press causing a stuck secondary fire button: still a problem
[ ] can't select menu items with Bluetooth controller “a” or with tv remote center of d pad button. stick as options-misc options-automatically start single player demos. this is a checkbox that isn't selectable. the base menus and start game flow work with these buttons, not but items in these sub menus
[ ] descent 2 preview movies have skips in their soundtracks
[ready to test] when loading a mod and close to device storage capacity, this can cause a crash (with no crash dump). 1. need kotlin crash dumps working in this case and 2. check for free space when extracting things into game data
[fixed] high scores menu needs to respond to "b" controller button the same as other menus (to exit it, back to the main menu). actually, this applies to apparently all menus
[fixed] brief "b" button presses are ignored in game menus
[fixed] android TV shows a generic controller name as the detected controller rather than the name of the first bluetooth gamepad. usb gamepads on regular android show their names currently
[fixed] d1 load game preview snapshots are corrupted (and also d2, even after the previous round of changes). could be some kind of dynamic palette thing that needs a special -> bitmap procedure
[fixed] in the android TV dxx launcher, only the d-pad can move the selected button around. the convention in other apps and in the system menus is for both d-pad and left analog stick to move around menus
[fixed] in-level pause and game menu (save/load/quit) have filtering applied even when the graphics settings have filtering off for menus
[fixed] on android TV, and maybe on non-TV, multiple debug logs are now being produced for a single app run. at one point they were combined into a single file per run. this is as of 22 May and seems to have changed in the last couple days
[fixed] android TV keyboard doesn't shift game menus up to keep the text input field visible like we have for regular android on screen keyboard
[fixed] level select game menu should be cancellable with back button, controller "B" button, etc. same as other menus
[fixed] android TV doesn't use the gamepad for the on-screen keyboard. only "a" works as a button press to select the current character, d-pad or sticks don't move around chars

# nice to haves
[ ] "prepare for descent" loading screen takes potentially up to ~5 seconds with high res textures. it would be nice to show a texture loading progress bar and maybe the text name of the most recently loaded texture or major step, or one text/bar update every 300ms max to limit drawing overhead
[ ] need some fixes for android TV to transition all remaining touch overlay bits to controller interfaces. the immediate need is for the in-game settings menu (the overlay one) to be the thing opened by the select button, rather than the in-game save/load/quit menu (its current function). there was a task at one point to move extra items into the settings menu when there was no touch interface: that needs to be rechecked
[ ] more launcher menus need highlighting. the sliders need green outlines when selected, for example
[ ] controls editor needs to have a route to open the edit axis/button binding page for a specific axis/button if it's held for 2 seconds. that would enable quick controller-only bindings setup. for axes, the detection would be looking for an axis held to >80% of full range in a single direction without any other axis going >30% or any other button being pressed (for 2 seconds). for buttons, a button held >2s with no axis >30% or other button presses. only run this detection on the main controller edit page, don't open it for other bindings once a particular binding sub menu is shown
[ ] allow buttons to be placed within the drag zones (they currently are allowed, but don't have special handling). if the button is pressed it stays active as long as the drag continues, then the button is released on drag stop. this will give another way to fire weapons while looking with the same thumb
[ ] in-game brightness adjustment that saves next to the af/msaa/etc. settings. also add a slider for it out of game (next to af/msaa/etc. in the graphics launcher sub menu)

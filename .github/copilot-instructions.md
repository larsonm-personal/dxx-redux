- this project builds a cross-platform 3D video game "descent 2/1", in C and C++, with SDL and a few other dependencies
- the current goal is to create an android port

## principles
- this project, as of now, is largely about porting via build systems, rather than detailed source code changes. try to make as few source changes as possible
- any changes that are made should keep the existing windows/linux/mac builds intact, using #defines or separate files or similar
- any changes should be accompanied by a successful cmake build and test run

## building
- standard cmake commands (`mkdir build`, `cd build`, `cmake ..`, `cmake --build .`)
- see .github\workflows\package-msvc.yml for specific cmake commands
- sometimes on windows cl.exe becomes a zombie. kill it before starting cmake builds
- don't run builds until 100 errors (the msbuild default). stop around 10 (`/errorlimit:10`). later errors are often useless anyway

## automated testing
- place automated test files (like .pngs) into "temp" within this repo so that the file writes don't need to be approved
- when testing with the android emulator, the game will initially load to the main menu. hitting enter opens the "new game" menu and hitting enter again goes to the briefing
- the briefing screens can be skipped by hitting enter about 15 times. then the first level will load and the ship will be ready to move and show "score: 0" in the upper right. it's not possible to get stuck in the briefing screen after hitting enter enough times
- at this point the in-game menu can be accessed with "back"/escape

# Cockpit HUD Resolution Fix

## Problem
The 128px .dxa texture pack downscales ALL textures to max 128x128, including cockpit
overlays (full-screen bitmaps). A 128x128 cockpit texture stretched across a 1080x1920
phone display looks extremely pixelated.

## Root cause
`convert_d2xxl_textures.ps1` applies MaxDim downscaling uniformly to all textures.
Cockpit bitmaps (cockpit, cockpitb, cockpitbx2, hires-cockpit) are full-screen overlays
that need to remain at their source resolution regardless of pack size.

## Fix
1. [x] Add `$noDownscalePattern` exemption in `convert_d2xxl_textures.ps1` for cockpit textures
2. [x] Add same exemption in `convert_all.ps1` merge pass
3. [ ] Rebuild all .dxa packs with fix
4. [ ] Verify cockpit sizes in 128px pack are now full resolution
5. [ ] Push to emulator and test visually

## Affected textures
- cockpit (1024x512 source from 512 archive)
- cockpitb (2048x1024 source)
- cockpitbx2 (4096x2048 source)
- hires-cockpit (2048x2048 source in 512 archive)

## Size impact on 128px pack
Before fix: cockpit=22KB, cockpitb=22KB, cockpitbx2=22KB, hires-cockpit=11KB
After fix: cockpit=~700KB, cockpitb=~2.8MB, cockpitbx2=~11.2MB, hires-cockpit=~2.8MB
Total increase: ~17MB for the 128px d2 pack (acceptable for full-screen quality)

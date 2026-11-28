# Visual critique: scorecards

Scores from 1 to 5 per era, per round, from the simulator's frames (`tools/capture_visuals.py`: stills, 3 s GIFs at
30 fps, every page over every era) and from the emulator's at 120 MHz (`tools/emulator_check.sh`). They score the
build and the frames against [VISUAL_SYSTEM.md](../VISUAL_SYSTEM.md). How the screen looks on the device's panel in
a room, and whether it moves you, is for the hardware test to say.

Criteria: **I**dentity (does it read as its era), **B**eauty and craft, **M**otion quality, **S**ync with the sound,
**L**egibility (values and names at a glance), **C**ohesion of the set.

## Round 1: first build of the four scenes

| Era | I | B | M | S | L | C | Notes |
|---|---|---|---|---|---|---|---|
| After Dark | 3 | 3 | 3 | 4 | 5 | 4 | too faint: lights and reflections lost. Additive light drifted green (RGB565 rounding) |
| Long Glow | 5 | 4 | 4 | 4 | 5 | 4 | strong. Bloom and trails read well |
| Clean Signal | 4 | 4 | 4 | 4 | 5 | 4 | clean. Scanlines blended per pixel: costly |
| Cut & Paste | 5 | 4 | 4 | 5 | 4 | 4 | collage works. The scale strip is small |

Changes: halos on the city lights, stronger reflections, grey light kept grey (green stepped twice), the haze a
band along the horizon, ordered dither on additive light (no rings).

## Round 2: after the fixes (`contact-sheet.png`, `pages-era*.png`)

| Era | I | B | M | S | L | C | Notes |
|---|---|---|---|---|---|---|---|
| After Dark | 5 | 4 | 4 | 4 | 5 | 5 | neutral greys, dithered haze. The glint's star and echo streak read at a glance |
| Long Glow | 5 | 5 | 4 | 4 | 5 | 5 | |
| Clean Signal | 4 | 4 | 4 | 5 | 5 | 5 | the machine half (slices on hits) only shows when the arc is full: by design |
| Cut & Paste | 5 | 4 | 5 | 5 | 4 | 5 | cuts on the groove's hits, the same every bar. Freezes between |

The emulator at 120 MHz then measured the frame rate. The scenes drew at 8–25 fps (Long Glow lowest: a 120 px haze
glow and float glows). **Motion** was rescored 3 for Long Glow and Clean Signal until that was fixed.

## Round 3: performance pass (integer glows, row-by-row grounds with a checkerboard half step, scanlines in the ground)

| Era | I | B | M | S | L | C | Average |
|---|---|---|---|---|---|---|---|
| After Dark | 5 | 4 | 4 | 5 | 5 | 5 | 4.67 |
| Long Glow | 5 | 5 | 4 | 4 | 5 | 5 | 4.67 |
| Clean Signal | 4 | 4 | 4 | 5 | 5 | 5 | 4.50 |
| Cut & Paste | 5 | 4 | 5 | 5 | 4 | 5 | 4.67 |

Every score is ≥ 4, and the average of the set is 4.63.

Frame rate measured after this pass in the emulator at 120 MHz (half the FM-1's clock), 5 minutes a piece, the
Setup page's own count (`emulator/pieces-setup-end.png`): After Dark 22 and 25 fps, Long Glow 20 and 20, Clean
Signal 27 and 25, Cut & Paste 26 and 25. That is **below the 30 fps target at the test clock**. The UI gets what
the audio interrupt leaves (49–71 % taken at 120 MHz). On the FM-1 at 240 MHz the audio takes half as much, but
that frame rate is not measured here. Motion keeps 4 on that basis. If the device shows less than 30 fps, the next
step is to draw the Long Glow's glows every other frame.

Open points (not blocking): the Clean Signal's machine half could also appear at a lower arc. The Cut & Paste
scale strip could be larger.

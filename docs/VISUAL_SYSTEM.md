# HALF-LIGHT: the visual system

The screen is a 240 × 240 RGB565 panel. HALF-LIGHT keeps its three bands: the **header** (28 px: the piece, the
key, the tempo, the beat light, the battery), the **main band** (144 px) and the **knob strip** (68 px: the
page's four values). The header and the knobs take each era's palette and stay plain, so a value can be read at a
glance. The main band is the era's **scene**. It redraws every second UI frame (30 fps) and is
the instrument itself, drawn in that era's way. Code: `app/era_palette.h` (palettes), `app/visuals.c` (scenes),
`app/ui.c` (the bands).

## Principles

1. **Nothing is lost.** Every scene shows what AMB-1's garden showed: the chord and its name, the scale with its
   root, the four bell loops (13, 17, 23, 29 steps) with their notes and playheads, the three drums' bars and
   their steps, the four layers' levels, and the way to the next chord (or "held" when frozen). The scenes only
   change how these look.
2. **Sound moves light, never the reverse.** Each visual element follows one engine value the UI already reads
   (`en_pad_amp`, `en_loop_glow`, `en_drum_glow`, `en_bass_level`, `en_arc`, `par[P_ECHO]`, `par[P_REVERB]`).
   Nothing is random except the grain and the paper's fibres.
3. **Tight, not flashing.** The kick and snare pulses are ~70 ms (`en_drum_glow` decays ×0.95 a chunk). They move
   a hairline, a ring or a grid's weight, never the whole screen's brightness.
4. **Slow things are slow.** The bass breathes on a 9 s cycle. The pad's glows drift at 0.01–0.02 Hz. The arc
   changes grain, haze and the glitch share over minutes.
5. **Cheap.** Fills and rows (`cv_rect`), blends of 1/256 (`px_mix`), additive light with ordered dither (`px_add`:
   no rings in dark gradients). Only the glows (radius ≤ 50 px, falloff `(1 − d²/r²)²`, no square root) are
   per-pixel.

## The mapping, per era

| Sound | After Dark | Long Glow | Clean Signal | Cut & Paste |
|---|---|---|---|---|
| Pad (each voice) | a reflection shimmering down a wet road | a big soft glow, pink or cold blue, drifting | a signal line, as tall as it sings | a paper bar |
| Bells (loops) | four rows of city lights; a strike: a star glint | neon rings round the scale; a strike: a bloom | sequencer rows; a strike lights its cell | torn strips; a strike: the accent |
| Echo | the glint's streak, as long as Echo × the strike | trailing blooms of the last three strikes | (clean: no trails) | (cut: no trails) |
| Reverb | (haze) | the glows' size | – | – |
| Bass | haze along the horizon, breathing | violet haze rising from the bottom | the ground's light | halftone dots swelling |
| Kick | the horizon tightens: a second, brighter hairline | a ring of light closing in | the grid's lines darken | a cut: the tiles turn cold on the hit |
| Snare | a short line above the horizon | – | a slice shifts (machine half) | a cut |
| Arc | more grain when sparse | – | the machine's half: glitch slices, a coral beat cursor | – |
| Pattern rests | – | – | – | a freeze: the tiles hold until the next hit |

## Era 1: After Dark

- **Brief:** monochrome, high contrast, film grain, a night city's light, strict minimal geometry, slow and
  intimate motion.
- **Palette:** greys only: ground `#0A0A0C` to `#181818`, lines `#3A3A40`, text `#E4E4E8`, accent pure white. No
  hue anywhere: grey light is added to green at twice the steps of red and blue, so it stays grey in RGB565.
- **Light:** points with small halos. Star glints with horizontal streaks. One hairline horizon.
- **Texture:** ~400 grain specks a frame, light and dark, denser as the arc thins.
- **Geometry:** horizontal only: four rows, the horizon, vertical reflections.
- **Motion:** the reflections shimmer (two slow sines per row). The rest moves only with the sound.

## Era 2: Long Glow

- **Brief:** neon pink and cold blue glow, haze and bloom, big soft shapes, cinematic and slow.
- **Palette:** indigo `#120A28` to plum `#3A1040`, pink `#FF60B8`, cold blue `#6ED6FF`, violet haze.
- **Light:** additive: glows stack and bloom. The scale and the chord sit in the middle, white-pink.
- **Geometry:** the garden's rings (as AMB-1's, a reference to its origin), round dots, the meters rounded.
- **Motion:** the four glows drift on independent slow Lissajous paths. Blooms decay with the bell's glow.

## Era 3: Clean Signal

- **Brief:** clean, bright, digital. Scanlines, a soft grid. Data and glitch accents enter in the middle of the
  cycle, with the same two halves as the sound.
- **Palette:** white `#ECF0F5`, slate, one blue `#0070E0`, a coral `#FF5C46` reserved for the pulse and the
  machine.
- **Geometry:** a 17 × 12 px grid. Cells, squares, one-pixel rules.
- **The two halves:** while the arc is sparse (floating), the screen is still and clean. As it turns full
  (`en_arc > 0`), on the pulse's hits 1–3 slices of the scene shift sideways by up to 12 px (the canvas's rows,
  moved in place) with a coral tint, and a beat cursor marks the kick.

## Era 4: Cut & Paste

- **Brief:** fragmented collage, cuts and freezes, cold and warm contrast, irregular timing that stays exactly in
  sync with the fixed pattern.
- **Palette:** warm paper `#E0A064`, card `#F2CEA0`, cold card `#96A8B6`, ink `#24140A`, a cold blue accent.
- **Geometry:** six overlapping cards with offset shadows: the chord (two typefaces, letter by letter), the loops,
  the pad, the bass, the scale, the drift.
- **Timing:** on each step where the kick or snare plays, the step's number (hashed) picks which two cards are
  cold. Between hits nothing changes: the freeze. The pattern is fixed, so the cuts are irregular within a bar and
  identical bar after bar.

## Transitions, intro, splash

- **A change of era** (PRESETS to the other piece of a pair stays in the era): over 0.9 s (smoothstep) the
  palette morphs (header, knobs, scene colours) and the old era's ground dissolves through a 4 × 4 Bayer dither.
  See `visuals/era-changes.gif` and `visuals/era-change-frames.png`.
- **Splash** (`app_splash`, the first frame at power-on): the name in white on near-black, a hairline, the tagline.
- **Intro** (1.8 s, once the app's screen is up): the name revealed by a line of light from the left, then the era
  name fades in. See `visuals/intro.gif`.

## Performance

- The main band is drawn every second frame: 30 fps when the main loop runs at its 60 passes a second.
  `expect fps >27` in `tests/scenarios/first.amb` checks it in the simulator.
- Setup's header shows the frame rate. Setup's main band shows the audio load now and at its worst, the xruns and
  the frame rate (`draw_diag`), so the emulator and the device report their own numbers. Measured results are in
  the PR and in `visuals/emulator/`.
- No tearing: the panel has no TE line. Each band goes out in one DMA transfer (12.6 ms for the main band at
  ~44 MHz SPI), and the next frame's drawing waits for it (`cv_begin` → `lcd_sync`).

## Critique (scorecards)

`visuals/scorecards.md` scores each era from 1 to 5 on era identity, beauty and craft, motion quality, sync with
the sound, legibility, and cohesion, with the iteration's frames.

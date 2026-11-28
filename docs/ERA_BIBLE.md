# HALF-LIGHT: the Era Bible

HALF-LIGHT is generative ambient firmware for the M-VAVE FM-1. It has four eras of two pieces each. Each era
follows the mood of one period of a band's ambient and instrumental work: the band from Wilmslow and Manchester,
not the year. The eras are the self-titled debut and its EPs, the second album (2016), the third (2018) and the
fourth (2020).

This is a reference for mood only. The firmware copies nothing: no melodies, chords, riffs, drum patterns,
samples or lyrics, and no names, titles, logos or artwork (see *Self-audit* at the end). It takes timbre, space,
tempo range, density and mix balance, and expresses them with the engine's own generative parts.

Each claim is marked **[sourced]** (with its source in *Sources*) or **[design]** (a choice made here, from
listening notes and the engine's limits). The audio facts come from interviews and reviews. The palettes come
from reviews of the campaigns, and from colour and light only.

## Acceptance (the sound targets, measured by `tools/render_all.sh`)

| Target | Threshold | Why |
|---|---|---|
| No clipping | peak ≤ −1 dBFS at the nominal MASTER (the simulator's half) | headroom before the soft limiter |
| No limiter saturation | no sample above 1.0 into the output tanh at the nominal MASTER | the limiter is a safety, not a sound |
| No NaN, denormals or DC | 0, 0, \|mean\| < 0.001 | stability |
| Loudness | each piece within ±2 dB of the mean of all eight (gated K-weighted LUFS, BS.1770 at 44.1 kHz) | changing pieces never jumps in level |
| No harsh tonal peaks in 2–6 kHz | no local peak of the average spectrum that is more than **12 dB** above the median of its neighbours (a fifth of an octave) **and** within **30 dB** of the loudest bin above 250 Hz | sustained notes always make tonal peaks. They are harsh only when they stand out *and* are loud. This is the region the player heard as harsh (2.8, 3.9, 5.6 kHz) |
| Kick | fundamental 40–65 Hz | deep on headphones, still there on the speaker's 2nd harmonic |
| Pulse | bars alike: cosine ≥ 0.98 between each bar's sixteenth energies and the second bar's | a fixed pattern is the same every bar |
| Bass is slow | every rise of its envelope (the start, each chord change) takes ≥ **0.30 s** from 10 % to 90 % | "slow attack, not plucked" |
| Bass is sustained | ≥ 0.70 of its peak is left 2 s later | a drone, not a pluck |
| Bass on small speakers | 45–250 Hz band ≥ −12 dB of the whole spectrum | energy where the FM-1's speaker plays |
| Stability | 30 min per piece with no fault (NaN, denormal, saturation, DC) | the instrument runs for hours |
| Audio CPU | ≥ 15 % headroom at 120 MHz in the emulator (`mhz=120`, half the FM-1's clock) | 2× margin over the hardware |

## Era 1: After Dark (slots 1–2), the debut and its EPs

**Sound target**
- Night, intimacy and space. The ambient interludes and the EPs' drones sit between the songs [sourced: reviews of
  the debut describe droning synths under the songs and an 80s pop palette].
- Pads: a warm, slightly detuned analog-style pad with chorus [design, from the Juno-106 sound the band's producer
  values: detuned oscillators and chorusing; sourced for the band's records in general, not dated to this one].
- Guitars through a phaser and long reverb [design: a common debut-era texture. Here it is a phaser on the pad
  bus, since the engine has no guitar].
- Drums: sparse. A heartbeat kick in the calm piece, an '80s-pop pulse with a gated snare in the other [design,
  after the "bombastic drums" reviewers heard; sourced].
- Space: long plate reverb, a tape echo of one bar or more. Tempo 64–90.

**Engine mapping**
- Monochrome: the Voices pad with **Chorus 55** and **Phaser 45**, the heartbeat pattern, a long reverb
  (78/86), Echo 2 bars. Root A, Aeolian. Tempo 64.
- Night Bus: the **Strings** pad with Chorus 60 and Phaser 30, the '80s pattern (snare on 2 and 4) with
  **Room 45** (the gated room), a brighter hat. Root A, Dorian. Tempo 90.
- New DSP: the phaser and the Juno-style chorus on the pad bus, the gated room on the snare.

**Visual target**
- Black and white only: no colour at all. High contrast, film grain, the strict minimal geometry of a night city.
  Lights pass like streetlights through a windscreen, a hairline horizon, reflections on a wet road [design: a
  greyscale identity is sourced for this era's campaign; the city at night is the design's own].

**Naming notes** (era name: **After Dark**; chosen over *Monochrome Night* and *Small Hours*: short, nocturnal,
no title)

| Slot | Candidates | Chosen | Why |
|---|---|---|---|
| 1 | Monochrome, Lamplight, Grey Hours | **Monochrome** | the era's look in one word. The provisional name the player knows |
| 2 | Night Bus, Wet Asphalt, Sodium Rows | **Night Bus** | movement through the city at night, matching the pulse. *Midnight Hours* was dropped: too generic, and too near common song titles |

## Era 2: Long Glow (slots 3–4), the second album

**Sound target**
- Expansive and cinematic. Ambient pieces between the songs (reviewers count four), big soft pads, soft piano,
  shimmer [sourced: reviews].
- The producer used an ambient-texture plugin on the album's title track and turned to a D-50 to counter the
  heavy FM sound of this album [sourced: interviews]. So: an FM-flavoured palette, softened.
- Space: very long reverb, slow tempo (66–84), low density.

**Engine mapping**
- Pale Piano and Neon Fog are the player's favourites. **Their character is kept.** Changes, each a defect fix:
  - *Trim* −6 dB and −1 dB: the loudness target. Pale Piano was 7 dB louder than the rest.
  - the kick's note (Neon Fog): fixed at the piece's fifth, G1 (49 Hz), not the chord's root. Before, it went
    down to C1 (33 Hz) and below the 40 Hz target, and it changed with every chord. The player asked for a deep,
    steady kick.
  - the top bells, above G6, are 2 dB softer per semitone. Their partials were the 2–6 kHz harshness. Only Neon
    Fog's rare top notes are affected.
  - PATTERN replaces HITS. The same groove is selected (Neon Fog: 43 → pattern 3).
- The Rhodes (Pale Piano) and the Voices (Neon Fog) pads, unchanged.

**Visual target**
- Neon pink and cold blue on indigo night, haze and bloom, big soft out-of-focus shapes, slow and cinematic [design,
  after the campaign's faded-pink neon and white; sourced. The signs and the cover are not copied: only the pink,
  the glow and the night].

**Naming notes** (era name: **Long Glow**; chosen over *Neon Haze* and *Soft Focus*)

| Slot | Candidates | Chosen | Why |
|---|---|---|---|
| 3 | Pale Piano, Slow Neon, Ivory Hours | **Pale Piano** | the player's favourite, kept |
| 4 | Neon Fog, Pink Haze, Glow Static | **Neon Fog** | the player's second favourite, kept |

## Era 3: Clean Signal (slots 5–6), the third album

**Sound target**
- Cleaner and brighter, digital. A vocoder-led ambient piece opens the instrumental side [sourced: the track is
  described as vocoder-led]. Vocoder and harmonizer softening of voices is part of the producer's palette [sourced].
- The two halves: floating, then mechanical. UK-dance and garage hints, glitch and stutter edits [sourced: Stutter
  Edit on vocals, for the producer's work around 2016. The two halves are a design choice].
- Soft synth strings [design].

**Engine mapping**
- Glass Choir: the **Formant** pad (a buzz through two narrow, wandering vowel resonances: a vocoder's choir with
  no speech), Chorus 30, **Glitch 25**, the half-time pattern, Motion 85 (the arc carries it from floating to
  full). Root G, Mixolydian. Tempo 72.
- Glitch Garden: the **Strings** pad, **Glitch 70** (bells stutter on 32nd notes), the UK-dance pattern, bells an
  octave down and softer. Root D, Dorian. Tempo 112.
- New DSP: the Formant and Strings pad colours, the bell stutter.

**Visual target**
- Clean, bright and digital: white, slate, one blue, a soft grid and scanlines. In the arc's full half, data and
  glitch accents enter (slices shift on the pulse's hits, a coral beat cursor): the same two halves as the sound
  [design].

**Naming notes** (era name: **Clean Signal**; chosen over *White Noise* (a common phrase, too near titles) and
*Soft Circuit*)

| Slot | Candidates | Chosen | Why |
|---|---|---|---|
| 5 | Glass Choir, Vocoder Drift, Paper Robot | **Glass Choir** | the Formant pad's clean, artificial choir. *Vocoder Drift* described a technique rather than a mood |
| 6 | Glitch Garden, Bit Bloom, Stutter Park | **Glitch Garden** | the provisional name fits (AMB-1's "garden", glitched) |

## Era 4: Cut & Paste (slots 7–8), the fourth album

**Sound target**
- Fragmented: house and UK garage drums next to folk, orchestral and ambient pieces. Drones with a piano that
  recall the band's first EPs. Glitchy cuts. A neo-noir ambience [sourced: reviews and a track-by-track
  interview].
- Uneasy bells, cold electronics, irregular but fixed rhythm [design].

**Engine mapping**
- Fragments: the broken pattern, the Voices pad, **Glitch 55**, Evolve 75 (phrases change often), Echo 4 bars,
  bells an octave down. Root F#, Phrygian. Tempo 100.
- Static Folk: the **two-step** pattern with **Swing 55** and the gated room (Room 30), the Formant pad, Glitch
  45. Root E, minor pentatonic. Tempo 112.

**Visual target**
- Collage: card tiles on warm paper, cold against warm. Cuts on the groove's own hits, freezes between them.
  The chord's name in two typefaces [design, after the campaign's orange-brown cover and its mixed typefaces;
  sourced. Only the colour and the idea of many typefaces are used].

**Naming notes** (era name: **Cut & Paste**; chosen over *Collage* and *Notes Apart*, the latter too near the
album's title)

| Slot | Candidates | Chosen | Why |
|---|---|---|---|
| 7 | Fragments, Torn Edges, Half Sentences | **Fragments** | the provisional name fits the broken pattern and the glitch |
| 8 | Static Folk, Two-Step Static, Field Static | **Static Folk** | folk next to electronics, the era's contrast in two words |

**Display check:** every name fits the header's tag in the bold font with the root, mode and tempo. Measured in
the simulator's screenshots: the longest, *Glitch Garden*, leaves the root visible. The engine has no hard length
limit; 23 characters is the limit of the screen's messages.

## The pieces' recipes

They are in `app/dsp/amb.c` (`PIECES`), one row each, with the columns named above them. The table below shows
only the values that set each era's character:

| Piece | Pad tone | Pattern | Chorus | Phaser | Glitch | Room | Tempo | Key | Trim |
|---|---|---|---|---|---|---|---|---|---|
| Monochrome | Voices | Heart | 55 | 45 | 0 | 0 | 64 | A Aeolian | −5 |
| Night Bus | Strings | '80s | 60 | 30 | 0 | 45 | 90 | A Dorian | +4 |
| Pale Piano | Rhodes | None | 0 | 0 | 0 | 0 | 66 | F Lydian | −6 |
| Neon Fog | Voices | '80s | 0 | 0 | 0 | 0 | 84 | C Ionian | −1 |
| Glass Choir | Formant | Half | 30 | 0 | 25 | 0 | 72 | G Mixolydian | +4 |
| Glitch Garden | Strings | UK | 20 | 0 | 70 | 0 | 112 | D Dorian | +6 |
| Fragments | Voices | Broken | 0 | 0 | 55 | 0 | 100 | F# Phrygian | +6 |
| Static Folk | Formant | 2-step | 25 | 0 | 45 | 30 | 112 | E pent. minor | +8 |

## Self-audit (gate 8)

- **Melodies and chords:** none are stored. The bells' phrases and the chords are generated at run time from the
  scale (`rand_offset`, `chord_change`, `en_voice_lead`), as in AMB-1.
- **Drum patterns:** the eight grooves in `GROOVE[]` are the textbook figures of their genres (four on the floor,
  a backbeat, a two-step shuffle) or AMB-1's own. None is transcribed from a recording.
- **Samples:** none. Every sound is synthesized (sines, saws, FM, filtered noise).
- **Lyrics, names, titles:** no lyrics anywhere. No band, member, album or song name appears on the screen, in the
  piece or era names, in the firmware or in the assets. The names were checked against the four albums' and EPs'
  track lists.
- **Visual identity:** no logo, wordmark or rectangle, and no cover art, photography or signs. The screens use
  colour, light, texture and motion only. There is no centred rectangle frame in any era.

## Sources

- Roland Articles, "A Few Minutes with George Daniel of The 1975" (2023): TR-08 and TR-909 sources, JUNO-106
  detune and chorus, the D-50 against the second album's FM, the JV-1080. https://articles.roland.com/?p=68807
- iZotope, "5 Questions with George Daniel of The 1975" (2016): the second album made on the road, Iris for
  ambient textures on its title track, Stutter Edit, vocoder and harmonizer influences.
  https://www.izotope.com/en/learn/5-questions-with-george-daniel-of-the-1975
- PAPER, *Notes on a Conditional Form* track-by-track (2020): house and UK garage, the drones of the long ambient
  piece recall the 2013 EP, neo-noir ambience, glitchy cuts.
  https://www.papermag.com/the-1975-notes-on-a-conditional-form-interview
- Wikipedia, *Notes on a Conditional Form*: genres, the tour-bus studio, the orange-brown cover with mixed
  typefaces. https://en.wikipedia.org/wiki/Notes_on_a_Conditional_Form
- Slant, review of the second album: four ambient songs, an '80s pop canon.
  https://www.slantmagazine.com/music/the-1975-i-like-it-when-you-sleep/
- Sputnikmusic, review of the second album: soundscapes in the ambient pieces.
  https://www.sputnikmusic.com/review/69934/The-1975-I-Like-It-When-You-Sleep-For-You-Are-So-Beautiful-Yet-So-Unaware-of-It/
- Creative Boom, the second album's neon signs (faded pink).
  https://creativeboom.com/inspiration/the-1975-drake-and-burgess-johnson-create-neon-signs-for-the-manchester-band
- The Harvard Crimson, review of the second album (the white cover, the pink after the black-and-white debut).
  https://thecrimson.com/article/2016/3/11/1975
- Gigwise and idobi, reviews of the debut: '80s synths, droning synths, big drums.
  https://www.gigwise.com/reviews/83930/the-1975---the-1975-universal , https://idobi.com/?p=228543
- Last.fm, the third album's ambient opener described as vocoder-led.
  https://build.last.fm/music/The+1975/A+Brief+Inquiry+Into+Online+Relationships/How+to+Draw+%2F+Petrichor

Not verified: the exact synths on each ambient track, reverb and delay settings (no source gives them), and the
drum machines per album (the sources give the producer's tools in general). Those parts are marked [design] above.

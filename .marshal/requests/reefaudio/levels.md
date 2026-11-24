# Levels (offline copy of the synth maths; nobody has listened)

All figures are dB relative to digital full scale (0 dB = a sample value of 1) at the synths' outputs, before the
system volume. "Loudest 50 ms" is the RMS of the loudest twentieth of a second. Produced by `render.py` from
`recipes.json`, which the game wrote.

## The references: the hub's own sound

| What | RMS dB | Peak dB |
|---|---|---|
| Engines idling in gear, boat still, with the water and wind loops (two engines) | -33.8 | -23.4 |
| Half speed, throttle 0.6 | -24.6 | -11.9 |
| Flat out | -19.4 | -6.6 |
| Under water 3 m down, boat idling above (hub low-pass 726 Hz): the idling boat | -35.2 | -23.1 |
| ... the hub's breathing loop | -47.4 | -33.2 |
| ... the hub's mask bubbles | -80.5 | -46.2 |
| ... the hub's old flat crackle | -91.5 | -60.7 |
| ... a stoplight parrotfish's crunch 2 m away (loudest 50 ms) | -39.8 | -23.9 |
| Under water 12 m down, boat idling above (hub low-pass 382 Hz): the idling boat | -34.1 | -23.3 |
| ... the hub's breathing loop | -49.3 | -34.7 |
| ... the hub's mask bubbles | -85.5 | -52.5 |
| ... the hub's old flat crackle | -100.0 | -68.6 |
| ... a stoplight parrotfish's crunch 2 m away (loudest 50 ms) | -42.6 | -27.0 |
| Under water 25 m down, boat idling above (hub low-pass 151 Hz): the idling boat | -37.2 | -25.7 |
| ... the hub's breathing loop | -53.9 | -39.6 |
| ... the hub's mask bubbles | -90.6 | -55.1 |
| ... the hub's old flat crackle | -106.3 | -71.3 |
| ... a stoplight parrotfish's crunch 2 m away (loudest 50 ms) | -48.5 | -33.8 |

## The reef's bed, by place

| Place | Clicks a second | RMS dB | Peak dB | Against the hub's whole under-water bed at that depth (dB) | Against the idling boat heard from there (dB) | Against the parrotfish crunch at 2 m, loudest 50 ms (dB) |
|---|---|---|---|---|---|---|
| reef top, 3 m down in 5 m, noon | 46.4 | -44.0 | -15.2 | +3.4 | -8.8 | -4.2 |
| reef top, 3 m down in 5 m, dusk | 64.9 | -43.4 | -14.4 | +4.0 | -8.2 | -3.6 |
| reef crest, 1.5 m down in 2.5 m, noon | 46.4 | -40.3 | -12.7 | +7.1 | -5.0 | -0.5 |
| 30 m off the reef, 4 m down in 6 m | 19.0 | -52.3 | -23.1 | -4.9 | -17.1 | -12.5 |
| open sand, 3 m down in 5 m, noon | 0.3 | -57.5 | -41.8 | -10.1 | -22.3 | -17.7 |
| fore reef, 12 m down in 14 m | 34.8 | -53.0 | -19.3 | -3.7 | -18.9 | -10.4 |
| the wall, 25 m down in 30 m | 12.9 | -56.6 | -28.5 | -2.7 | -19.4 | -8.1 |
| blue water, 25 m down in 60 m | 0.3 | -57.6 | -34.0 | -3.7 | -20.4 | -9.0 |

## The one-shot sounds, at their reference distance

| Sound | Where | Ref m | Never past m | Notes | Peak dB | Loudest 50 ms dB | Against the idling boat (dB) | Against half speed (dB) | Against flat out (dB) | Against the reef bed on the reef top at noon (dB) |
|---|---|---|---|---|---|---|---|---|---|---|
| grunt | under water | 2 | 12 | 5 | -23.7 | -37.7 | -2.5 | n/a | n/a | +6.3 |
| pop | under water | 1.5 | 10 | 4 | -17.6 | -31.6 | +3.6 | n/a | n/a | +12.5 |
| boom | under water | 4 | 25 | 2 | -19.7 | -25.5 | +9.7 | n/a | n/a | +18.6 |
| boomgoliath | under water | 6 | 35 | 3 | -15.6 | -21.4 | +13.8 | n/a | n/a | +22.6 |
| rasp | under water | 1.5 | 10 | 15 | -19.4 | -36.0 | -0.7 | n/a | n/a | +8.1 |
| whistle | under water | 15 | 150 | 2 | -28.0 | -31.0 | +4.3 | n/a | n/a | +13.1 |
| clicks | under water | 10 | 150 | 25 | -17.6 | -43.0 | -7.8 | n/a | n/a | +1.0 |
| croak.greatblue | above water | 15 | 200 | 2 | -13.1 | -29.6 | +4.2 | -5.1 | -10.2 | n/a |
| croak.egret | above water | 15 | 200 | 1 | -16.3 | -32.1 | +1.6 | -7.6 | -12.7 | n/a |
| croak.small | above water | 12 | 150 | 1 | -18.9 | -34.2 | -0.4 | -9.6 | -14.8 | n/a |
| skeow | above water | 12 | 150 | 1 | -26.9 | -30.1 | +3.7 | -5.5 | -10.7 | n/a |
| quok | above water | 15 | 200 | 1 | -18.6 | -30.6 | +3.1 | -6.1 | -11.2 | n/a |
| honk | above water | 12 | 150 | 3 | -22.4 | -32.7 | +1.1 | -8.2 | -13.3 | n/a |
| rattle | above water | 15 | 200 | 20 | -24.9 | -45.1 | -11.3 | -20.5 | -25.7 | n/a |
| kree | above water | 15 | 200 | 1 | -23.4 | -29.7 | +4.1 | -5.2 | -10.3 | n/a |
| osprey | above water | 20 | 250 | 16 | -26.4 | -29.8 | +4.0 | -5.2 | -10.4 | n/a |
| flush.big | above water | 8 | 100 | 10 | -17.9 | -28.6 | +5.2 | -4.1 | -9.2 | n/a |
| flush.small | above water | 6 | 60 | 10 | -23.0 | -36.8 | -3.0 | -12.2 | -17.3 | n/a |
| flight | above water | 15 | 200 | 14 | -25.0 | -35.7 | -1.9 | -11.1 | -16.2 | n/a |
| plunge.pelican | above water | 12 | 150 | 3 | -18.8 | -26.6 | +7.2 | -2.1 | -7.2 | n/a |
| plunge.small | above water | 6 | 60 | 2 | -22.5 | -36.2 | -2.4 | -11.6 | -16.8 | n/a |
| jump | above water | 6 | 80 | 2 | -16.8 | -29.8 | +4.0 | -5.2 | -10.4 | n/a |
| shower | above water | 8 | 100 | 13 | -18.5 | -28.6 | +5.2 | -4.0 | -9.2 | n/a |
| roll | above water | 5 | 50 | 2 | -22.0 | -29.1 | +4.6 | -4.6 | -9.7 | n/a |
| crabdrop | above water | 3 | 25 | 4 | -18.7 | -32.3 | +1.5 | -7.8 | -12.9 | n/a |
| strike | above water | 4 | 30 | 2 | -18.8 | -37.3 | -3.5 | -12.8 | -17.9 | n/a |

For an under-water sound "the idling boat" is the boat as heard 3 m down through the hub's low-pass.

## What each sound is made of (the game's own words, seed 1)

- `grunt`: 5 pulses of band-passed noise (662 Hz, Q 3, 22 ms each, 16-23 ms apart).
- `pop`: 2 pops, each a sine falling 450-520 -> 330 Hz in 30 ms over an 8 ms tick of noise at 1.2 kHz, 45-70 ms apart.
- `boom`: a sine falling 96 -> 70 Hz, 0.34 s, 30 ms attack, with its octave at a fifth of the level.
- `boomgoliath`: 1 boom(s): a sine falling 62 -> 46 Hz, 0.5 s, a triangle an octave up at a seventh of the level, a thump of noise under 130 Hz.
- `rasp`: 15 ticks of wide band-passed noise (2.6-3.6 kHz, Q 0.7, 6 ms each), 7-9 ms apart and closing up.
- `whistle`: 2 whistle(s): a sine rising from 5.2-7.8 kHz to 10.5-14.5 kHz and falling to 7-9.5 kHz, 0.45-0.9 s.
- `clicks`: a train of 25 ticks (noise round 9 kHz, 4 ms each), 46 ms apart closing to 11 ms.
- `croak.greatblue`: a saw falling 176 -> 140 Hz through a 950 Hz band-pass, chopped 36 times a second, 0.62 s, with breath noise chopped the same.
- `croak.egret`: a saw falling 232 -> 200 Hz through a 1.25 kHz band-pass, chopped 52 times a second, 0.38 s.
- `croak.small`: a saw falling 360 -> 300 Hz through a 1.7 kHz band-pass, chopped 60 times a second, 0.22 s.
- `skeow`: a square wave up from 2.1 to 2.4 kHz and down to 1.3 kHz in 0.2 s, through a 2.2 kHz band-pass.
- `quok`: a saw falling 540 -> 420 Hz through a 1.5 kHz band-pass, chopped 85 times a second, 0.13 s.
- `honk`: 3 notes, each a saw falling 312 -> 268 Hz through a narrow 850 Hz band-pass (the nasal part), 0.15 s, a quarter second apart.
- `rattle`: 20 ticks 42 ms apart (24 a second), each 17 ms of noise through a band-pass (Q 3) sliding 3.5 -> 2.9 kHz.
- `kree`: a saw up from 2.5 to 3.1 kHz and down to 2.3 kHz in 0.32 s, through a 2.9 kHz band-pass, roughened 95 times a second.
- `osprey`: 8 chirps 0.15 s apart, each a sine rising 2.25 -> 3.3 kHz in 0.1 s with its octave at a fifth of the level; the run is a sixth higher in the middle.
- `flush.big`: 5 wing beats 0.3 s apart, each 0.16 s of noise low-passed from 320 Hz down to 120 Hz with a tenth as much rustle at 2.4 kHz.
- `flush.small`: 5 wing beats 0.13 s apart, each 0.08 s of noise low-passed from 520 Hz down to 200 Hz with a tenth as much rustle at 2.4 kHz.
- `flight`: 14 wing beats scattered over 1.8 s and across the stereo field, each 0.09-0.15 s of noise low-passed from 320-520 Hz down to 150 Hz.
- `plunge.pelican`: noise low-passed from 1.1 kHz down to 180 Hz over 0.7 s, a sine thump falling 90 -> 45 Hz, and 0.9 s of spray (noise above 2.6 kHz).
- `plunge.small`: 0.14 s of noise through a band-pass falling 1.9 kHz -> 700 Hz and 0.2 s of spray above 3.5 kHz.
- `jump`: a slap: 0.2 s of noise through a band-pass falling 1.5 kHz -> 450 Hz over 0.3 s of noise low-passed from 420 Hz down.
- `shower`: 0.7 s of hiss (noise above 3.6 kHz, 90 ms attack) and 12 ticks of band-passed noise (2.4-5.2 kHz, 30 ms) scattered over the next 1.3 s.
- `roll`: a sine falling 170 -> 80 Hz in 0.2 s (the gulp) and 0.6 s of noise low-passed from 520 Hz down, swelling in over 0.12 s (the swirl).
- `crabdrop`: 2 plops, each a sine falling 560-700 -> 250 Hz in 50 ms over a 20 ms tick of noise at 2.2 kHz.
- `strike`: a 40 ms tick of noise at 2.7 kHz over 80 ms of noise below 600 Hz.

## Recording of the game's reef synth: dive.wav

30.0 s at 48000 Hz; peak -13.8 dB; RMS -47.8 dB; loudest 50 ms -37.0 dB.

Peak dB of each second: -20 -22 -19 -19 -21 -19 -19 -23 -21 -23 -19 -20 -19 -21 -23 -21 -18 -20 -19 -18 -18 -18 -19 -20 -20 -19 -21 -18 -19 -20

## Recording of the game's reef synth: deep.wav

8.0 s at 48000 Hz; peak -42.9 dB; RMS -57.3 dB; loudest 50 ms -53.3 dB.

Peak dB of each second: -46 -46 -44 -44 -44 -48 -46 -46


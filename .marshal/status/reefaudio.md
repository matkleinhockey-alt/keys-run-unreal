# reefaudio

Status: READY FOR MERGE
Updated: 2026-11-24
Branch: lane/reefaudio (see `git log -1`)

The acceptance criteria are the marshal's reading of the user's "reef sounds"; the user has not seen them.
**Nobody has listened to any of this.** A scripted run leaves a log, numbers and pictures, not sound. Whether a
croak sounds like a heron, whether the crackle is pleasant or tiring, and every level below are the user's to
judge by ear. Evidence lives in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-reefaudio/`
(`log_<run>.log`, `proof/`), called `LANES/` below.

## How it is built (three lines)

- A world subsystem (`UKeysReefAudioSub`) starts by itself in every game world, makes its own synth component
  (`UKeysReefSynth`) on the audio actor and reads the diver, the bottom, the clock, the spear targets and the
  mangrove ecosystem's sound queue through their public functions. No hub file spawns or ticks it.
- Its own synth, not `AKeysAudio::Burst`: the hub shuts everything behind a 150-900 Hz low-pass under water, which
  takes a 3-7 kHz crackle and a 5-15 kHz dolphin whistle away (the hub's old crackle comes out at -92 dB RMS).
- Grunt schools, a damselfish's chase and dolphin pods cannot be seen from outside their files: three one-line hub
  patches call `KeysReefAudio::Heard()`. Without them those three sounds never play; everything else does.
  `-KRNoReefAudio` turns the whole lane off.

## Acceptance criteria

- [x] 1. The reef's own voice, under water. Snapping-shrimp clicks as a random stream whose rate and level follow
  reef and hard bottom under the diver and 15 m and 30 m round, the water's depth and the hour; a surge layer over
  shallow reef; a low rumble deep; one function, `KeysReefAudio::BedFor`. Evidence, `LANES/log_ra2.log` and
  `log_ra3.log`, lines `KRReefAudio bed:`: reef top at Sombrero 51.3 clicks/s (156 counted in 3 s); the same spot
  at `clock=19:10` 64.9/s; arriving by boat cover 0.49 then 1.00 (20.6 then 51.1/s); the mangrove flat (bare
  bottom) 0.4/s, 1-4 clicks in 3 s; blue water 25 m down in 101 m: 0.4/s, output peak -48 dB against -14 dB on the
  reef; surge 0.0139 at 4 m down in 5.5 m, 0.0012 at 6.5 m down in 7.7 m. Recordings of the game's own synth:
  `LANES/proof/dive.wav` (RMS -47.8 dB) and `deep.wav` (-57.3 dB), pictures `proof/rec_dive_1.png`, `bed_1.png` to
  `bed_8.png`. Not shown in the game: a swim from the reef out over sand in one dive (`goto=hawk` and `goto=wall`
  both landed on reef); the "30 m off" row of the table is the function's answer, not a place visited.
- [x] 2. Animals you can hear close up (needs R1, R2, R3; tested with them applied locally). `log_ra2.log`,
  `KRReefAudio play:` lines: a goliath's boom as the diver came within 7 m (found among the spear targets, no
  patch); a grouper's boom "now and then" at 24.1 m; grunts from a real school of 24 at 7.3 m and 6.7 m (R1);
  dolphin whistles and click trains from real pods at 121-145 m (R3; `log_ra2.log`, `log_ra3.log`). The damselfish
  pop and the lobster rasp were played only through the test command that stands in for the hook
  (`con=kr.reefaudio heard chase sergeantmajor 3`): no real chase happened in a run, and lobsters are not in this
  branch. A parrotfish's chase makes no sound (logged, nothing played). Parrotfish crunch against the bed
  (`levels.md`): 2 m away at 3 m depth its loudest 50 ms is 4 dB over the reef-top bed's RMS, but its peak
  (-24 dB) is under the bed's loudest clicks (-15 dB), and at 25 m depth the hub's low-pass takes it down another
  9 dB. Left alone (not my file); an ear should decide whether the crunch wants lifting.
- [x] 3. The mangroves, above water. The ecosystem's queue is emptied every frame and played. Real events heard
  in runs (`log_ra2.log`, `log_ra3.log`, `log_ra4.log`): plunge (tern, cormorant), mullet jump, bait shower, tern
  call, kingfisher rattle, and on a fast pass (`KREco flush` lines in `log_ra4.log`) wing beats with the alarm
  croak of an anhinga, a great blue heron and a great egret, and a pelican's wings. Not set off by a real event in
  any run, so played only by the test command: ibis, night heron, green heron, osprey, tarpon roll, crab drop,
  bill strike, rookery flight. Levels (`levels.md`): calls sit 0 to 4 dB over the idling boat at their reference
  distance and 5 to 10 dB under the boat at half speed; the kingfisher's rattle is the weak one (11 dB under idle).
- [x] 4. Nothing where it should not be. `log_ra3.log`: `silent (ashore)`, `silent (a menu is open)`, and the
  hook test on the title screen logged `title screen` and played nothing; on deck the bed's master is 0.00 and a
  chase reported on deck was dropped (`log_ra2.log`); under water at the mangroves no call was played (dropped
  count 13 to 33) ; a rasp reported 30 m off was not played (limit 10 m); no line shows a sound past its "max".
  The muffled splash was shown only by `test jump 10 muffled` (the real splashes were over 40 m off, the limit
  under water). Cap: 48 notes and 12 shrimp clicks at once plus 3 noise layers; the most seen was 27 notes; a
  sound that would pass the cap is not played and logs `KRReefAudio skip` (seen in `log_ra1.log` at the old cap).
- [x] 5. Proof without ears. Log: one `KRReefAudio play:` line a sound (what, why, distance, gain, level, what it
  is made of); the bed every 3 s in scripted runs. What is synthesised: the last section of
  `.marshal/requests/reefaudio/levels.md`, in the game's own words. Pictures (waveform over spectrogram) of all
  26 sounds and 8 beds from the offline copy `render.py`: `LANES/proof/snd_*.png`, `bed_*.png`; I opened
  `bed_1`, `snd_croak_greatblue`, `snd_whistle`, `rec_dive_1`. Levels against the engine and the hub's
  under-water bed: `levels.md`. The offline copy and the game agree where compared: `deep.wav` -57.3 dB against
  -57.6 dB; in `log_ra1` recorded peaks of pop, boom, goliath boom, rasp and whistle were within 1 dB.
- [x] 6. What a paid sound would do better: the list below. Nothing bought, nothing generated.
- [x] 7. No slowdown. The lane's own count: game thread 0.008-0.012 ms a frame, audio thread 0.03-0.11% of one
  core (every `bed:` line). Frame time, Medium, same build with and without (`-KRNoReefAudio`), `log_pfon.log` and
  `log_pfoff.log`: diving at `goto=reef` game 17.09 / 16.30 ms with, 17.76 / 19.49 without; at
  `tp=-1150,452,4.71` game 11.41 with, 11.48 / 14.10 without; frame 33.3 ms throughout. Not slower. Caveats: this
  is on/off in one build, not against `main`; the second "without" samples were disturbed (worst frames 371 and
  342 ms); the second "with" sample on the flat is missing from the log.
- [x] 8. Nothing else moves. `KRTp checked 48 destinations, 0 bad` (`log_ra4.log`), `19 destinations, 0 bad` on
  `-KRClassicMap` (`log_ra5.log`). The branch changes no file outside its paths, so engine, reel, radio and
  regulator are as on `main`. R4, if taken, removes the hub's own flat crackle and nothing else.

## What the marshal's criteria made me guess

- "About 30 m": bottom counts in full under the diver, 85% at 15 m, 50% at 30 m.
- "A little busier at dusk and dawn": 40% more clicks at 6:10 and 19:10, 12% more through the night.
- Which birds call: the ecosystem never queues `croak`, `honk` or `quok`, so each bird's own call is played when
  it is flushed (and with a rookery flight). A pelican gets wings only.
- Dolphins are heard under water only (whistles do not carry into air); the brief's "reef sounds are not heard
  from the boat" was read to cover them.
- "In town" is `IsAshore()`; "menus" is `IsCapturingKeys()` and the title screen.
- Every level. Targets I chose: calls about level with the idling boat at 12-20 m; the bed 9 dB under the idling
  boat as heard from below.

## What a paid sound would do better (for the user; nothing bought)

1. Bird calls: great blue heron, great egret, white ibis, night heron, green heron, osprey, kingfisher, royal
   tern. A chopped saw wave is a croak-shaped noise, not a heron. This is the largest gap by far.
2. Wing beats of a large bird taking off, and a flock leaving a rookery. Mine are thumps of filtered noise.
3. A real snapping-shrimp reef recording, 30-60 s. Mine has the rhythm and spectrum, not the room.
4. A pelican's plunge and a mullet's slap. Splashes are where filtered noise sounds most like filtered noise.
5. Dolphin whistles and clicks: real ones have harmonics and a signature shape.
6. Not worth buying: grunts, damselfish pops, grouper booms, the lobster's rasp (few recordings exist; simple
   sounds that synthesis does about as well).

## Done

- `Source/KeysRun/KeysReefAudio.h`, `KeysReefAudio.cpp` (game thread), `KeysReefAudioSynth.cpp` (audio thread),
  `KeysReefAudioVoices.cpp` (26 recipes and the bed's function), `KeysReefAudioInt.h`.
- `.marshal/requests/reefaudio/`: `R1`-`R4.patch`, `render.py` (offline copy), `recipes.json` (the game's dump),
  `levels.md`.
- Test hooks, `con=kr.reefaudio ...`: `stats`, `list`, `test <sound> [m] [muffled]`, `all`, `dump`,
  `rec <s> <name>` (WAV in `Saved/KRReefAudio`), `heard <what> <who> <m> [s]`, `bed <cover> <water> <diver>`.

## Requests for the marshal

- R1 (open): `KeysSchools.cpp` - one call in `Tick` reporting reef and hole schools within 40 m. Without it no
  grunts, and only the groupers among the spear targets boom. Patch: `.marshal/requests/reefaudio/R1.patch`
- R2 (open): `KeysReefLife.cpp` - one call where a chase starts. Without it no damselfish pop. `R2.patch`
- R3 (open): `KeysLife.cpp` - one call per dolphin group in the schools loop. Without it no dolphins. `R3.patch`
- R4 (open, optional): `KeysAudio.cpp` - delete the flat 2.2-a-second crackle in `SetUnderwater`. It is all but
  silent behind the hub's low-pass, so leaving it costs nothing. `R4.patch`
- R5 (open): the lobster's rasp, now that reef dens are on `main`: in `KeysReefDenLife.cpp` beside the
  `KRDen link flick` log line, `KeysReefAudio::Heard(TEXT("rasp"), TEXT("spiny lobster"), <its position>);`
  (include `KeysReefAudio.h`). Not written as a patch: that file is not in this branch.
- R6 (open): do not also wire `mangrovelife` R2 into the hub: `PopSound` has one reader now.
- R7 (open): frame time against `main` itself on a quiet machine.

## Not verified

- The sound, by ear: all of it.
- A real damselfish chase and a real lobster; ibis, night heron, green heron, osprey, roll, crab drop, strike and
  rookery flight from real events; a real splash heard muffled from under water.
- The hub's under-water low-pass is taken to be one-pole in `levels.md`; if it is steeper the hub's sounds are
  quieter under water than the table says.
- Classic map: the bed follows the reef there (`log_ra5.log`, cover 1.00) but my dive in that run never put the
  camera under water, so nothing was heard-by-log there.
- The last build changed one gain (the rattle, 0.6 to 0.25) and one log wording after the last behaviour run;
  it was built and run only in the frame-time runs.
- Two synth components sum without a shared limiter: the worst case of bed plus loud one-shots plus the engine was
  not measured.

## Notes for integration

- `birdview` edits `KeysMangroveEco*`: a new bird kind falls through to "wings, no call" until given a voice in
  `KeysReefAudio.cpp`.
- The mullet at the Boot Key flat jump in runs of twenty or more; each is a separate slap.

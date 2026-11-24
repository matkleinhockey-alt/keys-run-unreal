# reefvisitors

Status: READY FOR MERGE (with R1; without R1 the lane's file builds and registers nothing)
Updated: 2026-11-23 23:50
Branch: lane/reefvisitors (see `git log`; the commit that carries this file)

The split of the user's reef list into the criteria below is the marshal's reading, not the user's words.
Evidence lives in `L = /Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-reefvisitors`
(`L/log_<tag>.log`, `L/shots/<tag>/`, contact sheets I looked at in `L/sheets/`).

Everything seen in the game was seen with R1 applied locally to `KeysReefLife.h/.cpp` (never committed). The
branch as committed builds (`Result: Succeeded`) and, by design, registers none of the three animals until R1 is
in: `KeysReefLifeVisitors.cpp` tests for the hooks at compile time, so a turtle is never drawn as a fish.

## Acceptance criteria

- [x] 1 Southern stingray, partly. `southernstingray` (Hypanus americanus), phases female (disc 0.8-1.5 m) and
  male (0.45-0.7 m), on sand, fore-reef grooves and seagrass edges from 3 m down. Lies on the sand, lifts off in
  a cloud of sand, glides off low, settles; digs, leaving a cloud and a pit; wings carry a travelling wave.
  Evidence: `L/sheets/f5_close.png` (top row, at rest), `L/sheets/f1_ray.png` (lift-off strip, dig with cloud
  and pit), `L/log_f1.log` lines `KRReef lift`, `flee`, `dig` (e.g. "left the sand ... because the diver came
  within 3.8 m at 1.5 m/s (it keeps 3.5 m at that speed)"; "a pit 34 cm across left at ...").
  NOT met: it does not read as half buried. By the numbers it lies 3 cm into the bottom
  (`KeysSea::SeafloorHeightAt`), but the seabed as drawn is lower there and the whole disc shows (R5).
  The trigger distance is a choice: a diver finning straight at it sets it off at about 4 m, one drifting in at
  about 1 m; only the fast case was run. With the first setting (2 m for a slow diver) a normal swim set it off
  at 10 m, so nobody would have seen one lying still.
- [x] 2 Green turtle and loggerhead, partly. `greenturtle` (shell 0.8-1.1 m, four costal pairs, small head,
  olive-brown with radiating streaks) and `loggerhead` (shell 0.8-1.0 m, five costal pairs, big yellow-tan head,
  red-brown shell). Front flippers beat together (about a third of a beat a second), hind flippers trail; both
  graze or crush nose-down, go up to breathe (green about every 240 s, loggerhead 300 s; 5 s at the top), sleep
  against the reef at night, ignore a slow diver, bolt from a spear.
  Evidence: `L/sheets/f5_close.png` (bottom row: loggerhead, green), `L/sheets/f1_strip.png` (the green
  turtle's scutes from above, the climb), `L/log_f1.log` `KRReef breath` (two lines: leaves the bottom, took air
  5.0 s), `KRReef crunch: loggerhead ... bit the bottom` (`L/log_v3.log`), `find ... asleep` at 22:00
  (`L/log_f1.log`).
  NOT met: at the surface the turtle is not drawn in the mask view (it is seen going up and coming down, not
  breathing; R6). Not seen from the boat either: the one top-down try shows nothing I can call a turtle.
  Daytime resting under ledges is not built (night sleep beside a coral head is the framework's own).
  Request R2 retires the generic turtle from the reef and channel tables.
- [x] 3 The audit: see "Audit" below. Requests R3, R4. The nurse shark was never caught in a shot.
- [x] 4 Proved, with the gaps named above. Close `reef:look` shots: `L/shots/f5/KRScript_50.png`, `_51` (ray),
  `_52` (loggerhead), `_53` (green turtle). Strips: `L/sheets/f1_ray.png` frames 2-5 (lift-off),
  `L/sheets/f1_strip.png` and `f5_strip.png` (surfacing: the last frames are empty, see R6). `reef:find` lines:
  Sombrero `L/log_f1.log` (all three), Looe Key `L/log_looe.log` (ray and loggerhead) and `L/log_f1.log` (green
  turtle), classic map `L/log_f2.log` (loggerhead only: no ray or green turtle within 84 m of `goto=reef` there;
  the census line dealt 18 green turtles, 20 loggerheads, 3 rays within 300 m).
- [ ] 5 Budget. Caps 5 + 4 + 3 animals at weight 3 = 36 units of 900 (registry line, `L/log_f1.log`). Nothing
  added to `KeysLife`. Near meshes: ray 444 + 2 x 180, green 870 + 2 x 48, loggerhead 894 + 2 x 48 triangles;
  far 30 and 132. Reef-life CPU 0.04-0.07 ms a frame before and after.
  Perf, Medium, `goto=reef`, same script, editor capped at 33.3 ms. Before is this branch's first build (main):
  | | frame | game | draw | gpu |
  |---|---|---|---|---|
  | boat, before (`log_base`) | 33.34 | 11.70 | 6.95 | 31.45 |
  | boat, after (`log_f5`) | 33.34 | 12.29 | 7.56 | 31.60 |
  | dive 6 m, before | 33.34 | 17.05 | 15.23 | 23.17 |
  | dive 6 m, after (two samples) | 33.34 / 33.47 | 17.54 / 17.14 | 15.14 / 15.18 | 24.39 / 25.06 |
  Frame time is the same; the dive's game and draw are within 3%; the boat's game (+5%) and draw (+9%) and the
  dive's GPU (+5 to +8%) are not. One sample each side on a machine other lanes and the user were using: I cannot
  say whether that is my two turtles (two were alive within 60 m) or noise, so this is not ticked. An earlier
  "after" (`log_f1`) had a 124 ms hitch in the boat sample; `log_f4` ran at 3 fps and is worthless. Handed to
  the marshal: an A/B on a quiet machine (R7).
- [x] 6 Nothing else moves. `tpcheck`: real map "checked 48 destinations, 0 bad" (`L/log_f1.log`), classic "19
  destinations, 0 bad" (`L/log_f2.log`). Spear: fired at the loggerhead 2.6 m off, "shaft spent", the turtle
  bolts, nothing speared; then "KRDive speared bullshark 334.5 lb at 2.4 m" (`L/log_f1.log`). Reef-life
  animals are not spear targets at all (the diver's targets come from `KeysLife` only).
- [x] 7 Report on the pattern: see below; all of it is in R1 or R5-R6.

## Audit (criterion 3)

Lengths: `KeysLife` `Len`, schools' range in brackets. Shots are poor: fish brought in front of the mask among
eighty others, mostly backlit. Names and looks are from the catch cards (`L/sheets/f3_cards.png`).

| Species | On the reef | Name | Length | Colour | Behaviour |
|---|---|---|---|---|---|
| Yellowtail snapper | picks 12-13%; school of 9-11 brought (`log_base`, `log_audit2`) | "Yellowtail Snapper" | 0.42 m (0.34-0.42) | blue-grey, yellow line and fins; mutton snapper's model repainted | in schools above the reef: yes (`audit2_b.png` frame 1) |
| Hogfish | picks 5-6% | "Hogfish" | 0.6 m | pink-red, own model | bottom level, never roots: R4 |
| Goliath grouper | picks 0-2%; one brought | "Goliath Grouper" | 2.2 m | mottled olive, grouper's model | roams open reef as well as wrecks; card offers release or tag only; can be speared: R3 |
| Great barracuda | picks 3-4%; one brought | "Great Barracuda" | 1.3 m (0.9-1.3) | silver, dark back | hangs still (schools' Hanger role; `audit2_b.png` frame 3). Card says 32 in for 20 lb: short for that weight |
| Tarpon | 0% in picks at this spot; school of 4-5 brought | "Tarpon" | 1.8 m | silver | mid-water, rolls at the surface; release only by rod; can be speared: R3 |
| Nurse shark | in the reef table; not catchable, so no picks and no card | none shown | 2.2 m | built from the mako's model (log line) | NOT seen in a shot (two tries); by the code it swims all day: R4 |
| Lionfish | picks 3% | "Lionfish" | 0.35 m | orange and white bars | bottom, wanders at 0.4 m/s rather than hovering; spearable, text says invasive, no limit: R4 |

Against the new animals: ray disc 0.45-1.5 m, turtle shells 0.8-1.1 m. Nothing is out of scale with the rest.

## Done
- `Source/KeysRun/KeysReefLifeVisitors.cpp`: the three species, their bodies (`Build`), limbs, traits.
- `.marshal/requests/reefvisitors/R1.patch` to `R4.patch`.

## In progress
- nothing

## Blocked
- nothing; the animals appear only once R1 is applied.

## Requests for the marshal
- R1 (open): `KeysReefLife.h`, `KeysReefLife.cpp` - hooks and fixes; patch `R1.patch` (applies to main, built
  and run by the lane). New `FSpecies` fields: `Build` (own mesh per level of detail; what a December model
  replaces), `Limb` and `LimbWave` (two moving parts placed by the species; a ray's wings bent by the boid
  wave), `bBitesBottom`, `BiteTilt`, `BreathEvery`, `BreathSecs`, `Bury`, `DigEvery`; states "up for air" and
  "digging"; `reef:do breath|lift|dig`; three counters in `reef:stats`. Faults fixed in passing, all of which
  hit every species: (a) a Sitter hung 0.2-0.5 m over its perch (the look-ahead took the bottom under it for
  rock ahead); (b) anything moving straight up or down stood on its nose (the pitch clamp left no heading);
  (c) a Sitter kept whatever pitch it arrived with.
- R2 (open): `KeysLife.cpp` - take `turtle` out of the Reef and Hawk Channel tables (it stays offshore); patch
  `R2.patch`. Compiles; not run.
- R3 (open): `KeysLife.cpp` - release-only species (goliath, tarpon, bonefish, billfish, protected sharks) are
  not spear targets; patch `R3.patch`. Compiles; not run. It changes the marshal's own goliath spear test.
- R4 (open): `KeysLife.cpp` - nurse shark built from its own shape and all but still by day; lionfish speed 0.4
  to 0.12; hogfish gets a rooting act; patch `R4.patch`. Compiles; not run, not looked at.
- R5 (open): the ray is not visibly half buried. Needs the seabed's height as drawn (a `KeysSeafloor` sample)
  for `Bury`, or a sand skirt. No patch: I do not know why the drawn bottom is lower than `SeafloorHeightAt`.
- R6 (open): a turtle within about a metre of the surface is not drawn in the mask view (five strips). Not
  explained: it is 10 cm under the mean level by the log. Whoever owns the underside of the sea should look.
- R7 (open): perf A/B on a quiet machine, boat and dive at `goto=reef` (numbers above).
- The generic stingray stays: it is not in the Reef table at all, and mine starts at 3 m, so the two do not
  meet (the generic one keeps the flats and backcountry).

## Notes for integration
- Pattern report (criterion 7): the recipe assumes a fish. It has no word on a body that is not `FBody`, on
  moving parts, on left and right (a limb mesh is used on both sides by rotation, so it must be the same fore and
  aft: the ray's outer wing and the flippers are), on air breathers, or on anything that rests on the bottom.
  `reef:look` puts the diver level with the animal, which is edge-on for a ray (follow it with `dv:depth`), and
  needs `flip` when the sun is behind. `reef:find` reports height over the 1 m tile, not the bottom as drawn.
  `cam=0,25,3` stops chunks being dealt (finds return "none"). Section 7 of the skill reads
  "MISTAKES_PLACEHOLDER".
- Guesses: ray sizes by sex, the tail at 1.3 disc lengths, breath intervals, densities (2.2, 1.2 and 0.9 groups
  a hectare), a green turtle on the reef top as well as on grass, adults only.
- The breath, dig and lift shots were asked for with `reef:do`; the rules that set them off unasked were seen
  only for the lift (diver) and the flee (spear).
- Scratch output: `L/` only. The hub files are as on main.

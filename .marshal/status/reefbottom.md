# reefbottom

Status: READY FOR MERGE (with R1 applied; see "What merging without R1 gives")
Updated: 2026-11-23 23:40
Branch: lane/reefbottom (this commit)

The split of the user's reef list into the eight criteria below is the marshal's reading, not the user's words.
What I had to guess is listed under "Decisions and guesses".

Evidence lives in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-reefbottom/`: `shots/<tag>/KRScript_NN.png`
and `log_<tag>.log`. Below a shot is written `<tag>/NN`. Every shot cited was opened and looked at.

## What merging without R1 gives

The conch and the anemone cannot be built as fish, so both files compile to nothing until the framework carries the
hooks of R1 (`#if defined(KEYS_REEFLIFE_BOTTOM_HOOKS)`). Without R1 the branch builds and runs (checked: `log_rb_plain.log`,
registry "2 species") and adds only the audit commands. All proof below was taken with R1 applied locally and the hub
files restored before every commit. R2 and R3 were applied only for the run `rb_r2`.

## Acceptance criteria

- [x] 1 Queen conch (needs R1) - `Source/KeysRun/KeysReefLifeBottomConch.cpp`, key `queenconch`, Aliger gigas, shell
  0.20-0.30 m, Sitter on the floor in scatters of 3-12 on sand and seagrass, ocean side only, 1-15 m (full 2-10).
  Still, eye stalks out: `rb4/54` (1.2 m) and `rb4/50`. Mid-hop, shell lifted and pitched, foot under it: `rb4/55`,
  `rb2/52`. From above at 0.3 m, stalks drawn in, spire, body whorl and pink-edged lip: `rb4/52`. Toast
  "Queen conch (Aliger gigas). Protected in Florida: look, don't take." in `rb2/50` and the
  `KRReef first sight: queenconch` line. It is in no catch table and nothing can spear or lift it.
  Limits: a 584-triangle shell that reads as a conch inside about 2 m and as a pale lump beyond; the pink shows on
  the lip's edge and underside only (as in life from above); the shell's banding is too faint to see; no snout.
  `reef:find` never says "hopping" in my logs (the one line taken during a hop was of a neighbour): the hop is shown
  by the shots only.
- [x] 2 Giant Caribbean anemone (needs R1) - `KeysReefLifeBottomAnemone.cpp`, key `giantanemone`, Condylactis
  gigantea, crown 0.15-0.30 m, three colours (white, green, pink; purple or magenta tips), singly against the nearest
  stony head within 3 m, reef zones, 1-15 m. It does sway in the framework: the mesh is built tail-up so the boid
  material's tail wave swings the tentacles, set to the surge's own period, phase and axis. `rb3/57`, `rb2/58`
  (white), `rb_classic/50` (green, against a finger coral), `rb1/53`. Sway: `rb1/53`, `rb1/58`, `rb1/59` are one
  animal from one camera seconds apart, the crown leaning left, upright, right. Drawing in: crown width times
  distance falls from about 215 to 172 px m between `rb2/58` (1.4 m) and `rb2/60` (0.7 m), the 20% the code asks
  for; a measurement off two shots, not a log line. No fish live in it.
  Limits: "against a head" means the edge of the footprint the framework holds for that coral kind; under wide
  corals (elkhorn, pillar) that is up to a metre from the trunk and reads as open sand (`rb3/57`, `rb_perfB/51`).
  The orange column shows as a short cup. Not tucked into crevices: the reef has none to tuck into (reefdens).
- [x] 3 Purple sea fans - checked at Sombrero, `krbottom.reef 150` in `log_rb1.log`. Colour: both kinds read purple
  to lavender (`rb1/60` code-built, `rb1/63` model, `rb_before/51`); no yellow-tan ones. Faults, fixed in R2:
  the modelled fan stands edge-on to the surge (face 80 degrees off square, `rb1/63` taken looking along the reef);
  the code-built fan is 0.85-2.77 m tall (mean 1.67); 79 of 1,358 stand below 15 m, some on the deep wall.
  With R2 (measured at the crest spot, before `log_rb_crest0.log`, after `log_rb_r2.log`): model face 80 to 10
  degrees off (`rb_r2/52`), code-built 0.85-2.77 to 0.60-1.50 m, fans below 15 m 17 to 5.
- [x] 4 Audit of the reef itself - table under "The audit". Fixes in R2 and R3; two things not verified.
- [x] 5 Proved - look shots above. `reef:find` lines: Sombrero `log_rb1.log` (anemone, 4.3 m) and `log_rb4.log`
  (conch, 7.5 m, 384 m inshore of the crest: there is no sand or grass zone within 300 m of the crest itself);
  Molasses Reef `log_rb_perfB.log` (anemone 5.3 m) and `log_rb4.log` (conch 9.1 m, group of 4); classic map
  `log_rb_classic.log` (anemone 2.6 m) and `log_rb_classic2.log` (conch 8.7 m, group of 10). Counts under "Counts".
- [ ] 6 Budget and perf - budget met: caps 36 + 24 = 60 units, Weight 1 each. The 3% is NOT shown: see "Perf" and R5.
- [x] 7 Nothing else moves - `tpcheck` 48, 0 bad (`log_rb_perfB2.log`) and classic 19, 0 bad (`log_rb_classic.log`);
  same corals in the same places: `rb_before/51` (main) against `rb_perfB/50` (same spot and camera); none of mine
  on the Boot Key flat at 1.3 m (`log_rb4.log`, last stats line: 0 individuals) and census rows show zero under
  every zone not listed. Canals: by rule only (HabitatAt gives no zone there, both start at 1 m); no canal was dived.
- [x] 8 Report on the pattern - "The pattern" below, as requests R1 and R4.

## Counts (census, dealt within 300 m)

| Where | Species | Total | 0-3 m | 3-8 m | 8-15 m | 15 m+ | Zones |
|---|---|---|---|---|---|---|---|
| Sombrero crest -1800, 2790 | anemone | 328 | 9 | 211 | 108 | 0 | crest 3, reef top 145, spur top 142, fore reef 38 |
| Sombrero crest | conch | 0 | | | | | no sand or seagrass zone in reach |
| Sombrero back reef -1419, 2714 | conch | 41 in 7 groups | 0 | 18 | 23 | 0 | sand 22, seagrass 19 |
| Molasses Reef 26298, -3038 | conch | 74 in 11 groups | 0 | 58 | 16 | 0 | sand 28, seagrass 46 |
| Molasses Reef 26249, -2960 | anemone | 272 | 11 | 199 | 62 | 0 | crest 5, reef top 141, spur top 88, fore reef 38 |
| Classic map -1444, 1142 | conch | 156 in 22 groups | 0 | 75 | 81 | 0 | sand 53, seagrass 103 |
| Classic map -1355, 1489 | anemone | 240 | 8 | 165 | 67 | 0 | crest 2, reef top 124, spur top 71, patch 26, fore reef 17 |

The census over-counts anemones: it cannot know which have no stony head within 3 m. At Sombrero 15 were dealt within
60 m and 13 were alive.

## Perf (Medium, editor, frame capped at 33.3 ms in every sample)

| Run | Boat game / draw / GPU ms | Dive 6 m game / draw / GPU ms |
|---|---|---|
| main, `rb_before` | 12.95 / 7.91 / 31.59 | 18.28 / 16.11 / 24.05 |
| this build, `-KRNoReefBottom`, `rb_perfA` | 11.99 / 7.25 / 30.62 | 17.22 / 15.16 / 24.54 |
| this build, mine on, `rb_perfB` | 15.86 / 11.46 / 32.08 (a 76 ms frame in it) | 17.03 / 13.87 / 25.77 |
| `rb_perfA2` | 11.27 / 6.59 / 31.08 | 16.26 / 16.49 / 23.47 |
| `rb_perfB2` | 11.62 / 7.02 / 30.94 | 16.88 / 14.59 / 25.52 |

Three samples with none of my animals differ by 15% in game time from the boat (11.27 to 12.95), so 3% cannot be
read off this machine while other lanes build and run. The second pair is +3.1% game from the boat and +3.8% game,
-11.5% draw, +8.7% GPU in the dive: inside that scatter, not proof. What is measured: reef life's own timer goes
from 0.067 to 0.075 ms a frame with 16 anemones added (150 to 166 individuals, 8,728 to 12,280 triangles).
A still animal costs about 0.0005 ms a frame, the same as a fish (it is posed each step like any other), and up
close as many triangles (anemone 912, conch 584 + two 55-triangle eye stalks; far 84 and 72). So a unit is a unit:
Weight 1, no discount for sitting still.

## The audit (Sombrero unless said; numbers from `krbottom.reef 150`, `log_rb1.log`)

| Item | There | Placed | Looks | Verdict |
|---|---|---|---|---|
| Elkhorn | code-built 539, model 112 | 2.4-8.1 m, mean 5.1; none above 2.4 m, 57% below 5 m | code-built: broad flat boards, rust-orange (`rb1/64`); model: branching, tan (`rb1/65`) | too deep and off the crest; code-built colour wrong. R2 (tan), R3 (crest) |
| Staghorn | 1,242 and 374 | 2.4-15.1 m | model: thin thicket (`rb1/66` centre); code-built: thick clubbed fingers (`rb1/66` left) | placed right; the code-built shape is not a thin thicket (no patch) |
| Brain coral | 378 and 274 | 2.8-18 m | model: grey dome with grooves (`rb2/61` background); code-built: smooth brown boulder, no grooves, to 3 m wide (`rb2/61` foreground) | domed yes; grooved only the model (no patch) |
| Barrel sponge | 40 and 73 | 10-18.6 m, fore reef and deep wall | model ribbed, reads grey-brown at 16 m (`rb2/61`, `rb2/62`); model 1.0-1.99 m, code-built to 1.67 m | right place; too tall. R2 (to 1.5 m) |
| Turtle grass | Hawk Channel: 190 model tufts and 8 code-built within 150 m, 4.6-8 m (`log_rb3.log`); Boot Key flat 1.3 m | on the Sombrero crest too: 900 code-built tufts at 1.6-2.6 m (`rb_crest0/50`) | flat strap blades (`rb3/60`, `rb3/66`), single tufts, not beds | grass on the reef crest is wrong: R3. Sparse in Hawk Channel, and none at all on seagrass-coloured bottom seaward of DZ 1250 (-1386, 2744: none within 120 m) |
| Mangrove roots | from the boat: prop roots under the crowns off Boot Key (`rb3/62`) | | fish over the grass in front of them: `rb4/59`, `rb4/60` | from the diver: NOT verified. Six tries up to 92 m in from the boat found grass and fish and no root in view (`rb_crest0/54`) |

Named: none of these is named anywhere in the game (no label, toast or logbook entry for a coral). No patch: it is
HUD or logbook work.

Also seen, not on my list: the lime-green encrusting mounds (2,068 within 150 m) are the loudest thing on the reef;
a dark flat-sided block in shallow water (`rb_r2/52`, left), which looks like a far-detail card.

## Done

- `KeysReefLifeBottomConch.cpp`, `KeysReefLifeBottomAnemone.cpp`: the two species, each with its reference block.
- `KeysReefLifeBottomAudit.cpp`: console commands `krbottom.reef [m]`, `krbottom.look <kind> [m] [bearing] [nth]`,
  `krbottom.goto <zone> [least] [greatest]`, `krbottom.ground <species key>` (reached as `con=...`). Read only.
- `-KRNoReefBottom`: the same build without my two species (the registry logs two "left out" warnings).

## In progress

- nothing

## Blocked

- nothing

## Requests for the marshal

- R1 (open): `KeysReefLife.h`, `KeysReefLife.cpp` - bottom-animal hooks, without which neither species exists.
  `FSpecies::Mesh` (own mesh for each level of detail and the twice-drawn part), `Pose` (own transform, part
  transforms and material floats each step), `Where` (own say in suitability: ocean side, not the bay), `bNoBolt`,
  `bOnFloor`, `HeadWithin`; and one fault: a Sitter's hop was held within Roam of the group's home, so every member
  of a scatter wider than Roam hopped to the same point (now its own ground, `FAnimal::Own`). Fish are untouched:
  150 parrotfish and tang alive at the same spot with and without (`log_rb_perfA2.log`, `log_rb_perfB2.log`). Patch: `.marshal/requests/reefbottom/R1.patch`.
  `Mesh` is also where a December model goes in without touching behaviour.
- R2 (open): `KeysReef.cpp` - sea fan model turned a quarter turn to stand across the surge; code-built fans
  0.6-1.5 m; fans thinned out from 13 m to none at 16 m; barrel sponges to 1.5 m; code-built elkhorn tan instead of
  rust. Tested (`log_rb_r2.log`, `rb_r2/52`). It moves nothing but re-deals some model barrels and deep fans.
  Patch: `.marshal/requests/reefbottom/R2.patch`. reefdens is editing this file: the hunks are small.
- R3 (open, your call): `KeysReef.cpp` `ReefZone` - the reef line is reef from 1 m down instead of 2.6 m. At
  Sombrero: seagrass on the crest 900 to 0, elkhorn from 1.7 m (52 on the crest, was 7), frame times unchanged
  (crest boat game 11.49 to 11.33). Before `rb_crest0/50`, after `rb_r2/50`. It changes what grows on every
  reef-line shallow on the map, I looked at one, and the corals there are cut flat to stay under the surface and
  look crude. Patch: `.marshal/requests/reefbottom/R3.patch` (applies with or without R2).
- R4 (open, no patch): the pattern, see below.
- R5 (open): the 3% perf check on a quiet machine or packaged build, with and without `-KRNoReefBottom`.
- R6 (open): mangrove prop roots from the diver's view, and a canal dive, were not verified by this lane.

## The pattern (R4)

- The framework could only draw fish. Everything else in this lane follows from that (R1).
- The recipe's section 7, "Mistakes already made", is the text `MISTAKES_PLACEHOLDER`.
- Sitter had never been used: it sat on coral tops only, hopped to the group's home, and `reef:find` reports a
  Sitter lying still as "roving". `reef:do bite` is how a Sitter is made to hop; the name does not say so.
- The registry demands a `Paint` even when the species brings its own mesh.
- `reef:census <m>` and `reef:find` do not say where a species is: at Sombrero and on the classic map `find`
  returned "none within 84 m" three times with 34 to 170 conch dealt within 300 m. `krbottom.ground <key>` was
  written for that; it belongs in the framework as `reef:ground`.
- `HabitatAt` and `KeysReef` disagree on where the reef starts: between DZ 1100 and 1250 the framework says "patch
  reef" where KeysReef plants turtle grass and no coral (all 190 tufts in Hawk Channel stood in "patch reef"), and
  seaward of 1250 it says "seagrass" where KeysReef plants none.
- The head table gives one radius per coral kind, so "at the foot of a head" is rough for wide-topped corals.
- `reef:look` on a bottom animal: the diver cannot go lower than 0.6 m over the floor, so under about 0.7 m it is
  a view from above. Fine for these two.
- `shotclean` leaves the diver's gauge panel and toasts in the shot; shots asked for less than about a second
  apart are sometimes dropped (`rb1`: 18 of 20), and the file numbers do not match the script's seconds.

## Decisions and guesses

- Both species are left out entirely without R1 rather than drawn as fish-shaped stand-ins.
- Conch: ocean side only (DZ 120 to about 2000), grass at full density, bare sand at 0.8 beside the reef and 0.3 in
  Hawk Channel; 2.2 scatters a hectare; a hop about every 70 s, 0.12 m/s in 1.15 s lurches; awake day and night;
  eye stalks in inside 0.8 m, out past 1.6 m; adults only (no juvenile "rollers").
- Anemone: 14 a hectare of full reef cover, stony heads only, never moves; draws in inside 1.3 m, fully at 0.55 m;
  the three colours dealt 45 / 30 / 25.
- Weight 1 each on the measurement above; caps 36 and 24 to fill the 60 units given.
- Scientific name Aliger gigas, as the brief gives it (older books: Strombus or Lobatus gigas).
- The fan depth limit (none below 16 m) and barrel height (1.5 m) in R2 follow the brief's figures; in life both
  run a little past them.

## Notes for integration

- `KeysReefLifeBottomAudit.cpp` repeats KeysReef's two species tables (order, sizes) for its report; if reefdens
  reorders them the report mislabels, nothing else.
- The unpatched build of this branch was run on both maps: `log_rb_plain.log`, `log_rb_plain_classic.log`
  (registry 2 species, `tpcheck` 48 and 19, 0 bad).
- After the last run with R1 applied, one line of `KeysReefLifeBottomAudit.cpp` (`krbottom.ground`'s use of
  `FSpecies::Where`) was put behind the R1 macro, because the unpatched build did not compile without it. The
  unpatched build was rebuilt and run after that; the R1-applied build was NOT rebuilt after it.
- My first unpatched build failed and my script went on to launch the game anyway, which sat on a rebuild prompt
  and held the game lock for about five minutes (23:13 to 23:18) until I killed that process, my own.

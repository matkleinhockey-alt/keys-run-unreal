# reeffish

Status: IN PROGRESS (stopped here: what is left needs the marshal or a quiet machine; see "Not met")
Updated: 2026-11-23 23:30
Branch: lane/reeffish at the commit that carries this file (species code last changed at 3eddd08)

The split of the user's reef list into the criteria below is the marshal's reading, not the user's own words.
The user's words: "Setting: the Florida Keys reef tract. Use only species found there." and "i just want to keep
it realistic". What this lane had to guess is under "Guesses".

Evidence: `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-reeffish/` (`log_<tag>.log`,
`shots/<tag>/KRScript_NN.png`). Every shot named below was opened and read against the reference block at the
top of the species' file. Tags `final_real`, `final_classic`, `perf_after`, `perf_after2` ran the committed code.
Tags `sombrero`, `molasses`, `classic` ran an earlier commit (same shapes and behaviour; paint and densities
changed a little after). Tags `r1trial`, `r1nest` ran with request patches applied locally.

## Acceptance criteria
- [x] 1 rainbow and midnight parrotfish - keys `rainbowparrot`, `midnightparrot`, role Grazer, in none of the
  catch or spear tables. Shots: sombrero/50 (rainbow: bronze head and shoulders, green rear with bronze-edged
  scales, green fins, blue-green beak), sombrero/52 and final_real/50 (midnight: navy, blue-green beak; the
  bright blue head patches show on the lit side in sombrero/52 and are hard to make out on the shaded side in
  final_real/50). Grazing: `KRReef crunch: rainbowparrot ...` and `... midnightparrot ...` and `KRReef puff:`
  lines in log_sombrero.log. Sizes: `reef:find` gives rainbow 0.54-0.89 m, midnight 0.59-0.73 m, stoplight
  terminal 0.44 m. Not done: young rainbows in mangroves; midnights travelling with tangs (no link between
  species in the framework).
- [ ] 2 queen, French and gray angelfish - keys `queenangel`, `frenchangel`, `grayangel`, role Pair (the
  framework had it). Shots: final_real/51 (queen: blue scales edged yellow, yellow face and tail, dark crown
  spot on the nape), final_real/52 (French: black, yellow scale marks, yellow eye ring), sombrero/57 (gray:
  grey, dark-centred scales, pale tail margin, square tail), sombrero/56 (a French pair). Unafraid: after
  `reef:look off` a gray stayed 1.1 m from the diver (`find grayangel` in log_sombrero.log). NOT MET in the
  committed code: "long trailing dorsal and anal fins" (the body builder only makes a rounded hump: R1 fixes it,
  shots r1trial/50, r1trial/52, r1nest/52); "nipping at sponges" (they nip at coral rock, the framework's bite:
  R4); the crown's blue ring is thin. Retiring the generic angelfish is R2.
- [ ] 3 sergeant major - keys `sergeantmajor` (Hoverer, groups of 10-40) and `sergeantmajorguard` (Territory,
  one dark blue male). Shots: look1/55 and sombrero/59 (five bars, yellow back), look1/57 (a group hanging over
  the heads), sombrero/62 and molasses/54 (the guard). Dropping when scared: 2.40 m over the reef before, 0.70 m
  after the diver closed to 1.5 m (two `find sergeantmajor` lines in log_sombrero.log, shot sombrero/61);
  `KRReef flee: sergeantmajor ... 30 of its group went with it` in log_classic.log. NOT MET: no purple egg
  patch (R3); the guard's chase was never seen in about 12 minutes of runs (R5); groups are not tied to coral
  heads, wrecks or mooring lines (R6).
- [x] 4 proof - side-on shots as above. `reef:find` lines for all seven keys at Sombrero, French Reef
  (27340, -3340) and Molasses Reef in log_final_real.log and on the classic map in log_final_classic.log
  (Grecian Rocks has no sergeant group in range, Looe Key's crest no midnights: lines in the same log). Counts
  by depth band: the `KRReef census` lines in the same logs (300 m at Sombrero: rainbow 3 / 58 / 8 / 0, midnight
  5 / 190 / 85 / 9, queen 0 / 22 / 36 / 14, French 9 / 43 / 31 / 0, gray 4 / 66 / 22 / 13, sergeant major
  17 / 649 / 35 / 0, guard 4 / 27 / 5 / 0 for 0-3 / 3-8 / 8-15 / 15 m+). Zero under sand, seagrass and hard
  bottom in every census (log_sombrero.log, log_molasses.log). Sizes, as a table (m): sergeant major 0.12-0.18;
  blue tang 0.15-0.25; stoplight 0.20-0.30 initial, 0.35-0.45 terminal; French 0.30-0.40; queen 0.30-0.45; gray
  0.35-0.50; midnight 0.50-0.75; rainbow 0.50-0.90; the diver about 1.8 (not measured). No line-up shot: there
  is no hook to stand animals side by side (R7).
- [ ] 5 budget - caps: rainbow 10 x 2, midnight 16 x 2, queen 8, French 10, gray 10, sergeant major 100, guard
  5 = 185 units of 250 (registry line: caps sum to 365, density scale 1.00). `reef:stats` at goto=reef from the
  boat: before 150 individuals, 150 units, 8,564 triangles, cpu 0.06-0.07 ms; after 236 individuals, 261
  units, 13,900 triangles, cpu 0.09-0.10 ms (log_perf_before.log, log_perf_after.log). Meshes: 932 / 220 / 56
  triangles, fin 12 (`KRReef mesh` lines). Frame times, Medium, 5 s samples, before = the same worktree built
  without this lane's three files:
  - boat: before game 12.23, draw 7.52, GPU 30.92 (2 samples; the first had a 243 ms hitch and is left out);
    after game 11.90, draw 7.17, GPU 31.05 (5 samples over 2 runs): -2.7%, -4.7%, +0.4%.
  - dive at 6 m: before game 16.79, draw 15.56, GPU 22.48 (3 samples, one run, GPU 21.55 to 23.65); after game
    16.90, draw 15.11, GPU 24.20 (6 samples, two runs, 23.20 to 24.62): +0.7%, -2.9%, **+7.6% GPU, over the
    3% limit**.
  NOT MET for GPU in a dive. One baseline run only, and other lanes were queuing games all evening, so the
  number is not settled either way; earlier runs of `main` alone read 22.4 to 26.1 at this spot (MARSHAL.md).
- [ ] 6 nothing else moves - `tpcheck` 48 destinations, 0 bad (log_final_real.log); classic 19, 0 bad
  (log_final_classic.log). Spear, real map: `KRDive speared mutton 6.0 lb` (log_sombrero.log), with R2 applied
  `speared goliath 141.0 lb` (log_r1trial.log); `GetTargets` in KeysDiver.cpp does not read reef life at all,
  so none of mine can be picked. NOT MET on the classic map: the shot missed twice (`aimfish: ... nearest
  3.3 m` / `3.5 m`, no `speared` line), as it did for the marshal on `main`. Away from reefs: census 300 at the
  Boot Key flat and in Hawk Channel deals 0 groups (log_molasses.log).
- [x] 7 report on the pattern - requests R1-R9 below.

## Not met, in one place
1. Angelfish trailing fins and sponge nips (framework: R1, R4).
2. Egg patch; the guard's chase never observed (R3, R5).
3. Dive GPU +7.6% on one baseline run (limit 3%).
4. Spear test did not land on the classic map.

## Done
- `Source/KeysRun/KeysReefLifeFishParrots.cpp`, `KeysReefLifeFishAngels.cpp`, `KeysReefLifeFishSergeant.cpp`
- `.marshal/requests/reeffish/R1.patch`, `R2.patch`
- Night: `KRReef sleep:` and `wake:` lines for rainbow, midnight, French and the guard (log_sombrero.log).

## Blocked
- nothing that this lane can do in its own files

## Requests for the marshal
- R1 (open): `KeysReefLife.h`, `KeysReefLifeBody.cpp`, `KeysReefLife.cpp` plus two of my files - `FBody::DorTrail`,
  `AnTrail` (a fin that rises to the rear with its tip drawn out past the base) and `FSpecies::bHomeOnHead`
  (home is the top of the nearest coral head within 8 m), set for the three angelfish and the guard - patch:
  .marshal/requests/reeffish/R1.patch. Both effects were seen in runs (r1trial/50, r1trial/52, r1nest/50,
  r1nest/52) of a build that also carried the egg-patch and sponge trials; the patch as saved was built but
  not run.
- R2 (open): `KeysLife.cpp` - drop `{"angelfish", 2}` from the Reef table - patch: R2.patch. Run once (r1trial).
- R3 (open): the egg patch. Tried twice as a quad in `Fx` (M_KR_Bubble), flat and facing the camera, purple, on
  the head under the guard: nothing shows. It needs a small mesh or decal and an opaque material (binary,
  marshal-only), placed at the group's home when `bHomeOnHead` is set.
- R4 (open): sponge nips. Tried `bNipsSponge` (PickBite takes tile Kind 2 only): two asked bites found no sponge
  within reach. Kind 2 mixes sponges with wrecks, and PickGoal does not lead a group to sponges.
- R5 (open): a test hook that sends an animal into a Territory holder's ground (as `reef:rival` does for males),
  and dealing a Territory species beside a named Hoverer species. The rule fired for the stoplight male in the
  same run but never for a guard, at 2 m or 2.5 m.
- R6 (open): Hoverer homes on a coral head; wrecks and mooring lines as habitat (HabitatAt knows neither).
- R7 (open): `reef:lineup <keys>`: one of each side by side at a fixed distance, for the size shot.
- R8 (open): shadows. Every near and middle pool casts shadows; a `bNoShadow` trait for fish under 20 cm (100
  sergeant majors here) is the first thing to try if the dive GPU cost is confirmed.
- R9 (open): the recipe (`.claude/skills/reef-creature/SKILL.md`): section 7 is the literal text
  `MISTAKES_PLACEHOLDER`; `FBody::Arch` is missing from section 3; a phase name with a space cannot be passed
  to `reef:look` or `reef:find`; `reef:look` can put the diver under a wreck or ledge (molasses/51) and often
  on the shaded side, where a dark fish reads black; `Weight` says 1 up to 50 cm and 3 over a metre and nothing
  between; `reef:find` only sees what is alive round the boat and the census gives no positions, so a species
  missing at a site can only be found by trying other sites; the "refused by a species' own cap" count is never
  reset after a teleport; the fin paint has two rows only (root, edge), which the recipe should say.

## Guesses and my own decisions
- Rainbow and midnight get Weight 2 (the rule has no value for 50-90 cm). Neither sleeps in a cocoon.
- Densities are set for the game, not from survey data: every kind is meant to turn up within 60 m of most reef
  dives. Rainbow 2.5 groups a hectare of 2-4, midnight 5 of 3-6 (at 3 it was absent at three of four sites).
- Sergeant major cap 100, under the recipe's "densest census plus a quarter" (113 at Looe Key), to stay in 250.
- The guarding male is a second registry key with the common name "Sergeant major (male guarding eggs)".
- Queen 65% alone, 35% pairs; French 85% pairs; gray 80% pairs. Shy: queen 1.5 m, French 1.1, gray 0.9.
- Angelfish row with their pectoral fins and nip at rock every 14 s or so, standing in for sponge feeding.
- "Protected in Florida" on the two parrotfish toasts is taken from the brief, not checked against the rules.
- The other named reef is French Reef (all seven present); Molasses also has all seven.

## Notes for integration
- Caps now sum to 365 of 900 with the two species already on `main`. `MaxBaitFish` was cut by 180 to pay for
  those two; nothing was cut for these 185.
- The queen and French bodies are `FPhase::Body` and `Paint` only; a model can replace them without touching
  behaviour.

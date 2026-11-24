# reefreal

Status: READY FOR MERGE
Updated: 2026-11-24 09:30
Branch: lane/reefreal (see `git log`; the commit that adds this file is the last)

The acceptance criteria are the marshal's reading of the user's words ("a realism cleanup but dont get rid of the
black marlin"; "i just want to keep it realistic"), not the user's own list. What I had to guess is under
"Decisions I made on my own".

Evidence lives in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-reefreal/` (below: `L/`).
Tags starting `b_` are the clean branch before any change, `a_` after. The patches R1-R4 were applied locally for
the `a_save`, `a_picks`, `a_classic`, `a_board`, `a_cards` and `a_flo` runs and are not committed.

## Acceptance criteria
- [x] 1. The audit - `.marshal/requests/reefreal/audit.md`: every kind in `KeysData`, `KeysLife`, `KeysSchools`,
  `KeysReefLife*`, `KeysReef`, `KeysMangroveEco*`, the birds and the Abyssal Garden, with verdicts. No outside
  lookup was made; the verdicts are from my own knowledge of the species, which is the audit's limit.
- [x] 2. Halibut retired - R1 (patch, not applied on the branch). Evidence:
  - hooked: 0 of 18 `KRPicks` lines name it after (`L/log_a_picks.log`, `L/log_a_classic.log`); before, it was in
    every Hawk Channel and Offshore line (`L/log_b_picks.log`).
  - listed: `L/shots/a_board/KRScript_00.png`: "You have caught 4 of 55 species" and, last in the list,
    "Atlantic Halibut (retired) 212.0 lb" (a player whose save holds none does not get the row).
  - save: the test save was written by the clean build with a 212 lb halibut catch and a halibut mounted at
    `pigeon-key` (`L/log_b_save.log`: `KRCatch logged halibut 212.0 lb ... written to the save: yes`); loaded by
    the patched build three times with 0 lines of `Error:`, `Fatal` or `Ensure` and
    `KRFree best halibut 212.0 lb` (`L/log_a_save.log`).
  - tables: Hawk Channel 32.4 and Offshore 28.35 in `ZoneTable`, Offshore 33.465 in `LifeZoneLife`, each kept by
    scaling the rows left by total / (total - halibut): 1.0125, 1.0366, 1.034. No row crosses the two
    thresholds that read absolute weights (2.0 in `TourneyPool`, 2.5 in `PickSpecies`).
  - NOT verified: that it is no longer SEEN (its row is out of `LifeZoneLife`; no shot can show an absence), and
    the mounted halibut on the wall (two shots at `estgo=pigeon-key` show the bridge and the island from far
    off, not the mount: `L/shots/a_board/KRScript_50.png`, `L/shots/a_cards/KRScript_51.png`). By the code the
    mount still draws: `Species("halibut")` still answers and its look and shape rows stay.
- [x] 3. Flounder made local - R2 (patch). `L/shots/a_cards/KRScript_00.png`: "GULF FLOUNDER, 3.0 lb, 19 in", the
  new text, a sand-brown fish with dark eye-spots on the flank it shows. Same key (`KRFree best flounder 5.0 lb`
  survives from the clean build's save). NOT verified: the count of three spots (the fish is 60 pixels long in
  the card shot; two are plain, the third cannot be told), and the flounder swimming (three were spawned,
  `L/log_a_flo.log`, none came into the five shots).
- [x] 4. No kelp in the Abyssal Garden - `Source/KeysRun/KeysGarden.cpp`, `.h` (committed). Before:
  `L/shots/b_shots/KRScript_50-55.png` (170 kelp stalks). After: `L/shots/a_shots/KRScript_50-56.png` (270
  colonies on 36 ledges, swaying with the kelp's own motion). Handed to the marshal and the user: **it is not as
  dramatic as the kelp was**, in my judgment. See "Notes for integration".
- [x] 5. Other plain errors - R3 (card length by species; barracuda 20 lb now reads 45 in, was 32:
  `L/shots/b_save/KRScript_00.png`, `L/shots/a_cards/KRScript_02.png`), R4 (hogchoker's size and "true sole" text,
  `L/shots/a_cards/KRScript_01.png`; Spanish mackerel text). Doubtful ones are listed in the audit, not changed.
- [x] 6. Nothing else moves - `tpcheck` 48 destinations, 0 bad (real map) and 19, 0 bad (`-KRClassicMap`), before
  and after (`L/log_b_picks.log`, `L/log_a_picks.log`, `L/log_b_classic.log`, `L/log_a_classic.log`). Picks in
  Flats, Backcountry, Hawk Channel, Reef and Offshore on the real map and Hawk Channel, Offshore, Reef and Flats
  on the classic map: same fish, shares within the noise of 400 draws. Perf below.

## Perf at the Abyssal Garden (editor build, Medium, 1600x900; the garden change only, no patch applied)

| | frame ms | game | draw | gpu | worst |
|---|---|---|---|---|---|
| from the boat, before (2 runs) | 33.34, 33.34 | 17.87, 17.86 | 6.71, 6.86 | 30.79, 30.88 | 33.4, 33.4 |
| from the boat, after (2 runs) | 33.34, 34.06 | 16.51, 18.04 | 7.12, 7.62 | 30.12, 30.46 | 33.4, 140.7 |
| diving in the thickets, before | 33.63, 33.67 | 23.12, 23.33 | 10.57, 11.27 | 22.56, 22.37 | 76.8, 82.8 |
| diving in the thickets, after | 23.91, 28.75 | 23.77, 22.08 | 14.52, 11.53 | 20.60, 21.74 | 72.6, 60.2 |
| Sombrero reef (not affected), before | 33.34, 33.34 | 11.98, 11.50 | 7.19, 6.58 | 31.55, 31.37 | |
| Sombrero reef, after | 33.34, 33.34 | 11.75, 12.44 | 6.94, 7.67 | 30.90, 31.46 | |

Lines: `KRPerf` in `L/log_b_perf1.log`, `log_b_perf2.log`, `log_a_perf1.log`, `log_a_perf2.log`.
Read: mean frame from the boat 33.34 -> 33.70 ms (+1.1%, inside 3%); game thread unchanged; the swaying mesh is
42,468 triangles rebuilt 30 times a second against about 43,000 for the kelp (170 stalks). Two things to know:
(a) one of the two after-samples from the boat (and one sample of an earlier version) holds a single frame of
140-150 ms, none before: the static mesh grew from 40,782 to 100,006 triangles and is built on arrival, so I
take it for a one-off hitch as the site comes in, not a steady cost; not proven. (b) the dive samples ran
uncapped after (24-29 ms) and capped before (33.6 ms); I do not know why the cap differed, so compare the game
and draw columns, not the frame. Draw thread: 10.9 -> 13.0 ms mean diving, but the two after-samples differ by
3 ms from each other. A packaged-build check is the marshal's.

## Done
- `Source/KeysRun/KeysGarden.cpp`, `KeysGarden.h`: kelp out; ledges, seven kinds of swaying colony, large
  sponges; barrel sponges recoloured from purple and orange to red-brown and grey.
- `.marshal/requests/reefreal/audit.md`, `R1.patch` to `R4.patch`, `all.patch`.

## In progress
- nothing

## Blocked
- nothing

## Requests for the marshal
Apply in order R1, R2, R3, R4 (each is a diff against the tree with the earlier ones applied; R2 and R4 touch
lines next to R1's), or `all.patch` for the four at once (checked: both ways apply to the branch and give the
same tree, which builds: `Result: Succeeded`).
- R1 (open): `KeysData.h/.cpp`, `KeysLife.cpp`, `KeysHUD.cpp`, `KeysBoatPawn.cpp` - halibut retired:
  `FSpecies::bRetired`, `RetiredKeys()`, `SpeciesKeys()` without it, its rows out of the three tables with the
  rest scaled to keep the totals, the species list shows a retired fish only when the save holds one. Also in
  it, because the check needed them: a test command `board=N` (opens the leaderboard on tab N; there was no
  scripted way), and the species list's rows fitted to the panel (at 31 U apiece the last rows of both columns
  ran off the panel's foot, under the hint line: `L/shots/a_save/KRScript_00.png` shows it, with the halibut row
  off the screen) - patch: `.marshal/requests/reefreal/R1.patch`
- R2 (open): `KeysData.cpp`, `KeysLife.cpp` - gulf flounder: name, text, 0.5-6 lb, 0.38 m, new pattern `Ocelli`,
  flatfish painted by their sides, and a flatfish's roll turned over (-90 degrees) so the eyed flank is the one
  up in the water and the one the catch card shows - patch: `R2.patch`
- R3 (open): `KeysData.h/.cpp`, `KeysBoatPawn.cpp` - `KeysData::LengthInches(Key, Lb)`, one figure per species,
  used for the card only (`CardInches`); the fish models' sizes are not touched - patch: `R3.patch`
- R4 (open): `KeysData.cpp`, `KeysLife.cpp` - "Hogchoker" (not "Hogchoker Sole"), 0.1-0.5 lb (was 0.4-4), 0.15 m
  (was 0.26), text no longer calls it a true sole; Spanish mackerel text "no stripe down the side" (was "no
  bars") - patch: `R4.patch`

## Decisions I made on my own
- Halibut: the row stays in the species map, flagged retired, instead of being deleted, so old saves keep a name,
  a colour and a shape for it. The test command `catch=halibut` therefore still works; no table can hook one.
- "The zone's total and the other fishes' shares stay as they were" cannot both hold to the letter once a row is
  gone: I kept the total and the others' shares of each other (every other fish, the black marlin included,
  gains its part of the halibut's 1.2% in Hawk Channel and 3.5% offshore).
- Black marlin: no field, text or table row of it is edited, and R3 leaves it on the old length figure (500 lb
  still reads 93 in: `L/shots/a_cards/KRScript_04.png`), though the old figure is short for a marlin.
- Gulf flounder weights 0.5-6 lb (was 1-12); points per pound left at 18, so a flounder is worth about half what
  it was. A mounted flounder over 6 lb in a save is clamped to 6 lb by the estate code; a record over 6 lb stays.
- The hogchoker counted as "plainly wrong" for its size and text, and was kept as a species although it is a
  mainland fish more than a Keys one (the lined sole would be the local sole). At 0.1-0.5 lb it scores 2-11
  points a fish.
- R3 changes only the number printed on the card, not the size of any model.
- Left alone though doubtful (all in the audit): albacore seen much more often than its own text allows; humpback,
  sperm whale, whale shark, giant manta, great white, mola, bigeye; the anhinga; the warsaw's "fifty years"; the
  garden's three mantas and its magenta glow; the gold bluefin; the "Oil Rig" zone; the boat named "JUST FOR THE
  HALIBUT"; `KeysFeed.cpp` still lists the halibut's feeding habit (harmless, and it gives the retired row its
  "morning").
- In the garden: ledges were added (rock, not a living thing) so that plants 1-4 m tall have some height; the
  site's layout east of them (ship, fluke, arch, sponges, fans, glow, mantas, light shafts, specks) is unchanged.
- The audit is one file in my requests folder rather than in this status file, for length.

## Notes for integration
- **The garden is less dramatic than it was.** The kelp filled the water from the sand to near the surface;
  nothing that lives on a Keys reef does. The tallest thing now is a wire coral on a two-tier ledge, about 6 m
  up in 9.7 m of water, and from mid-water the west half reads as low outcrops with bushes on them
  (`a_shots/KRScript_54.png` against `b_shots/KRScript_53.png`). The ledges are plain rounded blocks and look it.
  If the user wants the old drama back, it needs art I did not make (real rock shapes, or a wreck or pinnacle
  on that side), not more gorgonians.
- The before and after shots were taken an hour and a half apart and the light differs (the game follows the
  clock): the after shots are greener and darker for that reason too.
- A mistake of mine, no harm found: I moved a file named `build1.log` out of the shared scratchpad thinking it was
  mine; it was being written by another lane's build as well (same name). I put a copy back within minutes.
- Two of my own waiting scripts hung for about half an hour early on (they matched their own command line); no
  game or build was affected.
- `Saved/SaveGames/KeysRunTest.sav` in this worktree now holds halibut, flounder, hogchoker, barracuda and
  sailfish catches and a halibut mount at `pigeon-key`: useful for the marshal's own check with `-KRTestSave`.

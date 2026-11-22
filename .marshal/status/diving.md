# diving

Status: READY FOR MERGE
Updated: 2026-11-22 15:10 (second agent on this lane; the first was stopped early and its uncommitted work was read,
kept where sound and carried on)
Branch: lane/diving

The user's words were only: "can you work on the diving aspect and add a scuba tank and gear", and later, with a
picture, "use this to build the harpoon better". The eleven criteria below are the marshal's reading of that
(`main:.marshal/prompts/diving.md`), not the user's. What was guessed on top of the brief is under "Decisions and
guesses".

Evidence is in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-diving/`, called `L/` below:
`L/shots/<tag>/KRScript_NN.png` and `L/log_<tag>.log`. Every scuba run was made with requests R1-R4 applied in the
working tree (never committed) and the gear granted in memory with R1's `own=` test command; a scripted run writes
no save. The lane's own files also build and run with the hub files untouched: the last build before this
commit, and `L/log_nopatch.log` (a free dive, a fish speared, `KRTp checked 48 destinations, 0 bad`, no errors).

Two criteria are not ticked as passed but handed on: 9 (the frame-time comparison is inside the noise of a shared
machine, see R6) and the hub halves of 3, 4 and 5 (requests R1-R5, with patches).

## Acceptance criteria

- [x] 1 The scuba set, worn - `L/shots/s3/KRScript_57.png` right side (cylinder in two cam bands, valve, first
  stage, primary hose to the regulator in the mouth, yellow octopus, jacket with pocket and shoulder strap, waist
  band, pony bottle as bail-out, dive light, fin pockets), `_59.png` left side (inflator hose with its red button,
  the two-dial console at the hip, weight belt, mask strap), `_60.png` from above and behind (cylinder, cam
  buckles, the wrist computer on the left arm, weight belt), `_61.png` three-quarters. Without the set:
  `L/shots/g6/KRScript_55.png` and `_56.png` (the same diver as on `main`, plus the gun of criterion 11 in her
  hand). Gauge glanced at in the dive-mask view: `L/shots/s3/KRScript_00.png` (needle at 2,940 psi, depth, minutes
  down, minutes left).
- [x] 2 Scuba rules - `L/log_rules.log`: `KRScuba gas: 120 s at 5.0 m ... 139 psi used`, `KRScuba gas: 120 s at
  25.0 m ... 323 psi used`, `KRScuba hover: 20 s with no input from 5.00 m: vertical drift 0.008 m` and `from
  25.00 m: vertical drift 0.013 m`, and the `KRScuba rule:` table (full cylinder: 64.9 min resting at the surface,
  50.0 at 3 m, 43.3 at 5 m, 32.5 at 10 m, 18.6 at 25 m, 16.2 at 30 m; finning 47.7 / 36.7 / 31.8 / 23.9 / 13.6 /
  11.9). Reserve, critical, bail-out and out-of-gas: `L/log_s6.log` (`event: critical`, `event: bail-out`,
  `event: out of gas`, then the breath running down 76% -> 72% -> 68% and the swim up). Safety stop asked and
  done, and its reward on the next dive (`gas rate x0.90`); fast ascent warned and paid for (`event: fast ascent`,
  `event: bent`, then no sprint): `L/log_s4b.log`. Free-dive air trace against `main`: `L/log_base_free.log`
  (clean `main` build of this worktree) and `L/log_free_after.log`, same script: 95 94 85 84 78 72 66 65 59 55
  against 95 94 85 84 78 72 66 65 58 54 (the last two differ by one point; the second run was running a frame
  slower, 29.0 against 30.0 fps).
- [x] 3 Bubbles and sound - mid-exhale in the chase view: `L/shots/s3/KRScript_57.png` and `_59.png`; from the
  boat's side with the froth where they break: `L/shots/s7/KRScript_51.png` (also `_50`, `_52`). Nothing leaves
  the regulator on the breath in (the rule is in `UpdateBubbles`; not shown by a shot). The sound is R5: played by
  the diver through the audio actor's public synth, described there. Not listened to.
- [x] 4 Getting it and keeping it filled - with R1-R4 applied: `L/log_rules.log` (gear granted, dive, 780 psi
  used, `back aboard: cylinder 1 at 2220 psi`), `L/log_sc2.log` (`enter: ... cylinder 1 at 2399 psi` on the
  second dive of the run), `L/log_s6.log` (shop refusals, scuba / breath chosen, the swap at the surface, the
  pony taken as bail-out, both cylinders empty, the fill, the second fill refused). The gear page with the scuba
  rows and the low-gas warnings on the HUD: `L/shots/ui/`. The shop's rows are R2, its counter R1, the gauge R3,
  the loadout page R4. Not done by buying at the counter on foot: see "Not verified".
- [x] 5 On the boat - rack with the rigged set and the spare before the dive: `L/shots/s3/KRScript_50.png`;
  during the dive, set gone and spare left: `_52.png`; the flag: `_53.png` and `_54.png`. No hub patch was needed
  (see Decisions).
- [x] 6 Something to dive for - three of the five, in the lane's own files. Fish that let a scuba diver closer:
  `L/log_s3.log` `KRScuba fish: swimming 0.21 m/s, the fish see 0.09 m/s (scuba 1)` (`KeysLife` flees from the
  speed it is told; a diver on gas who is not sprinting is told 45% of hers, so a normal fin stroke no longer
  clears the reef). The dive light: `L/shots/deep_scuba2/KRScript_50.png` (off) and `_51.png` (on), at 30 m. The
  logbook: `L/log_s4b.log` `KRScuba log: Dive 1 - Cargo steamer - 27.0 m - 2:44 - 360 psi used - safety stop
  made`, and on the gear page. Lobster and the chart's dive-site markers with mooring balls were not done and no
  request is written for them.
- [x] 7 Free-diving and spearfishing unharmed - spear test on scuba: `L/log_s3.log` `KRDive speared mutton 13.7
  lb`; off scuba: `L/log_g6.log` `speared goliath 180.6 lb`, `L/log_g2.log` `speared mutton 5.2 lb`. Air trace as
  in 2.
- [x] 8 Both maps - classic map on scuba: `L/log_classic.log`, `L/log_classic3.log` (`speared goliath`),
  `L/shots/classic/KRScript_50.png`; classic free dive: `L/log_classic4.log` (two grouper speared). Errors in
  every log: 0 beyond the engine's own start-up test lines; 26 warnings in each, the same count as the clean-`main`
  baseline log.
- [ ] 9 No slowdown - not shown to 3%: handed on as R6. What was measured (Medium, 1600x900, reef, chase view, one
  session, scuba / free / scuba, `L/log_x.log`): game 17.75 ms mean of five scuba samples against 17.17 of three
  free (+3.4%; +2.2% without the first sample, which holds one 104 ms frame), draw 16.37 against 15.66 (+4.5%, the
  scuba samples alone range 14.5 to 18.1), GPU 23.84 against 24.07 (-1%). The clean-`main` baseline of the same
  dive: game 17.20, draw 18.34, GPU 27.87 (`L/log_base_free.log`). The diver actor's own frame, timed with the
  `dv:prof=1` hook: 0.22 to 0.25 ms on scuba and 0.22 to 0.23 ms free (`L/log_prof.log`). Triangles the set adds
  to the diver: 5,060 (set), 5,160 (with the light), 5,852 with the light and the pony bottle (`KRScuba enter:`
  lines; limit 6,000). Bubbles: the 420 pool that was there, no second pool; the froth at the surface is at most
  48 quads.
- [x] 10 Nothing else moves - `KRTp checked 48 destinations, 0 bad` (`L/log_x.log`, `L/log_perf_after.log`),
  classic `19 destinations, 0 bad` (`L/log_classic4.log`). With no cylinder owned nothing is built on the boat:
  no `KRScuba rack` line in `L/log_free_after.log` or `L/log_g6.log`.
- [x] 11 The harpoon - first person loaded `L/shots/g6/KRScript_50.png`, just fired `_51.png`, the new shaft
  being fed in `_52.png` and `_53.png`, chase view `_55.png` and `_56.png`, the tip in a speared fish `_57.png`
  and `_58.png` (crop: `L/shots/harpoon_tip_in_fish_crop.png`), the picture beside the game's gun
  `L/shots/harpoon_side_by_side.png` (the game's half is `g6/KRScript_54.png`, the same first-person mesh held
  out by the `dv:gunshow=1` test hook with the hands hidden). Spear test as in 7. There is no "mid-draw" to
  shoot: see Decisions.

## Done

- `KeysScuba.h/.cpp` (new): the cylinder's rules (psi, 3,000 full, 0.77 psi/s resting at the surface times
  absolute pressure times 1 + 0.8 x exertion), reserve at 500 and critical at 200, the bail-out bottle, the sprint
  that lasts 7 s, the safe and the bolting ascent, the nitrogen load, the safety stop, what a bend or a clean stop
  leaves for the next dive, the shop's rows and words, the cylinders' bookkeeping (in the pawn's `Counts`), the
  scuba / breath choice, the loadout rows, the logbook.
- `KeysScubaGear.cpp` (new): the worn set, the gauge console and its live face, the cockpit rack, the flag.
- `KeysScubaSpeargun.h/.cpp` (new): the roller gun's meshes (gun, reel, band pairs, wishbone, shaft and barbed
  head, leader, the carried gun, the shaft through a fish).
- `KeysDive.h/.cpp`: three switches on the diver's state (`bOnGas`, `bNeutral`, `AscendCap`), all off on a breath.
- `KeysDiver.h/.cpp`: the scuba dive round the same fixed step, the kit, gauge glance, regulator bubbles and the
  froth at the surface, the regulator's sound, the dive light, the rack and flag riding on the boat, the logbook
  line, the new gun and its leader, a fill light for the gun, the gun in her hand in the chase view, test hooks.

## In progress

- Nothing.

## Blocked

- Nothing.

## Requests for the marshal

- R1 (open, needed): `Source/KeysRun/KeysBoatPawn.cpp` - without it nobody can own, choose or work the set. Test
  commands `own=<effect>`, `disown=`, `scubashop=<effect>`, `scubamode`; in `ToggleDive` the pony bottle is not
  armed for the breath hold on a scuba dive (the diver spends its fill only if she switches to it) and the
  overboard toast says what the keys do; in the water G held lifts the gauge, T works the dive light, X swaps
  cylinders at the boat; on the boat 2 chooses scuba or a breath; `ShopBuy` asks `KeysScuba::ShopRefuse` before
  charging and `KeysScuba::ShopResolve` after. Patch: `.marshal/requests/diving/R1.patch`.
- R2 (open, needed): `Source/KeysRun/KeysTownData.cpp` - Scuba Steve's shelf takes its scuba rows from
  `KeysScuba::ShopRows()` (set $7,500; cylinder $1,200; jacket $2,400; regulators $3,200; computer $1,600; second
  cylinder $1,200; dive light $380; fill $18). Patch: `R2.patch`.
- R3 (open, needed): `Source/KeysRun/KeysHUD.cpp` - on scuba the panel shows the cylinder's psi and bar in place
  of "Air", and with the computer a line of minutes left, dive time, deepest point and the stop's countdown.
  Without it the old air bar shows the cylinder as a percentage. Patch: `R3.patch`.
- R4 (open, wanted): `Source/KeysRun/KeysHUD2.cpp` - the gear page lists `KeysScuba::LoadoutRows`. Patch:
  `R4.patch`.
- R5 (open, optional): `KeysAudio` - nothing is needed for the regulator to sound; `R5.md` says what is
  synthesised and the one change that would tidy it (a flag on `SetUnderwater`).
- R6 (open, needed before a release): the frame-time comparison of criterion 9 on a quiet machine or the packaged
  bookmarks. Script: `3:play;4:gfx=1;5:own=dive.scuba.set;6:goto=reef;9:dive;10:dv:chase;11:sink=4;16:dv:hold=4;
  17:dv:look=0,0;22:perf=dive_after;...` against the same without `own=`. Also look for one frame of 90 to 120 ms
  seen 13 to 20 s into the first scuba dive of a session in five scuba samples and in no free-dive sample; it is
  not inside `AKeysDiver::Tick` (the `prof` hook's 150-frame averages never rose) and its cause was not found.
- No request: the save (the set rides in `Owned`, the cylinders and the logbook's totals in `Counts`, both saved
  already) and the boats (see Decisions).

## Notes for integration

- New test hooks, all through the existing `dv:` route: see the comment above `AKeysDiver::DebugCmd` in
  `KeysDiver.h`.
- The rack and flag find the boat's mesh by its component name, `Boat`. If that name changes they stay at the
  diver actor's origin; `KeysScuba::BuildBoatRack` / `BuildFlag` are there to be called from `BuildBoat` instead.
- `KeysDiver.cpp` renamed two locals (`Sc` -> `Sch`, `FishSc`) because the new field `Sc` shadowed them.
- The dive at the Cargo steamer wreck ran at 18 fps in one sample and 48 to 54 fps in the others, scuba or not:
  the machine was shared with two other lanes' builds all afternoon.

## Not verified

- The regulator's sound was never heard.
- Buying at Scuba Steve's counter on foot, and the four keys as pressed: only `scubashop=` / `scubamode` and the
  functions behind them were run.
- A real save round trip (by rule, nothing was written).
- The rack on any boat but the one the save has; a boat with a short cockpit may put it somewhere odd.
- Night, a packaged build, and frame time to 3%.
- The bail-out bottle run to empty, a bad bend, a blackout after running out of gas.
- The site names in the logbook away from Marathon.

## Decisions and guesses

- One unit, psi; the numbers above. Near the surface a resting cylinder is 65 min, 48 min finning: the brief's 45
  to 60 sits between them. At 30 m it is a third of the 3 m figure.
- On scuba she swims at 1.0 against the freediver's 1.6, sprints for 7 s and needs 8 s to get half of it back.
- Space comes up at 0.3 m/s; with Shift it is 1.6 m/s and counts against her. A bend takes the sprint, costs 30%
  more gas (50% for a bad one), throbs at the edges of the view, and lasts four minutes on the surface.
- The safety stop is asked for on the way up past 10 m after 18 m or a loaded dive, is one minute between 3 and
  6.5 m, and is never forced. Done cleanly, the next dive breathes 10% easier.
- A wetsuit saves 6% of the gas.
- The pony bottle's fill is spent only when she has to switch to it.
- Keys: G gauge, T light, X swap, 2 scuba or breath. Names, words and prices of the shop's rows are invented.
- The rack and flag are the diver actor's own components attached to the boat's mesh, so no boat file changes.
- The regulator's sound is played from the diver's file through `AKeysAudio::Burst`, so no audio file changes.
- Criterion 6: fish, light and logbook; not lobster, not the chart's markers and mooring balls.
- The logbook also takes a line for a free dive (2 m and 8 s or more), and writes its three totals into `Counts`.
- The dive light comes on by itself below 14 m or with the sun low.
- Harpoon: the barrel stays 78 cm so the aim, the muzzle and the animation are where they were; the picture's gun
  is shorter and stockier. The reel is kept though the picture has none, because the brief keeps its animation.
  `main`'s rules have no band draw (`StepReload` loads the gun when the new shaft is home), so the "mid-draw"
  shot is the shaft being fed in. A gun was added to her hand in the chase view where there was none, which
  changes how the free diver looks there. The gun has a small fill light of its own.
- The new gun files are named `KeysScubaSpeargun.*` to stay inside the owned pattern.

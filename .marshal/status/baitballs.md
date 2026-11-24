# baitballs

Status: IN PROGRESS (handed back: 1-3 and 5 offshore pass; open items below)
Updated: 2026-11-24 evening
Branch: lane/baitballs (see git log for the commit)

The criteria are the marshal's reading of the user's "can you do more baitballs but with bigger fish in the deep
water"; what I had to guess is under "Guesses".

Logs and shots: `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-baitballs/` (`log_<tag>.log`,
`shots/<tag>/`). Runs t1-t4, perf_before2, perf_after, perfab, classic and fin were made with R1 and R2 applied
locally (not committed).

## Acceptance criteria
- [x] 1. More bait balls out deep - log_t1.log: `KRBait census: a ball within 300 m at 288 of 300 points in deep water (96%), 74 of 150 over the reef (49%)`; five offshore spots 1-3 balls each, Hawk Channel and a flat 0 (`KRBait census hawk ... 0 balls`, `census inshore ... 0 balls`). Bait: ballyhoo, Spanish sardine, round scad (repainted barracuda, herring and blue runner bodies), flying fish that glide when pushed (KeysLife's own model). Reef balls are half size with 1-2 predator groups (log_fin.log: `put a sardine ball (123 fish) ... in 8 m of water, ... 1 of 1 predator groups`). Budget: fish alive stay well inside MaxFish (most seen 3570 of 5200 at Sombrero, log_fin.log; 515-915 offshore). Paid for by: the Offshore ambient row no longer deals herring, silverside, blue runner and jack schools (inshore and reef fish); offshore fish alive fell from ~3000 to 500-900 between balls. The deep balls make room with the existing `bMakeRoom` rule (bait for bait, furthest first).
- [x] 2. Big fish working them - Worker role: blackfin and skipjack packs circle below and charge up through the ball, mahi at its depth, 1-3 sailfish herding close with the sail up and slashing, kingfish and wahoo long runs from out wide, now and then a silky or a dolphin pod. The ball's work builds while predators are on it and it rises from 6-14 m to just under the surface (log_t4.log: reef-edge ball #59 `9.6 under, work 0.48` then `2.8 under, work 0.84`), mills tight, showers (`27-30 in the air`), flying fish glide off. Terns (plunge-diving) and frigatebirds (soaring high) circle a worked ball, instanced. Shots: shots/t3/KRScript_53.png (ball with tuna and mahi from the mask, Gulf Stream), shots/t3/KRScript_55.png (two sailfish, sails up, herding sardines), shots/t4/KRScript_55.png (dolphin pod and blackfin off the reef edge), shots/t4/KRScript_00.png (terns over a worked ball beside the boat). Predators are KeysSchools instances, nothing added to KeysLife's mesh.
- [x] 3. It is fishing - with R1: log_t3.log `KRPicks at -1090, 4667 zone Offshore depth 100.2 m: skipjack 24% blackfin 22% mahi 20% kingfish 13% sailfish 8% wahoo 2% ...` (before, same spot: `mahi 12% marlin 12% sailfish 11% wahoo 10% ...`, log_perf_before.log). A worked ball is a hot spot (frenzy when crashed) for the bite timer. Sounder (R2) built and fed, NOT looked at in a shot. Nervous water and charge splashes go through `AKeysLife::Splash` (budgeted 8 a second, within 300 m).
- [ ] 4. Proved - have: worked ball from the mask in the Gulf Stream (t3/53) and off the reef (t4/55), sailfish herding (t3/55), birds over a ball from the boat (t4/00), census line, picks beside a worked ball. MISSING: a clear shot from the boat of a shower at the surface with birds; the splashes and the jumping bait are too small to read in the chase view at 25-40 m (t3/50, t4/50 show nothing of the ball). The census is one log line per spot (5 offshore, 2 reef, 2 inshore), from the deal, not five teleports.
- [ ] 5. No slowdown - offshore trolling passes; bait_ball is borderline. Same run, deep balls on/off (`fish:deepoff`), log_perfab.log: offshore on game 12.95/14.17, draw 7.79/7.62, GPU 25.53/24.54 against off 14.00/14.35, 7.18/7.83, 27.37/26.34 (no cost; fewer fish than main). bait_ball on game 20.76/19.44, draw 19.81/20.08, GPU 19.85/20.06 against off 17.62/18.68, 17.64/19.62, 17.40/19.60 (first off sample disturbed: worst 98.9 ms). Against main (log_perf_before2.log, game 19.47/18.75, draw 19.40/19.68, GPU 19.75/19.54): game +4%, draw +2%, GPU +1-3%. The user's packaged game (KeysRun-Mac-Shipping) was running through every run, load average ~7: numbers are noisy. Fish alive at the bait_ball scene 3472-3487 against 3510 on main.
- [ ] 6. Nothing else moves - `tpcheck` real map `KRTp checked 48 destinations, 0 bad` (log_perf_after.log), classic `KRTp checked 19 destinations, 0 bad` (log_classic.log). Reef schools as before (Reef, Hawk Channel, Flats, Creek, Bridge, Backcountry rows untouched; Sombrero 55-62 schools, 3470-3570 fish against 60/3510 on main); mangrove flat `tp=-1150,452,4.71` 31 schools, 1435 fish (no main number taken). SPEAR TEST NOT PASSED in my runs: `dv:fishahead=grouper,5; dv:aimfish; dv:spear` fired (`aimfish: 85 targets, nearest 4.9 m`, `shaft spent`) but no `KRDive speared` line (log_fin.log); the shot shows no fish ahead. My code does not touch spear targets (KeysLife), but I have no main run of the same script to compare.

## Done
- `KeysBait.h/.cpp` (new): the published ball list, hub hooks `PickAt`, `WorkedAt`, `SonarNear`; bodies for ballyhoo, sardine, scad, silky (repainted from KeysLife models), tern and frigatebird (built, two-sided).
- `KeysSchools.*`: 12 species (ballyhoo, sardine, scad, flyingfish; blackfin, skipjack, mahi, sailfish, kingfish, wahoo, silky, dolphin; role `Worker`); deep sites dealt on a 170 m grid every ~8 minutes per cell (deep water 26%, reef 7 m+ 7%, nothing outside the Reef and Offshore zones), put in the water within 165 m, published to 420 m; predators circle and charge; work, rise, mill, showers, flying-fish glides, nervous water and charge splashes, birds (two instanced pools, 96 terns, 16 frigatebirds, left alone when none are drawn), sounder samples; the grid key widened to 5 bits of species.
- Test commands (`fish:`): deep, census, balls, ballhere=<m>[,all|tuna|sail], toball[=m], lookpred=<key>, deepoff, deepon; `-KRNoBaitBalls` command-line switch.

## Requests for the marshal
- R1 (open): `KeysLife.cpp` - include `KeysBait.h`; in `SpotAt` a worked ball counts as a hot spot (bFrenzy when crashed); in `PickSpecies` first ask `KeysBait::PickAt` (at a worked ball 50-90% of bites are the fish working it) - patch: .marshal/requests/baitballs/R1.patch
- R2 (open): `KeysHUD.cpp` - include `KeysBait.h`; the sounder also takes `KeysBait::SonarNear` contacts - patch: .marshal/requests/baitballs/R2.patch
- R3 (ask): run the spear test on `main` with the same script as log_fin.log to see whether the miss is the lane's or the test's.

## Notes for integration
- Without R1 and R2 the lane builds and runs; fishing and the sounder simply ignore the balls.
- Offshore is quieter between balls than before (500-900 fish alive against ~3000): the herring and silverside balls were not Gulf Stream fish.
- Seen in shots but not fixed: from below some predators at a distance read as flat dark sheets (low detail level seen edge-on); dolphins use the fish tail-beat with its amplitude cut to a fifth (a cetacean beats up and down); the sail is always up.
- Sailfish group size rounds up to 4 (existing `NMax + .99` rounding in Spawn).

## Guesses
- "Deep water" = over 30 m in the Reef or Offshore zone; "over the reef" = Reef zone 7-30 m; "inshore of Hawk Channel's middle" = no balls in Hawk Channel at all.
- Predator odds per ball (blackfin 70%, mahi 45%, skipjack 40%, sailfish 30%, kingfish 30%, wahoo 20%, silky 12%, dolphins 10%, at most four groups) and group sizes are my reading of Keys charter reports, not data.
- Bites: at a worked ball 50% + 40% x work of bites come from its predators, weighted by the square root of their number (sailfish x0.5, wahoo x0.7, mahi x1.3); silky and dolphins never bite.
- Terns stand for royal/sandwich terns; offshore sooty terns and brown noddies were not modelled.

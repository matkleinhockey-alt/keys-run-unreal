# docks

Status: READY FOR MERGE
Updated: 2026-11-21 evening
Branch: lane/docks (see `git log -1` on the branch)

The user never wrote this lane's brief. The acceptance criteria are the marshal's reading of "lifts and docks"; what I
guessed on top of that is under "Guesses".

**Read this first: the criteria pass, the goal does not.** The brief's goal is shores that "look lived on". The rule
"at least 1 m of water at the head at low tide" leaves 26 docks on the whole Phase 1 map (936 conch houses). The map's
shores are flats: of the 503 houses within 55 m of the water, only 77 have a metre within 45 m of their shore at low
tide, and mangrove, plat lots, channels and the rest take most of those. Nothing short of a decision changes that; the
options are under "Notes for integration".

Evidence is in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-docks/` (`shots/<tag>/KRScript_NN.png`,
`log_<tag>.log`). Every run had R1 and R2 applied in the working tree (not committed). Without R1 nothing of this lane
runs. All the runs below are from the final code (plan checksum `803dd7c5`).

## Acceptance criteria
- [x] 1. Placement - `log_accept.log`:
  `KRDocks per island: Duck Key 1, Grassy Key 15, Marathon (Vaca Key) 10` (the names are the map's own nearest-island
  names). `KRDocks audit: 26 docks; standing on - marked channel 0, creek 0, marina 0, pier 0, estate 0, plat lot 0,
  Cowfish Cove 0, mangrove 0, town 0, another dock 0; heads with under 1.0 m at low tide 0 (least 1.01 m); docks with
  no house behind 0 (furthest house 54 m); the two nearest docks are 33.1 m apart`. The audit is a second pass over
  every half metre of every dock, measured against the things themselves, not the planner's margins.
  Top-down (`cam=0,25,3` and closer at 1.5), tag `accept`: Marathon `_52`, Grassy Key basin `_55`, Grassy Key east `_58`.
  Same docks every run: `checksum 803dd7c5` in `log_accept.log`, `log_idle.log`, `log_perf_after3.log`, `log_perf_after4.log`.
- [x] 2. Docks - `KRDocks kinds: T 13, L 7, finger 2, platform 4 | new pine 7, weathered 7, stained 7, composite 5`.
  Day, from the boat: `accept/_50` (Marathon), `_53` (Grassy Key basin), `_56` (Grassy Key east, two 77 m docks).
  After `sunset`: `accept/_72` (lamp lit on the head), `_74` (lamp, and the green of an underwater light).
  Fittings seen in the shots: cleats, ladder with hoops, dock box, light on a post / bollard / gooseneck, fish table,
  bench (`_74`), handrail (`_61`), pelican (`_59`), gull (`_61`). Kayak racks and trap stacks are built but I did not
  catch one in a shot.
- [x] 3. Lifts - `cradle 16, elevator 3, davits 4, no lift 3 | hung 17, afloat 4, empty up 0, empty down 2`.
  Close shots, tag `accept`: cradle hung `_59`, cradle lowered empty `_61`, elevator hung `_62`, davits hung `_64`.
  Tide pair on one dock (hung skiff and a boat afloat alongside): `_66` at `tide=-1`, `_67` at `tide=1`; the hung boat
  is clear of the water at high tide, the boat alongside rides down and up. A second pair on a lowered lift with its
  boat afloat was shot (`_68`, `_69`) but I only looked at the same pair from the previous build, so do not count it.
  Not in the final list: an empty lift left up (the deal gave none in 26 docks; `accept/_60` is therefore a repeat of
  `_59` and shows nothing).
- [x] 4. The fleet matches - every boat is `KeysYardProps::MakeBoat` and `BuildBoat` (`KeysLifts::PickBoat`,
  `KeysDocks.cpp` for the boats alongside); no hull code in this lane. Names and makers are the yards' invented ones,
  listed per dock in the `KRDock N ...` lines of `log_accept.log`.
- [x] 5. The player is not trapped - `log_idle.log`, `dk:pos` lines: a 260 m pass at 9 kn past the three Marathon
  docks, 19 to 59 m off the shore (23 m at the docks), steady progress, no "Hard aground" or "Beached" line in the log.
  **The player can drive through a dock**: nothing of this lane has collision (hub matter). `KeysDocks::InFootprint(X, Z)`
  is there for the hub to ask. This run's third leg was aimed through a dock and did not happen to log a sample inside
  one; the same leg on the previous build did.
  **Canal: none on this map.** `dk:canals` (`log_survey2.log`) finds no residential canal. The only water with a bank
  close on both hands is the mangrove creeks, bridge channels, and one cut on Grassy Key (2440, 420) that is 17 m at
  its narrowest and has 0.28 m at low tide 100 m in. The idle run's second leg goes down a bridge channel (105 m, no
  stop), which proves nothing about docks: no dock stands in any of these.
- [x] 6. Classic map - `log_classic.log`: `KRDocks: no docks on this map (0 houses looked at)`, no error lines beyond
  the engine's own start-up ones.
- [x] 7. No slowdown - numbers below. Tiles of 600 m; fittings draw to 380 m; `dk:stats` offshore reads
  `awake tiles 0` (`log_accept.log`).

## Perf (editor build, Medium, 1600x900; `-KRNoDocks` = before)
| Spot | Before | After |
|---|---|---|
| dock shore `tp=3736,159,1.33` (pair 3) | 37.61 ms | 37.96 ms (+0.9%) |
| dock shore (pair 4) | 37.48 ms | 37.79 ms (+0.8%) |
| far `tp=-1150,452,4.71` (pair 3) | 34.08 ms | 33.33 ms (-2.2%) |
| far (pair 4) | 33.33 ms | 34.60 ms (+3.8%) |

`log_perf_before3/4.log`, `log_perf_after3/4.log`. The far spot swings 2.2% between two identical "before" runs, so its
+3.8% is inside the noise; its mean over both pairs is +0.8%. Two earlier pairs on this machine were useless (19 to 27
fps, "after" faster than "before") and are not reported. The dock shore measured is the Grassy Key basin (3 docks in
view), not the Marathon shore the final list ranks first; both have 3 docks.
Total: 306,746 triangles in 86 meshes (17 tiles): structure 62,650 (drawn to 2.6 km), hung boats 108,856 (1.1 km),
fittings 31,592 (380 m), afloat 103,264 (1.1 km), lights 384. Nine point lights without shadows, on only at dusk and
only near the camera. Packaged-build bookmarks are the marshal's.

## Done
- `KeysDocks.cpp/.h`: the plan (from `KeysGeo`, plus `AKeysEstates::PlatParcelAt`, `KeysTown`, `KeysYards`), the audit,
  docks and fittings, tiles, tide for what floats, lamps, two people, test hooks (`dk:`).
- `KeysLifts.cpp/.h`: cradle lift, elevator lift, davits, in four states.

## In progress
- Nothing.

## Blocked
- Nothing.

## Requests for the marshal
- R1 (open): `KeysGameMode.cpp`, `KeysBoatPawn.cpp` - spawn `AKeysDocks`; route `dk:` script commands to
  `AKeysDocks::DebugCmd`. Without it the lane does nothing. patch: .marshal/requests/docks/R1.patch
- R2 (open): `KeysWorld.cpp` - `BuildScenery` keeps palms and the shore bushes off the docks
  (`KeysDocks::InFootprint(JX, JZ, 4)`). patch: .marshal/requests/docks/R2.patch

## Guesses
- A house owns a dock only if it stands within 55 m of the water and the shore lies in front of it.
- Walkways run up to 80 m to reach the metre of water (mean 36 m, two of 77 m). Capped at 60 m there are fewer docks.
- The head needs 1.0 m at low tide; a lift's berth and a boat alongside need 0.75 m.
- A dock may not reach more than five eighths of the way across a basin, nor stand where mangroves wade in front of it.
- **Elevator lifts stand on a dock's edge, not on a seawall.** The brief asks for "a few elevator lifts on a seawall".
  The map's bottom shoals to 0.35 m at every shoreline, so no seawall has a metre of water; there are no seawalls.
- Kinds are dealt round the list rather than rolled, so a short list has every shape, finish and lift.
- Underwater lights are real green point lights at dusk (three at most, nearest the camera), not glowing geometry.
- "One or two docks with a person": 1 fishing and 2 hosing on the map; only the two nearest within 260 m are drawn.
- What floats rides the tide but does not bob on the waves (one mesh per tile).

## Notes for integration
- **More docks needs a decision.** `KRDocks survey` in `log_accept.log`: shore houses with water within 45 m at low
  tide - 0.4 m deep: 155, 0.6 m: 122, 0.8 m: 98, 1.0 m: 77. `NeedDepth` and `BerthDepth` at the top of `KeysDocks.cpp`
  are the two numbers. Dropping the head to 0.6 m would roughly add half again, not fill the shores. Filling them
  would take dredged water off the built-up shores in `KeysGeo` (as the yards' cove was), which changes the flats and
  is not this lane's to ask for without the user.
- The house positions are a copy of `BuildScenery`'s dice (`Houses()` in `KeysDocks.cpp`). If the hub changes how it
  places conch houses, docks will stand in front of nothing; `docks with no house behind` in the audit line catches it.
  A house list exported by `AKeysWorld` would be sounder.
- `upperkeys`: the plan reads the map only through `KeysGeo`, so docks should appear on new islands; not tested there.
- Seen, not mine: conch houses overhang the water's edge in places (`accept/_59`); the hub's dark shore bushes sit on
  beaches beside the docks; the map's island names put most of Fat Deer and Crawl Key under "Grassy Key".
- `shotclean` does not hide the HUD from a `dk:look` / `dk:eye` camera.

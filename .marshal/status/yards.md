# yards

Status: READY FOR MERGE
Updated: 2026-11-21 evening
Branch: lane/yards (see `git log -1` on the branch)

Evidence lives in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-yards/` (`shots/<tag>/KRScript_NN.png`,
`log_<tag>.log`). All runs were made with R1, R2 and R3 applied in the working tree (they are not committed). Without
R1 nothing of this lane runs at all.

The site is Cowfish Cove (an invented name): the small cove on the Gulf side just east of Keys Fisheries, dry stack on
the west bank, boatyard on the east bank, fuel dock and dockmaster's office across the head. Game x -306..-186,
z -116..-46. It is over 350 m from Sister Creek and Whiskey Creek (the marshal's note).

## Acceptance criteria
- [x] 1. A site - dry stack `tp=-243,-98,1.57`, boatyard `tp=-243,-90,-1.57` (also logged as `KRYard site ...`). Top-down
  `shots/site/KRScript_51.png` and `_53.png` (`cam=0,25,1`): nothing overlapping. Idle pass in and out of the cove:
  `log_idle.log` has no "aground" or "Beached" line (`shots/idle/KRScript_51.png` is the boat inside the cove at 6 kn).
  Needs R2 (dredged basin) and R3 (palms and houses kept off); see Requests.
- [x] 2. Dry stack - 40 bunks on 4 levels, 30 boats, 10 empty; 9 skiffs, 10 bay boats, 11 centre consoles (log line
  `KRYard dry stack: ...`). Day: `shots/lift/KRScript_50.png` (rack, apron, well), `shots/idle/KRScript_51.png` (fuel
  dock, pumps, office, chase view), `shots/site/KRScript_51.png` (top-down). Night: `shots/site/KRScript_55.png`,
  `shots/lift/KRScript_57.png`. Note: `clock=21:00` only changes the hour the fish feed by; the scene is darkened with
  the `sunset` command, which is the darkest the game gets.
- [x] 3. Forklift - `shots/lift/KRScript_50..55.png` from a fixed camera (`yd:eye=2,-1,197,-9,7`) at script seconds
  22, 36, 50, 64, 78, 92 after `yd:cycle` at second 8: forks raised at the bay, backing out with the boat, lowering to
  carry height, turning on the well row, lowering over the bulkhead, the boat afloat and walked to the float with the
  slot empty. `log_lift.log` `KRYardLift` lines (twice a second) show position, heading and fork height changing
  continuously. Not shot: the reverse (fetch) cycle. It runs on its own about 26 s after a launch and uses the same
  steps; `log_y2.log`-style logging (`yd:log`) will show it.
- [x] 4. Boatyard - 9 boats on stands (3 sanded, 3 shrink-wrapped, the sportfisher with outboards off) plus one in the
  travel lift (log line `KRYard boatyard: ...`). Standing-height shots (`yd:eye`), tag `yard` unless noted:
  `_50` sportfisher, a sanded hull, gravel; `_51` painter, paint buckets, shrink-wrapped hulls; `_52` sander on a
  sanded bottom; `_53` pressure washer with spray, FOR SALE sign behind; `_54` travel lift over the slip with the boat
  in its slings; `_55` FOR SALE skiff; `_57` stacked jack stands and keel blocks; `_59` chase view;
  `yard2/_50` sportfisher's bare transom with her outboards on stands; `yard2/_51` trailer. Old outboards on stands
  are also along the south fence (`_55`, left).
- [x] 5. No new hulls, no real brands - every boat is `KeysMesh::BuildHullLoft` with different proportions
  (`KeysYardProps::HullOf`). Makers (Reefwright, Tidewell, Gumbo Limbo, Cowfish, Buttonwood, Hardbottom), boat names,
  "Keylift" and "Cowfish Cove" are invented.
- [x] 6. Classic map - `log_classic.log`: `KRYard: no yards on this map`, no Error/Fatal lines;
  `shots/classic/KRScript_50.png`, `_51.png`.
- [ ] 7. No slowdown - partly met, numbers below. The yards cost nothing measurable, but the yard spot is not "at or
  under" the Keys Fisheries spot, and was not before the yards existed either.

## Perf (editor build, Medium, 1600x900; `-KRNoYards` = before)
| Spot | Before | After |
|---|---|---|
| far, `tp=-1150,452,4.71` | 36.15 ms | 36.57 ms (+1.2%) |
| yard, facing the dry stack `tp=-243,-98,1.57` | 37.82 ms | 37.45 ms |
| yard, facing the boatyard `tp=-243,-90,-1.57` | 35.11 ms | 35.59 ms (+1.4%) |
| Keys Fisheries `tp=-300,-178,3.14` | 34.00 ms | 34.09 ms |

`log_perf_before.log`, `log_perf_after.log` (`KRPerf`). All within noise. The machine was busy with other lanes while
these ran (about 27 fps everywhere), so treat them as a comparison only; the packaged bookmarks are still owed.
The yards are 333,000 triangles (boats 247,000) in 14 meshes drawn to 3.5 km (buildings), 1.5 km (boats, forklift) and
450 m (small props); about 6,000 triangles a boat, mostly the shared hull loft and outboards. Nothing ticks or rebuilds
beyond 600 m: `log_idle.log`, two `yd:stats` 10 s apart at the far spot both read `awake 0` with the forklift frozen.

## Done
- `KeysYards.cpp/.h`: the site, made ground, rack building, launch well, fuel dock, office, boatyard, workers, tick.
- `KeysForklift.cpp/.h`: the forklift's script of moves (launch, fetch) and its mesh.
- `KeysYardProps.cpp/.h`: the fleet, shrink wrap, stands and blocks, travel lift, trailer, outboards on stands, clutter.

## In progress
- Nothing.

## Blocked
- Nothing.

## Requests for the marshal
- R1 (open): `KeysGameMode.cpp`, `KeysBoatPawn.cpp` - spawn `AKeysYards` and route `yd:` script commands to
  `AKeysYards::DebugCmd`. Without it the lane does nothing. patch: .marshal/requests/yards/R1.patch
- R2 (open): `KeysGeo.cpp` - `LandH` and `RealDepthAt` ask `KeysYards::Ground`: made ground under the pads, the cove
  dredged to 2.6 m and the slip to 2.4 m (it is 0.3 to 1.6 m now, too thin for the launch well and for the player to
  come in). patch: .marshal/requests/yards/R2.patch
- R3 (open): `KeysWorld.cpp` - `BuildScenery` skips palms and conch houses inside `KeysYards::InFootprint`.
  patch: .marshal/requests/yards/R3.patch
- R4 (open, for the mangroves lane's file): `KeysMangrove.cpp` should skip cells inside `KeysYards::InFootprint(CX, CZ, 6)`.
  There is no baked mangrove on the site today, so nothing shows; this is for when the cover map changes.

## Notes for integration
- New test commands (need R1): `yd:cycle`, `yd:fetch`, `yd:speed=<k>`, `yd:log`, `yd:stats`, `yd:where`,
  `yd:eye=x,y,yawdeg[,pitchdeg[,height]]` (a camera standing in the yard's own frame; `yd:eye=off`). Command-line
  `-KRNoYards` leaves the yards out; `-KRYardSurvey` logs a character map of the ground round the marinas.
- Bug seen, not mine to fix: `KeysMesh::BuildHullLoft` (`KeysHull.cpp`, `Profile`) writes NaN into the top edge of
  every hull's topsides (`Pow(Sin(F * PI), .6)` at F = 1 is a root of a tiny negative number, and it is multiplied in
  even when `Tumble` is 0). A mesh with those vertices has no valid bounds and is culled at random angles. I mend my
  own copies (`MendSheer` in `KeysYardProps.cpp`); every other user of the hull has the same 146 bad vertices.
- The player can drive through the floats, the fuel dock and a launched boat; only the pads stop a hull (R2).
  The walker cannot go ashore here (no `TryEnter` spot); that would be a hub change.
- The old box-built dry-stack racks at the town marina and Faro Blanco (`KeysLandmarks.cpp`) are untouched.
- The lifts and docks brief (P4) does not exist yet, so nothing here depends on it.
- Scripted runs lose shots when the game window has keyboard focus and someone types (Esc opens the pause menu, M the
  chart); several of my runs had to be repeated for that.

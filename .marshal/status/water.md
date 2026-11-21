# water

Status: READY FOR MERGE
Updated: 2026-11-21 12:05 EST
Branch: lane/water (code at bfc7079; this file and R1.patch in the commit after it)

Evidence lives outside the repo in `KeysRun-devkit/lanes/KeysRun-water/` (`shots/<tag>/`, `log_<tag>.log`), written `<tag>` below.

## Acceptance criteria
- [x] 1. The seafloor square is gone - `shots/v_seam/KRScript_50.png` (repro script, final build). Across the old edge
  (x 480..540, rows 600..700) the largest jump between neighbouring 6 px columns is 1.5 levels in any channel; the
  skirt-only reference (`seam_fix_nochunks`, no chunks at all) has 1.6 from the scene's own gradient; the untouched code
  (`seam_base`) has 6.1 at its edge, and the brief's 20 px measure was 10.6 red (`KeysRun-devkit/shots/seam2`).
  Dive at the same spot: `shots/v_flat_dive/KRScript_51.png` (at 0.9 m) looks right; `KRScript_50.png` (camera at the
  surface) matches the untouched code's `shots/before_flat_dive/KRScript_50.png`.
- [x] 2. The tide is visible (with R1 applied locally) - `shots/tide/KRScript_50.png` (`tide=-1`: a wide strip of dry
  sand, water's edge well off the mangroves, sounder 3.1 ft) and `KRScript_51.png` (`tide=1`: water up to the roots,
  5.2 ft). Log: `KRTide Low tide level -1.00 rise -0.32 m` / `High tide level 1.00 rise 0.32 m` in `log_tide.log`.
  Underwater at both tides: `shots/tide_dive/`. Patch committed as `.marshal/requests/water/R1.patch`;
  `git diff --name-only main...HEAD` lists no hub file.
- [x] 3. Bait balls on screen - reef: `shots/bait_reef4/KRScript_51.png` (461 silversides in mid-water, 5 m off), log
  `KRSchools tobait silverside #32: 461 fish, centre -1212 -4.3 2846 ... 6.7 m of water` and
  `KRSchools stats: 56 schools, 3804 fish (boids) ... CPU 1.296 ms/frame`. Offshore: `shots/bait_off3/KRScript_51.png`
  (158 herring with jacks and runners passing), log `tobait herring #31: 158 fish, centre -1090 -10.1 4660 ... 99.9 m of
  water` and `KRSchools stats: 14 schools, 3211 fish (boids) ... CPU 0.848 ms/frame`.
  Before (they were hard to find, and wrong): `shots/bait_reef_before`, `shots/bait_off_before`, `shots/bait_off2`.
- [x] 4. No slowdown - editor build, Medium, 1600 x 900, frame ms, two 6 s samples a run:
  | | before | after | change |
  |---|---|---|---|
  | flat `tp=-1150,452,4.71` | 36.19, 36.46 | 36.50, 36.22 | +0.1% |
  | reef `goto=reef` | 36.42, 36.48 | 36.82, (42.84) | +1.0% on the clean sample |
  An earlier pair of after runs, taken while the load average was 13 to 18, read 37.93 / 38.12 (flat) and
  37.26 / 37.33 (reef): +4.6% and +2.3%. The first before set of the day read 35.33 / 35.12 and 33.95 / 33.60, so two
  runs of the same code differ by up to 8% on this shared machine: the comparison cannot resolve much under 5%.
  The bracketed reef sample had one 712 ms frame (see notes). Logs: `log_perf_*`.

## Done
- `KeysSeafloor.*`, `KeysSea.*`: the far skirt is a grid on the chunks' 64 m lattice (32 m cells near the camera) with
  a hole and a 3 m wall wherever a chunk is built, at the true depth. Cause of the square: the skirt was sunk 0.1 m
  (flats) to 0.5 m under the chunks and the water absorbs by the metre. (The earlier `FarSkirtYBias` test could not
  show it: on the flats the offset was a fixed 0.1.) Skirt colours carry no per-vertex grain and get the chunks'
  slope shading. `-KRSeaNoChunks` / `-KRSeaNoSkirt` show one mesh alone.
- `KeysOcean.*`: `SeaLevel()` (the tide's rise once `DrawTide(true)` is called, else 0) for the surface, seafloor
  and schools to agree on. Without R1 it returns 0 and nothing changes (`shots/v_nopatch`).
- `KeysSchools.*`: found while looking for bait - the first 20 bait balls spawned at the start (all 5,200 fish)
  followed the boat everywhere through the "bubble" wrap, so the reef and blue water never got schools of their own
  (a new offshore "ball" had 1 fish). Now a school is only wrapped if it swam out by itself into the same zone; bait
  takes at most 3,200 fish; a school that cannot have half its size is not spawned; bait under 30 m spawns in
  mid-water (a dead assignment had left it on the bottom); offshore bait and hunters keep to the top 5 to 16 m
  (their goals ran down to 100 m). Fish keep under `SeaLevel()`. Reef now: 56 schools (snapper, grunts, yellowtail,
  jacks, 10 bait balls) where it had 20 bait schools.
- New test command (no hub change needed): `fish:tobait[=m]` puts the diver m metres (7) off the nearest real bait
  ball with a clear line of sight.

## In progress
- Nothing.

## Blocked
- Nothing.

## Requests for the marshal
- R1 (open): `Source/KeysRun/KeysGeo.cpp`, `KeysWorld.cpp` - draw the water at the tide. `WaveH` adds
  `KeysOcean::SeaLevel()` (so every hull, bird, buoy and the diver's surface rise with it); `UpdateWater` lifts the
  mesh and far ring by it and uses depth + tide for surf, beach wash and the deep/shallow blend; `BeginPlay` calls
  `KeysOcean::DrawTide(true)`. 5 lines changed, 3 added. Patch: `.marshal/requests/water/R1.patch`
  (`git apply --check` passes on this branch).

## Notes for integration
- Not checked with R1: the classic map (`-KRClassicMap`), docks and lifts at high water, traffic, the town. Things
  that float on `WaveH` follow the tide by themselves; anything placed at a fixed y = 0 will not. The diver's depth
  readout and `dv:depth` are still measured from the datum, not the tide level (KeysDiver, not mine).
- The "bright green dots from above" on the flats are not bait: they are still drawn with `fish:off`
  (`shots/dots_off/KRScript_50.png`) and are the seagrass tufts and sponges on the bottom, seen through the water.
  Whoever owns that dressing may want it darker from above.
- At the reef from above the chunk area still reads as a slightly lighter block (`shots/v_reef_top`); the untouched
  code shows the same (`shots/before_reef_top`). It is a level-of-detail difference over bumpy bottom, not the
  offset. Not in this lane's criteria; a possible follow-up.
- One 712 ms frame in `reef_after_f` and none in seven other samples. Not traced. A first-time species pool build
  (`PoolFor`, a static mesh build) is a candidate, since more species now turn up at the reef; unconfirmed.
- Every perf sample today, before and after, has 75 to 80 ms worst frames: something else on the machine.
- The top-down camera (`cam=0,25,3`) is above 120 m, where `Populate` stops spawning: fish counts in such shots are
  not representative.

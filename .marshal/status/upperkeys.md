# upperkeys

Status: READY FOR MERGE
Updated: 2026-11-21 19:25 EST
Branch: lane/upperkeys (see `git log`; this file is in the latest commit)

Read first:
- Merging this branch alone changes nothing in the game: it adds data, tools and a loader that still answers
  `Load("p1")` as before. Phase 2 appears when the marshal applies R1 + R2 + R12 (three small hub patches, below).
- With those three applied every criterion passes, with one thing to decide: the `KeysGeoReal` loading is cheap
  (14.5 MB, 6 ms) but the whole game then takes 54 s from world start to a running game instead of 9 s, and peaks
  2.6 GB higher, because the hub builds all the map's scenery at start-up. That needs hub work (R10) and I cannot fix
  it from my paths. See "Cost".
- Without R12 criterion 9 fails (+15% frame time at Marathon). With it Marathon is faster than before.

Evidence paths: shots and logs are under `KeysRun-devkit/lanes/KeysRun-upperkeys/` (`shots/<tag>/`, `log_<tag>.log`);
previews and `p2_checks.txt` under `KeysRun-devkit/geo/`.

## Acceptance criteria
- [x] 1 Phase 2 data - `p2.kgeo` x 7500..23500, z -6920..6200 (1601 x 1313 cells), plus `p2_bay.kgeo` x -8300..7500,
  z -6920..-4200 (the strip of Florida Bay north of p1, so the map is a full rectangle). North edge -6920 because the
  chain climbs to z -5300 by the east edge; the 2720 m shift is a multiple of 1360 m so palms (16 m), houses (34 m),
  terrain (8 m) and the depth grid (10 m) keep their spots. Preview looked at: `geo/p1p2_game.png`.
  Named islands include Long Key, Lower Matecumbe Key, Upper Matecumbe Key, Windley Key, Plantation Key,
  Tavernier (Key Largo). Bridges: Long Key Viaduct, Channel 5, Channel 2, Irving R Eyster, Indian Key Channel,
  Tea Table Key, Tea Table Relief, Whale Harbor, Snake Creek, Tavernier Creek. Summary lines below.
- [x] 2 No seam - along x = 7500 the largest height difference p1 last column / p2 first column is 0.000 m over 1041
  rows (the bake copies the edge and fades the difference over 30 cells; before the join it was already 0.00 m, the
  DEM is on the same pixel grid). Shore distance 0.00 m. Top-down across the join: `shots/p2_spots/KRScript_56.png`,
  `_57.png` (no step; the Long Key Viaduct builds across it).
- [x] 3 The game loads both - `KeysGeoReal::LoadAll()` (R1), reached only on the real map. Loader cost: p1 9.4 MB
  in 6 ms, Phase 2 (p2 + p2_bay, with marks and mangrove cover) 14.5 MB in 6 ms ("the map is 3 grids" in any
  `log_perf_A*.log`). Whole-game extra memory and start-up: see "Cost" (+2.6 GB peak, +45 s; hub builders, R10).
  Three spots, chase and top-down, `shots/p2_verify/KRScript_00..05.png`; picks: 9800,100 Hawk Channel 4.1 m;
  17250,-2050 Flats 1.5 m; 21850,-3750 Hawk Channel 2.1 m.
- [x] 4 A boat can make the trip - repeated with the Phase 2 marks in place, `log_hawk_run4.log`: 29 picks 20 s
  apart, x -400 -> 21647 (past Tavernier) at a steady 42 m/s, never stopped, zone Hawk Channel throughout, depth
  2.6..8.8 m, 7.0 m at the join. (The first run, before the marks, `log_hawk_run.log`: x -400 -> 22872.)
  Marked channels (`geo/p2_checks.txt`): 199 legs, 0 with no water way round; 4 where the straight line clips land
  (Cowpens 73-74, 74-74, 74-75; Tea Table Channel 6-7). In game: Snake Creek 2.7 m, Tavernier Creek 2.7 m, Ocean
  Point 2.6 m (`shots/p2_verify/KRScript_06..08.png`).
- [x] 5 The chart - `bigmap` at 1600 x 900, cropped: `shots/p2_chart/crop_east.png`. Read on it: Long Key, Lower
  Matecumbe Key, Lignumvitae Key, Shell Key, Upper Matecumbe Key, Windley Key, Plantation Key, Tavernier (Key Largo),
  Crab Keys; land, soundings and marks all from the data. Manatee Keys is cut by the map's edge and its name falls
  outside the frame (R11).
- [x] 6 Requests - R1..R12 below, each marked NEEDED or follow-up.
- [x] 7 Nothing else moves - `tpcheck` 23 destinations, 0 bad, lines identical to baseline (`base_tpcheck.txt` vs
  `after_tpcheck.txt`). Boot Key flat (`tp=-1150,452,4.71`, `cam=0,6,1.5`) matches tree for tree with R2
  (`shots/base_p1/KRScript_50.png` vs `shots/after_flat/KRScript_51.png`); `goto=reef` matches. Without R2 the
  mangroves re-roll. Known changes: chart soundings sit on a grid counted from the map's north edge, so they moved
  (R11); the gold bluefin's loop runs 93.6 km (was 44.3); the minimap now shows the whole 31.8 km map, so Phase 1
  is drawn at half the size.
- [x] 8 Classic map - `log_classic2.log`, run with R1 + R2 and the marks in place: no `KeysGeoReal` line, no Fatal or
  Ensure, and the same six "Error" lines as a real-map run before the change (five engine self-test lines and one
  audio sample-rate warning). Two extra `LogAudioMixer` time-out warnings, from the machine being busy. Classic chart
  intact.
- [x] 9 Perf - before and after back to back, Medium, 1600 x 900, two valid runs each (machine load under 4.5):

  | spot | before (B9, B10) | after R1+R2 (A10, A11) | after R1+R2+R12 (A12, A13) |
  |---|---|---|---|
  | `tp=-1150,452,4.71` | 35.91, 35.84 ms | 41.14, 41.70 ms (+15%) | 33.33, 33.33 ms (-7%, at the 30 fps cap) |
  | marina start | 38.02, 37.61 ms | 42.97, 43.90 ms (+15%) | 34.98, 34.84 ms (-8%) |
  | Islamorada `tp=17250,-2050,0` | - | 36.23, 36.60 ms | 33.34, 33.33 ms |

  The +15% was all in the HUD: the scene renders in the same time (32.4 ms before, 31.7 ms after, `ProfileGPU` in
  `log_gpu_before.log` / `log_gpu_after.log`), but the minimap's daybeacons went to the GPU as 248 batches instead
  of 93 (red triangles and green squares alternate down a channel, and each change of shape starts a batch; 452
  marks, was 146). R12 draws the greens, then the reds: 6 lines of `KeysHUD.cpp`, and it helps Phase 1 too.

## Cost
Measured with R1 + R2 applied, quiet machine (16 GB M4), two runs each:

| | before | with Phase 2 | Phase 2 without its mangroves (test only) |
|---|---|---|---|
| `KeysGeoReal` data | 9.4 MB, 6 ms | 23.9 MB, 12 ms | |
| `LoadMap` | 4.0, 4.2 s | 11.7, 11.1 s | 10.1 s |
| world start -> first frame | 6.3, 6.5 s | 17.6, 16.8 s | 16.2 s |
| world start -> game running (frame 10) | 9.2, 9.4 s | 54.1, 53.9 s | 34.1 s |
| peak memory (RSS) | 5.5, 5.5 GB | 8.1, 8.0 GB | 7.3 GB |

What causes it: not the loader, and not much the building (terrain 0.1 -> 0.3 s, mangroves 1.0 -> 2.3 s). The hub
makes every piece of scenery for the whole map at start-up: terrain 89 -> 228 tiles, mangroves 62 -> 178 tiles
(4.4 M -> 13.0 M triangles), bridges 51 -> 76 chunks, landmarks 44 -> 86 tiles, plus palms and houses. The engine
then walks all of it each time the graphics settings are applied, which happens twice (at load, and at `play`):
those stalls go from 1 - 2 s each to 4 - 25 s each (`log_perf_A11.log` against `log_perf_B10.log`). Phase 2's
mangroves alone are 20 s of the 45 s and 0.8 GB of the 2.6 GB.
What would bring it down: R10 - make the scenery meshes by tile only within sight of the boat (mangrove canopy is
drawn to 4.5 km, terrain to 9 km, houses to 2.6 km) and free them behind it. Mangroves first.
Once running, frame time does not suffer (criterion 9), and memory settles: the scenery is culled by distance.

## Done
- `Tools/geo/bake.py`: phases p2 and p2_bay; padded working rectangle, coastlines closed along the data box, edges
  joined to baked neighbours, marked channel legs the grid pinches are cut. p1 re-bakes byte for byte.
- `Tools/geo/fetch_p2.sh`, `enc_p2_s57.py`, `preview_p2.py`, `hawk_p2.py`.
- `Source/KeysRun/KeysGeoReal.*`: one grid per file, served as one map; `LoadAll`, `Add`, `Phases`, `PhaseAt`,
  `CellI0`, `CellJ0`.
- `Content/KeysRun/Geo/p2.kgeo`, `p2_places.json`, `p2_marks.txt`, `p2_mangrove.bin`, `p2_bay.kgeo`,
  `p2_bay_places.json`, `p2_bay_mangrove.bin`.

## Script summary lines
- p2: grid 1601 x 1313 at 10.0 m, 10.6 MB; land 2.39%, islands 171 (37 named), highest 15.7 m, deepest 259.8 m;
  10 channel legs cut through; join: height off by up to 0.00 m before the join to p1 (west)
- p2_bay: grid 1581 x 273 at 10.0 m, 2.2 MB; join: height off by up to 1.25 m before the join to p1 (south)
- 306 marks -> p2_marks.txt ({'R': 143, 'G': 125, 'r': 21, 'g': 17}), 30 lit, from 465 charted
- p2 mangrove: OSM wetlands 336, woods 148; mangrove land 2.22 km2 (of 5.02 km2 land), fringe cells 6766
- p2_bay mangrove: mangrove land 0.07 km2 (of 0.14 km2 land), fringe cells 378
- map: 3 grids, 3181 x 1313 cells, x -8300..23500, z -6920..6200; cells with no data 0
- Hawk Channel line x -400..22490 (22.9 km): shoalest 4.5 m at x 1200; at the join 7.1 m

## Sources added
- NOAA NCEI DEM mosaic (public domain), same service as p1: `dem_p2_half.tif`.
- OpenStreetMap (ODbL, already credited): `coast_p2.json`, `osm_p2_extra.json`, `osm_p2_mangrove.json`.
- NOAA ENC cells (S-57) from charts.noaa.gov, 21 cells: ENC Direct was not answering on 2026-11-21. User agreement
  read: free to download, use and redistribute; a derived product is not an official ENC. Same data and credit as
  ENC Direct.

## In progress
- Nothing.

## Blocked
- Nothing.

## Requests for the marshal

NEEDED for the acceptance criteria (R1, R2 and R12; all three tested applied locally, never committed). Everything
else is a follow-up, except R10, which is the marshal's call before Phase 2 ships:

- R1 (open, NEEDED: criteria 3, 4, 5): `Source/KeysRun/KeysGeo.cpp` - in `Init`, `KeysGeoReal::Load(TEXT("p1"))`
  becomes `KeysGeoReal::LoadAll()`. It is only reached on the real map, so the classic map loads nothing new. The
  world bounds (`WorldX0` ... `WBZ1`), the drop-off line, the depth grids and the chart then cover p1 + p2 + p2_bay
  with no further change. patch: .marshal/requests/upperkeys/R1.patch
- R2 (open, NEEDED: criterion 7): `Source/KeysRun/KeysMangrove.cpp` - the trees are seeded by cell number, and the
  map's cell (0, 0) has moved 272 rows north. Seed by the cell's number on p1's own grid
  (`KeysGeoReal::CellI0()`, `CellJ0()`), four lines. Without it every Phase 1 mangrove, wader and pelican re-rolls.
  patch: .marshal/requests/upperkeys/R2.patch

- R12 (open, NEEDED: criterion 9): `Source/KeysRun/KeysHUD.cpp` `DrawRealChart` - draw the green daybeacons, then
  the red ones, instead of in file order. Same picture (`shots/perf_A13/KRScript_00.png`), 155 fewer GPU batches a
  frame, 5.5 ms back at Marathon. patch: .marshal/requests/upperkeys/R12.patch

What the brief asked to be covered, and where each stands:

- R3 (no change needed) world bounds and chart: follow from R1. The chart draws Phase 2's land, soundings, marks and
  island names from the data (`shots/p2_chart/crop_east.png`).
- R4 (no change needed for the criteria; follow-up) US-1 and its bridges east of 7500: `KeysBridges` and the pilings
  in `KeysGeo::Init` already follow `ChainZ` from `WBX0` to `WBX1`, so the road and ten bridges build (76 chunks,
  was 51). Follow-up: `Channel 5 Bridge` is a high fixed span in reality (a = 11272, -1046; b = 11693, -774 in
  `p2_places.json` `bridges`); today only the Seven Mile Bridge gets an arch (`BridgeArchX0/X1`). The old railway
  bridges beside Long Key Viaduct and Channel 2 are in `other_bridges` and are not built.
- R5 (no change needed) daybeacons: `KeysLandmarks` and the chart read `KeysGeoReal::Marks()`; 268 Phase 2
  daybeacons stand, and `BuildNavSegs` dredges their channels (105 marked channels, 324 legs in the log). The 38
  buoys in `p2_marks.txt` (kinds `r`, `g`) are not drawn, as in Phase 1.
- R6 (open, follow-up) marinas: `KeysGeo.cpp` `MarsReal` has four, all in Marathon. `p2_places.json` `marinas` has
  15 sites (for instance 15889, -1735; 17809, -2596; 18646, -2856; 21218, -4053). They carry OpenStreetMap's real
  business names: give them invented names, as the no-brands rule says. A fuel and bait stop at Islamorada and one
  at Tavernier would make the trip a round one.
- R7 (open, follow-up) the reef line: `KeysLandmarks.cpp` builds Sombrero Key Light only. `p2_places.json` `lights`:
  Alligator Reef Light at 16573, 301 (grid depth 2.4 m), Tennessee Reef Light at 10093, 2261 (6.6 m). `reef_sites`:
  Tennessee Reef 10080, 2220; Alligator Reef 16580, 300; Cheeca Rocks 17040, -1660; Hen and Chickens 19822, -1858;
  Crocker Reef 20140, -790; Davis Reef 21170, -1050; Conch Reef 22780, -1540. `KeysGeo.cpp` `GHumps` and the `goto=`
  spots know none of them.
- R8 (no change needed; one caution) mangrove cover: `p2_mangrove.bin` and `p2_bay_mangrove.bin` are read through
  `MangroveCell`; with R2 the game grows 53,427 rooted trees in 178 tiles (17,374 in 62 before). They are all built
  at start-up: see R10. `Tools/geo/bake_mangrove.py` has a `BUILT` list of Phase 1 keys; Upper Matecumbe, Windley,
  Plantation and Key Largo are built-up too and are treated as unbuilt, so their low shores get mangrove.
- R9 (open, follow-up) fish zones: `picks` returns sensible zones in Phase 2 from the existing rules (Hawk Channel,
  Flats, Backcountry, Creek). Not done: `KeysLife.cpp` scatters life with `LRand(WBX0, WBX1)` over a map now 2.5
  times the area, so there is less of it near Marathon; teleport destinations, charters and Free Fish spots stop at
  Phase 1; `KeysFinal` runs the gold bluefin's loop through all deep water (93.6 km, was 44.3 km).
- R10 (open, NOT needed for a criterion to be measured, but decide before Phase 2 ships) start-up and memory:
  see "Cost". `KeysMangrove.cpp` `BuildMangroves`, `KeysWorld.cpp` `BuildTerrain` (terrain tiles, palm and house
  chunks), `KeysLandmarks.cpp`, `KeysBridges.cpp` make their meshes for the whole map before the first frame. Make
  them by tile within sight of the boat and free them behind it; mangroves first (20 s and 0.8 GB of the extra 45 s
  and 2.6 GB). No patch written: it is a restructuring of hub builders, not a few lines. The 10 m depth, inlet and
  colour grids in `KeysGeo.cpp` also cover the whole map now (4.18 M cells, was 1.65 M).
- R11 (open, small) `KeysHUD.cpp`: the chart's soundings sit on a 560 m grid counted from `WorldZ0`, so they have
  moved; counting from a fixed z keeps them where they were. An island cut by the map's edge (Manatee Keys) has its
  name drawn outside the chart frame.

## Notes for integration
- Apply order: merge the branch, then R1, R2, R12 together. R1 without R2 re-rolls every Phase 1 mangrove; R1 without
  R12 costs 15% at Marathon.
- `p2_places.json` `marinas` carries OpenStreetMap's real business names. Nothing in the game reads them yet; give
  them invented names before any are shown (R6).
- `Tools/geo/bake_mangrove.py` (not mine) has a `BUILT` list of Phase 1 keys only, so Phase 2's built-up shores get
  mangrove (R8).
- Bake order for a rebuild: p1, p2, `bake_marks` p2, p2 again (the channel cuts read the marks), `bake_mangrove`,
  p2_bay. p1 re-bakes byte for byte.
- Scripted runs: with Phase 2 the game is not running until about 55 s after launch, and the script clock is wall
  time, so put the first `tp` at 60 or later. A busy machine slows the boat and breaks clock-steered runs like the
  Hawk Channel one; I waited for a load under 3.
- Never run the game at 2560 x 1440 from `shoot.sh`: it froze at frame 4.
- The editor binary in this worktree was last built with R1 + R2 + R12 applied; the sources are clean. Rebuild.
- My earlier perf logs `perf_A1`, `A5`, `A8`, `A9`, `B1`..`B4`, `hawk_run2`, `hawk_run3` are from a busy machine
  or sampled during the start-up stall: ignore them.

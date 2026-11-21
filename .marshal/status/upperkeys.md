# upperkeys

Status: IN PROGRESS
Updated: 2026-11-21 14:40 EST
Branch: lane/upperkeys (see `git log`; this file is in the latest commit)

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
- [x] 3 The game loads both - `KeysGeoReal::LoadAll()` (R1). Loader cost measured: p1 9.4 MB in 5 ms, the rest
  12.1 MB in 6 ms (`log_p2_first.log`, "the map is 3 grids"). Three spots, chase and top-down, `shots/p2_verify/`
  `KRScript_00..05.png`; picks: 9800,100 Hawk Channel 4.1 m; 17250,-2050 Flats 1.5 m; 21850,-3750 Hawk Channel 2.1 m.
  NOT yet measured: whole-game extra memory. Whole-game start-up to first frame went 9.1 s -> 16.1 s in one pair of
  runs (hub builders now cover 2.4x the area: terrain 89 -> 228 chunks, mangrove 62 -> 178 tiles); needs a clean repeat.
- [x] 4 A boat can make the trip - `log_hawk_run.log`: 30 picks, x -400 -> 22872 at about 43 m/s, no stop, depth
  4.6..9.3 m, across the join. Run before the Phase 2 marks existed: REPEAT IT (marks add piles).
  Marked channels (`geo/p2_checks.txt`): 199 legs, 0 with no water way round; 4 where the straight line clips land
  (Cowpens 73-74, 74-74, 74-75; Tea Table Channel 6-7). In game: Snake Creek 2.7 m, Tavernier Creek 2.7 m, Ocean
  Point 2.6 m (`shots/p2_verify/KRScript_06..08.png`).
- [ ] 5 The chart - `bigmap` shows Phase 2 with marks from the data alone (`shots/p2_verify/KRScript_10.png`), but I
  have not read the island names at a legible size. A 2560 x 1440 run froze at frame 4 (killed); retake at 1600 x 900
  and crop.
- [ ] 6 Requests - R1 and R2 written; the follow-up list below is not yet numbered patches.
- [x] 7 Nothing else moves - `tpcheck` 23 destinations, 0 bad, lines identical to baseline (`base_tpcheck.txt` vs
  `after_tpcheck.txt`). Boot Key flat matches tree for tree with R2 (`shots/base_p1/KRScript_50.png` vs
  `shots/after_flat/KRScript_51.png`); reef spot matches. Without R2 the mangroves re-roll. Known changes: chart
  soundings sit on a grid from the map's north edge (moved), and the gold bluefin's loop now runs 93.6 km (was 44.3).
- [ ] 8 Classic map - `log_classic.log`: no `KeysGeoReal` lines, no Error/Fatal/Ensure. Run was before R2 and the
  marks; not yet compared warning-for-warning with a main classic log.
- [ ] 9 Perf - NOT DONE. The baseline runs were spoiled (they queued behind the other lane and ran on the new build).
  Needs before/after back to back at `tp=-1150,452,4.71` and the marina, plus Islamorada. One sample with p2 loaded:
  marina 38.7 ms (no valid before).

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
- Criteria 5, 8, 9 and the memory / start-up numbers for 3; rerun of the Hawk Channel run with marks.

## Blocked
- Nothing.

## Requests for the marshal
- R1 (open, NEEDED): `KeysGeo.cpp` - `KeysGeoReal::Load(TEXT("p1"))` -> `KeysGeoReal::LoadAll()`. World bounds,
  chart, US-1 bridges, daybeacons, channel dredging, mangroves, palms and houses then follow from the data.
  patch: .marshal/requests/upperkeys/R1.patch
- R2 (open, NEEDED for criterion 7): `KeysMangrove.cpp` - seed trees by cell number on p1's own grid
  (`CellI0`, `CellJ0`), or every Phase 1 mangrove re-rolls. patch: .marshal/requests/upperkeys/R2.patch
- Follow-ups, not yet written as patches: marinas in Phase 2 (15 in `p2_places.json`); Alligator Reef and its light,
  Tennessee Reef (`reef_sites`, `lights`); the high span at Channel 5; fish zones and hotspots east of 7500; life is
  spread by `LRand` over the whole map, so density near Marathon drops; the gold bluefin's loop; chart soundings
  anchored to absolute coordinates; start-up cost of the eager hub builders and the 10 m depth/colour grids
  (about 24 bytes a cell over 4.18 M cells, was 1.65 M).

## Notes for integration
- Never run the game at 2560 x 1440 from `shoot.sh`: it froze at frame 4.
- `shoot.sh` queues behind the other lane, so a "before" run can land after a build; check the `KeysGeoReal` log line.

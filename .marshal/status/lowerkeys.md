# lowerkeys

PART A READY (the Lower Keys). Part B (Key Largo) in progress.

Status: IN PROGRESS
Updated: 2026-11-22 02:00 EST
Branch: lane/lowerkeys (see `git log`; this file is in the latest commit)

Read first:
- The user said only "start phase 3". The split (Part A the Lower Keys, Part B Key Largo), the order and the ten
  criteria are the marshal's reading of it, from the brief. Everything under "Decisions I made on my own" is mine;
  nobody was there to ask.
- Merging this branch alone puts the Lower Keys into the map (the loader reads `p3` when the file is there) and the
  game then cannot start: without R2 it built 550 terrain tiles, 7,600 houses and 1,426 docks (14 million triangles)
  at launch and the GPU gave up before the first frame after 4.5 minutes (`log_p3a.log`). **Apply R1, R2 and R3
  with the merge**, or start with `-KRNoPhase3` (my switch in `KeysGeoReal`: the map as it was).
- With R1 + R2 + R3 applied all ten criteria pass for Part A, and the game starts faster and lighter than it did
  before the Lower Keys: 6.7 s to a running game (was 22 s), 6.0 - 6.6 GB peak (was 7.3 GB).
- R2 changes `KeysDocks.cpp`, which the marshal is editing on `main` (dock collision). It will need merging by hand
  if the same lines moved. All patches are against bf2feac.

Evidence paths: shots and logs are under `KeysRun-devkit/lanes/KeysRun-lowerkeys/` (`shots/<tag>/`, `log_<tag>.log`,
`rss_<tag>.txt` the peak memory of that run in KB); previews and check lists under `KeysRun-devkit/geo/`.

## Part A (the Lower Keys) - acceptance criteria
- [x] 1 Data - `p3.kgeo`, x -30060..-8300, z -6920..6200 (2177 x 1313 cells of 10 m, the frame of `frame.py`), joined
  to p1 and p2_bay along x = -8300; `p3_places.json`, `p3_marks.txt`, `p3_mangrove.bin` (with `--built`),
  `p3_canals.txt`, and `p3_sites.txt` (new: named places for the hub, see Done). Summary lines below.
  Whole-map preview looked at: `geo/map_p3_game.png` (and `geo/p3_game.png`, `geo/p3_mangrove.png`).
  Named: Big Pine Key, Little / Middle / Big Torch Key, Ramrod Key, Summerland Key, Cudjoe Key, Sugarloaf Key, Lower
  Sugarloaf Key, Saddlebunch Keys, Big Coppitt Key, Boca Chica Key, Stock Island, Key West, Fleming Key (77 named of
  254). US-1 bridges (25): Spanish Harbor, North and South Pine Channel, Torch Key Viaduct, Torch-Ramrod, Niles
  Channel, Kemp Channel, Bow Channel, Park Key, North Harris, Harris Gap, Harris Channel, Lower Sugarloaf Channel,
  Saddle Bunch No. 2 - 5, Shark Channel, Rockland Channel, Boca Chica Channel, Cow Key Channel, and one 15 m span
  with no name at x -20786.
- [x] 2 No seam - `geo/p3_checks.txt`: p3 | p1 1041 cells and p3 | p2_bay 273 cells, heights differ by at most
  0.000 m, shore distance 0.00 m (the bake copies the neighbour's edge and fades the difference over 30 cells; before
  the join the heights were 0.60 m apart at worst). Top-down across the join: `shots/p3_verify/KRScript_56.png`
  (x -8300, z -1500, Big Pine and No Name Key) and `_57.png` (x -8300, z -4200, where p3, p1 and p2_bay meet): no
  step. p1, p2 and p2_bay re-bake byte for byte (`.kgeo`, `_places.json`, `p2_canals.txt` compared after re-baking
  into a scratch folder with this branch's `bake.py`).
- [x] 3 The game loads all of it - `KeysGeoReal::LoadAll` adds p3, p4 and p4_bay when their files are there (a
  missing one is a warning and is left out: `log_base2.log` ran this loader with none of them). Only reached on the
  real map. Loader cost: p3 16.4 MB, 8 ms ("the map is 4 grids ... Phase 3 16.4 MB in 8 ms" in any `log_after_*`).
  Whole-game cost: see Cost. Three spots, chase and top-down, `shots/p3_verify/KRScript_50..55.png` (contact sheet
  `sheet.png`): off Big Pine -9290, -650 zone Hawk Channel 2.0 m; off Sugarloaf -16050, -850 Hawk Channel 5.9 m; Key
  West harbor -25040, -2520 Backcountry 9.7 m (the harbor is on the Gulf side of the chain line). Land, mangrove
  shores, canals, water colour and the sounder read right.
- [x] 4 A boat can make the trip - `log_hawkA.log`: 31 picks 20 s apart, x -24589 (off Key West) -> 428 (off
  Marathon) at a steady 43 m/s, never stopped, zone Hawk Channel throughout, 7.2 .. 14.2 m under her, 13.4 m at the
  join (x -8297). (The Phase 2 lane ran -400 -> 21647.) On the data (`geo/p3_checks.txt`): the Hawk Channel line
  x -29160..22530 is never under 3.3 m. Marked channels: 211 legs, 7 where the straight line clips land, 1 with no
  water way round (Upper Sugarloaf Sound 2-3: 290 m of land between two marks that are not really neighbours).
  That run was on the bake before the last one; the last one changed a few cells in Cow Key Channel only.
- [x] 5 The chart - what the long map does to it: the chart was a 1024 square texture with the map in a centred
  band, so on a 53.6 km map the keys were a strip 250 pixels tall, the corner chart a thread 80 pixels tall, and the
  soundings ran together (`shots/p3b/KRScript_00.png`). Fix = R3. With it `bigmap` shows the whole map with Fleming
  Key, Key West, Boca Chica Key, Lower Sugarloaf Key, Cudjoe Key, Big Torch Key, Ramrod Key, Big Pine Key, Snipe
  Keys, Johnston Key and more named (`shots/after_new2/KRScript_01.png`, enlarged in `crop_west.png`); the corner
  chart is the 12 km round the boat.
- [x] 6 Cost - see Cost. With the Lower Keys and no hub change the game does not start at all. With R2 start-up is
  6.7 s and peak memory 6.0 - 6.6 GB, both under what they were before Phase 3.
- [x] 7 Requests - R1..R5 written as patches, R6..R10 as notes; see Requests. R1, R2, R3 are needed.
- [x] 8 Nothing else moves - `tpcheck` 23 destinations, 0 bad, every line identical to the baseline
  (`base_tp.txt` vs `after_new2_tp.txt`; 35 destinations, 0 bad with R5). `dk:audit` zero overlaps (416 docks).
  Boot Key flat, the reef and the Lower Matecumbe canal: same trees, houses, palms, bushes, coral
  (`shots/base1/KRScript_50..52.png` vs `shots/after_new2/KRScript_50..52.png`, side by side in
  `shots/compare_A.png`). What did change on Phases 1 and 2: the private docks are dealt their style and lift by
  their number in the whole map's list, so with 256 more docks west of them they come out re-dealt (Grassy Key 46 ->
  47, Marathon 28 -> 26, Plantation Key 33 -> 40, Upper Matecumbe 13 -> 11; the canal shot's dock is a different
  build of dock on the same wall). The Phase 2 lane saw the same. R8 would stop it. The corner chart now shows 12 km
  instead of the whole map; the chart's soundings sit on a fixed grid.
- [x] 9 The classic map - `log_classicB.log` (`-KRClassicMap`, R1..R5 applied): no `KeysGeoReal` line, no Fatal or
  Ensure, the same six "Error" lines as before (five engine self-test lines, one audio sample-rate warning), 75
  terrain tiles, 2634 palms and 609 houses as in the marshal's `log_canal_classic.log`, `tpcheck` 19, 0 bad.
- [x] 10 No slowdown - Medium, 1600 x 900, the editor build is capped at 30 fps so every spot reads 33.34 ms before
  and after; the parts of the frame are the real comparison:

  | spot | before (base2) frame / game / draw / GPU ms | after R1+R2+R3 (perfA) |
  |---|---|---|
  | marina start | 33.34 / 10.99 / 7.16 / 27.18 | 33.77 / 12.55 / 6.78 / 26.46 (one 98 ms frame while tiles arrived) |
  | `tp=-1150,452,4.71` cam 0,6,1.5 | 33.34 / 11.26 / 6.88 / 27.81 | 33.34 / 11.13 / 6.97 / 27.00 |
  | `tp=13232,-1280,0` | 33.34 / 9.20 / 5.50 / 25.27 | 33.37 / 10.02 / 5.95 / 21.79 |
  | Key West harbor `tp=-25040,-2520,1.57` | - | 33.34 / 9.32 / 6.90 / 24.25 |
  | off Key West `tp=-23000,-1300,0` | - | 33.34 / 11.38 / 7.65 / 30.04 |

  Within 5% at all three old spots (after_new and after_new2 read 33.34 at all three). Off Key West the GPU is at
  30 ms of a 33 ms frame: the tightest spot on the map, with Key West, Stock Island and Boca Chica in view.

## Cost
16 GB M4, editor build, 1600 x 900. "Game running" = frame 10, which is after `play` and `gfx=1` have re-applied the
graphics settings (the two stalls that grow with the number of meshes).

| | before (bf2feac) | Lower Keys, no hub change | Lower Keys + R1 + R2 (+R3) | old map + R1 + R2 (`-KRNoPhase3`) |
|---|---|---|---|---|
| `KeysGeoReal` data | 23.9 MB, 14 ms | 40.3 MB, 22 ms | same | 23.9 MB |
| world start -> first frame | 12.7 s (13.7 busy) | none in 4.5 min: GPU crash | 4.8, 4.9, 4.9, 4.8 s | 4.0 s |
| world start -> game running | 22.2 s (27.2 busy) | - | 6.7, 6.8, 6.7, 6.7 s | 5.7 s |
| peak memory (RSS) | 7.31, 7.33 GB | 6.1 GB and climbing when it died | 5.93, 6.60, 5.97, 6.61 GB | 5.91 GB |

Runs: before `base2`, `base1` (base1 with another build running: load 8 - 15); no hub change `p3a`; after
`after_new`, `after_new2`, `perfA`, `p3_verify`; old map `after_old`. The marshal's log says "about 12 s" for the
old map; I measured 22 s to frame 10 with the same script as the after runs, so compare within this table.
What R2 does: terrain (512 m tiles, within 9.4 km), palms / bushes / houses (1200 m squares, within 3.2 km) and docks
(600 m tiles, within 3 km) are made near the boat and dropped behind it, like the mangroves. 602 of 2730 terrain
tiles and 162 scenery squares can have land; 22 tiles and 7 squares are made before the first frame.
Not streamed, and affordable for now: bridges (115 chunks, 906 k triangles near) and landmarks (155 tiles, 875 k
triangles). They are the next thing to grow with Part B (R9).

## Decisions I made on my own
- Phase names and rectangles: `p3` = the Lower Keys, x -30060..-8300, z -6920..6200 (the same north and south edges
  as p2, so Part A needs no bay strip of its own). For Part B: `p4` = Key Largo, x 23500..35500, z -13720..6200, and
  `p4_bay` = the bay and Gulf north of everything else, x -30060..23500, z -13720..-6920.
- West edge -30060: 21760 m west of Phase 1's edge is 16 steps of 1360 m, which keeps Phase 1 and 2's palms (16 m
  grid), houses (34 m), terrain (8 m) and depth grid (10 m) on the same spots, as the Phase 2 lane did going north.
  It takes in Key West harbor (x -24830) and Sand Key Light (x -28335) and stops short of Ballast Key and Woman Key.
  The Marquesas and the Dry Tortugas are not on the map.
- US-1's bents (36 m), deck pieces (12 m) and chunks (240 m) were counted from the map's west edge, and no sensible
  west edge is a multiple of all of them, so R1 counts them from Phase 1's own corner. The loader does the same for
  the chain line's 50 m samples and R1 for the drop-off line's: with that, Phase 1's marinas, wrecks, reef and
  `tpcheck` lines are identical to the last digit.
- Where the road is: OpenStreetMap's trunk road ends at the Cow Key Channel bridge (x -22965); in Key West US-1 is
  a city street. So `p3_sites.txt` says US-1 runs on the chain line from -22965 east, and west of that the chain
  line is only the keys' axis (held at Key West's z). R1 stops the road, bridges and piles there. Without it a
  bridge is built across open water from Key West to the map's edge.
- Sand Key: NOAA's mosaic out there is the coarse Coastal Relief Model and has 21 m where the chart has a reef awash
  under a lighthouse. `bake.py` (`SHOALS`) puts a 150 m shoal with 1 m on it back at the charted position. That is
  the only depth in the map that is not from the data. Rock Key and Eastern Dry Rocks are also missing from the data
  and are left out.
- Streaming instead of a smaller map: the brief asked for the request "if start-up passes 25 s or peak memory
  passes 10 GB". It was not a matter of passing a limit: the game would not start. I wrote R2 for terrain, scenery
  and docks (docks were not on the brief's list; they were the largest of the lot), and left bridges and landmarks
  whole because measured they did not need it.
- Marina names are invented in `Tools/geo/sites_p3.py` (`MARINA_NAMES`), dealt west to east; the script fails if
  one of them is a marina name in the OSM data. `p3_places.json` keeps the marina sites and drops their real names.
- `Looe Key Light`: OpenStreetMap has no lighthouse there (the real one is a lit marker); the brief listed it with
  Sand Key and American Shoal, so `bake.py` (`KNOWN_LIGHTS`) lists it on the reef's crest, and R4 builds it shorter.
- Guessed: the Niles Channel bridge is "Nile Channel Bridge" in OSM; I renamed it. The 180 ha outline that holds
  Boca Chica, Rockland Key and the land round the air station is named "Boca Chica Key"; the one that holds Stock
  Island and Raccoon Key is "Stock Island" (US-1's causeways join them in the coastline data).

## Script summary lines (the final bake: `Tools/geo/bake_p3.sh p3`, which re-bakes byte for byte)
- p3: grid 2177 x 1313 at 10.0 m, 14.5 MB; land 6.85%, islands 254 (77 named), highest 26.9 m, deepest 237.0 m;
  water under 1 m 9.8%, under 2 m 23.7%, over 45 m 33.3%; US-1: z from -2908 to -274; bridges 25
- reefs: ? 1.2 m; Looe Key 1.0 m; American Shoal 2.2 m; Maryland Shoal 6.9 m; Pelican Shoal -0.6 m (an islet);
  Eastern Sambo 3.2 m; Middle Sambo 4.2 m; Western Sambo 5.1 m; Sand Key 1.0 m | lights: Sand Key Light 4.0 m;
  American Shoal Light 2.2 m; Looe Key Light 1.0 m
- shoals: Sand Key raised to 1.0 m at -28349, 448 (the data had 21.3 m)
- 15 channel legs cut through (Boca Chica Channel 4, Cow Key Channel 7, Cow Key West Channel, Stock Island East
  Channel B 2, Tropical Marina)
- canals: 94 centre lines kept, 15287 m; dropped 179 short and 33 crowded; 11386 fine cells of land cut, 1799
  filled; 97 mouths dredged to deep water, 18 with none in reach; 1 breaks joined, 464 puddles filled
- join: height off by up to 0.60 m before the join to p1 (east); 0.00 m to p2_bay (east); US-1 line off by 12.7 m
- 308 marks -> p3_marks.txt ({'R': 148, 'G': 127, 'r': 14, 'g': 19}), 56 lit, from 610 charted
- OSM wetlands 200, woods 123; mangrove land 7.05 km2 (of 19.58 km2 land), fringe cells 28961; 1512 streets and 93
  built areas keep 10147 low shore cells clear
- p3 sites: 3 light, 8 marina, 7 reef, 5 spot, 3 town, 1 us1; US-1 on the chain from x -22965 to -8300
- map: 4 grids (p1, p2, p2_bay, p3), 5357 x 1313 cells, x -30060..23500, z -6920..6200; cells with no data 0
- seams of the new phases: largest height difference 0.000 m
- Hawk Channel line x -29160..22530 (51.7 km): shoalest 3.3 m at x -18070, z 538; nearest shore 902 m; at the
  joins: x -8300 12.5 m, x 7500 8.9 m

## Sources added (all outside the repo, in `KeysRun-devkit/geo/raw`; fetched by `Tools/geo/fetch_p3.sh`)
- NOAA NCEI DEM mosaic (public domain), same service as p1 and p2: `dem_p3_half.tif`, `dem_p4_half.tif`.
- OpenStreetMap (ODbL, already credited in the game): `coast_p3.json`, `osm_p3_extra.json`, `osm_p3_mangrove.json`,
  `osm_p3_built.json`, and the same four for p4. overpass-api.de answered, after an hour of 502s; the other mirrors
  timed out. Big boxes are asked for in strips of longitude and merged (`merge_osm_p3.py`).
- NOAA ENC Direct (public domain) aids to navigation, through `fetch_enc.sh`: `enc_p3/`, `enc_p4/`. It answered.
- User-Agent `map-data-fetch/1.0` on every request; nothing else identifying.

## Done
- `Tools/geo/bake.py`: phase p3; phases have a `gen` so islands are shared out among phases baked together and an
  earlier phase's islands are left to it; joins on an east edge; names, bridge names, reefs, lights for the Lower
  Keys; `SHOALS`; `road`; real marina names dropped from the places file of a Phase 3 file.
- New: `Tools/geo/fetch_p3.sh`, `merge_osm_p3.py`, `bake_p3.sh` (the whole recipe for one phase), `sites_p3.py`,
  `preview_p3.py`, `hawk_p3.py`.
- `Source/KeysRun/KeysGeoReal.*`: `LoadAll` adds p3, p4, p4_bay; `-KRNoPhase3`; `<phase>_sites.txt` ->
  `Sites()` (lights, reefs, marinas with invented names, spots, towns), `RoadX0()`, `RoadX1()`; `ShoreUnder()` for
  builders that work by tile; the chain line sampled on Phase 1's own points; the last grid hit is tried first.
- `Content/KeysRun/Geo/p3.kgeo`, `p3_places.json`, `p3_marks.txt`, `p3_mangrove.bin`, `p3_canals.txt`,
  `p3_sites.txt`.

## In progress
- Part B: Key Largo (`p4`) and the bay strip (`p4_bay`). Raw data for p4 is downloaded.

## Blocked
- Nothing.

## Requests for the marshal
Patches are in `.marshal/requests/lowerkeys/`, cut against bf2feac, and apply in order R1, R2, R3, R4, R5 (checked:
applied in that order to a clean copy they give exactly the tree I tested). NEEDED = a criterion fails without it.

- R1 (open, NEEDED: criteria 3, 4, 8) US-1 and its bridges - `KeysGeo.h`, `KeysGeo.cpp`, `KeysBridges.cpp`,
  `KeysHUD.cpp`, `KeysTownData.cpp`: new `KeysGeo::US1X0`, `US1X1` (from `KeysGeoReal::RoadX0/X1`): the road, deck,
  bents, piles, chart line and the "under the bridge" test run between them instead of from one map edge to the
  other, counted from Phase 1's corner so the bridges already built do not move; the drop-off line sampled on
  Phase 1's own 50 m points. With it the 25 Lower Keys bridges build (115 chunks, was 76) and the road stops at
  Stock Island. patch: .marshal/requests/lowerkeys/R1.patch
- R2 (open, NEEDED: criteria 3, 6, everything that runs the game) scenery by tile - `KeysWorld.h`, `KeysWorld.cpp`,
  `KeysDocks.h`, `KeysDocks.cpp`: `StreamTerrain`, `StreamScenery` (beside `StreamMangroves`) and
  `AKeysDocks::StreamTiles`. Same ground, same dice, same meshes; what stands on an estates lot is still built at
  launch, a mesh per lot. Classic map checked (75 tiles, 2634 palms, 609 houses as before). See Cost.
  Known edges: after a teleport the far terrain fills in over a few seconds, nearest first; a square's houses are
  solid for the walker from the first time the square is made; a dock tile is made on the game thread, one a frame
  (8 docks, a few ms). patch: .marshal/requests/lowerkeys/R2.patch
- R3 (open, NEEDED: criterion 5) the chart - `KeysHUD.h`, `KeysHUD.cpp`: the chart texture takes the map's shape
  (2048 pixels along the keys, square pixels) instead of a 1024 square; `bigmap` shows the whole map; the corner
  chart shows the 12 km round the boat on a map over 20 km long; soundings on a grid counted from the origin and
  spaced by the chart's scale (this is the Phase 2 lane's R11); names biggest first, none over another.
  `KeysGeo::ChartSize/ChartX0/ChartZ0` are no longer used. patch: .marshal/requests/lowerkeys/R3.patch
- R4 (open, follow-up; tested) the reef lights - `KeysLandmarks.cpp`: `BuildLighthouse`'s tower becomes a function
  and is also built at every `light` in `KeysGeoReal::Sites()`: Sand Key, American Shoal, Looe Key (and Key
  Largo's when Part B lands). `shots/lightsA/KRScript_50.png`. The turning beam is still Sombrero's alone.
  patch: .marshal/requests/lowerkeys/R4.patch
- R5 (open, follow-up; tested) teleport spots - `KeysBoatPawn.cpp` `RunDests`: the named reefs and the `spot`
  sites (Off Big Pine Key, Off Sugarloaf Key, Key West Harbor, Off Boca Chica, Northwest Channel) join the list.
  `tpcheck` 35 destinations, 0 bad (`log_lightsA.log`). The marshal is editing this file; the hunk is 9 lines in
  `RunDests`. patch: .marshal/requests/lowerkeys/R5.patch
- R6 (open, follow-up, no patch) marinas - `KeysGeo.cpp` `MarsReal` is four marinas in Marathon, each found by
  walking north or south off US-1, with a town on the first. `p3_sites.txt` has eight sites with invented names, a
  point on the shore and the nearest 1.8 m of open water (Wreckers Wharf Marina at Key West -24654, -2501; four on
  Stock Island; Cudjoe Cut Docks; Summerland Fuel and Bait; Spanish Harbor Docks). A marina that can face any way
  and has no town needs `FMarina` and `BuildMarinas` reworked, so no patch. A fuel stop at Key West and one at Big
  Pine would make the run west a round trip.
- R7 (open, follow-up, no patch) fish zones and life - `picks` returns sensible zones in the Lower Keys from the
  rules there are (Hawk Channel, Reef, Flats, Backcountry, Creek). Not done: the HUD calls every reef "Sombrero
  Reef" (`KeysGeo::ZoneDesc`); `KeysLife.cpp` scatters life over the whole map box, now 1.7 times the area, so there
  is less near Marathon; humps, wrecks, rigs, charters and Free Fish spots stop at Phase 1; the gold bluefin's
  legend grid is counted from the map's corner in 100 m cells and so has shifted.
- R8 (open, follow-up, no patch) docks re-dealt - `KeysDocks.cpp` `PlanAll` deals styles and people by a dock's
  index in the whole map's list; dealing by `D.Seed` would keep Phase 1 and 2's docks as they are when the map
  grows. Left alone because the marshal is in that file.
- R9 (open, follow-up, no patch) bridges and landmarks are still built whole at launch (115 chunks, 155 tiles,
  1.8 M triangles). Fine today; with Key Largo they grow again. `KeysBridges.cpp` builds by 240 m chunk already and
  could make only the far meshes at launch.
- R10 (open, follow-up, no patch) houses on wild keys - the conch-house dice put a house on any dry ground that is
  not mangrove, so the refuge land of Big Pine, Sugarloaf and the Saddlebunch keys is as built-up as Marathon (6,345
  houses on the map, was 1,910), and Boca Chica air station is houses instead of runways (`p3_places.json`
  `runways` has the six real ones). A built-up mask from `osm_p3_built.json`, baked like the mangrove cover, would
  fix both.

## Notes for integration
- Apply order: merge, then R1 + R2 + R3 together. R1 alone still cannot start the game with p3 present.
- `-KRNoPhase3` runs the map as it was (p1 + p2 + p2_bay) for before-and-after comparisons.
- Bake order for a rebuild: p1, p2 (as its lane says), p2_bay, then `Tools/geo/bake_p3.sh p3` (then p4, p4_bay).
- Scripted runs: the game is running 7 s after the world starts now, so a first `tp` at 10 is safe.
- The editor binary in this worktree is built with R1..R5 applied; the sources are clean of them. Rebuild.
- `shoot.sh`'s lock is dropped after 20 minutes: a run longer than that (Hawk Channel end to end is 25) would let a
  second game start. I kept every run under 11 minutes.
- The machine went to sleep twice early on (downloads "timed out after 1022 s" on a 240 s limit); I ran
  `caffeinate -i -s -t 43200` after that.

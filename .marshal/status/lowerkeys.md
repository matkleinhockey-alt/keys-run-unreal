# lowerkeys

PART A READY (the Lower Keys). PART B READY (Key Largo and the bay north of the map).

Status: READY FOR MERGE
Updated: 2026-11-22 02:40 EST
Branch: lane/lowerkeys (see `git log`; this file is in the latest commit)

Read first:
- The user said only "start phase 3". The split (Part A the Lower Keys, Part B Key Largo), the order and the ten
  criteria are the marshal's reading of it, from the brief. Everything under "Decisions I made on my own" is mine;
  nobody was there to ask.
- Merging this branch alone puts the Lower Keys, Key Largo and the bay into the map (the loader reads `p3`, `p4` and
  `p4_bay` when the files are there) and the game then cannot start: without R2 it built 550 terrain tiles, 7,600 houses and 1,426 docks (14 million triangles)
  at launch and the GPU gave up before the first frame after 4.5 minutes (`log_p3a.log`). **Apply R1, R2 and R3
  with the merge**, or start with `-KRNoPhase3` (my switch in `KeysGeoReal`: the map as it was).
- With R1 + R2 + R3 applied all ten criteria pass for both parts, and the game starts faster and lighter than it
  did before Phase 3: 8.0 s to a running game on the whole 65.6 km map (was 22 s on 31.8 km), 5.9 - 6.5 GB peak
  (was 7.3 GB).
- The map is now x -30060..35500, z -13720..6200: 65.6 km by 19.9 km, six files, 807 islands, 971 marks.
- All patches are cut against bf2feac. I checked them against `main` as it stood at 02:30 (1c7e877, the dock
  collision commit): R1..R6 apply cleanly in order.

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
- [x] 7 Requests - R1..R6 written as patches, R7..R11 as notes; see Requests. R1, R2, R3 are needed.
- [x] 8 Nothing else moves - `tpcheck` 23 destinations, 0 bad, every line identical to the baseline
  (`base_tp.txt` vs `after_new2_tp.txt`; 35 destinations, 0 bad with R5). `dk:audit` zero overlaps (416 docks).
  Boot Key flat, the reef and the Lower Matecumbe canal: same trees, houses, palms, bushes, coral
  (`shots/base1/KRScript_50..52.png` vs `shots/after_new2/KRScript_50..52.png`, side by side in
  `shots/compare_A.png`). What did change on Phases 1 and 2: the private docks are dealt their style and lift by
  their number in the whole map's list, so with 256 more docks west of them they come out re-dealt (Grassy Key 46 ->
  47, Marathon 28 -> 26, Plantation Key 33 -> 40, Upper Matecumbe 13 -> 11; the canal shot's dock is a different
  build of dock on the same wall). The Phase 2 lane saw the same. R9 would stop it. The corner chart now shows 12 km
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

## Part B (Key Largo) - acceptance criteria
- [x] 1 Data - `p4.kgeo`, x 23500..35500, z -13720..6200 (1201 x 1993 cells), joined to p2 along x = 23500, and
  `p4_bay.kgeo`, x -30060..23500, z -13720..-6920 (5357 x 681 cells: Florida Bay to the mainland at Flamingo, and the
  Gulf north of the Lower Keys), joined to p3, p2_bay and p2 along z = -6920 and to p4 along x = 23500. Each with
  `_places.json`, `_marks.txt`, `_mangrove.bin` (with `--built`), `_sites.txt`; `p4_canals.txt` (the bay strip has no
  canal neighbourhoods). Whole-map preview looked at: `geo/map_p4_game.png`. Names Key Largo (912 ha in p4; its
  south end is p2's "Tavernier (Key Largo)"), Rodriguez Key, Rattlesnake Key, Boggy Key, Totten Key; in the bay Black
  Betsy Keys, Russell Key, Pass Key, Big Key, Palm Key and more (29 named). `bridges` has the Jewfish Creek Bridge
  (27798, -9053 to 28230, -8420) and `side_bridges` has it and the Card Sound Bridge with both ends. `waters`:
  Largo Sound, Blackwater Sound, Barnes Sound, Card Sound, Tarpon Basin, Buttonwood Sound. `reef_sites`: Molasses
  Reef 5.9 m, French Reef 1.2 m, Grecian Rocks, Key Largo Dry Rocks, Elbow Reef 2.2 m, Carysfort Reef 1.3 m, Pickles
  Reef. `lights`: Molasses Reef, Elbow Reef, Carysfort Reef.
- [x] 2 No seam - `geo/p4_checks.txt`: p2 | p4 1313 cells, p4_bay | p3 2177, p4_bay | p2_bay 1581, p4_bay | p2 1601,
  p4_bay | p4 681: heights differ by at most 0.000 m on every one, shore distance 0.00 m; no cell of the map's box
  without data. Top-down across the joins, `shots/p4_verify/sheet.jpg` rows 4 and 5 (`KRScript_56..59.png`):
  x 23500 on Key Largo (US-1 runs across it unbroken), the corner 23500, -6920, and z -6920 at x 15000 and
  x -15000: no step. All six phases re-baked into a scratch folder with this branch's `bake.py` came out byte for
  byte (24 files compared: every `.kgeo`, `_places.json`, `_canals.txt`, and p3 / p4 / p4_bay's marks, mangrove
  cover and sites).
- [x] 3 The game loads all of it - "the map is 6 grids, 6557 x 1993 cells ... Phase 3 50.9 MB in 13 ms"
  (`log_p4_verify.log`); the files cover the whole box. Three spots: off Rock Harbor 25170, -5110 Hawk Channel 3.6 m
  (`shots/p4_verify/KRScript_50, 51.png`); Largo Sound 27570, -6640 Hawk Channel 2.0 m (`shots/p4a/KRScript_52,
  53.png`; the sound's charted middle, 27390, -6660, is a 0.4 m flat in the game and the boat sits aground there:
  `p4_verify/KRScript_52, 53.png`); off Carysfort Reef 33770, -8350 Reef 7.4 m (`shots/p4_verify/KRScript_54.png`
  chase, `shots/carys/KRScript_51.png` top-down with the reef and R4's lighthouse). Off Flamingo 10250, -12190
  Backcountry 2.0 m (`p4_verify/KRScript_60.png`).
  The `p4a` shots were taken before `p4_bay` existed; the place and the p4 file are the same.
  One oddity, not the map's: from straight above at full height (`cam=0,25,3`) that one spot off Carysfort renders
  flat grey (`p4_verify/KRScript_55.png`); at `cam=0,25,1.5` or `cam=0,14,3` it is the reef (`shots/carys/`), and
  the same camera is fine off Elbow Reef and at 33000, -7400. It looks like the camera sitting in a cloud. Not
  chased further.
- [x] 4 A boat can make the trip - `log_hawkB.log`: 18 picks 20 s apart, x 21910 (off Tavernier) -> 35127 (past the
  top of Key Largo; Carysfort Reef is at 34020), never stopped, 4.1 m or more under her all the way, 6.2 m at the
  join (x 23509); zone Hawk Channel to x 30500, Reef beyond (the channel narrows among the reefs off north Key
  Largo). With `hawkA` (Key West to Marathon) and the Phase 2 lane's run (Marathon to Tavernier) that is the whole
  trip, in three runs because `shoot.sh` drops its lock after 20 minutes. On the data: the line x -29160..34530
  (63.7 km) is never under 2.6 m (100 m of it under 3 m, at x 29730). Marked channels: p4 142 legs, 3 with no water
  way round (Hawk Channel Garden Cove 26-27, Mariners Club Marina 6-8, Tarpon Basin 42-44); p4_bay 5 legs, all open.
  `hawkB` ran before `p4_bay` was baked (it is all on the ocean side, in p2 and p4).
- [x] 5 The chart - R3 as in Part A. `bigmap` on the whole map: `shots/p4_verify/KRScript_00.png`, east half
  enlarged in `crop_east.jpg`: Key Largo, Rattlesnake Key, Boggy Key, Black Betsy Keys, Russell Key, the mainland
  (unnamed) and the Jewfish Creek bridge's line (R6).
- [x] 6 Cost - whole map with R1..R6: world start -> first frame 5.9, 6.0, 6.0, 5.9 s; -> game running 7.9, 8.1,
  8.0, 8.0 s; peak memory 6.20, 5.89, 6.34, 5.86 GB (`p4_verify`, `perfB`, `carys`, `p4b`). Loader: 75 MB, 23 ms.
  Under both limits and under the figures from before Phase 3. See Cost.
- [x] 7 Requests - the same R1, R2, R3 are all Part B needs; R6 (new) builds the two bridges off the chain line.
- [x] 8 Nothing else moves - `tpcheck` 48 destinations, 0 bad; the 23 that existed before are identical to the
  baseline line for line (`base_tp.txt` vs `perfB_tp_old.txt`). `dk:audit` zero overlaps (545 docks). The three
  screenshot spots were compared in Part A (`shots/compare_A.png`) and not shot again side by side with the two new
  files in: nothing those spots read changed (same p1 and p2 files, same hub patches). Docks are re-dealt again as
  the list grows (R9).
- [x] 9 The classic map - `log_classicC.log`: no `KeysGeoReal` line, no Fatal or Ensure, the same six "Error"
  lines, 75 terrain tiles, 2634 palms, 609 houses, `tpcheck` 19, 0 bad.
- [x] 10 No slowdown - `perfB`, Medium, 1600 x 900, frame / game / draw / GPU ms (30 fps cap):
  marina 33.34 / 11.61 / 6.09 / 26.04; flat 33.34 / 10.76 / 6.34 / 27.04; canal 33.46 / 9.78 / 5.54 / 24.38 (before:
  33.34 at all three); off Key West 33.34 / 10.74 / 6.45 / 29.66; off Key Largo (Rock Harbor) 33.34 / 11.00 / 8.00 /
  32.35; Largo Sound 33.35 / 11.46 / 6.93 / 29.08. Off Key Largo the GPU has 1 ms to spare under the cap: any
  graphics work there will show first.

## Cost
16 GB M4, editor build, 1600 x 900. "Game running" = frame 10, which is after `play` and `gfx=1` have re-applied the
graphics settings (the two stalls that grow with the number of meshes).

| | before (bf2feac) | Lower Keys, no hub change | Lower Keys + R1 + R2 (+R3) | whole map (all six files) + R1..R6 | old map + R1 + R2 (`-KRNoPhase3`) |
|---|---|---|---|---|---|
| `KeysGeoReal` data | 23.9 MB, 14 ms | 40.3 MB, 22 ms | same | 74.8 MB, 23 ms | 23.9 MB |
| world start -> first frame | 12.7 s (13.7 busy) | none in 4.5 min: GPU crash | 4.8, 4.9, 4.9, 4.8 s | 5.9, 6.0, 6.0, 5.9 s | 4.0 s |
| world start -> game running | 22.2 s (27.2 busy) | - | 6.7, 6.8, 6.7, 6.7 s | 7.9, 8.1, 8.0, 8.0 s | 5.7 s |
| peak memory (RSS) | 7.31, 7.33 GB | 6.1 GB and climbing when it died | 5.93, 6.60, 5.97, 6.61 GB | 6.20, 5.89, 6.34, 5.86 GB | 5.91 GB |

Runs: before `base2`, `base1` (base1 with another build running: load 8 - 15); no hub change `p3a`; after
`after_new`, `after_new2`, `perfA`, `p3_verify`; whole map `p4_verify`, `perfB`, `carys`, `p4b` (p4b without
`p4_bay`); old map `after_old`. Only `base2` is a quiet-machine "before": one run, not the two the brief asked for.
Peak memory depends on where a run goes: these teleport round the map. The whole map costs 1.2 s more than the
Lower Keys alone, in the hub's own whole-map grids (depth, colour, mangrove nearness: 13 million cells, was 4.2). The marshal's log says "about 12 s" for the
old map; I measured 22 s to frame 10 with the same script as the after runs, so compare within this table.
What R2 does: terrain (512 m tiles, within 9.4 km), palms / bushes / houses (1200 m squares, within 3.2 km) and docks
(600 m tiles, within 3 km) are made near the boat and dropped behind it, like the mangroves. 602 of 2730 terrain
tiles and 162 scenery squares can have land (whole map: 1071 of 5031 and 292); 25 tiles and 7 squares are made
before the first frame.
Not streamed, and affordable: bridges (115 chunks, 906 k triangles near; Key Largo adds none, its road is on land)
and landmarks (227 tiles, 1.17 M triangles on the whole map) (R10).

## Decisions I made on my own
- Phase names and rectangles: `p3` = the Lower Keys, x -30060..-8300, z -6920..6200 (the same north and south edges
  as p2, so Part A needs no bay strip of its own). `p4` = Key Largo, x 23500..35500, z -13720..6200: east edge past
  Carysfort Reef (34020) and Ocean Reef (33470), north edge five more 1360 m steps, which takes in the Card Sound
  bridge (z -12300). `p4_bay` = the bay and Gulf north of everything else, x -30060..23500, z -13720..-6920.
- Squaring the map off with `p4_bay` rather than leaving the north-west empty: the loader copes with a hole (it reads
  the nearest file's edge) but the hole then plays as islands smeared north in stripes, with houses and docks on
  them (`log_p4a.log`: 131 docks on smeared "Manatee Keys" and "Nest Keys"). The strip is real Florida Bay as far
  as Flamingo. Its west third is open Gulf with nothing in it; the file is 18 MB in the repo.
- The chain line past US-1's turn: at x 27000 US-1 leaves the island for Jewfish Creek and then runs up the map to
  the mainland, which a line of "z at each x" cannot follow. From 27000 the chain line follows the middle of Key
  Largo itself to its north end (x 33940) and is held from there; `p4_sites.txt` says the road is on it only as far
  as 27000. So the game's bay side / ocean side, reef and drop-off all read right along north Key Largo, and the
  last 1.5 km of US-1 on the island, Lake Surprise's causeway and the 18 Mile Stretch are not drawn as road (R6
  builds the Jewfish Creek bridge itself).
- The mainland is left without a name on purpose: the hub builds houses on named keys, and an island name tag
  ("Cross Key") sat on it. It shows on the chart as unnamed land.
- `p4_bay`'s elevation is fetched at 1 arc-second (the game grid's own 10 m) instead of 1/2: the box is 1.65 degrees
  wide and would be 240 MB. Its mainland is counted as land when the data has it above -0.15 m (`low_land`): the
  marsh there sits at the height of the tide and failed the 0.1 m test the other phases use.
- The Hawk Channel line is now found as a least-cost path (deep water, off the shore, inside the reef) instead of
  "the middle of the widest deep stretch", which ran over a 1.6 m bank off Key Largo. `hawkA` was steered by the old
  line; `hawkB` by the new one.
- I wrote R4, R5 and R6 (lights, teleport spots, the two side bridges) as tested patches although the brief only
  asked for the needed ones to be applied locally; they are follow-ups and nothing depends on them.
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

## Script summary lines, Part B (`Tools/geo/bake_p3.sh p4`, then `p4_bay`)
- p4: grid 1201 x 1993 at 10.0 m, 12.1 MB; land 10.52%, islands 127 (17 named), highest 12.9 m, deepest 620.2 m;
  water under 1 m 1.4%, under 2 m 8.4%, over 45 m 57.9%
- US-1 bridges: Marvin D Adams Waterway Bridge 26918..26936; Manatee Creek Bridge 27125..27130; ? 27402..27409;
  Jewfish Creek Bridge 27798..28230
- reefs: Molasses Reef 5.9 m; French Reef 1.2 m; Grecian Rocks 1.2 m; Key Largo Dry Rocks 1.4 m; Elbow Reef 2.2 m;
  Carysfort Reef 1.3 m; Pickles Reef 1.9 m | lights: Molasses Reef Light 1.5 m; Carysfort Reef Light 1.3 m; Elbow
  Reef Light 2.2 m
- chain line: US-1 to x 27000, then the middle of the island it is on (912 ha) for 138 samples to x 33937, held
  from there
- bridges off the chain line: Card Sound Bridge 29708, -12328 to 29916, -12136; Jewfish Creek Bridge 27802, -9055
  to 28232, -8420
- waters: Largo Sound 1.0 m; Blackwater Sound 2.4 m; Barnes Sound 2.8 m; Card Sound 3.7 m; Tarpon Basin 1.6 m;
  Buttonwood Sound 2.4 m
- 8 channel legs cut through (Angelfish Creek, Baker Cut, Dispatch Creek Channel, Grayvik, Hawk Channel Garden
  Cove, Jewfish Creek, Manatee Creek, Molasses Reef Marina Channel)
- canals: 46 centre lines kept, 8324 m; dropped 77 short and 8 crowded; 5524 fine cells of land cut, 684 filled; 60
  mouths dredged to deep water, 7 with none in reach; 1 breaks joined, 136 puddles filled
- join: height off by up to 0.00 m before the join to p2 (west); US-1 line off by 0.0 m
- 194 marks -> p4_marks.txt ({'R': 107, 'G': 87}), 31 lit, from 341 charted
- p4 mangrove: OSM wetlands 2116, woods 115; mangrove land 17.13 km2 (of 25.18 km2 land), fringe cells 10528
- p4 sites: 2 bridge, 3 light, 6 marina, 7 reef, 5 spot, 1 town, 1 us1, 6 water; US-1 on the chain from 23500 to 27000
- p4_bay: grid 5357 x 681 at 10.0 m, 18.3 MB; land 6.68%, islands 144 (29 named), highest 9.5 m, deepest 21.0 m;
  water under 1 m 7.0%, under 2 m 21.0%
- join: height off by up to 0.45 m before the join to p3 (south), 0.18 m to p2_bay, 0.83 m to p2, 0.18 m to p4 (east)
- 16 marks -> p4_bay_marks.txt ({'R': 10, 'G': 5, 'r': 1}), 7 lit, from 19 charted
- p4_bay mangrove: OSM wetlands 3264, woods 368; mangrove land 10.17 km2 (of 24.37 km2 land), fringe cells 11076
- p4_bay sites: 1 marina (Snake Bight Landing, at Flamingo), 1 spot (Off Flamingo)
- map: 6 grids (p1, p2, p2_bay, p3, p4, p4_bay), 6557 x 1993 cells, x -30060..35500, z -13720..6200; cells with no
  data 0
- seams of the new phases: largest height difference 0.000 m
- Hawk Channel line x -29160..34530 (63.7 km): shoalest 2.6 m at x 29730, z -6458; nearest shore 239 m; under 3 m
  for 100 m; at the joins: x -8300 10.7 m, x 7500 9.0 m, x 23500 6.3 m

## Script summary lines, Part A (`Tools/geo/bake_p3.sh p3`, which re-bakes byte for byte)
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
- NOAA NCEI DEM mosaic (public domain), same service as p1 and p2: `dem_p3_half.tif`, `dem_p4_half.tif`,
  `dem_p4_bay_half.tif` (this one at 1 arc-second, whatever its name says).
- OpenStreetMap (ODbL, already credited in the game): `coast_p3.json`, `osm_p3_extra.json`, `osm_p3_mangrove.json`,
  `osm_p3_built.json`, and the same four for p4. overpass-api.de answered, after an hour of 502s; the other mirrors
  timed out. Big boxes are asked for in strips of longitude and merged (`merge_osm_p3.py`).
  For p4_bay the four files are the p1..p4 files put together plus two new pieces north of them (`p4_bay_*_A.json`,
  the Gulf, which came back empty: there is nothing there; `p4_bay_*_B.json`, Florida Bay up to the mainland; the
  Everglades' wetlands only came in four strips).
- NOAA ENC Direct (public domain) aids to navigation, through `fetch_enc.sh`: `enc_p3/`, `enc_p4/`, `enc_p4_bay/`.
  It answered.
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
  `p3_sites.txt`; the same for `p4`; `p4_bay.kgeo`, `p4_bay_places.json`, `p4_bay_marks.txt`, `p4_bay_mangrove.bin`,
  `p4_bay_sites.txt`. 54 MB of new data in all.
- Part B in `bake.py`: phases p4 and p4_bay; the chain line's axis on a named island (`chain`); `chain_from` as a
  list; `side_bridges`, `waters`; the mainland unnamed; `low_land`; bridge spans one above the other kept apart.
  `preview_p3.py`: every shared edge found and checked; the Hawk Channel line as a least-cost path.

## In progress
- Nothing.

## Blocked
- Nothing.

## Requests for the marshal
Patches are in `.marshal/requests/lowerkeys/`, cut against bf2feac, and apply in order R1 .. R6 (checked: applied
in that order to a clean copy they give exactly the tree I tested, and they apply to `main` at 1c7e877). NEEDED = a
criterion fails without it. R1, R2 and R3 are needed; everything else is a follow-up.

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
  Largo's: Molasses, Elbow, Carysfort). `shots/lightsA/KRScript_50.png`, `shots/carys/KRScript_51.png`. The
  turning beam is still Sombrero's alone.
  patch: .marshal/requests/lowerkeys/R4.patch
- R5 (open, follow-up; tested) teleport spots - `KeysBoatPawn.cpp` `RunDests`: the named reefs and the `spot`
  sites (Off Big Pine Key, Off Sugarloaf Key, Key West Harbor, Off Boca Chica, Northwest Channel) join the list.
  `tpcheck` 48 destinations on the whole map, 0 bad (`log_perfB.log`). 9 lines in `RunDests`.
  patch: .marshal/requests/lowerkeys/R5.patch
- R6 (open, follow-up; tested) the bridges off the chain line - `KeysLandmarks.h`, `KeysLandmarks.cpp`,
  `KeysHUD.cpp`: `BuildSideBridges` builds a plain humped concrete deck on pile bents at every `bridge` in
  `KeysGeoReal::Sites()` (Jewfish Creek Bridge 767 m, Card Sound Bridge 283 m), and the chart draws them
  (`shots/p4b/KRScript_51.png`, on `sheet.jpg` top right). Not done: the piles are not solid to the boat, and the
  road either side of them (Lake Surprise's causeway, the 18 Mile Stretch, Card Sound Road) is not drawn.
  patch: .marshal/requests/lowerkeys/R6.patch
- R7 (open, follow-up, no patch) marinas - `KeysGeo.cpp` `MarsReal` is four marinas in Marathon, each found by
  walking north or south off US-1, with a town on the first. `p3_sites.txt` has eight sites with invented names, a
  point on the shore and the nearest 1.8 m of open water (Wreckers Wharf Marina at Key West -24654, -2501; four on
  Stock Island; Cudjoe Cut Docks; Summerland Fuel and Bait; Spanish Harbor Docks); `p4_sites.txt` has six (Mosquito
  Bank Marina at Rock Harbor, Hammock Point Docks, Sound Side Landing, three at the top of Key Largo) and
  `p4_bay_sites.txt` one at Flamingo. A marina that can face any way
  and has no town needs `FMarina` and `BuildMarinas` reworked, so no patch. A fuel stop at Key West and one at Big
  Pine would make the run west a round trip.
- R8 (open, follow-up, no patch) fish zones and life - `picks` returns sensible zones in the Lower Keys from the
  rules there are (Hawk Channel, Reef, Flats, Backcountry, Creek). Not done: the HUD calls every reef "Sombrero
  Reef" (`KeysGeo::ZoneDesc`); `KeysLife.cpp` scatters life over the whole map box, now 1.7 times the area, so there
  is less near Marathon; humps, wrecks, rigs, charters and Free Fish spots stop at Phase 1; the gold bluefin's
  legend grid is counted from the map's corner in 100 m cells and so has shifted. The whole map is 3.1 times the
  area it was. Largo Sound's middle plays as a 0.4 m flat (charted 1.0 m, drawn down by the bank rule in
  `RealDepthAt`): the boat grounds there.
- R9 (open, follow-up, no patch) docks re-dealt - `KeysDocks.cpp` `PlanAll` deals styles and people by a dock's
  index in the whole map's list; dealing by `D.Seed` would keep Phase 1 and 2's docks as they are when the map
  grows. Left alone because the marshal is in that file.
- R10 (open, follow-up, no patch) bridges and landmarks are still built whole at launch (115 chunks, 155 tiles,
  1.8 M triangles). Fine today; with Key Largo they grow again. `KeysBridges.cpp` builds by 240 m chunk already and
  could make only the far meshes at launch.
- R11 (open, follow-up, no patch) houses on wild keys - the conch-house dice put a house on any dry ground that is
  not mangrove, so the refuge land of Big Pine, Sugarloaf and the Saddlebunch keys is as built-up as Marathon (6,345
  houses on the map, was 1,910), and Boca Chica air station is houses instead of runways (`p3_places.json`
  `runways` has the six real ones). On the whole map there are 8,493 houses, and two docks on Everglades keys
  (Corinne Key, Palm Key). A built-up mask from the `osm_*_built.json` files, baked like the mangrove cover, would
  fix all of it. The mainland's marsh that OpenStreetMap does not map as wetland draws as grass.

## Notes for integration
- Apply order: merge, then R1 + R2 + R3 together. R1 alone still cannot start the game with p3 present.
- `-KRNoPhase3` runs the map as it was (p1 + p2 + p2_bay) for before-and-after comparisons.
- Bake order for a rebuild: p1, p2 (as its lane says), p2_bay, then `Tools/geo/bake_p3.sh p3`, `p4`, `p4_bay`.
- Scripted runs: the game is running 8 s after the world starts now, so a first `tp` at 10 is safe.
- The editor binary in this worktree is built with R1..R6 applied; the sources are clean of them. Rebuild.
- NOAA's names for some aids to navigation carry a business's name ("Mariners Club Marina 6", "Molasses Reef Marina
  Channel", "Tropical Marina" in `p3_marks.txt` / `p4_marks.txt`). The game uses mark names only to group a
  channel's marks; nothing shows them. Worth knowing before anything does.
- Marina names are invented to the best of my knowledge and checked only against OpenStreetMap's list for the same
  area. Somebody should read `MARINA_NAMES` in `sites_p3.py` once before any of them is shown.
- Not verified: anything at night or underwater in the new areas; the packaged build; R2's tiles under a real player
  for longer than the two Hawk Channel runs (10 and 5 minutes at 43 m/s, no stop); the Hawk Channel run on the final
  Lower Keys bake (it ran on the one before, which differs in a few cells of Cow Key Channel); whether the grey
  top-down view off Carysfort is a cloud.
- One leftover outside the repo: `KeysRun-devkit/geo/raw/p4_bay_mangrove_B.json.tmp` (695 bytes, a failed
  download). A safety check stopped me deleting it by pattern; it is harmless.
- `shoot.sh`'s lock is dropped after 20 minutes: a run longer than that (Hawk Channel end to end is 25) would let a
  second game start. I kept every run under 11 minutes.
- The machine went to sleep twice early on (downloads "timed out after 1022 s" on a 240 s limit); I ran
  `caffeinate -i -s -t 43200` after that.

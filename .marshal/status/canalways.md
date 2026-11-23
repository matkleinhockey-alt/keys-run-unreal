# canalways

Status: READY FOR MERGE
Updated: 2026-11-23 13:05 EST
Branch: lane/canalways (this file in the last commit)

Evidence is outside the repo in `KeysRun-devkit/lanes/KeysRun-canalways/` (`shots/<tag>/KRScript_NN.png`,
`log_<tag>.log`), written `<tag>` below. Three states were run: main as it is (tags `base`, `before2`, `before3`),
the lane alone (what is committed: `lane_only`, `lane_only2`, the trips, the soundings) and the lane with R1
applied (`after2`, `after3`, `looks`, `ways10`, `near_a`, `near_b`, `ways_largo`). Criterion 6 is the request R1;
every other criterion passes on the lane alone.

## Whose reading this is

The user's words, as the marshal passed them on:
- "Open the 46 canals that have no way out to sea, mostly in the Lower Keys and Key Largo." Criteria 1 - 8 are the
  marshal's reading of that (`.marshal/prompts/canalways.md`).
- "make it the canals lead to open water or they shortcut going into mainland, just make it realistic". Criterion 9
  is the marshal's reading of that, relayed while the lane ran.
- "dont make too many canals either keep it realistic". Criterion 10 is the marshal's reading of that, relayed later.

## What I guessed or decided myself

- "Open sea" is what the game's own test calls it: water 1.5 m deep, 60 m from any shore and from any canal,
  reached through 1.0 m of water within 1400 m of the canal's middle (`KeysSea::LogCanalWays`).
- A way ends in the nearest water 1.5 m deep that is joined to the open sea, not at the first 1.0 m: at dead low
  tide (-0.32 m) that leaves 1.18 m. The marshal's reading says "water of 1 m or more"; 1.5 m is my margin.
- Ways are 16 m wide and play 1.9 m deep, canal lines 24 m and 2.1 m: the sizes the bake already used for every
  other canal and way. I took "the width and depth a real dredged residential channel has" to mean those, since
  boats in the game are full size on a map compressed 1:3.
- "Water in life" is the OpenStreetMap coastline in the raw downloads, drawn at 2.5 m and, for inlets narrower
  than that, again at 0.5 m. I had no imagery and fetched nothing. The only chart data in the raw downloads is
  aids to navigation (beacons, buoys, lights); "a charted channel" therefore means a marked one.
- The Layton canal: the hand-added straight cut through mangrove is gone. The tool makes Layton's way along the
  real creek the coastline has there (112 m of canal line, 15 m of way), which is a different line from R3 of
  water2.
- The canal count. The game counts centre lines. To keep it at 195 the tool writes a real mouth or link as part of
  the centre line it continues (14 of the 18 are), and three pairs of canals that the real water joins end to end
  in the Lower Keys and one pair on Key Largo became one line each. One mouth on Upper Matecumbe leaves its canal
  29 m short of the canal's dead end: its line runs to the dead end and back to the turning (the same water, drawn
  once over). Four canal lines stand alone (all on Ramrod and Big Coppitt, T-junctions).
- A sliver of grid land under 10 m on a route is dredged as a way, not walled as a canal.
- Trips are driven by a script that cannot see: heading and throttle are set ahead of time along the bake's lines.
  "Idle" is throttle 0.05 (about 5 kn), "15 kn" is throttle 0.165 (15.6 - 16.7 kn measured). Low tide was held
  for every trip and sounding, which is harsher than the brief asks.
- The six trip canals were chosen for routes a blind script can follow (few bends), not at random. To cover the
  rest I sounded every added line in the game instead (criterion 2).
- Daybeacons (R1): which ways "cross a flat" is my rule: 60 m of way or more, ending 30 m or more off the shore,
  not at a junction with another way, no mark within 45 m. It applies to the bake's old ways as well as the new.

## Acceptance criteria

- [x] 1. The list - table below. `log_base.log`: "KRCanal 195 canals, 46 with no 1.0 m water out to the open sea"
  (44 `KRCanal shut` lines: two canals share a network with one already listed). The tool's own model of the
  hull's water finds the same 46. Why each was shut: 17 are ponds on the grid, 9 have a pinched mouth, 15 have a
  flat outside that plays too shallow, 5 are joined to one of those. None is cut off by a road or bridge and none
  turned out not to be a canal: every one has water to the sea in the real coastline.
- [x] 2. Every canal has a way out - `log_lane_only2.log` (lane alone) and `log_after3.log` (with R1): "KRCanal
  195 canals, 0 with no 1.0 m water out to the open sea". None left shut. Top-down shots of ten ways:
  `shots/ways10/KRScript_50..56.png` (Lower Keys 50 - 53, Upper Keys 54 - 56) and `shots/ways_largo/KRScript_50..52.png`
  (Key Largo; the three Key Largo frames of `ways10` show the pause menu, see Notes). Beyond the brief: the boat
  was put down every 12 m along every added or changed line at dead low tide (`log_sound_a.log`,
  `log_sound_b.log`, read by `Tools/geo/canalways_sound.py`): 638 of 647 spots sounded, least water 1.50 m, no
  "aground" line; nine spots (one each on nine ways) were missed by the script's timing.
- [x] 3. Made by the tool - `Tools/geo/canals.py p2|p3|p4 --check` prints "the same, byte for byte" for all three.
  Also: `bake_p3.sh p3`, `bake_p3.sh p4` and `bake.py p2`, run whole into a scratch copy of the geo dir with the
  final tool, left every file identical to the committed one (.kgeo, marks, mangrove, places, sites, canals).
  The diff against main: p2 53 -> 52 canal lines and 45 -> 53 ways (three lines of shut canals extended with
  their real mouth; the hand-added Layton line removed, its water now part of Layton's own line; 8 ways added);
  p3 95 -> 97 canal lines and 97 -> 135 ways (9 lines rewritten as 7 longer ones, 4 new lines, 38 ways added);
  p4 47 -> 46 canal lines and 60 -> 67 ways (4 lines rewritten as 3, 7 ways added). Every other line is unchanged
  and in the same order. Not quite "canals open before keep the same lines": four canals that were open are now
  one centre line with the shut canal the real water joins them to (same points, one line instead of two).
- [x] 4. A boat can make the trip - six canals, each out at idle, out at 15 kn and back in at idle at dead low
  tide, no "aground" line in any run (`Tools/geo/canalways_trip.py read <log> <route>`): Lower Keys canal 119
  (-10205, -2005; `log_trip_lower.log`, least water 1.41 m) and canal 141 (-17252, -3298; `log_trip_lower3.log`,
  0.88 m at 15 kn past the way's end, 1.02 m in it); Upper Keys canal 10 (12960, -1008) and canal 35 (20115,
  -3402; `log_trip_upper.log`, 1.50 m); Key Largo canal 190 (27082, -11065; `log_trip_largo2.log`, 1.22 m) and
  canal 172 (28002, -8778; `log_trip_largo.log`, 0.88 m at the canal's own mouth, 1.43 m at speed). Strayed at
  most 7.4 m from the line. Two runs were upset from outside and repeated (see Notes); the first open-loop run
  for canal 141 put the boat on the canal's bank on the way back in (`trip_lower`, first version): the script
  turned 35 m early. That was the script, and the repeat is clean.
- [x] 5. They look right - chase view and top-down at four: `shots/looks/KRScript_50, 51.png` (way across the
  Cudjoe flat), `52, 53` (Little Torch mouth, walled), `54, 55` (Lower Matecumbe mouth, walled), `56, 57` (Key
  Largo way, with its two daybeacons); `58` is the diver's view at a way on Sugarloaf: pale bottom, nothing
  growing near. Ways read as darker, calm channels with clean edges; no house, dock or tree stands in any cut I
  shot (35 top-down frames). `dk:audit`: 545 docks, every "standing on" count 0 (`log_after3.log`,
  `log_lane_only2.log`); no dock lies within 18 m of a new way's centre, and the twelve docks within 22 m of a
  new canal line are canal docks on its wall. Not shown: the bottom inside a walled cut from under water.
- [x] 6. Marked, as a request - R1. With it: "255 ways out of the canals, 45 of them marked with a pair of
  daybeacons at the outer end" (`log_after3.log`); the 45 `KRWayMark` lines are the same, to the decimetre, as
  `python Tools/geo/canalways.py beacons`. Seen in `shots/looks/KRScript_56.png` and `shots/ways10/KRScript_51,
  53.png`. Positions below.
- [x] 7. What moved - houses 8,375 -> 8,368; docks 545 -> 545 (426 with water of their own, 119 dredged, both
  unchanged) (`log_before3.log` against `log_lane_only2.log`; R1 changes neither, `log_after3.log`). The two old
  spots look the same: `shots/base/KRScript_50.png` against `shots/lane_only2/KRScript_50.png` (-1150, 452: the
  same dock, the same trees) and `KRScript_51.png` in each (13232, -1280: the same canal, houses and palms).
- [x] 8. Nothing else moves - `tpcheck` 48 destinations, 0 bad in every run. Classic map (`log_classic.log`):
  "KRCanal: the classic map has no canals", "KRCanals: none on this map", `tpcheck` 19, 0 bad, six lines with
  "Error" in them, the same six as the real map before the lane. Start-up, map load to the second frame: main
  7.47, 7.82 s (and 9.47 in the first run of the day); lane alone 8.09, 7.31 s; with R1 8.10, 7.70 s. Perf,
  Medium, frame 33.3 ms in every clean sample; game / draw / GPU ms at `tp=13232,-1280,0`: main 10.54 / 6.10 /
  21.49 (and 9.92 / 5.70 / 20.67); lane 9.62 / 5.87 / 20.46 and 9.69 / 5.54 / 20.32; with R1 9.68 / 5.59 / 21.25
  and 9.97 / 5.75 / 25.91. In the newly opened Little Torch canal (`tp=-10103,-1985,-1.795`): main 11.38 / 12.63
  / 30.07 and 11.24 / 11.42 / 29.73; lane 10.72 / 11.35 / 30.43 and 10.87 / 9.80 / 29.72; with R1 11.03 / 7.59 /
  30.46 and 11.07 / 6.98 / 30.41. Game and draw are no slower; GPU is within 3% except one R1 sample at the old
  canal (25.91, +21%) whose pair read 21.25. Thrown out: `before2`'s first sample (the frame cap was off, 48 fps)
  and `after1` (taken while the machine was busy: 37 ms frames).
- [x] 9. Realistic: every way leads to open water, none through land that is land in life - list "Every line
  added" below. The tool never crosses real land: a route keeps to the coastline's water and, where an inlet is
  narrower than 2.5 m, to the shoreline's own cells, and each such stretch is checked against the coastline drawn
  at 0.5 m (water must join its two ends within 6 m of the route). US-1's roadbed is never crossed. Of the 71
  lines, 18 are real mouths or links the grid had closed, 32 are grid pinches, 19 cross a flat by the shortest
  clear line and 2 run along a marked channel. No way, old or new, ends in water that is not joined to the open
  sea (model check over all 255). The 21 ways that pass within 30 m of land other than their canal's banks are
  shot top-down: `shots/near_a/KRScript_50..57.png`, `shots/near_b/KRScript_50..56.png`, `shots/near_c/KRScript_50..52.png`
  (`near_a` 58 - 60 show the pause menu and a cast: repeated as `near_c`). The first version cut 12 m of real
  land at one Ramrod Key canal; with land forbidden the three Ramrod canals now leave by the collector canal the
  coastline has behind them, into the marked Ramrod Key Channel (`shots/near_c/KRScript_53, 54.png`).
- [x] 10. Not too many canals - canals (centre lines) 195 before, 195 after (`KRCanals built: 195 canals` in
  `log_base.log` and `log_after3.log`); centre line 32,332 -> 33,760 m, seawall 87,918 -> 90,294 m. Ways out 202
  -> 255. New dredged water 4,981 m: 1,328 m of real mouths and links (canal line) and 3,654 m of way, for 47
  canals (the 46 and Layton). What each piece serves is listed under "New water" below: 36 pieces serve one
  canal, 4 serve two, 2 serve three; a later canal's way stops as soon as it meets water already dredged, so the
  Cudjoe and Summerland flats got one trunk with spurs. No canal that had a way out was given another. Nothing
  was left shut because nothing is shut in the coastline data. More canal water than in life: see Notes.

## The 46 (criterion 1)

Canal numbers are the game's on main (`fish:canals` on the untouched branch). "Layton" is the canal water2 opened
by hand; it was not one of the 46 and is listed because the tool now makes its way.

| Canal (index on main) | Middle x, z | Part | Why it was shut | Nearest 1 m water joined to the sea, along the route | How it is opened |
|---|---|---|---|---|---|
| 10 | 12960, -1008 | Upper Keys | a pond on the grid | 108 m | 60 m of canal line (real mouth or link) + 58 m of way, to 12765, -878 |
| 26 | 14530, -1648 | Upper Keys | a pond on the grid | 78 m | 74 m of canal line (real mouth or link) + 15 m of way, to 14638, -1708 |
| 35 | 20115, -3402 | Upper Keys | a pinched mouth | 10 m | 33 m of way, to 20052, -3400 |
| 36 | 13300, -1168 | Upper Keys | a pinched mouth | 45 m | 56 m of way, to 13465, -1178 |
| 59 | -22232, -2428 | Lower Keys | a pinched mouth | 10 m | 33 m of way, to -22142, -2485 |
| 64 | -10880, -1560 | Lower Keys | a pond on the grid | 325 m | 188 m of canal line (real mouth or link) + 144 m of way, to -10982, -1190 |
| 66 | -22352, -2470 | Lower Keys | a pond on the grid | 14 m | 26 m of way, to -22215, -2588 |
| 67 | -10845, -1670 | Lower Keys | a pond on the grid | 68 m | 74 m of canal line (real mouth or link) + 7 m of way, to -10878, -1572 |
| 68 | -16145, -2775 | Lower Keys | the flat outside is too shallow | 412 m | 403 m of way, to -15915, -2450 |
| 70 | -15800, -2098 | Lower Keys | the flat outside is too shallow | 52 m | 65 m of way, to -15932, -2448 |
| 72 | -10948, -1370 | Lower Keys | joined to canal 64, shut with it | - | with canal 64 |
| 76 | -17495, -2440 | Lower Keys | the flat outside is too shallow | 195 m | 199 m of way, to -17392, -2395 |
| 86 | -19952, -2385 | Lower Keys | a pond on the grid | 84 m | 92 m of canal line (real mouth or link) + 6 m of way, to -19912, -2490 |
| 90 | -15508, -2038 | Lower Keys | a pond on the grid | 157 m | 111 m of canal line (real mouth or link) + 120 m of way, to -15572, -2248 |
| 96 | -14968, -3280 | Lower Keys | the flat outside is too shallow | 152 m | 377 m of way, to -15298, -3112 |
| 97 | -8872, -312 | Lower Keys | a pond on the grid | 386 m | 31 m of canal line (real mouth or link) + 393 m of way, to -9132, -565 |
| 98 | -12095, -1855 | Lower Keys | the flat outside is too shallow | 76 m | 84 m of way, to -12182, -1845 |
| 107 | -10188, -1472 | Lower Keys | a pond on the grid | 68 m | 74 m of canal line (real mouth or link) + 6 m of way, to -10162, -1548 |
| 110 | -10985, -1252 | Lower Keys | joined to canal 64, shut with it | - | with canal 64 |
| 113 | -22205, -1735 | Lower Keys | a pinched mouth | 22 m | 37 m of way, to -22152, -1665 |
| 117 | -19900, -2155 | Lower Keys | the flat outside is too shallow | 360 m | 361 m of way, to -19928, -1752 |
| 118 | -14590, -3058 | Lower Keys | a pond on the grid | 117 m | 105 m of canal line (real mouth or link) + 22 m of way, to -14462, -3038 |
| 119 | -10205, -2005 | Lower Keys | a pond on the grid | 143 m | 130 m of canal line (real mouth or link) + 34 m of way, to -10022, -1958 |
| 120 | -15082, -3170 | Lower Keys | the flat outside is too shallow | 43 m | 53 m of way, to -15082, -3268 |
| 121 | -16042, -2690 | Lower Keys | the flat outside is too shallow | 14 m | 29 m of way, to -16088, -2662 |
| 122 | -16185, -2562 | Lower Keys | the flat outside is too shallow | 51 m | 56 m of way, to -16052, -2565 |
| 123 | -10085, -2060 | Lower Keys | joined to canal 119, shut with it | - | with canal 119 |
| 125 | -12932, -2425 | Lower Keys | the flat outside is too shallow | 59 m | 71 m of way, to -12865, -2385 |
| 126 | -19820, -1488 | Lower Keys | a pond on the grid | 91 m | 89 m of canal line (real mouth or link) + 14 m of way, to -19882, -1382 |
| 130 | -15952, -2565 | Lower Keys | the flat outside is too shallow | 16 m | 26 m of way, to -16018, -2542 |
| 134 | -22295, -2615 | Lower Keys | a pinched mouth | 41 m | 58 m of way, to -22265, -2718 |
| 137 | -19842, -2095 | Lower Keys | joined to canal 117, shut with it | - | with canal 117 |
| 138 | -13015, -2390 | Lower Keys | the flat outside is too shallow | 39 m | 46 m of way, to -12895, -2388 |
| 139 | -12975, -2310 | Lower Keys | joined to canal 138, shut with it | - | with canal 138 |
| 141 | -17252, -3298 | Lower Keys | the flat outside is too shallow | 177 m | 299 m of way, to -17538, -3172 |
| 143 | -15998, -2628 | Lower Keys | a pinched mouth | 17 m | 28 m of way, to -16012, -2592 |
| 144 | -23912, -2280 | Lower Keys | a pond on the grid | 27 m | 50 m of way, to -23992, -2358 |
| 145 | -13018, -2260 | Lower Keys | a pinched mouth | 34 m | 43 m of way, to -12958, -2298 |
| 146 | -22302, -2118 | Lower Keys | a pinched mouth | 20 m | 59 m of way, to -22245, -2118 |
| 155 | 26360, -6720 | Key Largo | a pinched mouth | 92 m | 101 m of way, to 26245, -6590 |
| 172 | 28002, -8778 | Key Largo | the flat outside is too shallow | 44 m | 63 m of way, to 28098, -8725 |
| 173 | 26495, -7222 | Key Largo | a pond on the grid | 94 m | 96 m of canal line (real mouth or link) + 6 m of way, to 26548, -7292 |
| 177 | 28145, -8358 | Key Largo | a pond on the grid | 23 m | 31 m of way, to 28142, -8388 |
| 179 | 28162, -8288 | Key Largo | a pond on the grid | 23 m | 33 m of canal line (real mouth or link), to 28138, -8348 |
| 183 | 27855, -8918 | Key Largo | a pond on the grid | 71 m | 57 m of canal line (real mouth or link) + 47 m of way, to 27798, -8822 |
| 190 | 27082, -11065 | Key Largo | the flat outside is too shallow | 17 m | 78 m of way, to 26982, -11125 |
| Layton (p2 line 45) | 10062, -825 | Upper Keys | a pond on the grid | 117 m | 112 m of canal line (real mouth or link) + 15 m of way, to 10158, -848 |

## Every line added or changed (criterion 9)

What the tool says of each line it wrote (`canals.py <phase> --check` prints these). Canal numbers as above.

- canal 10, canal line, 60 m, 12855, -938 to 12808, -902: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads to open water 1.5 m deep, 23 m off the shore, joined to the open sea by 1.0 m or more
- canal 10, way, 6 m, 12858, -940 to 12852, -938: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.5 m deep, 23 m off the shore, joined to the open sea by 1.0 m or more
- canal 10, way, 53 m, 12810, -905 to 12765, -878: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.5 m deep, 23 m off the shore, joined to the open sea by 1.0 m or more; passes 20 m from land that is not its canal's bank, at 12772, -882
- canal 26, canal line, 74 m, 14592, -1642 to 14638, -1700: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads to open water 1.4 m deep, 14 m off the shore, joined to the open sea by 1.0 m or more
- canal 26, way, 5 m, 14590, -1642 to 14595, -1642: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.4 m deep, 14 m off the shore, joined to the open sea by 1.0 m or more
- canal 26, way, 10 m, 14638, -1698 to 14638, -1708: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.4 m deep, 14 m off the shore, joined to the open sea by 1.0 m or more
- canal 35, way, 33 m, 20085, -3392 to 20052, -3400: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.6 m deep, 24 m off the shore, joined to the open sea by 1.0 m or more
- canal 36, way, 56 m, 13412, -1168 to 13465, -1178: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.4 m deep, 11 m off the shore, joined to the open sea by 1.0 m or more; passes 7 m from land that is not its canal's bank, at 13449, -1168
- canal Layton (p2 line 45), canal line, 112 m, 10090, -778 to 10155, -845: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads into the marked channel Layton Channel, 1.3 m deep there
- canal Layton (p2 line 45), way, 9 m, 10085, -780 to 10092, -775: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into the marked channel Layton Channel, 1.3 m deep there
- canal Layton (p2 line 45), way, 6 m, 10155, -842 to 10158, -848: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into the marked channel Layton Channel, 1.3 m deep there
- canal 59, way, 33 m, -22152, -2455 to -22142, -2485: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.5 m deep, 20 m off the shore, joined to the open sea by 1.0 m or more
- canal 64, canal line, 48 m, -10910, -1472 to -10928, -1428: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads into the marked channel Ramrod Key Channel, 1.4 m deep there
- canal 64, canal line, 66 m, -10948, -1362 to -10970, -1300: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads into the marked channel Ramrod Key Channel, 1.4 m deep there
- canal 64, canal line, 74 m, -10988, -1250 to -10985, -1190: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads into the marked channel Ramrod Key Channel, 1.4 m deep there
- canal 64, way, 7 m, -10908, -1475 to -10912, -1470: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into the marked channel Ramrod Key Channel, 1.4 m deep there
- canal 64, way, 74 m, -10925, -1430 to -10950, -1360: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into the marked channel Ramrod Key Channel, 1.4 m deep there
- canal 64, way, 58 m, -10970, -1302 to -10988, -1248: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into the marked channel Ramrod Key Channel, 1.4 m deep there
- canal 64, way, 5 m, -10988, -1190 to -10982, -1190: a charted channel: it runs along the marked channel for 100% of its length; leads into the marked channel Ramrod Key Channel, 1.4 m deep there
- canal 66, way, 26 m, -22235, -2572 to -22215, -2588: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.3 m deep, 6 m off the shore, joined to the open sea by 1.0 m or more
- canal 67, canal line, 74 m, -10852, -1640 to -10878, -1572: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads into canal 11 (middle -10880, -1560), which is open: the two are joined in life and share its mouth
- canal 67, way, 7 m, -10850, -1642 to -10855, -1638: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into canal 11 (middle -10880, -1560), which is open: the two are joined in life and share its mouth
- canal 68, way, 403 m, -16138, -2765 to -15915, -2450: the shortest clear line across the flat (no channel is charted there); leads into a way already dredged (way 37), which runs on to the open sea; passes 10 m from land that is not its canal's bank, at -15915, -2450
- canal 70, way, 65 m, -15930, -2382 to -15932, -2448: the shortest clear line across the flat (no channel is charted there); leads to open water 1.2 m deep, 25 m off the shore, joined to the open sea by 1.0 m or more; passes 27 m from land that is not its canal's bank, at -15932, -2448
- canal 76, way, 199 m, -17542, -2310 to -17392, -2395: the shortest clear line across the flat (no channel is charted there); leads to open water 1.3 m deep, 67 m off the shore, joined to the open sea by 1.0 m or more; passes 20 m from land that is not its canal's bank, at -17505, -2295
- canal 86, canal line, 92 m, -19945, -2408 to -19912, -2490: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads into canal 78 (middle -19902, -2490), which is open: the two are joined in life and share its mouth
- canal 86, way, 6 m, -19945, -2405 to -19942, -2410: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into canal 78 (middle -19902, -2490), which is open: the two are joined in life and share its mouth
- canal 90, canal line, 111 m, -15510, -2040 to -15560, -2138: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads to open water 1.5 m deep, 57 m off the shore, joined to the open sea by 1.0 m or more
- canal 90, way, 5 m, -15510, -2038 to -15510, -2042: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.5 m deep, 57 m off the shore, joined to the open sea by 1.0 m or more
- canal 90, way, 115 m, -15558, -2135 to -15572, -2248: the shortest clear line across the flat (no channel is charted there); leads to open water 1.5 m deep, 57 m off the shore, joined to the open sea by 1.0 m or more; passes 25 m from land that is not its canal's bank, at -15572, -2176
- canal 96, way, 377 m, -14985, -3285 to -15298, -3112: the shortest clear line across the flat (no channel is charted there); leads to open water 1.5 m deep, 131 m off the shore, joined to the open sea by 1.0 m or more; passes 18 m from land that is not its canal's bank, at -15071, -3276
- canal 97, canal line, 31 m, -9018, -302 to -9045, -310: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads to open water 1.5 m deep, 78 m off the shore, joined to the open sea by 1.0 m or more
- canal 97, way, 102 m, -8930, -292 to -9020, -302: the shortest clear line across the flat (no channel is charted there); leads to open water 1.5 m deep, 78 m off the shore, joined to the open sea by 1.0 m or more; passes 18 m from land that is not its canal's bank, at -8930, -292
- canal 97, way, 291 m, -9042, -308 to -9132, -565: the shortest clear line across the flat (no channel is charted there); leads to open water 1.5 m deep, 78 m off the shore, joined to the open sea by 1.0 m or more; passes 9 m from land that is not its canal's bank, at -9102, -389
- canal 98, way, 84 m, -12115, -1865 to -12182, -1845: the shortest clear line across the flat (no channel is charted there); leads into a way already dredged (way 73), which runs on to the open sea; passes 14 m from land that is not its canal's bank, at -12178, -1848
- canal 107, canal line, 74 m, -10185, -1478 to -10162, -1548: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads into canal 87 (middle -10198, -1572), which is open: the two are joined in life and share its mouth
- canal 107, way, 6 m, -10188, -1475 to -10185, -1480: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into canal 87 (middle -10198, -1572), which is open: the two are joined in life and share its mouth
- canal 113, way, 37 m, -22185, -1675 to -22152, -1665: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.3 m deep, 6 m off the shore, joined to the open sea by 1.0 m or more; passes 2 m from land that is not its canal's bank, at -22161, -1665
- canal 117, way, 361 m, -19865, -2055 to -19928, -1752: the shortest clear line across the flat (no channel is charted there); leads to open water 1.4 m deep, 26 m off the shore, joined to the open sea by 1.0 m or more; passes 22 m from land that is not its canal's bank, at -19821, -1899
- canal 118, canal line, 105 m, -14565, -3045 to -14468, -3028: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads to open water 1.4 m deep, 19 m off the shore, joined to the open sea by 1.0 m or more
- canal 118, way, 8 m, -14570, -3048 to -14562, -3045: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.4 m deep, 19 m off the shore, joined to the open sea by 1.0 m or more
- canal 118, way, 15 m, -14470, -3025 to -14462, -3038: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.4 m deep, 19 m off the shore, joined to the open sea by 1.0 m or more
- canal 119, canal line, 65 m, -10165, -1990 to -10105, -1985: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads to open water 1.5 m deep, 26 m off the shore, joined to the open sea by 1.0 m or more
- canal 119, canal line, 65 m, -10105, -1985 to -10045, -1965: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads to open water 1.5 m deep, 26 m off the shore, joined to the open sea by 1.0 m or more
- canal 119, way, 7 m, -10108, -1982 to -10102, -1988: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.5 m deep, 26 m off the shore, joined to the open sea by 1.0 m or more
- canal 119, way, 27 m, -10048, -1968 to -10022, -1958: the shortest clear line across the flat (no channel is charted there); leads to open water 1.5 m deep, 26 m off the shore, joined to the open sea by 1.0 m or more
- canal 120, way, 53 m, -15092, -3215 to -15082, -3268: the shortest clear line across the flat (no channel is charted there); leads into a way already dredged (way 110), which runs on to the open sea; passes 29 m from land that is not its canal's bank, at -15086, -3252
- canal 121, way, 29 m, -16062, -2678 to -16088, -2662: the shortest clear line across the flat (no channel is charted there); leads to open water 1.4 m deep, 37 m off the shore, joined to the open sea by 1.0 m or more
- canal 122, way, 56 m, -16105, -2545 to -16052, -2565: the shortest clear line across the flat (no channel is charted there); leads to open water 1.3 m deep, 50 m off the shore, joined to the open sea by 1.0 m or more; passes 29 m from land that is not its canal's bank, at -16075, -2556
- canal 125, way, 71 m, -12922, -2425 to -12865, -2385: the shortest clear line across the flat (no channel is charted there); leads to open water 1.5 m deep, 25 m off the shore, joined to the open sea by 1.0 m or more; passes 19 m from land that is not its canal's bank, at -12893, -2403
- canal 126, canal line, 89 m, -19860, -1470 to -19882, -1385: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads into the marked channel Tamarac Park Channel, 1.1 m deep there
- canal 126, way, 9 m, -19855, -1475 to -19860, -1468: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into the marked channel Tamarac Park Channel, 1.1 m deep there
- canal 126, way, 5 m, -19882, -1388 to -19882, -1382: a charted channel: it runs along the marked channel for 100% of its length; leads into the marked channel Tamarac Park Channel, 1.1 m deep there
- canal 130, way, 26 m, -15992, -2550 to -16018, -2542: the shortest clear line across the flat (no channel is charted there); leads to open water 1.3 m deep, 25 m off the shore, joined to the open sea by 1.0 m or more
- canal 134, way, 58 m, -22308, -2680 to -22265, -2718: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.8 m deep, 6 m off the shore, joined to the open sea by 1.0 m or more; passes 6 m from land that is not its canal's bank, at -22281, -2704
- canal 138, way, 46 m, -12935, -2365 to -12895, -2388: the shortest clear line across the flat (no channel is charted there); leads to open water 1.2 m deep, 32 m off the shore, joined to the open sea by 1.0 m or more; passes 29 m from land that is not its canal's bank, at -12909, -2380
- canal 141, way, 299 m, -17262, -3280 to -17538, -3172: the shortest clear line across the flat (no channel is charted there); leads to open water 1.5 m deep, 164 m off the shore, joined to the open sea by 1.0 m or more
- canal 143, way, 28 m, -16015, -2618 to -16012, -2592: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into a way already dredged (way 30), which runs on to the open sea
- canal 144, way, 50 m, -24015, -2315 to -23992, -2358: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.3 m deep, 5 m off the shore, joined to the open sea by 1.0 m or more; passes 2 m from land that is not its canal's bank, at -24008, -2339
- canal 145, way, 43 m, -12972, -2258 to -12958, -2298: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into canal 86 (middle -12975, -2310), which is open: the two are joined in life and share its mouth; passes 9 m from land that is not its canal's bank, at -12959, -2292
- canal 146, way, 59 m, -22302, -2110 to -22245, -2118: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.6 m deep, 29 m off the shore, joined to the open sea by 1.0 m or more; passes 22 m from land that is not its canal's bank, at -22263, -2112
- canal 155, way, 101 m, 26335, -6630 to 26245, -6590: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into canal 33 (middle 26198, -6660), which is open: the two are joined in life and share its mouth; passes 6 m from land that is not its canal's bank, at 26245, -6590
- canal 172, way, 63 m, 28035, -8730 to 28098, -8725: the shortest clear line across the flat (no channel is charted there); leads to open water 1.5 m deep, 49 m off the shore, joined to the open sea by 1.0 m or more
- canal 173, canal line, 96 m, 26498, -7228 to 26548, -7292: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads into canal 3 (middle 26582, -7330), which is open: the two are joined in life and share its mouth
- canal 173, way, 6 m, 26495, -7225 to 26498, -7230: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads into canal 3 (middle 26582, -7330), which is open: the two are joined in life and share its mouth
- canal 177, way, 31 m, 28145, -8358 to 28142, -8388: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.3 m deep, 13 m off the shore, joined to the open sea by 1.0 m or more
- canal 179, canal line, 33 m, 28130, -8315 to 28138, -8348: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads into canal 29 (middle 28145, -8358), which is open: the two are joined in life and share its mouth
- canal 183, canal line, 57 m, 27848, -8902 to 27818, -8855: the real mouth - water in life in the coastline, which the 10 m grid has as land; leads to open water 1.5 m deep, 30 m off the shore, joined to the open sea by 1.0 m or more
- canal 183, way, 6 m, 27848, -8905 to 27845, -8900: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.5 m deep, 30 m off the shore, joined to the open sea by 1.0 m or more
- canal 183, way, 42 m, 27820, -8858 to 27798, -8822: a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow; leads to open water 1.5 m deep, 30 m off the shore, joined to the open sea by 1.0 m or more
- canal 190, way, 78 m, 27055, -11095 to 26982, -11125: the shortest clear line across the flat (no channel is charted there); leads to open water 1.5 m deep, 56 m off the shore, joined to the open sea by 1.0 m or more

How wide the real water is where a route follows an inlet narrower than the raster (game metres; three times that
in life):

- canal 10: 24 m on the shoreline's cells; water there in life 4.1 m wide at its narrowest (12 m at full scale), 4.1 m typical
- canal 26: 26 m on the shoreline's cells; water there in life 3.0 m wide at its narrowest (9 m at full scale), 7.6 m typical
- canal Layton (p2 line 45): 20 m on the shoreline's cells; water there in life 5.0 m wide at its narrowest (15 m at full scale), 7.1 m typical
- canal 64: 130 m on the shoreline's cells; water there in life 4.0 m wide at its narrowest (12 m at full scale), 4.5 m typical
- canal 67: 81 m on the shoreline's cells; water there in life 4.1 m wide at its narrowest (12 m at full scale), 5.4 m typical
- canal 86: 84 m on the shoreline's cells; water there in life 3.6 m wide at its narrowest (11 m at full scale), 4.5 m typical
- canal 90: 16 m on the shoreline's cells; water there in life 4.1 m wide at its narrowest (12 m at full scale), 4.1 m typical
- canal 107: 65 m on the shoreline's cells; water there in life 4.1 m wide at its narrowest (12 m at full scale), 4.7 m typical
- canal 118: 16 m on the shoreline's cells; water there in life 3.6 m wide at its narrowest (11 m at full scale), 4.0 m typical
- canal 119: 44 m on the shoreline's cells; water there in life 4.0 m wide at its narrowest (12 m at full scale), 4.1 m typical
- canal 126: 93 m on the shoreline's cells; water there in life 3.2 m wide at its narrowest (10 m at full scale), 3.6 m typical
- canal 173: 88 m on the shoreline's cells; water there in life 4.0 m wide at its narrowest (12 m at full scale), 4.5 m typical
- canal 177: 8 m on the shoreline's cells; water there in life 4.0 m wide at its narrowest (12 m at full scale), 4.0 m typical
- canal 179: 48 m on the shoreline's cells; water there in life 2.8 m wide at its narrowest (8 m at full scale), 3.2 m typical
- canal 183: 48 m on the shoreline's cells; water there in life 2.2 m wide at its narrowest (7 m at full scale), 3.6 m typical

## New water and what it serves (criterion 10)

- canal 10: 3 lines, 60 m of canal line and 58 m of way; it serves 1 canal (10)
- canal 26: 3 lines, 74 m of canal line and 15 m of way; it serves 1 canal (26)
- canal 35: 1 line, 0 m of canal line and 33 m of way; it serves 1 canal (35)
- canal 36: 1 line, 0 m of canal line and 56 m of way; it serves 1 canal (36)
- canal Layton (p2 line 45): 3 lines, 112 m of canal line and 15 m of way; it serves 1 canal (Layton (p2 line 45))
- canal 59: 1 line, 0 m of canal line and 33 m of way; it serves 1 canal (59)
- canal 64: 7 lines, 188 m of canal line and 144 m of way; it serves 3 canals (64, 72, 110)
- canal 66: 1 line, 0 m of canal line and 26 m of way; it serves 1 canal (66)
- canal 67: 2 lines, 74 m of canal line and 7 m of way; it serves 1 canal (67)
- canal 68: 1 line, 0 m of canal line and 403 m of way; it serves 3 canals (68, 121, 122)
- canal 70: 1 line, 0 m of canal line and 65 m of way; it serves 1 canal (70)
- canal 76: 1 line, 0 m of canal line and 199 m of way; it serves 1 canal (76)
- canal 86: 2 lines, 92 m of canal line and 6 m of way; it serves 1 canal (86)
- canal 90: 3 lines, 111 m of canal line and 120 m of way; it serves 1 canal (90)
- canal 96: 1 line, 0 m of canal line and 377 m of way; it serves 2 canals (96, 120)
- canal 97: 3 lines, 31 m of canal line and 393 m of way; it serves 1 canal (97)
- canal 98: 1 line, 0 m of canal line and 84 m of way; it serves 1 canal (98)
- canal 107: 2 lines, 74 m of canal line and 6 m of way; it serves 1 canal (107)
- canal 113: 1 line, 0 m of canal line and 37 m of way; it serves 1 canal (113)
- canal 117: 1 line, 0 m of canal line and 361 m of way; it serves 2 canals (117, 137)
- canal 118: 3 lines, 105 m of canal line and 22 m of way; it serves 1 canal (118)
- canal 119: 4 lines, 130 m of canal line and 34 m of way; it serves 2 canals (119, 123)
- canal 120: 1 line, 0 m of canal line and 53 m of way; it serves 1 canal (120); it runs into the water made for canal 96
- canal 121: 1 line, 0 m of canal line and 29 m of way; it serves 1 canal (121); it runs into the water made for canal 68
- canal 122: 1 line, 0 m of canal line and 56 m of way; it serves 1 canal (122); it runs into the water made for canal 68
- canal 125: 1 line, 0 m of canal line and 71 m of way; it serves 1 canal (125)
- canal 126: 3 lines, 89 m of canal line and 14 m of way; it serves 1 canal (126)
- canal 130: 1 line, 0 m of canal line and 26 m of way; it serves 1 canal (130)
- canal 134: 1 line, 0 m of canal line and 58 m of way; it serves 1 canal (134)
- canal 138: 1 line, 0 m of canal line and 46 m of way; it serves 2 canals (138, 139)
- canal 141: 1 line, 0 m of canal line and 299 m of way; it serves 1 canal (141)
- canal 143: 1 line, 0 m of canal line and 28 m of way; it serves 1 canal (143)
- canal 144: 1 line, 0 m of canal line and 50 m of way; it serves 1 canal (144)
- canal 145: 1 line, 0 m of canal line and 43 m of way; it serves 1 canal (145)
- canal 146: 1 line, 0 m of canal line and 59 m of way; it serves 1 canal (146)
- canal 155: 1 line, 0 m of canal line and 101 m of way; it serves 1 canal (155)
- canal 172: 1 line, 0 m of canal line and 63 m of way; it serves 1 canal (172)
- canal 173: 2 lines, 96 m of canal line and 6 m of way; it serves 1 canal (173)
- canal 177: 1 line, 0 m of canal line and 31 m of way; it serves 1 canal (177)
- canal 179: 1 line, 33 m of canal line and 0 m of way; it serves 1 canal (179)
- canal 183: 3 lines, 57 m of canal line and 47 m of way; it serves 1 canal (183)
- canal 190: 1 line, 0 m of canal line and 78 m of way; it serves 1 canal (190)

## Daybeacon pairs (R1, criterion 6)

Way numbers are the game's, over the three files in order.

- way 0 (206 m): green 23587.1, -5707.1 red 23572.9, -5692.9
- way 1 (128 m): green 23542.1, -5557.1 red 23527.9, -5542.9
- way 2 (268 m): green 23332.9, -4862.9 red 23347.1, -4877.1
- way 5 (332 m): green 23052.9, -4557.9 red 23067.1, -4572.1
- way 11 (115 m): green 21850.0, -4485.0 red 21850.0, -4465.0
- way 15 (139 m): green 20707.1, -4152.9 red 20692.9, -4167.1
- way 22 (190 m): green 19607.9, -2732.9 red 19622.1, -2747.1
- way 23 (283 m): green 19242.9, -2627.1 red 19257.1, -2612.9
- way 31 (62 m): green 16957.1, -2417.1 red 16942.9, -2402.9
- way 32 (77 m): green 17017.1, -2422.9 red 17002.9, -2437.1
- way 34 (96 m): green 16902.1, -2392.1 red 16887.9, -2377.9
- way 38 (200 m): green 16450.0, -1705.0 red 16470.0, -1705.0
- way 53 (158 m): green -13562.9, -4297.9 red -13577.1, -4312.1
- way 56 (845 m): green -10577.9, -4247.1 red -10592.1, -4232.9
- way 57 (1072 m): green -9377.1, -4477.9 red -9362.9, -4492.1
- way 62 (114 m): green -14662.1, -3442.9 red -14647.9, -3457.1
- way 64 (441 m): green -8777.9, -3017.9 red -8792.1, -3032.1
- way 69 (70 m): green -14230.0, -2740.0 red -14230.0, -2720.0
- way 77 (187 m): green -8552.1, -2582.9 red -8537.9, -2597.1
- way 78 (137 m): green -14215.0, -2685.0 red -14215.0, -2665.0
- way 81 (377 m): green -8532.9, -2847.9 red -8547.1, -2862.1
- way 89 (263 m): green -20012.9, -2687.1 red -20027.1, -2672.9
- way 108 (242 m): green -9732.1, -2032.1 red -9717.9, -2017.9
- way 132 (140 m): green -8585.0, -1650.0 red -8585.0, -1670.0
- way 133 (277 m): green -19222.9, -1717.9 red -19237.1, -1732.1
- way 141 (79 m): green -8570.0, -1505.0 red -8570.0, -1525.0
- way 144 (222 m): green -9657.9, -1537.1 red -9672.1, -1522.9
- way 159 (199 m): green -17385.3, -2388.1 red -17399.7, -2401.9
- way 162 (115 m): green -15562.5, -2247.5 red -15582.5, -2247.5
- way 163 (377 m): green -15304.3, -3119.8 red -15290.7, -3105.2
- way 165 (291 m): green -9122.5, -564.8 red -9142.5, -565.2
- way 183 (299 m): green -17537.5, -3182.5 red -17537.5, -3162.5
- way 203 (292 m): green 33357.9, -12027.9 red 33372.1, -12042.1
- way 217 (156 m): green 29472.9, -8612.9 red 29487.1, -8627.1
- way 219 (211 m): green 27722.1, -8427.1 red 27707.9, -8412.9
- way 220 (303 m): green 27652.1, -8387.1 red 27637.9, -8372.9
- way 226 (681 m): green 29047.9, -7827.9 red 29062.1, -7842.1
- way 228 (94 m): green 26797.1, -7482.1 red 26782.9, -7467.9
- way 230 (121 m): green 25442.1, -6632.1 red 25427.9, -6617.9
- way 232 (947 m): green 25602.9, -5932.1 red 25617.1, -5917.9
- way 234 (346 m): green 24762.1, -6402.9 red 24747.9, -6417.1
- way 239 (442 m): green 24337.9, -5572.1 red 24352.1, -5557.9
- way 242 (81 m): green 24937.9, -5587.9 red 24952.1, -5602.1
- way 249 (63 m): green 28096.7, -8715.0 red 28098.3, -8735.0
- way 254 (78 m): green 26986.3, -11134.2 red 26978.7, -11115.8

## Done

- `Tools/geo/canalways_model.py` (new): the water the hull reads, in numpy, from the baked files; finds the same 46.
- `Tools/geo/canalways.py` (new): opens every shut canal along the real water; called at the end of `canals.cut()`.
- `Tools/geo/canals.py`: calls it; runs as a script (`canals.py <phase> [--check] [--preview dir]`).
- `Tools/geo/canalways_trip.py`, `Tools/geo/canalways_sound.py` (new): the trip and sounding scripts and their readers.
- `Content/KeysRun/Geo/p2_canals.txt`, `p3_canals.txt`, `p4_canals.txt`: as the tool writes them.

## In progress

- Nothing.

## Blocked

- Nothing.

## Requests for the marshal

- R1 (open, criterion 6): `Source/KeysRun/KeysGeoReal.cpp` - `MarkWays()`, called at the end of `LoadAll`: a pair
  of daybeacons (green to port, red to starboard coming in) at the outer end of every way that crosses a flat,
  added to `Marks()` so the landmarks, the chart and the pile collision take them with no other change. Each of
  the two is a channel of its own by name, so `BuildNavSegs` dredges nothing between them. 45 pairs today, 90
  marks on top of 1,063. Applies cleanly to this branch (`git apply --check`); built and run. Patch:
  `.marshal/requests/canalways/R1.patch`.
- R2 (open, small, no patch): `Tools/geo/bake_p3.sh` and the `geo-bake` skill - add
  `"$PY" "$HERE/canals.py" "$PH"` as the last step for p3 and p4, and say the same for p2. Today the bake in the
  middle of the sequence already gives the same canals file (tested), but it plans the ways on the map as it
  stood before the second bake; running `canals.py` last makes that independent of the order.

## Notes for integration

- The worktree is left on the lane-only build, hub files as on the branch, `git status` clean.
- `canals.cut()` now needs the phase to have been baked once (it reads every phase's `.kgeo` and marks from the
  geo dir, or from `CANALWAYS_GEO`); with no baked map it adds nothing and says so. It changes no `.kgeo`.
- The game pauses when its window loses focus. Six frames (`ways10` 57 - 59, `near_a` 58 - 60) show the pause menu
  or a cast, one trip leg was cut short (`trip_lower`, canal 141 back in) and one run crawled (`trip_largo`,
  canal 190). All were repeated; the repeats are the ones cited.
- A script longer than about 9,000 characters did not start the game at all (28,000 did not; 17,000 for the
  soundings was split in two and ran). I did not find the limit.
- More canal water than in life (criterion 10, third point): I could not judge this without imagery. What the
  data shows: every inlet the routes follow is 2.2 m or more wide in game metres (6.6 m in life), so none is a
  mosquito ditch, but two on Key Largo are narrow for a canal: canal 183 (27855, -8918; 2.2 m, 7 m in life) and
  canal 179 (28162, -8288; 2.8 m, 8 m in life). If those are creeks in life rather than canals, the two canals
  behind them should be shut again or dropped by the bake. Follow-up, not done: the bake widens every kept canal
  to 24 m (8 - 10 m is their real width at this scale) and that is the upperkeys design, not mine to change.
- The 43-footer's name in the HUD is a real boat builder's (seen in every shot; already noted by the marshal).
- Seen, not mine: the terrain beside a walled cut steps in 10 m blocks (`shots/looks/KRScript_54.png`).

# water2

Status: READY FOR MERGE
Updated: 2026-11-22 13:20 EST
Branch: lane/water2 (code at 3ef6c29; this file in the commit after it)

Evidence is outside the repo in `KeysRun-devkit/lanes/KeysRun-water2/` (`shots/<tag>/KRScript_NN.png`,
`log_<tag>.log`), written `<tag>` below. Three builds were compared by swapping the game module: B0 (the branch
point, untouched), L (this lane alone: what is committed) and F (the lane with R1 - R5 applied). Tags ending in
`_B0`, `_L`, `_F` say which. Criteria 2, 3, 4, 5 and 6 pass only with the requests named beside them.

## Whose reading this is, and what I guessed

The user said only "Start the water lane". The split into nine criteria is the marshal's reading of that
(`.marshal/prompts/water2.md`), not the user's words. Within it I had to guess:

- "Largo Sound" (criterion 4). The map has no Largo Sound: the bake made the real sound dry land (see R6). The
  place the map calls Largo Sound (`p4_sites.txt` `water|Largo Sound|27390|-6660`, teleport spot 27570, -6640) is
  the ocean-side shelf off El Radabob Key. I took "idles across Largo Sound" to mean a run from the teleport spot
  across that shelf to the charted 1 m point.
- "a boat" (criterion 4). The scripted runs use whatever hull the user's save has: a 43-footer, hull draft 0.52 m.
  The starting 23-footer draws 0.35 m. I give numbers for both and took "floats a boat" to mean the 43-footer at
  any tide.
- "the smallest change" (criterion 4). I took it to mean the fewest lines that open the named waters while leaving
  the starting boat's Middle Keys groundings where they are at mid tide. It is one line (R2). It does move where
  bigger hulls and low tides ground: the numbers are under criterion 4.
- "as thick as they were off Marathon before the map grew" (criterion 5). The schools (`KeysSchools`) never
  thinned: they were always made round the camera. What thinned is `KeysLife`'s 40 bird-and-bait hotspots, dealt
  over the whole map box (R5). I took the criterion to cover both.
- "each named reef" (criterion 5). The reefs the game can name through `KeysGeoReal::Sites()` (the Lower Keys and
  Key Largo), plus Sombrero and the two patch reefs. The Upper Keys' reef names are only in `p2_places.json`,
  which the game does not load (R7).
- "a creek" in Florida Bay (criterion 6). No creek there floats a boat on this map; I shot the widest one the rule
  finds (17200, -10860, 0.3 m of water) with the boat sitting aground in it.
- Which hull edits belong to "water" at all. I kept to depth, bottom, surface and schools; zone names on the HUD
  (every reef is "Sombrero Reef", Key West harbour is "Florida Bay backcountry") are noted, not changed.

## Survey (criterion 1)

Thirteen spots, before any change: chase view, top-down (`cam=0,25,3`; Carysfort `cam=0,25,2`), camera under the
surface. Shots `shots/survey_a/KRScript_50..70.png` and `shots/survey_b/KRScript_50..67.png` (three a spot, in
this order), logs `log_survey_a.log`, `log_survey_b.log`. The same two scripts on the final build with every
request applied: `shots/survey_a2`, `shots/survey_b2`.

| # | Spot (`tp=`) | What looks wrong | Owner |
|---|---|---|---|
| 1 | Key West harbour -25040, -2520 | Water and bottom fine. HUD zone is "Florida Bay backcountry" in 9.7 m of harbour. | `KeysGeo::ZoneAt` (hub, noted) |
| 2 | Flats west of Key West -27500, -3250 | Nothing: 0.6 m sand flat, props in the bottom as they should be. | - |
| 3 | Hawk Channel off Sugarloaf -16050, 130 | Nothing wrong; plain sand with tufts at 6.5 m. | - |
| 4 | Looe Key -12140, 2330 | From above a lighter block round the boat with straight, stepped edges (the seafloor's near chunks against its far skirt). The crest is charted 1.0 m, plays 0.44 m, so the zone is "Flats" (bonefish 45% on a reef crest) and no coral grows on it. The crest itself is boxy in the bathymetry (straight south edge at z 2370, west edge at x -12330): that part is the data. | `KeysSeafloor` (mine); `KeysGeo` bank rule and `ZoneAt`, `KeysReef` (hub); the DEM |
| 5 | American Shoal -16140, 1870 | The same stepped block of lighter bottom round the boat. | `KeysSeafloor` (mine) |
| 6 | Lower Keys backcountry basin -14000, -5870 | Reads well from above (channel and banks). The basin floor is the same sand as the banks: no mud. | `KeysSea` (mine) |
| 7 | Florida Bay north of Islamorada 15500, -4707 | Bottom is sand with seagrass in regular diagonal stripes about 250 m apart (two sine waves): a repeated stretch. No mud. | `KeysSea` (mine) |
| 8 | "Largo Sound" 27570, -6640 | Not a sound: an open shelf on the ocean side. The real sound is land on this map. The shelf charted 1.0 - 1.4 m plays 0.44 - 0.75 m. | bake (R6); `KeysGeo::RealDepthAt` (R2) |
| 9 | Molasses Reef 26248, -2870 | Reef and water fine. HUD says "Sombrero Reef". No schools that belong to the reef. | `KeysGeo::ZoneDesc` (hub, noted); `KeysSchools` (mine) |
| 10 | Off Carysfort Reef 33770, -8350 | Two sets of rectangles from above: a stepped outline 460 m across where the bottom's tint changes (seafloor chunks against the skirt), and inside it a 250 m square of coral dots with straight edges (the reef dressing's 5 x 5 chunks). | `KeysSeafloor` (mine); `KeysReef` (hub, R1) |
| 11 | Drop-off off Key Largo 27000, -2000 | From `cam=0,25,3` the whitecap foam shows as a lattice of pale ovals 133 m apart (the wave tile repeating) and the surface is speckled. Not visible from the chase view. | `KeysOcean` foam tile as `KeysWorld` draws it (noted, not changed) |
| 12 | Lower Matecumbe canal 13232, -1280 | Seagrass, algae and sargassum all over the canal bottom. The bottom is olive, not grey, and climbs toward each wall (the 10 m depth grid across a 24 m canal). Nothing pokes through a wall. | `KeysReef` (hub, R1); `KeysSea` (mine) |
| 13 | Way out of that canal 13400, -1228 | The same tufts on a pale sand bottom: the dredged way does not read as dredged. | `KeysReef` (hub, R1); `KeysSea` (mine) |

## Acceptance criteria

- [x] 1. Survey - the table above; `shots/survey_a`, `shots/survey_b` (before), `survey_a2`, `survey_b2` (after, F).
- [x] 2. The bottom (lane, and R1 for the reef square). Cause of the Carysfort rectangles, two things:
  (a) the seafloor's far skirt sampled single points of the reef's relief (30 m spurs, terraces) on a 32 m lattice,
  so it aliased and had a different mean tint from the fine chunks inside it; with `-KRSeaNoChunks` the stepped
  outline goes (`shots/rect_nochunks/KRScript_50.png`), with `-KRSeaDebug` the outline is the chunk set
  (`shots/rect_debug/KRScript_50.png`). Fixed in the lane: the skirt carries the mean height and colour of the
  floor round each vertex. (b) a 250 m square of coral dots, the reef dressing's 5 x 5 chunks: `KeysReef`, R1.
  Carysfort before `shots/looks3_B0/KRScript_54.png`; lane alone `shots/looks3_L/KRScript_54.png` (outline gone,
  square of dots still there); with R1 `shots/survey_b2/KRScript_57.png` (no rectangles; the dots end on a round
  edge). Other spots that changed, before / after: American Shoal `survey_a/KRScript_63` / `survey_a2/KRScript_63`;
  Looe Key `survey_a/60` / `survey_a2/60` (the light patch that remains is the crest itself, boxy in the data);
  Florida Bay `survey_a/69` / `survey_a2/69` and `looks_final/KRScript_57` (mud in the basin, no stripes);
  Lower Keys basin `survey_a/66` / `survey_a2/66`; canal and way out under criterion 3.
  What the bottom is now: one rule (`KeysSea::BottomAt`) gives reef, hard bottom, seagrass, bay mud and canal
  marl from depth and from where the point lies between the bay and the drop-off, each edge a fade.
- [x] 3. Canals (lane for the floor and its colour, R1 for the plants, R3 for the mouth). With R1:
  Lower Matecumbe canal `shots/after2/KRScript_51.png` (chase), `52` (under water: flat grey floor wall to wall,
  nothing growing); its way out `53`, `54`; a Key West canal at -23300, -2045 `55`, `56`. Lane alone the floor is
  flat and grey but the tufts are still there: `shots/looks3_L/KRScript_55, 56.png`. Nothing pokes through a wall
  in any of them. The mouth with no deep way out is the Layton canal on Long Key: a scratch re-run of the p2 bake
  (byte-identical output) prints "mouth at 10095 -775: no deep water joined to it by water within 700 m"; on the
  10 m grid that canal is a pond. R3 opens it; with it, `log_ground_final.log`: out of the canal through the cut
  in 2.1 m, down Layton Channel in 2.7 m, on east in 1.7 - 1.9 m, "afloat" on every line.
  `shots/after2/KRScript_63.png` shows the cut.
- [x] 4. Largo Sound (R2). Before, `log_gb_low_L.log`: at dead low tide she is aground at 27504, -6641 in 0.50 m;
  `log_gb_mid_L.log`: at mid tide she stops at 27491 in 0.70 m (afloat by the hull, props in the bottom). With R2,
  `log_ground_final.log`: 27570 -> 27364 at low tide, least 0.69 m under a 0.52 m hull, and at mid tide least
  1.06 m; no "aground" on either leg. Middle Keys flats, mid tide, before / after: 1600, 260 aground 0.30 m /
  aground 0.30 m; 540, 370 the same; -1390, -330 the same. A fourth, charted 1.0 m, does change: 1770, 490 aground
  in 0.45 m before, afloat in 0.94 m after.
  How much the rule grounds: charted 1.0 - 2.4 m water is 196 km2, 16% of the map's water. In it the rule grounds
  the 23-footer (hull 0.35 m) nowhere at mid tide and in 34 km2 (17.5%) at dead low; the 43-footer (0.52 m) in
  14 km2 (7.0%) and 56 km2 (28.4%); the 54-footer (1.2 m) in 47.8% and 63.9% (11.1% and 31.7% as charted). As
  charted the first two ground nowhere in that band.
  What it shuts: the Largo Sound point (aground: 43-footer always, 23-footer at low water); Tarpon Basin (cut off
  from the sea at mid tide, aground at low); Buttonwood Sound (cut off at low water); and at low water 59 - 62 km2
  of Florida Bay's basins that still float a boat but cannot be reached (10 - 12 km2 as charted), the largest
  38 km2 round Park Key, then Russell Key 4.2, Whipray Keys 3.1, End Key 2.2, Captain Key 1.9. Marked channels
  shut by the rule and not by the grid: Holiday Isle Marina and Manatee Bay (23-footer, mid tide); also Baker Cut,
  Harry Harris Park, Little Basin East, Shell Key Channel (43-footer, mid tide); also Boot Key Harbor, Lake
  Passage Channel, Tropical Marina (low water). Blackwater Sound is shut by the data, not the rule (R6).
  With R2: the Largo Sound point and Buttonwood Sound are open for both hulls at both tides; cut-off water is
  11 - 12 km2 in all four cases; Tarpon Basin stays cut off at mid tide (its entrance is charted under 0.8 m).
  What R2 costs: of the Middle Keys water where a hull grounds today it keeps 91% for the 23-footer at mid tide
  (58% at dead low) and 64% for the 43-footer (54% at low). These figures are from a model of `RealDepthAt` on
  the 10 m grid (bank rule, marked channels, shore easing, canals), not from the game.
- [x] 5. Schools (lane; R5 for `KRLife`). Rule: a school belongs to the water within 140 m of the camera. Each
  26 m cell there rolls once for a school from the mix of the zone under it, and a school is let go 190 m off.
  Never more than 72 schools and 5,200 fish (3,200 of them bait) are alive, wherever the boat is. A named reef
  within reach gets 14 schools of its own round its crest first (grunts, schoolmaster, yellowtail, lane snapper,
  two grouper, two barracuda, a bait ball, blue runners), and the furthest ordinary schools make way.
  Counts, B0 / L (`log_looks3_B0.log`, `log_looks3_L.log`, same seconds of the same script): flat off Boot Key
  34 schools 2,268 fish / the same; `goto=reef` 49, 3,820 / the same; off Key West 39, 3,608 / the same; Looe Key
  54, 3,650 / 62, 3,758 of which 9 schools, 621 fish are the reef's own; Molasses 54, 3,907 / 67, 4,158 of which
  14 schools, 586 fish are the reef's own. Shots: Looe Key `shots/checks_after/KRScript_52, 53.png`, Molasses
  `54, 55`. `KRLife`: its creature schools were already kept at 24 round the boat (`KRLife verts ... schools 24`
  at every spot, before and after). Its 40 hotspots were dealt over the whole map; with R5,
  `log_abn_life5_F.log`: "38 of 38 dealt round the boat" at each stop, 2 within 2 km off Marathon, 2 off Key West,
  4 at Looe Key, 1 at Molasses. There is no logged before count for hotspots: dealt over the whole map the
  expectation is about 0.4 within 2 km.
- [x] 6. Creeks (R4). `shots/abn_creeks_F/KRScript_50, 51.png` Lower Keys (-21960, -1640), `52, 53` Florida Bay
  (17200, -10860), `54, 55` Key Largo (33710, -12790): calm and tea-stained; `56` the Lower Matecumbe canal:
  clear. Tide held at 0.6. Before, the same Florida Bay creek with no stain: `shots/abn_creeks_B0/KRScript_53.png`.
  In my own files nothing was left to do: the peat bottom and the creek mix of schools already follow
  `MangroveNear`.
- [x] 7. Classic map. `shots/classic3_B0/KRScript_50, 51.png` against `classic3_F` (looked at) and `classic3_L`
  (not opened): the start and `goto=reef` look the same; `log_classic3_*.log`: tpcheck 19, 0 bad, no "Error:"
  line in any of the three.
- [x] 8. Nothing else moves. Lane alone (`log_gb_mid_L.log`): tpcheck 48, 0 bad; dock audit 545 docks, every
  "standing on" count 0; docks and houses identical to B0 (8,493 houses, 331 docks with water of their own, 214
  dredged, the same per island). With every request (`log_abn_checks_F.log`): tpcheck 48, 0 bad; audit 545 docks,
  all counts 0 - but the requests re-deal them (R2 by the depths, and `log_checks_final.log` with R3 too: 8,375
  houses, 426 docks with water of their own, 119 dredged), and at the old flat spot a different dock stands
  (`shots/looks3_B0/KRScript_50.png` against `looks3_F`). There the HUD zone also turns from "Skinny-water flats"
  to "Hawk Channel" (charted 1.8 m is no longer drawn down). `goto=reef` is the same (`KRScript_51.png`).
- [x] 9. No slowdown - for the lane alone, and only to the machine's noise, not to 3%. Another lane's builds
  and other use of the machine ran throughout; two runs of the same build differ by up to 10%, and of ten
  interleaved runs four had to be thrown out in whole or part (frame times of 30 - 100 ms, or the window not
  drawing). Game-thread ms, Medium, clean samples, B0 / L / F: flat 10.92 / 11.22 / 10.90; reef 10.46 / 10.22 /
  11.00; Key West 10.90 / 10.28 / 11.22; Carysfort 9.68 / 9.93 / 10.63. Draw 6.56 / 6.60 / 6.44; 6.24 / 6.18 /
  6.49; 7.01 / 6.28 / 6.56; 5.75 / 6.10 / 6.37. GPU within 3% everywhere. An earlier round gave L against B0
  +1.7%, -6.3%, -2.4%, +6.9% on the game thread: no trend. F is up to 10% over B0 at Carysfort (0.95 ms).
  (`log_ab_perf*`, `log_abn_perf*`.) Hawk Channel at full throttle for two minutes: every build has a frame or
  two over 50 ms - B0 113 and 136 ms, L 69 and 61, F 172, 139, 121 (`log_ab_hawk_*`, `log_abn_hawk_*`) - at the
  same places in B0 and F in the last pair; two I traced fall where species' models are built for the first
  time. I cannot show that none is new. Start-up, world start to the second frame: B0 7.0 - 7.9 s, L 6.9 - 7.7 s,
  F 7.3 - 8.6 s; R4's creek grid is under 0.35 s of F's.

## Done

- `KeysSea.*`: `BottomAt`; a canal floor flat from wall to wall; `SeafloorMean`; `DredgedAt`; seagrass with two
  octaves of noise over the old sines; `LogCanalWays`. The classic map keeps its two old bottom rules.
- `KeysSeafloor.*`: the far skirt carries the mean floor near the camera and the smooth depth far out, and is
  made off the game thread (2 - 18 ms a time; it used to be made on it).
- `KeysSchools.*`: named reefs' own schools; `fish:stats` adds what is within 100 m of the boat and the reef's
  own; `fish:sound` (a `KRSound` line a second: afloat or aground); `fish:canals[=m]`; pool build time logged.

## In progress

- Nothing.

## Blocked

- Nothing.

## Requests for the marshal

All five patches were made with `git diff` in this worktree, apply alone and in any order to this branch
(`git apply --check` on the clean tree, each alone; R2 after R4 and R4 after R2, which share `KeysGeo.cpp`), and
were tested together in the game. R1 needs this lane's `KeysSea::DredgedAt`, so it goes on after the merge.

- R1 (open, needed for criteria 2 and 3): `Source/KeysRun/KeysReef.cpp` - (a) nothing is planted on a canal's
  bottom or on a way out (`Suitability` and `ModelSuitability` return 0 where `KeysSea::DredgedAt > .2`): this is
  what takes the seagrass, algae and sargassum out of the canals; (b) the reef dressing is drawn within 110 m of
  the camera's own chunk, growing in over the last 24 m, in place of the 5 x 5 chunks (`ResidentR` 2 -> 3, the same
  62,500 m2): this is the 250 m square of dots off Carysfort. 12 lines. It changes the classic map's reef the same
  way (a round edge where it had a square one). Patch: `.marshal/requests/water2/R1.patch`.
- R2 (open, needed for criterion 4): `Source/KeysRun/KeysGeo.cpp` `RealDepthAt` - one line: the bank rule fades
  out between charted 0.75 and 1.1 m (`D = Lerp(rule, D, SS(.75, 1.1, D))`). Under 0.75 m as before; 1.1 m and
  more as surveyed; charted 1.0 m plays 0.89 m (was 0.44). Patch: `.marshal/requests/water2/R2.patch`. A gentler
  pair, `SS(.8, 1.2, D)`, leaves every grounding of the 23-footer at mid tide where it is on the whole map, but
  the 43-footer still grounds at the Largo Sound point at dead low water (0.72 - 0.32 = 0.40 m under a 0.52 m
  hull): say if you want that one instead; it is the same line.
- R3 (open, needed for criterion 3): `Content/KeysRun/Geo/p2_canals.txt` - one line added,
  `24 10082.5 -802.5 10105.0 -822.0 10150.0 -850.0`: a 24 m cut from the Layton canal (Long Key) to the head of
  the marked Layton Channel, which the game already keeps at 2.7 m. No "w" line is needed. Patch:
  `.marshal/requests/water2/R3.patch`. This is the mouth the bake reports ("1 with none in reach"): the bake puts
  it at 10095, -775 and finds no deep water joined to it by water. A re-bake of p2 writes the file again without
  the line, so `Tools/geo/canals.py` needs the same cut if p2 is ever baked again (not written: the bake is not
  mine). The seafloor, the seawalls and the docks take the new line from the file with no other change.
- R4 (open, needed for criterion 6): `Source/KeysRun/KeysGeo.cpp`, `KeysGeo.h`, `KeysWorld.cpp` - creeks by rule.
  `KeysGeo::CreekWater(X, Z)`, 0..1: a water cell with mangrove ground within 60 m on both sides of it along the
  grid or a diagonal, the two banks no more than 80 m apart, never within 14 m of a canal; built once at start-up
  (4.8 km2 of it on the map), feathered three cells, with a coarse grid in front so open water costs one byte
  read. `ShelterDist` takes it (so `AmpAt` and `SurfAt` calm it), and `UpdateWater` takes it for the lee and for
  the stain it already sends the material in UV2.y. No material change. Patch: `.marshal/requests/water2/R4.patch`.
- R5 (open, needed for the `KRLife` half of criterion 5): `Source/KeysRun/KeysLife.cpp`, `KeysLife.h` - the 40
  bird-and-bait hotspots are dealt over a box the size of the map before it grew (31.8 x 13.1 km) centred on the
  boat, and one left more than 1.3 boxes behind is dealt again near her, one a second; one whose kind of water
  is not to be had near her stays where it is. Real map only. Logs `KRLife hotspots: N of M dealt round the
  boat ...`. Patch: `.marshal/requests/water2/R5.patch`.
- R6 (open, no patch, not needed for a criterion as I read them): the bake has no Largo Sound. The real sound
  (about game 27570, -7420) is land at the fill height (1.0 m) in `p4.kgeo`, 325 m from any shore: the OSM
  coastline runs round the outside of Key Largo and El Radabob Key and the sound is mapped as water inside it,
  so the land mask takes it for land. 18 km2 of p4's land is at exactly 1.0 m, so other enclosed waters may have
  gone the same way. `sites_p3.py` then puts "Largo Sound" on the nearest water it finds, the ocean shore. The
  fix is in `Tools/geo/bake.py` (take OSM `natural=water` / `natural=bay` polygons out of the land mask) and a
  re-bake of p4; South Sound Creek and North Sound Creek would then need cutting like the canals.
  Also in the data, not the rule: Blackwater Sound is cut off from the sea for any hull at any tide (Jewfish Creek
  and Dusenbury Creek are narrower than the 10 m grid), and for the 43-footer at mid tide 36 legs of 27 marked channels cross land or
  water too shoal on the grid even as charted (Angelfish, Broad, Grouper, Jewfish, Snake and Tavernier
  creeks, Vaca Cut, Cow Key Channel and others: list under criterion 4).
- R7 (open, no patch, follow-up): reef names. The schools give a crowd of its own to every reef
  `KeysGeoReal::Sites()` names (Lower Keys, Key Largo) and to Sombrero, Coffins Patch and Delta Shoal. The Upper
  Keys' seven (`reef_sites` in `p2_places.json`: Alligator, Tennessee and the rest) and the Middle Keys' are not
  in any `_sites.txt`. A `p1_sites.txt` and `p2_sites.txt` with `reef|name|x|z` lines would give them theirs with
  no code change. Related, hub: a reef crest charted under about 1.3 m plays under 1.5 m and so is zone "Flats"
  (`KeysGeo::ZoneAt` tests the depth before the reef band), and `KeysReef` grows no coral in under 2.6 m: Looe
  Key's crest is bare sand with bonefish on it.
- R8 (open, no patch, follow-up): canals a boat cannot leave. `fish:canals[=m]` (new, in `KeysSchools::DebugCmd`,
  code in `KeysSea::LogCanalWays`) floods from every canal through the water the hull itself reads and logs each
  canal with no m of water out to the open sea, with the "w" line that would open it. With R2 and R3 applied and
  m = 1.0: 46 of 195 (47 of 194 with R2 alone), four of them in the Upper Keys - canals round 12960, -1008; 14530, -1648; 13300, -1168 (all
  three ponds on the grid, their mouths pinched like Layton's was) and 20115, -3402 (70 m short:
  `w 16 20075.0 -3400.0 20050.0 -3400.0 20025.0 -3400.0 20005.0 -3400.0`). The other 42 are in the Lower Keys and
  Key Largo, whose bakes report 18 and 7 mouths with no deep water in reach. `log_checks_final.log`.

## Notes for integration

- The worktree is left on the lane-only build with the hub files as on the branch.
- The patches as committed were built and run together (F) except R3's file, which was applied for
  `ground_final`, `checks_final` and the survey re-runs and left out of the last perf rounds.
- R5 was first tried with a Middle Keys-sized box: that put 3 - 4 times the bird mesh near the boat and cost
  about 10% of game-thread time. The patch as committed uses the pre-growth map's size.
- A school species' mesh is built the first time it is seen: 10 - 27 ms each (`KRSchools: ... built in` lines).
  Building them one a frame after start-up would take those out of play. Not done.
- Scripted runs take the hull from the user's save (a 43-footer). Tide follows the clock unless `tide=` holds it:
  the 0.3 m creeks of Florida Bay are dry sand at low water.
- Twice a run was upset from outside (the boat back at the marina mid-run, window focus changes in the log);
  those runs were repeated.
- Seen, not mine: Key West harbour's zone is "Florida Bay backcountry"; every reef is "Sombrero Reef" on the HUD;
  from `cam=0,25,3` the foam tile repeats every 133 m; the diver's view at the way out has a black sky above the
  surface line (`shots/survey_b/KRScript_67.png`).

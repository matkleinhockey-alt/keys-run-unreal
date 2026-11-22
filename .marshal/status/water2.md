# water2

Status: IN PROGRESS
Updated: 2026-11-22 11:50 EST
Branch: lane/water2 at d7f108e (this file and the requests in the commit after it)

Evidence is outside the repo in `KeysRun-devkit/lanes/KeysRun-water2/` (`shots/<tag>/KRScript_NN.png`,
`log_<tag>.log`), written `<tag>` below. Interim file: written so the state survives an interruption. The criteria
are not ticked yet; the last before / after runs (`looks3_*`, `classic3_*`, `gb_*`, `creeks_*`, `ab_*`) are taken
but not all read.

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

## Acceptance criteria (interim: what is measured so far)

- [x] 1. Survey - table above.
- [ ] 2. Bottom - done in `KeysSea` / `KeysSeafloor`; Carysfort with no rectangles needs R1 for the reef square:
  `shots/survey_b2/KRScript_57.png` (all requests applied). Cause: the far skirt sampled single points of the
  reef's relief (30 m spurs, terraces) on a 32 m lattice and so had a different mean tint from the chunks; and the
  reef dressing was drawn for 5 x 5 square chunks. Before / after pairs still to be listed.
- [ ] 3. Canals - with R1: `shots/after2/KRScript_51, 52` (Lower Matecumbe, chase and under water), `53, 54` (its
  way out), `55, 56` (a Key West canal). The mouth with no deep way out is the Layton canal on Long Key (the bake
  says "mouth at 10095 -775: no deep water joined to it by water within 700 m"); R3 opens it; drive out of it
  `log_ground_final.log` (2.1 m in the cut, 2.7 m in Layton Channel, 1.7 - 1.9 m beyond, afloat throughout).
- [ ] 4. Largo Sound - with R2, `log_ground_final.log`: across the shelf at dead low tide least 0.69 m under a
  0.52 m hull, at mid tide least 1.06 m, no "aground" on either leg; Middle Keys flats 1600, 260 / 540, 370 /
  -1390, -330 aground in 0.30 m as before. The before run (`gb_low_L`, `gb_mid_L`) is taken, not read yet.
  Numbers for the rule: charted 1.0 - 2.4 m water is 196 km2, 16% of the map's water. In it the rule grounds the
  23-footer (hull 0.35 m) nowhere at mid tide and in 17.5% at dead low; the 43-footer (0.52 m) in 7.0% and 28.4%;
  as charted neither grounds anywhere in it. It shuts Tarpon Basin, Buttonwood Sound at low water, the Largo
  Sound point, and at low water 59 - 62 km2 of Florida Bay's basins (10 - 12 km2 as charted).
- [ ] 5. Schools - rule and counts in `log_life5_final.log`; shots `shots/checks_after/KRScript_52..55.png`.
- [ ] 6. Creeks - with R4; shots to be retaken at a fixed tide (`creeks_F`): the Florida Bay creek is dry at low
  water.
- [ ] 7. Classic map - `log_classic_final.log`: tpcheck 19, 0 bad, no errors; before shots taken (`classic3_B0`),
  not compared yet.
- [ ] 8. Nothing else moves - with every request: `log_checks_final.log` tpcheck 48 destinations, 0 bad; dock
  audit 545 docks, every "standing on" count 0.
- [ ] 9. No slowdown - interleaved runs of the untouched build (B0), the lane alone (L) and the lane with every
  request (F), two rounds, four samples a spot: L against B0 game thread +1.7% / -6.3% / -2.4% / +6.9% at the four
  spots (noise: no trend); F against B0 +12.6% / -0.7% / +9.3% / +11.4%. The cost is R5: with the hotspots dealt
  over a Middle Keys-sized box the bird mesh near the boat was 3 - 4 times the size. R5 now deals them over the
  pre-growth map's size (417 km2) instead; that version is NOT built or measured yet.

## Done

- `KeysSea.*`: `BottomAt` (reef, hard bottom, seagrass, bay mud, canal marl, every edge a fade; the classic map
  keeps its two old rules), a canal floor flat from wall to wall, `SeafloorMean`, `DredgedAt`, `LogCanalWays`.
- `KeysSeafloor.*`: the far skirt carries the mean floor and is made off the game thread.
- `KeysSchools.*`: a named reef's own 14 schools, the furthest ordinary ones making room (never more than 72
  schools / 5,200 fish alive); `fish:stats` says what is within 100 m of the boat; `fish:sound`, `fish:canals`.

## In progress

- R5 with the wider box: build, measure, regenerate the patch if it changes.
- Reading the last before / after runs and ticking the criteria.
- Uncommitted in the worktree: the request patches applied to the hub files for testing (never to be committed)
  and one log line in `KeysSchools.cpp`.

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
  boat ...`. Patch: `.marshal/requests/water2/R5.patch` (this version of it is not yet built or run: the one tested used a
  15.8 x 10.4 km box and cost about 10% of game-thread time near Marathon).
- R6 (open, no patch, not needed for a criterion as I read them): the bake has no Largo Sound. The real sound
  (about game 27570, -7420) is land at the fill height (1.0 m) in `p4.kgeo`, 325 m from any shore: the OSM
  coastline runs round the outside of Key Largo and El Radabob Key and the sound is mapped as water inside it,
  so the land mask takes it for land. 18 km2 of p4's land is at exactly 1.0 m, so other enclosed waters may have
  gone the same way. `sites_p3.py` then puts "Largo Sound" on the nearest water it finds, the ocean shore. The
  fix is in `Tools/geo/bake.py` (take OSM `natural=water` / `natural=bay` polygons out of the land mask) and a
  re-bake of p4; South Sound Creek and North Sound Creek would then need cutting like the canals.
  Also in the data, not the rule: Blackwater Sound is cut off from the sea for any hull at any tide (Jewfish Creek
  and Dusenbury Creek are narrower than the 10 m grid), and 36 legs of 27 marked channels cross land or water
  under 0.5 m on the grid whatever the bank rule is (Angelfish, Broad, Grouper, Jewfish, Snake and Tavernier
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
  m = 1.0: 46 of 195, four of them in the Upper Keys - canals round 12960, -1008; 14530, -1648; 13300, -1168 (all
  three ponds on the grid, their mouths pinched like Layton's was) and 20115, -3402 (70 m short:
  `w 16 20075.0 -3400.0 20050.0 -3400.0 20025.0 -3400.0 20005.0 -3400.0`). The other 42 are in the Lower Keys and
  Key Largo, whose bakes report 18 and 7 mouths with no deep water in reach. `log_checks_final.log`.

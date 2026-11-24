# Marshal

How parallel work on Keys Run is organised. One marshal session owns `main`, the hub files and every merge. Each lane
is a separate session in its own git worktree on its own branch.

## Lanes

Two lanes run at a time (one machine: each worktree needs its own full compile, and only one copy of the game runs at
once).

| Lane | Goal | Branch | Worktree | State |
|---|---|---|---|---|
| `water` | Sea and seafloor polish | `lane/water` | `../KeysRun-water` | merged 2026-11-21 |
| `mangroves` | Finish the mangrove brief | `lane/mangroves` | `../KeysRun-mangroves` | merged 2026-11-21 |
| `yards` | Dry stack and boatyard | `lane/yards` | `../KeysRun-yards` | merged 2026-11-21 |
| `upperkeys` | Map Phase 2 (Upper Keys) | `lane/upperkeys` | `../KeysRun-upperkeys` | merged 2026-11-21 |
| `docks` | Lifts and docks (P4, brief written by the marshal) | `lane/docks` | `../KeysRun-docks` | merged 2026-11-21 |
| `lowerkeys` | Map Phase 3 (Lower Keys, then Key Largo; brief written by the marshal) | `lane/lowerkeys` | removed (branch kept) | merged 2026-11-22 |
| `mangrovelife` | Mangrove ecosystem (brief written by the marshal) | `lane/mangrovelife` | `../KeysRun-mangrovelife` | merged 2026-11-22 |
| `water2` | Water, seafloor and schools over the whole map (brief written by the marshal) | `lane/water2` | `../KeysRun-water2` | merged 2026-11-22 |
| `diving` | Scuba tank and gear, diving made better (brief written by the marshal) | `lane/diving` | `../KeysRun-diving` | merged 2026-11-22 |
| `birdview` | Mangrove wildlife visible from the boat: bird size and contrast, a closer camera at idle (brief written by the marshal) | `lane/birdview` | `../KeysRun-birdview` | running since 2026-11-23 (background agent) |
| `canalways` | A way out to sea for every residential canal (brief written by the marshal) | `lane/canalways` | `../KeysRun-canalways` | merged 2026-11-23 |
| `reefdens` | Hidden holes on the reefs and the animals that live in them (brief written by the marshal) | `lane/reefdens` | `../KeysRun-reefdens` | running since 2026-11-23 (background agent) |
| `reefparrot` | Reef ecosystem, template lane: the reef-life framework, the stoplight parrotfish end to end, and the recipe later lanes follow (brief written by the marshal) | `lane/reefparrot` | `../KeysRun-reefparrot` | merged 2026-11-23 |
| `reeffish` | Rainbow and midnight parrotfish, queen, French and gray angelfish, sergeant major (brief written by the marshal) | `lane/reeffish` | `../KeysRun-reeffish` | merged 2026-11-24 |
| `reefvisitors` | Southern stingray, green and loggerhead turtles, audit of the fish already there (brief written by the marshal) | `lane/reefvisitors` | `../KeysRun-reefvisitors` | merged 2026-11-24 |
| `reefbottom` | Queen conch, giant Caribbean anemone, purple sea fans, audit of the reef itself (brief written by the marshal) | `lane/reefbottom` | `../KeysRun-reefbottom` | merged 2026-11-24 |

Map Phase 3 becomes a lane after `upperkeys` merges. `docks` has no order against `upperkeys`: whichever is ready
first merges first.

## Ownership

No two lanes own the same file. Every lane also owns `.marshal/status/<lane>.md` and `.marshal/requests/<lane>/`.

| Lane | Owned paths |
|---|---|
| `water` | `Source/KeysRun/KeysSeafloor.*`, `KeysSea.*`, `KeysOcean.*`, `KeysSchools.*` |
| `mangroves` | `Source/KeysRun/KeysMangrove.cpp`, new `Source/KeysRun/KeysMangroveLife.*`, `Tools/geo/bake_mangrove.py`, `Content/KeysRun/Geo/p1_mangrove.bin` |
| `yards` | new `Source/KeysRun/KeysYards.*`, `KeysForklift.*`, `KeysYardProps.*` |
| `docks` | new `Source/KeysRun/KeysDocks.*`, `KeysLifts.*` |
| `upperkeys` | `Source/KeysRun/KeysGeoReal.*`, `Tools/geo/bake.py`, new `Tools/geo/*p2*`, `Content/KeysRun/Geo/p2_*` and `p2.kgeo` |
| `lowerkeys` | `Source/KeysRun/KeysGeoReal.*`, `Tools/geo/bake.py`, new `Tools/geo/*p3*` and `*p4*`, `Content/KeysRun/Geo/p3*` and `p4*` |
| `mangrovelife` | `Source/KeysRun/KeysMangroveLife.*`, `Source/KeysRun/KeysMangrove.cpp`, new `Source/KeysRun/KeysMangroveEco*` |
| `water2` | `Source/KeysRun/KeysSeafloor.*`, `KeysSea.*`, `KeysOcean.*`, `KeysSchools.*` |
| `diving` | `Source/KeysRun/KeysDive.*`, `KeysDiver.*`, new `Source/KeysRun/KeysScuba*` |
| `birdview` | `Source/KeysRun/KeysMangroveEco*`, `Source/KeysRun/KeysMangroveLife.*` (after `mangrovelife` merged) |
| `canalways` | `Content/KeysRun/Geo/p2_canals.txt`, `p3_canals.txt`, `p4_canals.txt`, `Tools/geo/canals.py`, new `Tools/geo/canalways*` |
| `reefdens` | `Source/KeysRun/KeysReef.*` (while the lane runs), new `Source/KeysRun/KeysReefDen*` |
| `reefparrot` | new `Source/KeysRun/KeysReefLife*` (the framework becomes marshal-only once merged; each later reef lane owns its own `KeysReefLife<Group>.cpp`) |
| `reeffish` | new `Source/KeysRun/KeysReefLifeFish*.cpp` |
| `reefvisitors` | new `Source/KeysRun/KeysReefLifeVisitors*.cpp` |
| `reefbottom` | new `Source/KeysRun/KeysReefLifeBottom*.cpp` |

## Marshal-only files

Lanes never edit these. They ask with the `marshal-request` skill.

- Hub code: `Source/KeysRun/KeysBoatPawn.*`, `KeysGeo.*`, `KeysHUD.*`, `KeysWorld.*`, `KeysLife.*`, `KeysData.*`,
  `KeysGameMode.*`
- Build and config: `Source/KeysRun/KeysRun.Build.cs`, `Source/*.Target.cs`, `KeysRun.uproject`, `Config/`, `.gitignore`
- Binary assets: `Content/KeysRun/Materials`, `Content/KeysRun/Maps`
- Shared pipeline code: `Tools/geo/frame.py`, `Tools/geo/kgeo.py`, `Tools/perf_bookmarks.sh`
- Docs and release: `README.md`, `DEVELOPMENT.md`, `PERF.md`, `MARSHAL.md`, `.claude/`, `.marshal/prompts/`, the
  version string, every packaged build and release
- Any source file no lane owns
- The devkit scripts (`KeysRun-devkit/build.sh`, `shoot.sh`), which live outside the repo

## Merge order

`water`, then `mangroves`, then `yards`, then `upperkeys`.

- `water`, `mangroves` and `yards` do not depend on each other.
- `upperkeys` goes last: it changes how the map loads, and the others read the map through it.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

## Integration (marshal)

1. A lane must be marked READY FOR MERGE and `git diff --name-only main...lane/<name>` must stay inside its owned
   paths. Otherwise it goes back with notes.
2. Merge into `integration` in the order above; the marshal resolves conflicts and applies the lane's requests.
3. Full check: the editor build, then a scripted smoke run on the real map and on `-KRClassicMap` (title to play,
   the marina, a reef dive, a catch, the lane's own acceptance shots). This project has no automated test suite.
   Graphics changes also get the perf bookmarks on a packaged build with no other copy of the game open.
4. Only when that is green does `integration` merge to `main`.
5. Nothing is pushed to GitHub, and no version is released, until the user says so.

## Log

- 2026-11-21: set up. `main` at the commit that adds this file; `lane/water` and `lane/mangroves` created from it.
- 2026-11-21: `water` merged (lane code at bfc7079, request R1 applied: the water is drawn at the tide). Editor build and
  smoke runs on the real and classic maps green. Packaged-build perf bookmarks still owed before any release.
  Follow-ups it left: reef chunk block from high above, diver depth readout ignores the tide, one unexplained 712 ms
  frame at the reef, seagrass tufts read bright green from above.
- 2026-11-21: `lane/yards` created from `main` after the `water` merge; the `water` worktree removed (branch kept).
- 2026-11-21: `mangroves` merged (lane at 9426997). Requests applied: R1 creek ground, R2 `creek=` and numeric
  `heading=` test commands, R3 creatures allowed in creeks, R4 four Free Fish spots and three charter rows (added in
  `KeysCharter::LoadTable` because the table asset cannot be re-imported without the editor), R5 mangrove snapper
  weight 4 in the Creek table. Marshal additions: teleport and `tpcheck` accept creek spots; exposure held down in the
  roofed tunnel. Editor build and smoke runs on both maps green. Follow-ups: an estate dock with a boat lift now
  stands in the Whiskey Creek pond; the chase camera clips bank trees on bends; `SpawnHotspot("Creek")` uses the old
  map's creeks; manatees come through a debug spawn hook; real boat and engine brand names in the `KeysData` boat
  table. Packaged-build perf bookmarks still owed.
- 2026-11-21: `lane/upperkeys` created from `main` after the `mangroves` merge (it also owns `Tools/geo/bake.py`); the
  `mangroves` worktree removed (branch kept).
- 2026-11-21: on `main` after the `mangroves` merge (marshal): mangroves reshaped on the user's reference photo
  (root cages, trunks, clumped crowns, `SurfCanopy` leaf shading), creek water calm and tea-stained (`M_KR_Ocean`).
- 2026-11-21: `yards` merged (lane at 9cc66fc): Cowfish Cove dry stack and boatyard east of Keys Fisheries. Requests
  applied: R1 spawn and `yd:` commands, R2 made ground and dredged cove, R3 palms and houses kept off, R4 mangroves
  kept off. Marshal addition: the hull loft no longer writes NaN into the sheer (`KeysHull.cpp`). Editor build and
  smoke runs on both maps green. Follow-ups: the player can drive through the floats, fuel dock and a launched boat;
  no going ashore there; the old box racks at the town marina and Faro Blanco still stand; the reverse (fetch)
  forklift cycle was not shot. Packaged-build perf bookmarks still owed.
- 2026-11-21: `lane/docks` created from `main` after the `yards` merge. The user asked the marshal to take care of the
  missing lifts and docks brief, so the marshal wrote it (`.marshal/prompts/docks.md`) and runs the lane as a
  background agent; the `yards` worktree removed (branch kept).
- 2026-11-21: `docks` merged (lane at 2152ed5) on the user's "merge docks", as it stands: 26 private docks with lifts
  (Marathon 10, Grassy Key 15, Duck Key 1). Requests applied: R1 spawn and `dk:` commands, R2 palms and bushes kept
  off. Editor build and smoke runs on both maps green. Open: only 77 of 503 shore houses have 1 m of water within
  reach, so the shores are not filled; the marshal proposed dredging a pocket under each dock (user chose the plain
  merge, pockets not done). No collision on docks; not tested on Phase 2 islands; house positions are a copy of
  `BuildScenery`'s dice. Packaged-build perf bookmarks still owed.
- 2026-11-21: `upperkeys` merged (lane at 0465566): Phase 2 data (p2, p2_bay), a loader that serves several grids as
  one map. Requests applied: R1 `LoadAll`, R2 mangroves seeded on p1's grid, R12 chart marks batched. Editor build and
  smoke runs on both maps green; Phase 2 land, depth, picks and chart seen. Open and owed by the marshal: R10 start-up
  9 s -> 54 s and +2.6 GB peak (all scenery built at start-up; build by tile near the boat, mangroves first) - do not
  release before this. Follow-ups R4 Channel 5 high span and old rail bridges, R6 marinas (invented names: the data
  carries real business names), R7 reef lights and sites, R8 `BUILT` list in `bake_mangrove.py`, R9 life density,
  teleports, charters, R11 chart soundings grid and edge names. Docks re-rolled slightly on Phase 1 (Marathon 10 -> 9,
  Grassy Key 15 -> 17) and now also stand on Plantation Key (4) and Walkers Island (1).
- 2026-11-21: upperkeys R10, first part (marshal): mangrove tiles are grown within 5 km of the boat, one at a time
  off the game thread, and taken down past 6.2 km (`AKeysWorld::StreamMangroves`). World start to a running game in
  the editor build: about 12 s (54 s with everything built at launch, 9 s before Phase 2). Peak memory not
  re-measured. Terrain, landmarks, bridges, palms and houses are still built whole at start-up. All lanes merged;
  no lane is running.
- 2026-11-21: dock pockets (marshal, on the user's "add the dock pockets"): a house on a thin shore gets a short dock
  with a 1.6 m berth dredged under its head, lift and boat (`KeysDocks::Pocket`, read by `KeysGeo::LandH` and the
  depth; the depth grid is re-read there). 79 docks now (32 with water of their own, 47 dredged); 253 thin-shore
  houses are still refused for mangrove in front of them (fix with R8, the `BUILT` list) and 272 for no wet ground
  in reach. Mangrove streaming: a tile's roots are only set up within 1.15 km (they caused 130-300 ms frames).
- 2026-11-21: mangrove list fixed (upperkeys R8). `bake_mangrove.py --built <osm streets json>`: a low shore within
  40 m of a residential street or residential/commercial land is not mangrove unless OSM maps it as wetland or wood.
  New raw file `osm_p2_built.json` (OSM, ODbL; `fetch_p2.sh` fetches it). Only `p2_mangrove.bin` changed (mangrove
  land 2.22 -> 1.83 km2); p1 and p2_bay are byte-identical. Docks 79 -> 126 (Tavernier 18, Upper Matecumbe 12,
  Plantation 11, Lower Matecumbe 6 are new or grew), houses 1847 -> 1928, audit clean, tpcheck 0 bad on both maps.
  Still open: 303 thin-shore houses with no wet ground in reach, 242 with mangrove in front (mostly real mangrove).
- 2026-11-22: canals cut (Upper Keys). `Tools/geo/canals.py` (called by `bake.py` for p2, needs scikit-image in the
  devkit pyenv): narrow water near residential streets is a canal; centre lines are followed longest first, every
  second one of a tight comb is kept, widened to 24 m and 2 m deep, the dropped ones filled; from each mouth a 16 m
  way is dredged along the water to charted 2.2 m. New `p2_canals.txt` (48 lines + 4 joins, 9.1 km; 45 ways out);
  `p2.kgeo`, `p2_mangrove.bin` re-baked, p1 and p2_bay byte-identical. Game: `KeysGeoReal::Canals/CanalDist/
  CanalBank/WayOutDist`, `KeysGeo::LandH` cuts the banks square with made ground at 0.85 m, `RealDepthAt` holds
  2.1 m in a canal and 1.9 m in a way out, canals are sheltered water (no surf, foam or chop), grey marl bottom,
  zone Creek (HUD "Residential canal"), terrain not drawn under the walls, houses and mangroves kept off them.
  New `KeysCanals.*` (spawned in `KeysGameMode`): 24 km of seawall and a lawn strip in 600 m tiles, 151k tris,
  `-KRNoCanals` leaves the walls out. `KeysDocks`: a canal dock is a platform on the wall with the boat along it
  (`FDock::bCanal`). Docks 126 -> 160, houses 1928 -> 1910. Checks: audit clean, tpcheck 0 bad on both maps, five
  scripted drives out of canal mouths with 1.9-2.6 m all the way, classic map builds none of it.
  Not done: seagrass tufts still grow in canals; no collision with walls beyond the ground's own; Phase 1 canals
  (Marathon, Key Colony) are not cut; one mouth has no deep water in reach; packaged perf not run.
- 2026-11-22: `lane/lowerkeys` created from `main` (c5d3ab2 + this entry) for map Phase 3, run as a background agent.
  The marshal fixes dock collision on `main` meanwhile (`KeysBoatPawn`, `KeysDocks`).
- 2026-11-22: dock collision (marshal, `KeysBoatPawn.cpp`): the player's hull stops against a private dock, its lift
  and a boat tied up there (`KeysDocks::InFootprint` at bow, middle and stern, half a beam of margin), with a toast;
  a hull already inside one is let out. Tested by driving at an open-water dock (stops 4.8 m off the head, backs
  away) and a canal dock. Not covered: traffic boats, the yards' floats and fuel dock, marina and estate docks.
- 2026-11-22: `lowerkeys` merged (lane at ac4a4fe) with its requests R1-R6 applied: US-1 extent, terrain / scenery /
  docks made by tile near the boat, rectangular chart, reef lights, teleport spots, Jewfish Creek and Card Sound
  bridges. The map is x -30060..35500, z -13720..6200 (six files: p1, p2, p2_bay, p3, p4, p4_bay). Marshal's runs
  on the merged build: world start to first frame about 6 s; dock audit 545 docks, zero overlaps; `tpcheck` 48
  destinations, 0 bad; classic map loads no real-map data, `tpcheck` 19, 0 bad; Hawk Channel on the final bake,
  Key West to Marathon (31 picks) and Tavernier to north Key Largo (18 picks), no stop, shoalest 2.9 m; peak
  memory 4.8 GB on the 10 minute run; frame 33.3 ms at the old spots, off Key West and off Key Largo. After a
  teleport the frame runs 40-48 ms for about 10 s while tiles arrive. Open from the lane: R7 marinas, R8 fish
  zones and reef names (every reef is "Sombrero Reef" on the HUD; Largo Sound's middle plays as a 0.4 m flat),
  R9 docks re-dealt by index, R10 bridges and landmarks still whole at launch, R11 houses on wild keys and the air
  station. Not checked: night, underwater, packaged build in the new areas.
- 2026-11-22: `lane/mangrovelife` created from `main` after the `lowerkeys` merge; run in a Terminal window the
  user watches. The `lowerkeys` worktree removed (branch kept).
- 2026-11-22: `lane/water2` created from `main` (this entry), run as a background agent. Schools among the
  mangrove roots belong to `mangrovelife`; open-water schools to `water2`.
- 2026-11-22: `water2` merged early at the user's word ("merge", lane at 52cb338, not READY FOR MERGE, no status
  file yet; the lane keeps running): only its own six files (one rule for the bottom over the whole map, far skirt
  carries the mean floor, named reefs' own schools). Builds without its local hub patches. Marshal's runs: real map
  `tpcheck` 48, 0 bad, dock audit zero overlaps, frame 33.3 ms at the reef, off Key West and off Carysfort (one
  222 ms hitch in the sample at `tp=-1150,452`); classic map `tpcheck` 19, 0 bad, reef looks right (its perf
  sample was taken while the lane was building, not comparable). Not yet fixed by this part: seagrass in canals,
  Largo Sound, creek stain. Merge the rest when the lane reports.
- 2026-11-22: `lane/diving` created from `main` (this entry), run as a background agent.
- 2026-11-22: `water2` merged in full (lane at 231c0eb) with R1-R5: nothing planted in canals or ways out and a
  round edge to the reef dressing (`KeysReef`), the bank rule fades out between charted 0.75 and 1.1 m
  (`RealDepthAt`; charted 1.0 m now plays 0.89 m, was 0.44), a cut from the Layton canal to Layton Channel
  (`p2_canals.txt`; `canals.py` needs the same if p2 is re-baked), creeks by rule (`KeysGeo::CreekWater`: calm,
  stained water between mangrove banks anywhere), bird-and-bait hotspots dealt round the boat. Marshal's runs:
  real map `tpcheck` 48, 0 bad; dock audit 545, zero overlaps; 33.3 ms at the reef, Layton, Carysfort, the Largo
  shelf, a Lower Keys creek and Florida Bay (a first run read 100-330 ms from its second minute on while another
  lane was compiling; the re-run was clean). Classic map `tpcheck` 19, 0 bad; at its reef the frame is 34.1 ms
  (draw 33.3, GPU 33.8), the same as after the first part of this lane; not measured before the lane. R2 re-deals
  docks and houses (8,375 houses, was 8,493; the old screenshot spot at -1150, 452 now has a dock and reads zone
  "Hawk Channel"). Open from the lane: R6 the real Largo Sound is land in p4 (bake takes enclosed water for land;
  the teleport spot is the ocean shelf), Blackwater Sound cut off; R7 reef names for p1 and p2 and reef crests
  playing as Flats; R8 46 of 195 canals have no 1 m way out to sea (4 in the Upper Keys).
- 2026-11-22: `diving` merged at the user's word (lane at 43200e4, READY FOR MERGE) with R1-R4: the scuba set worn,
  scuba rules (3,000 psi; about 65 min resting at the surface, 16 at 30 m), regulator bubbles and sound (played
  from the diver through `AKeysAudio::Burst`), gauge console, dive light, logbook, cockpit rack and diver-down flag
  (the diver's own components on the boat's `Boat` mesh: they break if that component is renamed), shop rows at
  Scuba Steve's, the cylinder gauge on the HUD, and the speargun rebuilt on the user's picture
  (`KeysScubaSpeargun.*`). Test commands: `own=`, `disown=`, `scubashop=`, `scubamode`; keys G gauge, T light,
  X swap, 2 scuba or breath. Marshal's runs: real map, two scuba dives (61 and 15 psi used, second from 2,939),
  a goliath speared on scuba and on a breath, `tpcheck` 48, 0 bad, dock audit 545; classic map, a scuba dive,
  `tpcheck` 19, 0 bad (the one shot there did not land: nearest fish 11.4 m). Frame times at the reef, Medium:
  scuba game 17.8 / 17.6, draw 15.5 / 16.1, GPU 25.8 / 23.9; free game 17.2 / 16.6, draw 15.4 / 15.2, GPU 25.5 /
  26.1: about 3% more game time on scuba, inside the noise of a shared machine. The lane's unexplained 90-120 ms
  frame turned up in a free dive here too (147.5 ms worst in one free sample), so it is not the scuba set's. Open:
  R5 (optional flag on `SetUnderwater`), R6 (perf on a quiet machine or packaged), lobster and chart markers with
  mooring balls not done, the sound never heard, buying on foot and the keys as pressed not tried, a free-diver's
  save gains three logbook keys in `Counts`.
- 2026-11-22: `mangrovelife` merged at the user's word (lane at fa7efff, READY FOR MERGE) with R1 (`PickSpecies`
  weighted by `KeysMangroveEco::PickBoost`, first bite by `BiteScale`, `eco:<command>` in scripts) and R3 (Creek
  school density .8 -> .4 in `KeysSchools.cpp`). Birds that stalk, strike, flush and roost, twelve rookery keys
  (three in each part of the chain; the busiest is rookery 7 at 10891, -4804 with 154 birds), schools under the
  roots that go out on the ebb and back on the flood, predators, crabs, all by tile near the boat. Marshal's runs:
  real map 33.3 ms off Boot Key, at rookery 7, off the Saddlebunch Keys and off Key Largo (game 8.9-11.0, draw
  5.9-7.7, GPU 25-29; the ecosystem's own cost 0.13-0.19 ms a frame); at tide -1 0 of 23 schools under the roots,
  at +1 they go back; wader strikes logged in all four parts; `tpcheck` 48, 0 bad; dock audit 545. One mangrove
  tile frame of 68 ms after a teleport (the lane's limit was 50) and 400 ms on the second frame of start-up.
  Classic map: no ecosystem built, `tpcheck` 19, 0 bad, reef frame 34.9 ms (34.1 after water2). The lane's
  rookery-7 bookmark `tp=11030,-4734,1.102` puts the boat aground. Open: R2 sound (`PopSound` not polled, nothing
  bought), R4 (8 of 13 bird kinds shown only by the test camera: birds are small in the chase view), sparse
  oysters, cormorants that seem to sit in mid-air, manatee haunts outside the Middle Keys not visited. The lane's
  worktree is left in place while its Terminal window is open.
- 2026-11-23: `lane/birdview` created from `main` (this entry), run as a background agent.
- 2026-11-23: `lane/canalways` created from `main` (this entry), run as a background agent beside `birdview`.
- 2026-11-23: `lane/reefdens` created from `main` (this entry), run as a background agent beside `birdview` and `canalways`. The user: "create hidden holes then, even if i cant explore them". `birdview` was also given the user's two pelican pictures (white pelican added, brown rebuilt, mixed), and `canalways` the user's rules: ways lead to open water, no cuts through land, no new canals, realistic.
- 2026-11-23: reef ecosystem. The user approved a roster of real Florida Keys reef species (survey and roster
  in the marshal's session; clownfish, giant clams and table corals left out as not of the region; Atlantic
  halibut, black marlin, southern flounder and the Abyssal Garden's kelp flagged as unrealistic). Order of work,
  at the user's word: `lane/reefparrot` first (created from `main` at this entry: framework, stoplight parrotfish,
  recipe), merged, and only then the other reef lanes: reef fish (tangs, damsels, wrasses, butterflyfish, grunts),
  pairs and visitors (angelfish, turtles, rays, reef shark), invertebrates and sessile life (urchins, stars,
  anemones, worms, conch, new corals), behaviour links (cleaning stations, territories, night), audio, and the
  unrealistic species. Budget: schooling fish stay at 5,200 in 72 schools within 140 m; reef-life individuals
  under 900 within 60 m; larger animals 24 groups; static reef life 3,000 pieces within 110 m; den residents 30
  animating within 25 m. Ten Artlist models (option A) approved, waiting for credits on 2026-12-13.
- 2026-11-23: `canalways` merged (lane at 220b011, READY FOR MERGE) with R1 (45 pairs of daybeacons at the outer
  ends of ways across flats, `KeysGeoReal::MarkWays`). All 195 canals now have 1.0 m of water out to the open sea
  (`fish:canals`: 0 shut, was 46); 195 canal lines before and after, ways out 202 -> 255, 4,981 m of new dredged
  water, none through land that is land in the coastline data; `canals.py` reproduces the three `_canals.txt`
  files, Layton's cut included. Houses 8,375 -> 8,368, docks 545. Open: R2 (make `canals.py` the last step of
  `bake_p3.sh` and the `geo-bake` skill); two Key Largo inlets (canals 183 and 179) may be creeks in life.
- 2026-11-23: `reefparrot` merged EARLY at the user's word ("merge reefparrot"; lane at 44c2e1f, status IN
  PROGRESS, no criterion ticked by the lane, stopped by the user) with R1-R4: `AKeysReefLife` (registry, chunks
  near the camera, instanced, cap 900 units), stoplight parrotfish (both phases, grazing, sand puffs, crunch
  through `AKeysAudio::Burst`, first-sight toast "Protected in Florida: look, don't spear") and blue tang; the
  generic parrotfish retired from `KeysLife`'s reef table; `MaxBaitFish` 3200 -> 3020 and the reef schools thinned
  to pay for it. The recipe is installed as the `reef-creature` skill. `KeysReefLife.h/.cpp` and
  `KeysReefLifeBody.cpp` are now marshal-only; each reef lane owns its own `KeysReefLife<Group>*.cpp`.
  Marshal's runs (both merges together): real map 33.3 ms at an old and a new canal and at the reef from the
  boat (game 9.7-10.9, draw 6.7-7.8, GPU 22.8-25.7); diving at the reef game 18.5, draw 17.2, GPU 22.4, worst
  49 ms; 60-66 reef-life animals alive, 0.06-0.07 ms a frame; a goliath speared; `tpcheck` 48, 0 bad; dock audit
  545. Classic map: reef life runs there too (88 animals), `tpcheck` 19, 0 bad, reef frame 34.3 ms (34.9 before).
  NOT verified by lane or marshal: night sleep and cocoon, the male's chase, counts by depth band, the perf A/B
  against `main`, the recipe's proof. Species keys are `stoplightparrot` and `bluetang`.
- 2026-11-23: the user's priority list for the reef (Florida Keys reef tract only): stoplight, rainbow and
  midnight parrotfish; queen, French and gray angelfish; green and spotted moray; Caribbean reef octopus; stone
  crab, spiny lobster, queen conch; blue tang, sergeant major, yellowtail snapper, hogfish, goliath grouper,
  great barracuda, tarpon; nurse shark, southern stingray; loggerhead and green turtles; lionfish as an
  invasive; no clownfish; elkhorn, staghorn and brain coral, purple sea fans, barrel sponges, giant Caribbean
  anemones, turtle grass beds, mangrove roots near shore. Lanes chosen for it: `reeffish` (parrotfish kinds,
  angelfish, sergeant major), `reefvisitors` (southern stingray, loggerhead and green turtle, and the audit of
  what is already there), `reefbottom` (queen conch, giant Caribbean anemone, purple sea fans, the audit of the
  reef itself), and `reefdens` resumed (morays, octopus, spiny lobster, stone crab). `reeflinks`, `reefaudio`
  and `reefreal` are held back. `birdview` was stopped by the user and is not restarted.
- 2026-11-23: `lane/reeffish`, `lane/reefvisitors` and `lane/reefbottom` created from `main` (this entry), run as background agents. `reefdens` could not be resumed (stopped by the user; its work is committed on its branch at 1be066d): a new agent only at the user's word.
- 2026-11-24: `reeffish` (5968793, IN PROGRESS by its own status), `reefbottom` (22c5f4c, READY) and `reefvisitors`
  (bc5545e, READY) merged together at the user's word. The three framework patches (visitors R1: `Build`, `Limb`,
  `LimbWave`, breathing, burying, digging; bottom R1: `Mesh`, `Pose`, `Where`, `bNoBolt`, `bOnFloor`, `HeadWithin`;
  fish R1: `DorTrail`/`AnTrail`, `bHomeOnHead`) were joined by hand in `KeysReefLife.h/.cpp`: all hooks kept,
  `Build` tried before `Mesh`, `Pose` runs after the fins or limbs are placed, a Sitter's height honours both
  `bOnFloor` and `Bury`. Also applied: fish R2 and visitors R2 (generic angelfish and turtle out of the Reef and
  Hawk Channel tables), bottom R2 (sea fans across the surge and 0.6-1.5 m, none below 16 m, barrels to 1.5 m,
  tan elkhorn), visitors R4 (nurse shark's own shape and near-still by day, slower lionfish, hogfish rooting;
  built, not looked at closely). NOT applied: visitors R3 (release-only fish not spear targets: with it the
  spear test's nearest target was 9-13 m off and nothing was speared; the game already makes a speared goliath
  a release) and bottom R3 (reef from 1 m down: changes every shallow reef). In the game now, 14 registered
  species: stoplight, rainbow and midnight parrotfish, blue tang, queen, French and gray angelfish, sergeant
  major and its guard, queen conch, giant Caribbean anemone, southern stingray, green turtle, loggerhead.
  Marshal's runs: real map 230-320 of 900 units used at Sombrero, `tpcheck` 48, 0 bad, dock audit 545, a
  goliath speared; classic map 220-254 units, `tpcheck` 19, 0 bad. Same build with and without `-KRNoReefLife`
  at the reef, Medium, frame 33.3 ms both ways: diving, game 16.4-16.6 with against 17.0-17.4 without, draw
  13.2-13.6 against 15.1-15.6, GPU 23.5-23.8 against 21.1-21.5 (about 2.3 ms, 11%, more: over the 3% rule;
  shadows from the nearest level of detail only did not change it); from the boat no difference (GPU 32-33
  both ways). To settle on a packaged build. Seen in shots: midnight parrotfish, sergeant majors, green turtle,
  stingray, anemone, queen angelfish at a distance; `reef:look` found no conch or loggerhead near the Sombrero
  spot. Open from the lanes: angelfish nip rock not sponges, no egg patch, the guard's chase never seen,
  hoverers not tied to heads or wrecks, the ray is not half buried, a turtle is not drawn in the last metre
  under the surface, no turtles resting under ledges, the recipe's section 7 is a placeholder and its other
  faults (fish R9, bottom R4), staghorn as thick fingers, turtle grass on Sombrero's crest (bottom R3).

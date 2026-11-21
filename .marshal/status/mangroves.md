# mangroves

Status: READY FOR MERGE
Updated: 2026-11-21 17:40
Branch: lane/mangroves (see `git log -1 lane/mangroves`)

Evidence lives outside the repo in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-mangroves/`
(`shots/<tag>/`, `log_<tag>.log`). Everything marked "with R1-R3" was run with those three request patches applied
locally (`git apply`), then reversed; the hub files on this branch are untouched.

## Acceptance criteria

- [x] 1. Creeks and tunnels - handed to the marshal as R1 (the ground) + R2 (the test command); passes with them applied.
  - Sister Creek (open, 20 m, 560 m): mouth `tp=-310,285,-2.9442` (Boot Key Harbor end); ocean end -330, 760.
  - Whiskey Creek (roofed tunnel, 16 m, 235 m, off Sister Creek to a pond): mouth `tp=-290,462,1.4317`; pond -518, 498.
  - Runs (with R1-R3): `idle_sister` and `idle_whiskey`, `boat=robalo`, `throttle=0.08`, `creek=0` / `creek=1`.
    Sister: -308,294 -> -332,765, zone Creek and depth 2.3-3.2 m at every sample, 4.1-4.3 kn the whole way, stops
    at the end. Whiskey: -299,461 -> -520,498, depth 2.3 m, 3.8-5.1 kn, stops in the pond.
    The exact scripts are in "Notes for integration".
  - Honest note on "no aground message in the log": toasts are not written to the log, so that grep is empty whatever
    happens. The evidence used instead is the `KRHull` speed (a grounding caps her near 1.4 kn, a beaching cuts speed
    to a tenth; it never dropped below 3.5 kn in Sister or 3.3 kn in Whiskey until she stopped at the end) and the
    `KRPicks` depth along the track.
  - `heading=` alone could not do it: the script's `heading=` took only head/follow/beam, and with a numeric heading
    (now in R2) timed legs drifted, because idle speed changes with wind and swell (she nosed into a bank in the
    tunnel). R2 therefore adds `creek=N`, which steers along the line from `KeysMangroveLife::Pilot()`.
  - Shots: tunnel, roots both sides and canopy overhead, chase view: `shots/idle_whiskey/KRScript_50.png`, `_51`,
    `_52`. Open creek, chase view: `shots/idle_sister/KRScript_50.png`, `_51`, `_52`.
- [x] 2. Wildlife.
  - Pelicans: `shots/pelicans/KRScript_50.png` (three on a snag and one on a crown at the Sister Creek harbour mouth,
    `tp=-336,304,-1.5708`, `cam=0,-0.3,0.55`). 199 pelicans map-wide without the creeks, 229 with them. Built in my
    files; no request needed.
  - Manatees: `shots/idle_whiskey/KRScript_53.png` (two in the Whiskey Creek pond beside the boat), log line
    `KRMangrove manatees lying up at -518, 497`. Needs R1 (the creek is water) and R3 (the exact KeysLife change:
    two lines) - handed to the marshal.
- [x] 3. Canopy - before `shots/canopy_before/KRScript_50.png` (cam=0,6,1.5) and `_51` (chase); after
  `shots/canopy_after/KRScript_50.png` and `_51`, same script, `tp=-1150,452,4.71`.
- [x] 4. Fishing - snook first in every sample; mangrove snapper second except around dawn. Handed on as R5 (one number).
  `KRPicks at -378, 457 zone Creek depth 2.3 m: snook 28% mangrove 16% redfish 15% tarpon 11% jackcrevalle 9% ladyfish 7% flounder 7%`
  (`log_picks_creek.log`, `clock=19:30`, with R1). 16 samples there and at -275, 425: dusk 8 of 8 snook + mangrove
  snapper on top, noon 5 of 5, dawn (06:30) 1 of 3 (redfish second twice). Mangrove snapper and redfish both weigh 3
  in KeysData's Creek table, so it is a near tie whenever neither is feeding harder; the table is a hub file.
- [x] 5. Requests written, not done: `.marshal/requests/mangroves/R4-freefish-spots-and-charters.md`.
- [x] 6. No slowdown - `tp=-1150,452,4.71`, Medium, 1600x900, editor build, two 5 s samples each:
  before 35.04 / 34.87 ms; after (this branch as it is) 35.18 / 35.00 ms (+0.4%); after with R1-R3 applied
  35.91 / 35.68 ms (+2.4%). Tiles and draw distances unchanged (600 m, roots 700 m, canopy 4.5 km).
  Caveat: the log says `paced 30`, and both numbers sit just above 33.3 ms with the GPU as the limit, so this spot may
  understate a difference. Canopy triangles went from 1.35 M to 2.34 M (2.47 M with creeks), but with shared vertices
  the vertex count went down (about 3.2 M to 1.3 M).

## Done

- `Source/KeysRun/KeysMangrove.cpp`: crowns are lumpy, smooth-shaded domes with shared vertices, dark underneath and
  light on top; creek banks (trees leaning over the water, roots wading in), the tunnel roof (limbs arching from both
  banks, canopy about 7-8 m clear over mid-channel, gaps for light), pelican snags and crown-top pelicans; nothing is
  planted in a channel or on an estate lot. The creek scenery is built only when KeysGeo has the creeks (R1).
- `Source/KeysRun/KeysMangroveLife.cpp/.h` (new): the creek lines (`Creeks()`, `Near()`), the test helmsman
  (`Pilot()`), the perched pelican and snag, `AKeysMangroveLife` (asks KeysLife for manatees at three haunts when the
  boat comes within 200 m; spawned from `BuildMangroves()`, no hub call needed).
- `Tools/geo/bake_mangrove.py`: reads the creek table from KeysMangroveLife.cpp and puts mangrove on land within 34 m
  of a creek's edge, estate lots left out; the preview shows the channels.
  `OSM wetlands 102, woods 87; mangrove land 1.18 km2 (of 4.54 km2 land), fringe cells 3552, 3 creeks with 196 new bank cells`
- `Content/KeysRun/Geo/p1_mangrove.bin`: re-baked with the above (same format and size).

## In progress

- Nothing.

## Blocked

- Nothing.

## Requests for the marshal

- R1 (open): `Source/KeysRun/KeysGeo.cpp` - on the real map fill `GCreeks` from `KeysMangroveLife::Creeks()` instead
  of leaving it empty (LandH already cuts along `GCreeks`), and keep 2.3 m of water in them in `RealDepthAt`. Without
  it there are no creeks at all. patch: `.marshal/requests/mangroves/R1.patch`
- R2 (open): `Source/KeysRun/KeysBoatPawn.cpp` - test commands only: `creek=N` (hold the boat on creek N's line to
  its head, then throttle off) and `heading=<radians>`. Needed to re-run criterion 1.
  patch: `.marshal/requests/mangroves/R2.patch`
- R3 (open): `Source/KeysRun/KeysLife.cpp` - the two `ShoreInfo(...).D < 4` tests (`SpawnGroupAt`, and the look-ahead
  in `UpdateCreatures`) also let a creature be where `CreekDist(...) <= -3`. The baked shore distance says a cut creek
  is dry land, so without this no visible creature, manatees included, can be in one.
  patch: `.marshal/requests/mangroves/R3.patch`
- R4 (open): Free Fish spots (`RunDests()` in KeysBoatPawn.cpp) and snook, tarpon and mangrove snapper charter rows
  (`Content/KeysRun/Data/CharterRequests.json`), with names and coordinates:
  `.marshal/requests/mangroves/R4-freefish-spots-and-charters.md`
- R5 (open): `Source/KeysRun/KeysData.cpp`, Creek zone table - `mangrove` 3 -> 4, so snook and mangrove snapper are
  the top two at every hour (criterion 4). Described at the end of the R4 file.

## Notes for integration

- Apply order: merge the branch, then R1, R2, R3 (`git apply`; all three checked against this branch's base).
  R1 and R2 make KeysGeo.cpp and KeysBoatPawn.cpp include `KeysMangroveLife.h`.
- Re-running criterion 1 (after R1-R3):
  - Whiskey: `3:play;4:gfx=1;5:boat=robalo;6:tp=-290,462,1.4317;7:cam=0,-0.25,0.6;9:throttle=0.08;9.1:creek=1;40.5:shotclean;58.5:shotclean;74.5:shotclean;108:cam=0,0.5,0.9;112:shotclean;123:quit`
  - Sister: `3:play;4:gfx=1;5:boat=robalo;6:tp=-310,285,-2.9442;7:cam=0,0,1;9:throttle=0.08;9.1:creek=0;45.5:shotclean;110.5:shotclean;170.5:shotclean;246:quit`
  - add `picks` every 10-20 s for the track; `grep KRHull` for the speed.
- Things I saw and do not own:
  - In the tunnel the water burns out to white in the chase view. The roof shades everything, auto-exposure opens
    up, and the water still reflects open sky (reflections do not seem to see the procedural canopy). Exposure and
    the sea material are hub / water-lane; a reflection or exposure clamp where `MangroveNear` is high would fix it.
  - The chase camera has no collision: at the default zoom on bends it sits in the bank trees. `cam=...,0.55` is fine.
  - With R1 the hub's hashed waterfront-estate picker (`KeysTownData::WaterfrontEstates`) sees deep water in new
    places: one estate dock with a boat lift now stands in the Whiskey Creek pond, and the set on Boot Key and along
    Sister Creek may have reshuffled. If the pond should stay wild, reject sites whose dock ends where
    `CreekDist < 0`.
  - `AKeysLife::SpawnHotspot("Creek")` still samples the old map's creek end points; on the real map it could use
    `KeysMangroveLife::Creeks()`.
  - Sister Creek's own daybeacons stand in mid-channel at its ocean mouth (8 at -320, 771): my line stops at
    -330, 760, just short of them. A first version ran over number 8 and the boat stuck on it for 25 s.
  - The manatees come from `AKeysLife::DebugSpawn` (a public test hook, ignores the population cap: at most three
    groups of 1-2, re-armed only after the boat has been 330 m away). A proper "resident" call would be cleaner.
  - The boat table in KeysData.cpp carries real boat and engine brands. Not
    mine and not touched; flagged because the lane rule says no real brands.
- The `world-builder`, `geo-data` and `verifier` agent types were not available in this session; the work and the
  checks were done in one session following those three briefs (every screenshot cited above was opened and read).
- `-KRClassicMap` was not run: `BuildMangroves()` still returns at once on the old map and nothing else of mine runs there.

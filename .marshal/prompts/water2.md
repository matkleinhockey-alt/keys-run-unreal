You are the `water2` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the Florida Keys. You know
nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-water2` (run every command from inside it)
- Branch: `lane/water2`
- Read first: `MARSHAL.md` (the log at the end says what the map is today), `.claude/agents/sea-life.md`,
  `.claude/agents/world-builder.md`, `.claude/agents/verifier.md`, and the skills in `.claude/skills/` (`build`,
  `scripted-run`, `perf-check`, `lane-status`, `marshal-request`).
- What the first water lane did and how it measured: `git show lane/water:.marshal/status/water.md`. Read it whole.

## Goal

The sea, the seafloor and the bait schools were tuned when the map was the Middle Keys alone. The map is now three
times the area: the Lower Keys to Key West, the Upper Keys, Key Largo and a long strip of Florida Bay. Make the
water, the bottom and the schools right over all of it, and fix the water faults found since.

The user said only "Start the water lane", after the marshal listed the jobs below. The split into criteria is the
marshal's reading. Say so in your status file and list anything you had to guess.

## Background

- The world is generated in C++ at run time; there are no placed assets. Game coordinates are metres: x along the
  chain (east is +x), y up, z south (ocean side). `KeysGeo::ToUE(X, Y, Z)` converts to Unreal units. The map is
  x -30060..35500, z -13720..6200, from six baked files in `Content/KeysRun/Geo` (p1 Middle Keys, p2 Upper Keys,
  p2_bay, p3 Lower Keys, p4 Key Largo, p4_bay Florida Bay). `-KRClassicMap` runs the older invented map.
- Depth and ground come from `KeysGeo` (hub): `LandH`, `DepthAt`, `RealDepthAt`, `ShoreInfo`, `SurfaceKind`,
  `ZoneAt`, `CanalDist`, `CreekDist`. The game plays charted water under 2.4 m shallower than charted (a bank
  rule in `RealDepthAt`).
- Terrain, houses, docks and mangroves are made by tile near the boat (`AKeysWorld::StreamTerrain`,
  `StreamScenery`, `StreamMangroves`); the seafloor has its own resident-tile scheme (`KRSeafloor` log lines).
- Residential canals (Upper Keys so far, `KeysGeo::CanalDist`, built in `KeysCanals.cpp`) are seawalled, held at
  about 2 m, sheltered (no surf or chop) with a grey marl bottom within 3 m of the canal (`KeysSea.cpp`). Their
  ways out to deep water are `KeysGeoReal::WayOutDist`.
- Mangrove creeks with calm, tea-stained water exist only as named lines in the Middle Keys
  (`KeysMangroveLife::Creeks()`, `KeysGeo::CreekDist`; the stain is in the ocean material and the water mesh's
  vertex data in `KeysWorld.cpp`).
- Another lane, `mangrovelife`, is building animals in the mangroves at the same time, including small fish
  among the prop roots, in `KeysMangrove.cpp`, `KeysMangroveLife.*` and new `KeysMangroveEco*` files. Yours are the
  schools of open water, reef, channel and flat. Leave the roots to it, and do not touch its files.

## Owned paths

- `Source/KeysRun/KeysSeafloor.cpp`, `KeysSeafloor.h`
- `Source/KeysRun/KeysSea.cpp`, `KeysSea.h`
- `Source/KeysRun/KeysOcean.cpp`, `KeysOcean.h`
- `Source/KeysRun/KeysSchools.cpp`, `KeysSchools.h`
- `.marshal/status/water2.md`, `.marshal/requests/water2/`

Everything else is read-only for you, including `KeysGeo`, `KeysGeoReal`, `KeysWorld`, `KeysCanals`, `KeysLife`,
`KeysData`, the materials and config. What you need from them goes to the marshal as numbered patch requests
(`marshal-request` skill): apply them in your worktree to test, never commit them. Binary assets (materials) can
only be changed by a script the marshal runs: describe the change exactly.

## Deliverables and acceptance criteria

1. **Survey first.** Before changing anything, a scripted run visits at least twelve spots across the new areas
   (Key West harbor and the flats west of it, Hawk Channel off Sugarloaf, Looe Key and American Shoal reefs, a
   Lower Keys backcountry basin, Florida Bay north of Islamorada, Largo Sound, Molasses and Carysfort reefs, the
   drop-off off Key Largo, a Lower Matecumbe canal at `tp=13232,-1280,0`, a way out of that canal) with a
   chase-view shot, a top-down shot (`cam=0,25,3`) and a shot with the camera under the surface at each. Put a
   table in your status file: spot, what looks wrong, which file owns it. This table decides the rest of your work.
2. **The bottom is right everywhere.** Reef, hard bottom, grass, sand and mud appear where the depth and zone say
   they should, in every part of the map, with no bare or repeated stretch and no hard edge between kinds. A
   top-down shot off Carysfort Reef (`tp=33770,-8350,3.14`, `cam=0,25,2`) showed rectangular patches of a
   different tint in the water: find the cause, fix it or hand it on as a request.
   - Pass: before and after shots at the survey spots that changed; the Carysfort shot with no rectangles and the
     cause stated.
3. **Canals are clean.** No seagrass tufts, coral or reef props on the bottom of a residential canal or its way
   out; the bottom reads as grey marl from wall to wall; nothing of the seafloor pokes through a seawall.
   - Pass: chase-view and underwater shots in two canals and one way out. One canal mouth in the Upper Keys has no
     deep way out (the bake's report, `Content/KeysRun/Geo/p2_canals.txt`, and MARSHAL.md): find which, and write
     the request that opens it.
4. **Largo Sound floats a boat.** The middle of Largo Sound (`tp=27570,-6640,0`) is charted about 1 m and plays
   as a 0.4 m flat, so the boat grounds. The rule is in `KeysGeo::RealDepthAt` (hub). Say how much of the map's
   charted 1.0 - 2.4 m water the rule grounds a boat in, which named sounds, basins and marked channels it closes,
   and deliver the smallest change that opens them as a request, tested locally.
   - Pass: a scripted run idles across Largo Sound with no "aground" in the log, with your request applied; the
     Middle Keys flats still ground a boat where they did (give three spots before and after).
5. **Schools over the whole map.** Bait and school fish are as thick off Key West and Key Largo as they were off
   Marathon before the map grew, without the total alive near the boat going up: schools belong to the water near
   the boat, not to the whole map box. Each named reef has schools of its own.
   - Pass: `KRLife` / `KRSchools` log counts near the boat at five spots (two old, three new), and shots at Looe
     Key and Molasses. State the rule.
6. **Stained water in every creek.** Narrow water between mangrove banks anywhere on the map is calm and
   tea-stained as the Middle Keys creeks are, found by rule from the map data and not from the list of named
   creeks. Canals stay clear. This is mostly hub and material work: deliver it as a request with a patch, and do
   in your own files what can be done there.
   - Pass: with the request applied, shots of a creek in the Lower Keys, one in Florida Bay and one on Key Largo,
     and of a canal for contrast.
7. **The classic map is unharmed.** `-KRClassicMap` looks the same at the marina start and at `goto=reef`, with
   no new log errors.
8. **Nothing else moves.** `tpcheck` reports 0 bad; `dk:audit` reports zero overlaps; the old screenshot spots
   look the same or better (`tp=-1150,452,4.71` with `cam=0,6,1.5`; `goto=reef`).
9. **No slowdown, measured.** `perf-check` on Medium at `tp=-1150,452,4.71`, at `goto=reef`, off Key West
   (`tp=-24660,-243,4.49`) and off Carysfort: game, draw and GPU times within 3% of before. No new frame over
   50 ms while running at full throttle along Hawk Channel for two minutes. Start-up to a running game does not
   grow by more than 0.5 s.

## Agents and skills

- Do the schools as `sea-life`, the bottom and the surface as `world-builder`; check each criterion as `verifier`
  before you tick it. If those agent types are not available in your session, follow their briefs yourself.
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first.
  Check the build log for `Result: Succeeded` before any scripted run: a failed build followed by a run hangs the
  editor on a rebuild prompt.
- Only one copy of the game runs at a time on this machine (`shoot.sh` takes a lock that expires after 20
  minutes), and another lane and the marshal run tests too: a scripted run may wait its turn. Keep runs short.
  Never kill a game process you did not start.
- The editor build is capped at 30 fps (33.3 ms); read game, draw and GPU times, not the frame time alone. The
  game is running about 8 s after the world starts; a first `tp` at 10 is safe. After a teleport, wait 10 s for
  tiles to arrive before a shot or a perf sample.
- Disk space is tight (about 15 GB free): do not make a packaged build, and delete scratch files you no longer
  need.
- Open and look at every screenshot you cite.

## Dependencies

None. `main` holds everything merged so far. The `mangrovelife` lane runs at the same time in its own files.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never use a bare `git stash` (it is
shared across worktrees), never push anything, use no real brands or logos, and send nothing about the user or the
project to any outside service. End every commit message with these two lines:

Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_015oSiK1WwZ5JkZ11PikjADc

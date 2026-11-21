You are the `upperkeys` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the Florida Keys. You know
nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-upperkeys` (run every command from inside it)
- Branch: `lane/upperkeys`
- Read first: `MARSHAL.md`, `.claude/agents/geo-data.md`, `.claude/agents/world-builder.md`,
  `.claude/agents/verifier.md`, and the skills in `.claude/skills/` (`geo-bake`, `build`, `scripted-run`,
  `perf-check`, `lane-status`, `marshal-request`).

## Goal

Map Phase 2: extend the real-geography map east from Marathon through the Upper Keys, so a boat can run from Boot Key
Harbor past Long Key and Islamorada to Tavernier over real coastline and real depths.

## Background

- The map is real Florida Keys geography in a fixed frame: 1:3 horizontal compression (depths not compressed),
  rotated 19 degrees so US-1 runs along game +x, anchored on Vaca Key at game (-120, 0), on a 10 m grid. Game
  coordinates are metres: x along the chain (east is +x), y up, z south (ocean side).
- Phase 1 (Middle Keys) is shipped: `Content/KeysRun/Geo/p1.kgeo` covers x -8300..7500, z -4200..6200, with
  `p1_places.json`, `p1_marks.txt`, `p1_plat.txt` and `p1_mangrove.bin` beside it. `Tools/geo/bake.py` documents the
  `.kgeo` format in its header and has a `PHASES` table with one entry.
- `Source/KeysRun/KeysGeoReal.cpp` loads one phase (`Load(TEXT("p1"))`) and answers `Height`, `Shore`, `IslandAt`,
  `ChainZ`, `Marks`, `Plat`, `MangroveAt`. `KeysGeo` (marshal-only) reads the world through it and holds the world
  bounds (`WorldX0`, `WorldZ0`, `WorldSizeX`, `WorldSizeZ`), the chart, and where content is placed.
- Raw downloads live outside the repo in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/geo/raw`. Sources so
  far: NOAA NCEI elevation (public domain), OpenStreetMap coastline, roads and land use (ODbL, credited in the game),
  NOAA ENC Direct chart marks (public domain).
- Phase 1 is described in `DEVELOPMENT.md` and `PERF.md`. Terrain is drawn in 512 m tiles with a draw distance;
  landmarks go into 1200 m tiles.

## Owned paths

- `Source/KeysRun/KeysGeoReal.cpp`, `KeysGeoReal.h`
- `Tools/geo/bake.py`, and any new `Tools/geo/*p2*` script
- `Content/KeysRun/Geo/p2_*` and `Content/KeysRun/Geo/p2.kgeo`
- `.marshal/status/upperkeys.md`, `.marshal/requests/upperkeys/`

Everything else is read-only for you, including the Phase 1 baked files, `Tools/geo/frame.py` and `Tools/geo/kgeo.py`.
Changes to `KeysGeo`, the chart, bridges, landmarks or config go to the marshal as patch requests (`marshal-request`
skill): apply them in your worktree to test, never commit them.

## Deliverables and acceptance criteria

1. **Phase 2 data.** `p2.kgeo` on the same frame and 10 m grid, joining Phase 1 at x = 7500 and running east to
   Tavernier (choose the east edge and the z range from the data and state them), with `p2_places.json` (islands with
   names and outlines, US-1 bridges with end points, marinas, reefs), `p2_marks.txt` and `p2_mangrove.bin`.
   - Pass: each script's summary line in your status file; a preview PNG of the whole of p1 + p2 that you have looked
     at; the named islands include Long Key, Lower Matecumbe, Upper Matecumbe, Windley Key, Plantation Key and
     Tavernier; the US-1 bridges include the Long Key Viaduct and the Channel 5 and Channel 2 bridges.
2. **No seam.** Heights and shore distances agree where the two phases meet.
   - Pass: along x = 7500 the height difference between p1's last column and p2's first is under 0.2 m at every row
     (give the largest), and a top-down screenshot across the join shows no step in land or water.
3. **The game loads both.** `KeysGeoReal` serves p1 and p2 as one map through its existing functions, and loads p2
   only on the real map. Loading is streamed or otherwise kept cheap: state the extra memory and the extra start-up
   time, measured.
   - Pass: with your `KeysGeo` request applied locally, a scripted run teleports to three places in Phase 2 (off Long
     Key, off Islamorada, off Tavernier) and takes a chase-view and a top-down shot at each: land, shoreline, water
     depth colour and the sounder reading all look right, and `picks` returns a sensible zone at each.
4. **A boat can make the trip.** The marked channels in Phase 2 are open, and Hawk Channel on the ocean side is
   clear water from Marathon to Tavernier.
   - Pass: a scripted run at speed along Hawk Channel across the join, with `picks` every 20 seconds for depth, and
     no stop.
5. **The chart covers it.** The chart (`bigmap`) shows Phase 2 with its islands named. This is `KeysGeo` and
   `KeysHUD` work: deliver it as a request if it does not follow from the data.
6. **Requests written, not done.** Everything Phase 2 needs from hub files, each as a numbered request: the world
   bounds and chart, US-1 and its bridges east of 7500, daybeacons, marinas, the reef line (Alligator Reef and its
   light, Tennessee Reef), mangrove cover, fish zones. Say which ones are needed for the criteria above and which
   are follow-ups.
7. **Nothing else moves.** Phase 1 content stays where it is: `tpcheck` reports 0 bad, and the Phase 1 screenshot
   spots look the same (`tp=-1150,452,4.71` with `cam=0,6,1.5`; `goto=reef`).
8. **The classic map is unharmed.** `-KRClassicMap` runs with no new log errors and loads no Phase 2 data.
9. **No slowdown at Marathon.** `perf-check` at `tp=-1150,452,4.71` and at the marina start: within 5% of before.
   Report frame time off Islamorada as well.

## Agents and skills

- Bake work as `geo-data`, loader and terrain as `world-builder`; check each criterion as `verifier` before you tick
  it. If those agent types are not available in your session, follow their briefs yourself.
- Skills: `geo-bake`, `build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`.
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first, and
  fetch data while it runs.
- Outside requests carry nothing about the user or the project: User-Agent `map-data-fetch/1.0` and nothing else
  identifying. Read a source's licence before adding it; say in your status file what you added.

## Dependencies

`main` already contains the merged `water` and `mangroves` lanes: the sea is drawn at the tide, and two mangrove
creeks are cut near Boot Key (`KeysMangroveLife::Creeks()`, read by `KeysGeo`). You merge last, after `yards`. The
`yards` lane runs at the same time in another worktree and shares the machine: only one game runs at once, so a
scripted run may wait its turn. Do not make a packaged build.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never push anything, and use no real
brands or logos.

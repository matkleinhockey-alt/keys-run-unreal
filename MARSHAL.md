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
| `upperkeys` | Map Phase 2 (Upper Keys) | `lane/upperkeys` | `../KeysRun-upperkeys` | running |

Not a lane yet: lifts and docks (P4), whose brief has not been given. Map Phase 3 becomes a lane after `upperkeys`
merges.

## Ownership

No two lanes own the same file. Every lane also owns `.marshal/status/<lane>.md` and `.marshal/requests/<lane>/`.

| Lane | Owned paths |
|---|---|
| `water` | `Source/KeysRun/KeysSeafloor.*`, `KeysSea.*`, `KeysOcean.*`, `KeysSchools.*` |
| `mangroves` | `Source/KeysRun/KeysMangrove.cpp`, new `Source/KeysRun/KeysMangroveLife.*`, `Tools/geo/bake_mangrove.py`, `Content/KeysRun/Geo/p1_mangrove.bin` |
| `yards` | new `Source/KeysRun/KeysYards.*`, `KeysForklift.*`, `KeysYardProps.*` |
| `upperkeys` | `Source/KeysRun/KeysGeoReal.*`, `Tools/geo/bake.py`, new `Tools/geo/*p2*`, `Content/KeysRun/Geo/p2_*` and `p2.kgeo` |

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

You are the `canalways` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the Florida Keys. You know
nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-canalways` (run every command from inside it)
- Branch: `lane/canalways`
- Read first: `MARSHAL.md` (the log at the end says what the map is today), `.claude/agents/geo-data.md`,
  `.claude/agents/world-builder.md`, `.claude/agents/verifier.md`, the skills in `.claude/skills/` (`geo-bake`,
  `build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`), and the header of `Tools/geo/canals.py`.
- What found the problem: `git show lane/water2:.marshal/status/water2.md` (its criterion 3, its request R8 and
  how the Layton canal was opened with one "w" line in `p2_canals.txt`). How the canals were made:
  `git show lane/upperkeys:.marshal/status/upperkeys.md` and `git show lane/lowerkeys:.marshal/status/lowerkeys.md`.

## Goal

The map has 195 residential canals (seawalled, about 2 m deep, in `Content/KeysRun/Geo/p2_canals.txt`,
`p3_canals.txt`, `p4_canals.txt`). A boat that needs 1 m of water cannot get from 46 of them to the open sea: the
canal is a pond on the grid, or its mouth is pinched, or the flat outside is too shallow and no way was dredged.
Open every one of them, so a boat kept at any canal dock can run out to deep water and back.

The user picked this from a list, in the marshal's words: "Open the 46 canals that have no way out to sea, mostly
in the Lower Keys and Key Largo." Everything below is the marshal's reading of that. Say so in your status file
and list anything you had to guess.

## What is already there

- Game coordinates are metres: x along the chain (east is +x), y up, z south (ocean side); the map is
  x -30060..35500, z -13720..6200 on a 10 m grid, baked per phase (p1 Middle Keys, p2 Upper Keys, p3 Lower Keys,
  p4 Key Largo and bay strips). `-KRClassicMap` runs the older invented map, which has none of this.
- `Tools/geo/canals.py` writes each phase's `_canals.txt` (canal centre lines widened so a boat fits, and "w"
  lines: ways dredged out to deep water). The game reads them in `KeysGeoReal` (`CanalDist`, `WayOutDist`),
  `KeysCanals.cpp` builds the seawalls, `KeysGeo::RealDepthAt` gives the depth the hull reads, and `KeysSea.cpp`
  keeps canals and ways calm with a marl bottom. The marked ways out get no daybeacons today.
- The test: the script command `fish:canals` (or `fish:canals=1.0`) runs `KeysSea::LogCanalWays`, which floods
  from every canal through the water the hull itself reads and logs each canal with no 1.0 m of water out to
  the open sea, with the "w" line that would open it. Today: 46 of 195. Four are in the Upper Keys: canals round
  12960, -1008; 14530, -1648; 13300, -1168 (ponds on the grid, mouths pinched as Layton's was) and 20115, -3402
  (70 m short: `w 16 20075.0 -3400.0 20050.0 -3400.0 20025.0 -3400.0 20005.0 -3400.0`). The other 42 are in the
  Lower Keys and Key Largo, whose bakes report 18 and 7 mouths with no deep water in reach.
- The Layton fix was a hand-added line in `p2_canals.txt` (`24 10082.5 -802.5 10105.0 -822.0 10150.0 -850.0`)
  that `canals.py` does not know about: a re-bake of p2 would lose it.
- Raw downloads live outside the repo in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/geo/raw`. Python is
  `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/pyenv/bin/python`.

## Owned paths

- `Content/KeysRun/Geo/p2_canals.txt`, `p3_canals.txt`, `p4_canals.txt`
- `Tools/geo/canals.py`, and any new `Tools/geo/canalways*` script
- `.marshal/status/canalways.md`, `.marshal/requests/canalways/`

Everything else is read-only for you, including every `.kgeo`, `KeysGeo`, `KeysGeoReal`, `KeysCanals`, `KeysSea`,
`KeysDocks`, `KeysWorld` and config. What you need from them goes to the marshal as numbered patch requests
(`marshal-request` skill): apply them in your worktree to test, never commit them. Another lane, `birdview`, is
working in `KeysMangroveEco*` and `KeysMangroveLife.*` at the same time: do not touch its files.

## Deliverables and acceptance criteria

1. **The list.** Run `fish:canals` on `main` as it is and put the 46 in your status file: canal, where, which
   phase, why it is shut (pond on the grid, pinched mouth, shallow flat outside, cut off by a road or bridge
   with no opening, not a canal at all), and how far the nearest 1 m water is.
2. **Every canal has a way out.** Each is opened in the way that fits: the mouth widened, a cut through a
   pinch, or a dredged way across the flat to the nearest marked channel or deep water, as short and as straight
   as the ground allows, never through a house lot, a road, a bridge approach, a mangrove key or a named
   landmark, and where there is a real channel there (look at the chart data in the raw downloads), along it. A
   canal that turns out not to be one (a pond, a borrow pit, a lake with no opening in life) may be left shut:
   say which and why.
   - Pass: `fish:canals` reports 0 shut (or only the ones you left on purpose, listed), with requests applied;
     top-down shots (`cam=0,25,2`) of ten of the new ways, spread over the Lower Keys, Upper Keys and Key Largo.
3. **Made by the tool, not by hand.** `canals.py` produces every way, including Layton's, so a re-bake gives
   the same files: running it for p2, p3 and p4 reproduces the committed `_canals.txt` byte for byte. Canals
   that were open before keep the same lines (state the diff: lines added, lines changed, none removed without
   a reason).
4. **A boat can make the trip.** A scripted run takes the boat from inside six of the newly opened canals (two
   in each part) out to open water at idle and then at 15 kn with no "aground" in the log, and back in.
5. **They look right.** The new ways read as dredged channels: calm, deeper-coloured water with the marl bottom,
   clean edges, no seagrass or reef dressing in them, nothing of the seafloor poking through a seawall, no docks
   or houses left standing in the water of a new cut.
   - Pass: chase-view and top-down shots at four of them; `dk:audit` zero overlaps.
6. **Marked, as a request.** A way across a flat should carry a pair of daybeacons at its outer end, as the
   other channels do. That is hub work (`KeysGeo` / marks): deliver it as a request with a patch and the list
   of positions, tested locally.
7. **What moved.** The depth change re-deals docks and houses: state the dock and house counts before and after,
   and show the old screenshot spots are the same or better (`tp=-1150,452,4.71` with `cam=0,6,1.5`;
   `tp=13232,-1280,0` in a Lower Matecumbe canal).
8. **Nothing else moves.** `tpcheck` 0 bad; the classic map (`-KRClassicMap`) loads no real-map data and has
   no new log errors; start-up time to a running game does not grow by more than 0.5 s; `perf-check` on Medium
   at `tp=13232,-1280,0` and in one newly opened canal: 33.3 ms, game, draw and GPU within 3% of before.

## Agents and skills

- Bake work as `geo-data`, anything in the game as `world-builder`; check each criterion as `verifier` before
  you tick it. If the agent types are not available in your session, follow their briefs yourself.
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first.
  Check the build log for `Result: Succeeded` before any scripted run: a failed build followed by a run hangs the
  editor on a rebuild prompt and holds the game lock.
- Only one copy of the game runs at a time on this machine (`shoot.sh` takes a lock that expires after 20
  minutes), and another lane and the marshal run tests too: a scripted run may wait its turn. End every script
  with a `quit` step, or the game runs for ever. Never kill a game process you did not start.
- The editor build is capped at 30 fps (33.3 ms); read game, draw and GPU times, not the frame time alone. After a
  teleport, wait 10 s for tiles before a shot or a perf sample; never teleport inside a `perf=` sample (5 s).
  Frame times taken while another lane is compiling are worthless: take them again.
- Outside requests, if you need any, carry nothing about the user or the project: User-Agent
  `map-data-fetch/1.0` and nothing else identifying. Prefer the raw data already downloaded.
- Do not make a packaged build. Delete scratch files you no longer need, and only your own: never `rm` with a
  wildcard in a shared directory.
- Open and look at every screenshot you cite. No real brands, business names or logos.

## Dependencies

None. `main` holds everything merged so far.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never use a bare `git stash`, never
push anything. Write your status file and commit early and often: a usage limit can stop you at any moment, and
what is committed is what survives. End every commit message with these two lines:

Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_015oSiK1WwZ5JkZ11PikjADc

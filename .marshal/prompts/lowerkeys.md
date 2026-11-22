You are the `lowerkeys` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the Florida Keys. You know
nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-lowerkeys` (run every command from inside it)
- Branch: `lane/lowerkeys`
- Read first: `MARSHAL.md` (the log at the end says what the map is today), `.claude/agents/geo-data.md`,
  `.claude/agents/world-builder.md`, `.claude/agents/verifier.md`, the skills in `.claude/skills/` (`geo-bake`,
  `build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`), and the header of `Tools/geo/bake.py`.
- How Phase 2 was done, by the lane before you: `git show lane/upperkeys:.marshal/status/upperkeys.md`. Read it
  whole: its method, its measurements and its requests are the pattern for yours.

## Goal

Map Phase 3: extend the real-geography map to both ends of the Keys, so a boat can run from Key West to the top of
Key Largo over real coastline and real depths.

- **Part A, the Lower Keys (do this first):** west from Phase 1's edge at x = -8300 through Big Pine, the Torch Keys,
  Summerland, Cudjoe, Sugarloaf, the Saddlebunch Keys, Boca Chica and Stock Island to Key West.
- **Part B, Key Largo:** east and north from Phase 2's edge at x = 23500 through Rock Harbor and Key Largo to the top
  of the island (Jewfish Creek and the Card Sound end), with Largo Sound and the reef line off it.

The user said only "start phase 3"; an earlier plan names it "Lower Keys + Key Largo". The split, the order and the
criteria below are the marshal's reading. Say so in your status file and list anything you had to guess.

## Background

- The map is real geography in a fixed frame (`Tools/geo/frame.py`, never redefine it): 1:3 horizontal compression
  (depths not compressed), rotated 19 degrees so US-1 runs along game +x, anchored on Vaca Key at game (-120, 0), on
  a 10 m grid. Game coordinates are metres: x along the chain (east is +x), y up, z south (ocean side).
- Baked today, in `Content/KeysRun/Geo`: `p1` (Middle Keys, x -8300..7500, z -4200..6200), `p2` (Upper Keys,
  x 7500..23500, z -6920..6200) and `p2_bay` (the bay strip north of p1, x -8300..7500, z -6920..-4200). Each phase
  has `.kgeo`, `_places.json`, `_mangrove.bin`, and some have `_marks.txt`, `_plat.txt`, `_canals.txt`.
- `Tools/geo/bake.py` has a `PHASES` table; a later phase joins the edges of ones already baked so heights and shore
  distances meet with no step. `bake_marks.py`, `bake_mangrove.py` (pass `--built <streets json>` as p2 does) and
  `canals.py` (PHASES `canals`: residential canals widened so a boat fits, with ways dredged out to deep water)
  run after it. `fetch_p2.sh` shows every download Phase 2 needed.
- `Source/KeysRun/KeysGeoReal.cpp` loads every phase (`LoadAll`) and serves them as one map. `KeysGeo` (marshal-only)
  holds the world bounds, the chart and where content goes.
- Raw downloads live outside the repo in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/geo/raw`. Python is
  `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/pyenv/bin/python` (numpy, scipy, shapely, pillow,
  scikit-image). The public Overpass servers often fail or cut a reply short: retry, try the mirrors, split a big
  box into pieces, and check that every download parses before you use it.
- The whole world's terrain, bridges, landmarks, palms and houses are still built at start-up (mangroves are
  streamed by tile). Phase 2 took start-up from 9 s to 54 s before the mangroves were streamed; it is about 12 s
  now, and peak memory was 8 GB on this 16 GB machine. Phase 3 roughly doubles the map again, so cost is a
  first-class deliverable here, not a footnote.

## Owned paths

- `Source/KeysRun/KeysGeoReal.cpp`, `KeysGeoReal.h`
- `Tools/geo/bake.py`, and any new `Tools/geo/*p3*` or `*p4*` script
- `Content/KeysRun/Geo/p3*` and `Content/KeysRun/Geo/p4*`
- `.marshal/status/lowerkeys.md`, `.marshal/requests/lowerkeys/`

Everything else is read-only for you, including the baked files of p1, p2 and p2_bay, `Tools/geo/frame.py`,
`kgeo.py`, `canals.py`, `bake_marks.py` and `bake_mangrove.py`. Changes to any of those, to `KeysGeo`, the chart,
bridges, landmarks, terrain streaming or config go to the marshal as numbered patch requests (`marshal-request`
skill): apply them in your worktree to test, never commit them. The marshal is working on `main` at the same time
(dock collision, in `KeysBoatPawn` and `KeysDocks`), so expect those files to move.

## Deliverables and acceptance criteria

Name the phases as fits the data (for example `p3` for the Lower Keys with a bay strip to square the map off, `p4`
for Key Largo) and state every rectangle. Each criterion applies to Part A and to Part B.

1. **Data.** `.kgeo` on the same frame and 10 m grid, joined to its neighbours, with `_places.json` (islands with
   names and outlines, US-1 bridges with end points, marinas, reefs, lights), `_marks.txt`, `_mangrove.bin` (with
   `--built`) and `_canals.txt` where there are canal neighbourhoods (Key West, Stock Island, Big Pine, Cudjoe,
   Sugarloaf, Key Largo all have them).
   - Pass: each script's summary line in your status file; a preview PNG of the whole map (all phases) that you have
     looked at. Part A names Big Pine Key, the Torch Keys, Ramrod, Summerland, Cudjoe, Sugarloaf, Boca Chica, Stock
     Island and Key West, and its US-1 bridges include Niles Channel, Bow Channel, Kemp Channel and the Boca Chica
     Channel bridge. Part B names Key Largo and has the Jewfish Creek bridge, Largo Sound, and Molasses, French,
     Elbow and Carysfort reefs.
2. **No seam.** Along every edge shared with a phase already baked, heights differ by under 0.2 m at every cell
   (give the largest), and a top-down screenshot across each join shows no step in land or water. p1, p2 and p2_bay
   re-bake byte for byte (compare before and after).
3. **The game loads all of it.** `KeysGeoReal` serves every phase as one map, only on the real map, and tolerates a
   missing phase. State the extra memory and the extra load time, measured.
   - Pass: with your `KeysGeo` request applied locally, a scripted run teleports to three places in each part (Part
     A: off Big Pine, off Sugarloaf, Key West harbor; Part B: off Rock Harbor, Largo Sound, off Carysfort Reef) and
     takes a chase-view and a top-down shot at each: land, shoreline, water colour and the sounder look right, and
     `picks` returns a sensible zone.
4. **A boat can make the trip.** Hawk Channel on the ocean side is clear water from Key West to the top of Key
   Largo, and the marked channels are open.
   - Pass: a scripted run at speed along Hawk Channel across each new join, `picks` every 20 seconds, no stop.
5. **The chart covers it.** `bigmap` shows the whole map with the new islands named. The chart is a square texture
   today and the map becomes very long: say what that does to it, and deliver the fix as a request.
6. **Cost, measured and bounded.** Start-up time to the first frame and to the game running, and peak memory, before
   and after, two runs each on a quiet machine. If start-up passes 25 s or peak memory passes 10 GB, the lane is not
   done until you have written the request that brings it back (terrain, bridges, landmarks and houses built by
   tile near the boat, as the mangroves already are in `AKeysWorld::StreamMangroves`), applied it locally, and
   measured again.
7. **Requests written, not done.** Everything the new phases need from hub files, each as a numbered request with a
   patch: world bounds and chart, US-1 and its bridges, daybeacons, marinas (invent the names: the data carries real
   business names, which the project does not use), the reef line and its lights (Sand Key, American Shoal, Looe
   Key; Molasses, Carysfort), fish zones, teleport spots. Say which are needed for the criteria above and which are
   follow-ups.
8. **Nothing else moves.** `tpcheck` reports 0 bad; the dock audit (`dk:audit`) still reports zero overlaps; the
   Phase 1 and Phase 2 screenshot spots look the same (`tp=-1150,452,4.71` with `cam=0,6,1.5`; `goto=reef`;
   `tp=13232,-1280,0` in a Lower Matecumbe canal).
9. **The classic map is unharmed.** `-KRClassicMap` runs with no new log errors and loads no real-map data.
10. **No slowdown once running.** `perf-check` at `tp=-1150,452,4.71`, at the marina start and at
    `tp=13232,-1280,0`: within 5% of before. Report frame time off Key West and off Key Largo as well.

When Part A passes all ten, commit, write "PART A READY" at the top of your status file with its evidence, and go
straight on to Part B. Set READY FOR MERGE only when both parts pass, or when you are blocked on Part B and say why.

## Agents and skills

- Bake work as `geo-data`, loader work as `world-builder`; check each criterion as `verifier` before you tick it.
  If those agent types are not available in your session, follow their briefs yourself.
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first, and
  fetch data while it runs. Check the build log for `Result: Succeeded` before any scripted run: a failed build
  followed by a run hangs the editor on a rebuild prompt.
- Only one copy of the game runs at a time on this machine (`shoot.sh` takes a lock), and the marshal is running
  tests too: a scripted run may wait its turn. Never kill a game process you did not start.
- Outside requests carry nothing about the user or the project: User-Agent `map-data-fetch/1.0` and nothing else
  identifying. Read a source's licence before adding it; say in your status file what you added.
- Disk space is tight (about 16 GB free): do not make a packaged build, and delete scratch files you no longer need.
- Open and look at every screenshot you cite.

## Dependencies

None on another lane. `main` holds everything merged so far, including the canals of the Upper Keys.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never use a bare `git stash` (it is
shared across worktrees), never push anything, and use no real brands or logos. End every commit message with these
two lines:

Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_015oSiK1WwZ5JkZ11PikjADc

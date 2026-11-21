You are the `mangroves` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the Florida Keys. You know
nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-mangroves` (run every command from inside it)
- Branch: `lane/mangroves`
- Read first: `MARSHAL.md`, `.claude/agents/world-builder.md`, `.claude/agents/geo-data.md`,
  `.claude/agents/verifier.md`, and the skills in `.claude/skills/` (`build`, `scripted-run`, `perf-check`,
  `geo-bake`, `lane-status`, `marshal-request`).

## Goal

Finish the mangrove brief on the real map: creeks and tunnels a skiff can idle through, the missing wildlife, and a
better-looking canopy.

The brief, as the user gave it: red mangrove shorelines and islands with arching prop roots, dense canopy, darker
tannin-stained water in creeks; creeks and tunnels a skiff can idle through; juvenile schools, mangrove snapper and
snook in the shade under the roots; tarpon rolling at dawn and dusk, redfish tailing on shallow edges, bonefish and
permit on nearby flats; barracuda, lemon and blacktip sharks cruising the edges; herons, egrets, pelicans, an osprey
nest, manatees in calm creeks, crabs on the roots; fish follow the tide; good spots in Free Fish and charters (snook
and tarpon requests).

Already in the game: mangrove cover baked from map data (`Content/KeysRun/Geo/p1_mangrove.bin`), rooted trees and
canopy (`KeysMangrove.cpp`), tannin water, wading birds, crabs, osprey nests on channel marks, the fish mix by tide,
tarpon rolling more at dawn and dusk, tails up on the flood. Snook and mangrove snapper lead the catch in creeks.

## Owned paths

- `Source/KeysRun/KeysMangrove.cpp`
- new `Source/KeysRun/KeysMangroveLife.cpp`, `KeysMangroveLife.h`
- `Tools/geo/bake_mangrove.py`
- `Content/KeysRun/Geo/p1_mangrove.bin`
- `.marshal/status/mangroves.md`, `.marshal/requests/mangroves/`

Everything else is read-only for you.

## Deliverables and acceptance criteria

1. **Creeks and tunnels.** At least two creeks through mangrove that a skiff can idle along without grounding, at
   least one roofed by canopy from both banks.
   - The depth of the ground is `KeysGeo` (marshal-only) reading the baked grid. Deliver the creek lines as data or
     as a patch request (`marshal-request`), and build the banks, arching roots and canopy in your own files.
   - Pass: with your patch applied locally, a scripted run idles the boat the length of each creek (`throttle=`,
     `heading=`) with no "aground" message in the log, and screenshots from the chase view show roots on both sides
     and canopy overhead in the tunnel. Give the `tp=` for each creek mouth.
2. **Wildlife.** Pelicans perched in the mangroves, and manatees in calm creeks.
   - Visible sea creatures are drawn by `KeysLife` (marshal-only; it already has a manatee and a "Creek" zone table).
     Perched birds can be built in your own files the way the waders in `KeysMangrove.cpp` are.
   - Pass: a screenshot with perched pelicans on a mangrove edge; a creek screenshot with a manatee, or a request to
     the marshal naming the exact `KeysLife` change needed if it cannot be done from your files.
3. **Canopy.** The crowns read as rounded, dense mangrove, not faceted cones.
   - Pass: before and after screenshots from `tp=-1150,452,4.71` with `cam=0,6,1.5` and from the chase view.
4. **Fishing still right.** `picks` in a creek returns snook and mangrove snapper as the top two; paste the `KRPicks`
   line.
5. **Requests written, not done:** Free Fish spots in the mangroves and snook and tarpon charter requests live in hub
   files. Write them as requests with the spots' coordinates and names.
6. **No slowdown.** `perf-check` at `tp=-1150,452,4.71`: within 5% of the number before your changes. Keep the tile
   and draw-distance scheme (600 m tiles, roots 700 m, canopy 4.5 km).

## Agents and skills

- Scenery as `world-builder`, bake changes as `geo-data`; check each criterion as `verifier` before you tick it.
- Skills: `build`, `scripted-run`, `perf-check`, `geo-bake`, `lane-status`, `marshal-request`.
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first.

## Dependencies

None on other lanes. You merge second, after `water`. The `water` lane runs at the same time in another worktree and
shares the machine: only one game runs at once, so a scripted run may wait its turn.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never push anything, and use no real
brands or logos.

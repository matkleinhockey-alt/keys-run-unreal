You are the `rootlife` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the real Florida Keys. You
know nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-rootlife` (run every command from inside it)
- Branch: `lane/rootlife`
- Read first: `MARSHAL.md` (the last log entries), `.claude/agents/sea-life.md`, `.claude/agents/verifier.md`, and
  the skills in `.claude/skills/` (`build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`,
  `reef-creature`).

## Goal

Make the water under the mangrove roots worth looking at. The marshal took diver's-eye screenshots under the
roots in Sister Creek and told the user it was the weakest part of the game: roots as plain dark sticks, a little
seagrass, one small school in the distance. The user then asked for exactly this: "thicker schools right in the
roots, visible growth on the roots below the tide line, and snook and snapper holding where a diver or the camera
can actually see them." The user's standing rule is "i just want to keep it realistic" (a real Florida Keys red
mangrove edge is a crowded nursery). The criteria are the marshal's reading: say so in your status file and list
what you had to guess.

## What is already there

- `KeysMangrove.cpp` builds the red mangroves by 600 m tile on a worker thread: prop roots (drawn to 700 m),
  canopy; oysters, barnacles and sponges are a few flat-coloured lumps on the roots (about four a tree: more
  made creek tiles slow to arrive; the limit is a tile's frame cost when it lands, logged as `KRMangrove tile ...
  the frame after took N ms`, to stay under 50 ms).
- `KeysMangroveEco*.cpp` is the ecosystem: by tile near the boat, schools under the roots (silverside, pilchard,
  juvenile snapper, juvenile grunt) on about a third of trees, needlefish, mullet, barracuda, small sharks, a
  stingray, upside-down jellyfish, snook, jacks, tarpon, crabs; fish are instanced with the boid material and
  culled at 70-80 m; schools leave the roots on the ebb and return on the flood. Test commands `eco:stats`,
  `eco:fish`, `eco:find <kind>`, `eco:look <kind> [dist] [height] [side] [fov]` (a negative height puts the test
  camera under the surface, and in ankle-deep water under the seabed: that is why the marshal's close shots
  failed), `eco:look off`. `KindName()` lists the kinds.
- Water under most mangroves is 0.2-0.6 m deep; creeks (Sister Creek `tp=-272,426,0`, Whiskey Creek
  `tp=-292,462,0`) and channel edges are 1-2.5 m. `tide=1` is high water. The diver: `dive`, `dv:mask`, `sink=N`,
  `dv:hold=N`, `dv:look=yaw,pitch` (radians).
- The marshal's shots to beat: `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/shots/mshots2/KRScript_05.png`,
  `_06.png`, `_07.png` (diver in Sister Creek at high tide) and `shots/mshots/KRScript_07.png` to `_12.png` (the
  failed close shots). Open them first.

## Owned paths

- `Source/KeysRun/KeysMangrove.cpp`, `KeysMangroveLife.cpp`, `KeysMangroveLife.h`
- `Source/KeysRun/KeysMangroveEco*.cpp`, `KeysMangroveEco*.h`
- `.marshal/status/rootlife.md`, `.marshal/requests/rootlife/`

Everything else is read-only (changes are numbered patch requests: apply locally to test, never commit).
Another lane, `reeflinks`, is working in `KeysReefLife*` and `KeysReefDen*`: do not touch those.

## Deliverables and acceptance criteria

1. **Thick schools in the roots.** Where the water under or beside the roots is deep enough to hold fish (say
   0.4 m and more at the state of tide), the schools are dense and close: clouds of silversides and pilchards
   weaving through the prop roots, bands of juvenile grunts and snappers (gray, schoolmaster, lane: use what the
   game has) hanging in the shade between them, sergeant majors and small barracuda at the edge, as on a real
   mangrove edge. Thickest on creek banks and channel edges, thinner on shallow flats, none where it is dry.
   They part round a diver and close behind.
2. **Growth on the roots.** Below the high-tide line each root in reach of deeper water carries growth that
   reads from 3 m: oyster clusters, barnacle crust, orange and red fire sponge, purple and blue tunicates, green
   algae fuzz, an anemone here and there; banded by height (barnacles and oysters high, sponges and tunicates
   lower), more at the creek edge, bare above the tide line. Keep the tile cost in hand: build the detail only
   near the camera (instances or a near-only mesh), not into the far tile mesh.
3. **Snook and snapper where they can be seen.** Snook lying in the shade under the outer roots and in the
   creek's undercut bank, nose to the current, in ones to fives; gray (mangrove) snapper holding in loose groups
   just inside the root line and eyeing the diver; a tarpon or two rolling in the creek; placed in water a diver
   can swim in and facing so they read side-on from the open-water side.
4. **A camera that can show it.** `eco:look` never puts the camera below the seabed or inside a root: in shallow
   water it sits just under the surface and looks along; add what the marshal needs to take the shots (for
   example `eco:look roots` for the best root wall near the boat).
5. **Proved.** From the diver's mask in Sister Creek and at one creek bank in the Lower Keys and one on Key
   Largo, at `tide=1` and `clock=10:30`: a shot along the root wall with schools, growth and snook or snapper in
   one frame; close shots of the growth and of each fish kind; the same view in the marshal's `mshots2` shot
   and yours, side by side in one image. Each opened and looked at; say plainly whether it now looks like a
   nursery.
6. **No slowdown.** `eco:stats` before and after (fish, instances, triangles, cpu); `perf-check` on Medium at
   `tp=-1150,452,4.71`, in Sister Creek from the boat and diving there: game, draw and GPU within 3% of before
   (take "before" with a switch that leaves your additions out); no `KRMangrove tile` frame over 50 ms; say what
   you cut to stay inside it.
7. **Nothing else moves.** `tpcheck` 0 bad on both maps; `dk:audit` zero overlaps; the trees from the boat look
   the same (a pair of shots at `tp=-1150,452,4.71`); the classic map builds nothing of yours.

## Working conditions

- Check each criterion as `verifier` before you tick it (follow the agent briefs yourself if the agent types are
  not available).
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first.
  Check the build log for `Result: Succeeded` before any scripted run.
- One copy of the game runs at a time (`shoot.sh` takes a lock that expires after 20 minutes); other lanes test
  too, and the user may be playing a packaged copy of the game on this machine: a run may wait, and frame times
  may be disturbed (take them twice). Batch your shots. End every script with a `quit` step. Never kill a game
  process you did not start.
- The editor is capped at 30 fps (33.3 ms): read game, draw and GPU times. Never teleport inside a `perf=`
  sample (5 s).
- No packaged build. Buy and generate nothing. Delete only your own scratch files; never `rm` with a wildcard in
  a shared directory. Open and look at every screenshot you cite.
- Usage is limited and you can be cut off at any moment: keep runs few, commit early and often with the status
  file current.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never use a bare `git stash`, never
push anything, and send nothing about the user or the project to any outside service. End every commit message
with these two lines:

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_015oSiK1WwZ5JkZ11PikjADc

You are the `baitballs` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the real Florida Keys. You
know nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-baitballs` (run every command from inside it)
- Branch: `lane/baitballs`
- Read first: `MARSHAL.md` (the last log entries), `.claude/agents/sea-life.md`, `.claude/agents/verifier.md`, and
  the skills in `.claude/skills/` (`build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`,
  `reef-creature`).

## Goal

More bait balls in deep water, worked by big fish. The user asked: "can you do more baitballs but with bigger
fish in the deep water". The user's standing rule is "i just want to keep it realistic" (the real Florida Keys:
blue water off the reef line and the Gulf Stream, where ballyhoo, sardines and flying fish are pushed up by
blackfin and yellowfin tuna, skipjack, mahi, sailfish, wahoo, king mackerel, amberjack, sharks, dolphins and
birds). The criteria are the marshal's reading: say so in your status file and list what you had to guess.

## What is already there

- `KeysSchools.cpp/.h`: boid schools as instanced meshes. Bait (role `Bait`): silverside (320-520 fish) and
  herring (140-220) balls; `MaxSchools 72`, `MaxFish 5200`, `MaxBaitFish 3020`; blue water already deals "bait
  balls up in the top of the water column, with the runners and jacks that work them" (`ZoneMixes()`); there is
  a `Hunter` role (blue runner, jack crevalle) and "our own predators: cruising they unsettle bait, charging they
  scatter everything". Test commands through the script: `fish:stats`, `fish:bait`, `fish:look`.
- `KeysLife.cpp` (hub, read-only): the big fish seen and caught (tunas, mahi, sailfish, wahoo, kingfish,
  amberjack, sharks, dolphins), drawn by rewriting one procedural mesh every frame (expensive: do not add many
  there); bird piles ("hotspots") over feeding fish, dealt round the boat; the fishing tables. A bait ball the
  big fish work should agree with what bites there (`picks`).
- Zones: `goto=offshore` is the Gulf Stream; `goto=reef` the reef; depth from `KeysGeo`. The diver can see bait
  balls (`dive`, `dv:mask`, `dv:look=yaw,pitch`).
- Performance: the perf bookmark "bait_ball" (in `Tools/perf_bookmarks.sh`) dives at the reef with a silverside
  ball put ahead: keep it as cheap as it is.

## Owned paths

- `Source/KeysRun/KeysSchools.cpp`, `KeysSchools.h`
- new `Source/KeysRun/KeysBait*.cpp`, `KeysBait*.h`
- `.marshal/status/baitballs.md`, `.marshal/requests/baitballs/`

Everything else is read-only, including `KeysLife`, `KeysData`, `KeysReefLife*`, `KeysMangroveEco*`, audio
(changes are numbered patch requests: apply locally to test, never commit; keep them small). Two other lanes
are running: `rootlife` (mangrove files) and `reeflinks` (`KeysReefLife*`, `KeysReefDen*`): do not touch theirs.

## Deliverables and acceptance criteria

1. **More bait balls out deep.** Off the reef line, over the drop-off and in the Gulf Stream (water deeper than
   about 30 m), bait balls are a common sight: within 300 m of a boat running offshore there is usually one or
   more; fewer and smaller over the reef; none inshore of Hawk Channel's middle. Use realistic offshore bait
   (ballyhoo and Spanish sardine or round scad from the bodies the game has, recoloured; flying fish that break
   the surface and glide when pushed). The total fish alive near the boat stays inside `MaxFish`; say what you
   gave up or re-dealt to pay for it.
2. **Big fish working them.** Each deep ball has predators that fit the water: blackfin and skipjack tuna
   crashing it from below in packs, mahi in small groups, a sailfish or two herding it with the dorsal up, king
   mackerel and wahoo slashing through, now and then a shark or a pod of dolphins; the ball tightens, rises,
   balls up at the surface and showers; birds (terns, frigatebirds) gather over it. Predators are drawn the
   cheap way (instanced, as the schools are), not added to `KeysLife`'s per-frame mesh; if they must be the
   catchable fish, write the request.
3. **It is fishing.** A cast or a troll near a worked ball bites with the fish that are working it (a request to
   `KeysLife`/`KeysData`'s picks if needed), and bait balls show on the sounder and from the boat as nervous
   water and birds.
4. **Proved.** From the boat and from the diver's mask in the Gulf Stream and off the reef: a ball being worked
   (predators in frame), a shower at the surface with birds, a sailfish herding; counts of balls and predators
   within 300 m at five offshore spots and two reef spots in a log line; `picks` beside a worked ball.
5. **No slowdown.** `perf-check` on Medium at `goto=offshore` trolling and the "bait_ball" scene, against
   `main`: game, draw and GPU within 3%; fish alive and schools near the boat before and after.
6. **Nothing else moves.** `tpcheck` 0 bad on both maps; the reef and mangrove schools as before; the spear test
   lands a fish.

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

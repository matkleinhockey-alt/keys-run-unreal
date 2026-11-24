You are the `reeflinks` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the real Florida Keys. You
know nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-reeflinks` (run every command from inside it)
- Branch: `lane/reeflinks`
- Read first: `MARSHAL.md` (the last log entries), `.claude/agents/sea-life.md`, `.claude/agents/verifier.md`, and
  the skills in `.claude/skills/` (`build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`,
  `reef-creature`).

## Goal

Make the reef one living system: the animals react to each other, to the hour and to the diver, and the loose
ends left by the lanes that built them are tied up. The user asked for "behaviour links" after seeing this
list from the marshal: "cleaning stations, fish guarding territory, angelfish nipping sponges, parrotfish sleeping
in the new holes at night." The user's standing rule is "i just want to keep it realistic" (the real Florida Keys
reef). The criteria are the marshal's reading: say so in your status file and list what you had to guess.

## What is already there

- `KeysReefLife.h/.cpp`, `KeysReefLifeBody.cpp`: the reef-life framework (registry, chunks near the camera,
  instanced, roles Grazer, Pair, Territory, Hoverer, Sitter, Cruiser; hooks `Build`, `Mesh`, `Pose`, `Limb`,
  `Where`, breathing, burying, digging; cap 900 units). Species files: `KeysReefLifeParrot.cpp` (stoplight),
  `KeysReefLifeTang.cpp`, `KeysReefLifeFish*.cpp` (rainbow and midnight parrotfish, queen, French and gray
  angelfish, sergeant major and its guard), `KeysReefLifeBottom*.cpp` (queen conch, giant anemone),
  `KeysReefLifeVisitors.cpp` (southern stingray, green turtle, loggerhead). Test commands `reef:stats`,
  `reef:find`, `reef:look`, `reef:do`, `reef:census`.
- `KeysReefDen*.cpp/.h`: five kinds of den in the reef rock with residents (green and spotted moray, Caribbean
  reef octopus, spiny lobster, stone crab, nurse shark, squirrelfish, sweepers, clinging crab, shrimp, a
  goliath); console commands `kr.den stats|census|find|goto|calm|solid`.
- `KeysSchools.cpp` (hub): the boid schools, including groupers at coral heads; `KeysLife.cpp` (hub): the big
  animals drawn the old way. `KeysReefAudio` plays sounds when told through `KeysReefAudio::Heard(what, who, pos)`.
- What the earlier lanes left undone (read their status files with `git show lane/<name>:.marshal/status/<name>.md`
  for reeffish, reefvisitors, reefbottom, reefdens): angelfish nip rock, not sponges; the sergeant major's
  purple egg patch never showed and its guard was never seen to chase; hoverers are not tied to coral heads;
  the ray does not look half buried (the drawn seabed sits lower than `KeysSea::SeafloorHeightAt`); a turtle is
  not drawn in the last metre under the surface in the mask view, and none rest under ledges; parrotfish sleep
  in the open; the grouper-and-moray hunt was never seen; cave and plate dens show a pale lit floor from above;
  the nurse shark in a den is thin and stiff; the green moray hangs from the ledge roof and lies stick-straight
  at night; the octopus shows its whole head by day; the `reef-creature` skill's section 7 is the literal text
  `MISTAKES_PLACEHOLDER`.

## Owned paths

- `Source/KeysRun/KeysReefLife*.cpp`, `KeysReefLife*.h` (the framework and every species file: no other lane is
  in them now)
- `Source/KeysRun/KeysReefDen*.cpp`, `KeysReefDen*.h`
- `.marshal/status/reeflinks.md`, `.marshal/requests/reeflinks/`

Everything else is read-only, including `KeysReef.cpp`, `KeysSchools`, `KeysLife`, `KeysDiver`, `.claude/`
(changes are numbered patch requests: apply locally to test, never commit). Another lane, `rootlife`, is working
in the mangrove files: do not touch those.

## Deliverables and acceptance criteria

Each link is driven by state, not a timer alone, logs a line that states cause and effect, and is shown in a
mask-view shot. Do them in this order; stop adding when time is short and say what is left.

1. **Sleeping in the holes.** By the feeding clock's night, parrotfish go to a den or a crevice at the foot of
   a coral head and sleep there in the cocoon; turtles rest under ledges by day now and then; squirrelfish
   leave their ledge at night and return by day.
2. **Territory.** The sergeant major's guard and the damselfish-role holders chase off any reef-life fish that
   comes inside their ground and return; the egg patch shows (a small opaque mesh on the rock). Hoverers hold
   station over a coral head.
3. **Cleaning stations.** At a few coral heads and at dens with a cleaner shrimp: a parrotfish, grouper or
   angelfish comes in, holds still at an odd angle with its fins spread for some seconds while small cleaners
   (juvenile blue tang or a cleaner shrimp: use what exists; add neon goby or juvenile bluehead only if cheap)
   work over it, then leaves; a queue of one or two waits.
4. **Angelfish nip sponges**: they feed at the reef's sponges (find them through `KeysReef`'s public coral
   list; ask for a request if sponges cannot be told from corals).
5. **Hunters together.** Now and then a grouper waits at a den while a moray goes through the rock, and small
   fish near that den bolt; jacks or a trumpetfish-like follower shadow a stingray that is digging (use
   existing fish).
6. **The faults listed above**, as many as can be fixed in your files: the pale den floor, the moray's poses,
   the octopus by day, the den's nurse shark, the turtle near the surface, the ray's half-buried look (or the
   request that would give you the drawn seabed height).
7. **The recipe.** Replace the `reef-creature` skill's placeholder section with the real mistakes made across
   the reef lanes, as a request (the corrected file).
8. **No slowdown.** No new animals beyond the few cleaners; `reef:stats` and `kr.den stats` before and after;
   `perf-check` on Medium at `goto=reef` from the boat and diving at 6 m: within 3% of before.
9. **Nothing else moves.** `tpcheck` 0 bad on both maps; `dk:audit` zero overlaps; the spear test
   (`dv:aimfish`, `dv:spear`, `dv:haul=1`, straight after diving) lands a fish; day and night runs differ as
   the rules say (`clock=12:00`, `clock=22:00`).

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

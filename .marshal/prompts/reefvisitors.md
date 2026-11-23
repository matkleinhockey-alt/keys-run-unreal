You are the `reefvisitors` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the real Florida Keys. You
know nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-reefvisitors` (run every command from inside it)
- Branch: `lane/reefvisitors`
- Read first: `MARSHAL.md` (the last log entries), `.claude/agents/sea-life.md`, `.claude/agents/verifier.md`,
  the skills in `.claude/skills/` (`reef-creature` above all, then `build`, `scripted-run`, `perf-check`,
  `lane-status`, `marshal-request`), `Source/KeysRun/KeysReefLife.h`, `KeysReefLifeParrot.cpp` and
  `KeysReefLifeTang.cpp` (the working pattern you copy).

## Goal

The user is turning the game's reefs into a real reef ecosystem and gave a list of what must be there: "Setting:
the Florida Keys reef tract. Use only species found there." No clownfish. Your share of that list is below.
Real species, correct common names, sizes relative to each other, colours and behaviour; the user's standing
rule is "i just want to keep it realistic". The split into criteria is the marshal's reading: say so in your
status file and list anything you had to guess.

## What is already there

- Coordinates are metres: x along the chain, y up, z south. `goto=reef` is Sombrero Reef; `-KRClassicMap` runs
  the older invented map, which has reefs too.
- `AKeysReefLife` (merged today, early and not fully verified: expect to find faults, and report them as
  requests rather than editing it): a registry where one file registers a species; placement by chunk near the
  camera from a fixed seed; instanced drawing with the boid material; roles; a cap of 900 units within 60 m
  shared by every lane; test commands `reef:stats`, `reef:find <key> [phase]`, `reef:look <key> [phase]`,
  `reef:census`, `reef:look off`. Species so far: `stoplightparrot`, `bluetang`.
- The older systems (all marshal-only): `KeysLife.cpp` (77 visible kinds drawn by rewriting one mesh every
  frame: add nothing to it), `KeysSchools.cpp` (boid schools, caps 72 schools and 5,200 fish), `KeysData.cpp`
  (catchable fish), `KeysReef.cpp` (corals and sponges), `KeysMangrove*.cpp` (mangroves and their animals).
- The diver: `own=dive.scuba.set` then `dive`, `dv:mask`, `dv:chase`, `sink=N`, `dv:hold=N`, `dv:look=yaw,pitch`,
  `dv:aimfish`, `dv:spear`, `dv:haul=1`, `jkey` (back aboard). Do not spear before a `reef:look` shot: the catch
  card covers the screen. `clock=HH:MM` sets the hour; there is no night lighting.
- Bodies are built in C++ (`KeysReefLifeBody.cpp` helpers, `FKeysMesh`). Buy and generate nothing. Ten generated
  models are on order for December (green and spotted moray, octopus, spiny lobster, queen and French angelfish,
  green and hawksbill turtle, queen conch, queen triggerfish): build those animals in code now so a model can
  replace the body later without touching behaviour.

## Owned paths

- new `Source/KeysRun/KeysReefLifeVisitors*.cpp`
- `.marshal/status/reefvisitors.md`, `.marshal/requests/reefvisitors/`

Everything else is read-only, including the framework. Changes to `KeysLife`, `KeysSchools` or `KeysData` are
patches for the marshal. Other lanes: `reeffish`, `reefbottom`, `reefdens`.

## Deliverables and acceptance criteria

1. **Southern stingray** (*Hypanus americanus*, disc to 1.5 m across in females, half that in males: plain
   grey-brown diamond, white below, long whip tail): on sand patches, grooves between spurs and the sand at
   the reef's foot; lies half buried with eyes and spiracles showing, lifts off in a puff of sand when the
   diver comes within about 2 m, glides away low and settles again; digs for food leaving a pit and a sand
   cloud. Today `KeysLife` has one generic "stingray": decide whether yours replaces it on reefs (a request)
   or lives beside it, and say why.
2. **Green turtle** (*Chelonia mydas*, shell 0.8-1.1 m: smooth brown-olive shell with radiating streaks,
   small blunt head, pale underside) grazing turtle grass beside the reef and resting under ledges, and
   **loggerhead** (*Caretta caretta*, shell 0.8-1.0 m: reddish-brown shell, very large blocky head, yellowish
   skin) over hard bottom and reef, crunching conch and crabs. Both swim with the front flippers (a slow
   wing beat, hind flippers steering), come up to breathe every few minutes and go back down, and are
   unbothered by a slow diver. Today `KeysLife` has one generic "turtle": write the request that retires it
   from the reef and channel tables once yours are in. Both are protected: not spear targets.
3. **The audit.** These from the user's list are already in the game through the older systems: yellowtail
   snapper, hogfish, goliath grouper, great barracuda, tarpon, nurse shark, lionfish. For each, check in the
   running game that it is on the reefs, named correctly, sized correctly against the others (give lengths),
   coloured and behaving as in life (lionfish hovering near ledges as an invasive that may be speared; nurse
   shark resting on the bottom by day; barracuda hanging still; goliath at wrecks and big ledges, release
   only; tarpon over the reef edge and channels; hogfish rooting in sand; yellowtail in schools above the
   reef), with a shot and a `picks` or log line each. Where one is wrong, write the request with a patch; do
   not rebuild them in the new framework unless that is the smaller fix.
4. **Proved**: a close `reef:look` shot of each new animal; a strip of stills of a turtle surfacing and of a
   ray lifting off; `reef:find` lines at Sombrero Reef, one other named reef and the classic map.
5. **Budget**: at most 40 units of the 900; no growth in `KeysLife`'s per-frame mesh; `perf-check` on Medium
   at `goto=reef` from the boat and during a dive at 6 m against `main`: within 3%.
6. **Nothing else moves**: `tpcheck` 0 bad on both maps; the spear test lands a fish and never a turtle.
7. **Report on the pattern**: what in the framework or the recipe was wrong, missing or hard, as requests.

## Agents and skills

- Work as `sea-life`; check each criterion as `verifier` before you tick it. If the agent types are not
  available in your session, follow their briefs yourself.
- The first build in this worktree compiles everything (15 minutes or more, longer while other lanes build):
  start it in the background first. Check the build log for `Result: Succeeded` before any scripted run.
- One copy of the game runs at a time on this machine (`shoot.sh` takes a lock that expires after 20 minutes),
  and other lanes and the marshal test too: a run may wait a long time. Batch your shots. End every script with
  a `quit` step. Never kill a game process you did not start.
- The editor is capped at 30 fps (33.3 ms): read game, draw and GPU times. Never teleport inside a `perf=`
  sample (5 s). Frame times taken while another lane compiles are worthless: take them again.
- No packaged build. Delete only your own scratch files; never `rm` with a wildcard in a shared directory.
- Open and look at every screenshot you cite. Usage is limited: keep runs few and commit early and often, with
  the status file current, so a stop loses nothing.

## Dependencies

None. `main` holds the reef-life framework. Other reef lanes run beside you in their own files.

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

Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_015oSiK1WwZ5JkZ11PikjADc

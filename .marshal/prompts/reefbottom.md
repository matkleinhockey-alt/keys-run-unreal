You are the `reefbottom` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the real Florida Keys. You
know nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-reefbottom` (run every command from inside it)
- Branch: `lane/reefbottom`
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

- new `Source/KeysRun/KeysReefLifeBottom*.cpp`
- `.marshal/status/reefbottom.md`, `.marshal/requests/reefbottom/`

Everything else is read-only, including the framework, `KeysReef.cpp` (another lane, `reefdens`, is editing
it), `KeysSea*`, `KeysSeafloor*` and the mangrove files. Changes there are patches for the marshal. Other
lanes: `reeffish`, `reefvisitors`, `reefdens`.

## Deliverables and acceptance criteria

1. **Queen conch** (*Aliger gigas*, shell 20-30 cm: heavy spired shell, sandy outside, glossy pink flared
   lip): on sand and turtle grass beside the reef and in Hawk Channel's grass, 1-15 m, in loose scatters of
   3-12, mostly still, now and then a slow lurching hop; eye stalks out, drawn in when the diver is close.
   Protected in Florida: cannot be taken (a toast the first time, as the parrotfish has). Build it in the
   reef-life framework as a bottom animal.
2. **Giant Caribbean anemone** (*Condylactis gigantea*, 15-30 cm across: thick tentacles white to pale
   green or pink with swollen purple or pink tips, column tucked into a crevice): singly in crevices and at
   the base of coral heads, 1-15 m, tentacles swaying with the surge and drawing in a little when touched or
   crowded. No fish live in it (no clownfish here). In the framework if it can sway there; otherwise say what
   is needed.
3. **Purple sea fans.** The reef's sea fans are in `KeysReef.cpp` (a code-built `SeaFan` and a modelled
   `seafan`; `Tools/reeftint.py` names *Gorgonia ventalina*). Check in the game that common sea fans read
   as purple to lavender (some yellow-tan is real), stand across the surge, 0.6-1.5 m tall, on the reef top
   and fore reef at 2-15 m; deliver any colour or placement fix as a request with a patch.
4. **The audit of the reef itself.** For each of these from the user's list, check in the running game that
   it is there, named, placed and coloured as in life, with a shot each and a fix as a request where it is
   not: elkhorn coral (reef crest, 0-5 m, broad flattened branches, tan), staghorn (3-15 m, thin branching
   thickets), brain coral (domed, grooved), barrel sponges (fore reef and deep reef, to 1.5 m, red-brown),
   turtle grass beds (off the reef proper: back reef and Hawk Channel, flat strap blades), and mangrove roots
   near shore (the red mangrove prop roots with their fish, from the boat and the diver's view).
5. **Proved**: close `reef:look` shots of the conch (still and mid-hop) and the anemone; `reef:find` lines at
   Sombrero Reef, one other named reef and the classic map; counts by depth band and zone.
6. **Budget**: at most 60 units of the 900 (conch and anemones are cheap: say what a unit costs for a still
   animal); `perf-check` on Medium at `goto=reef` from the boat and during a dive at 6 m against `main`: within
   3%.
7. **Nothing else moves**: `tpcheck` 0 bad on both maps; the same corals in the same places as before (a pair
   of shots); nothing of yours in canals or on dry flats.
8. **Report on the pattern**: what in the framework or the recipe was wrong, missing or hard, as requests.

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

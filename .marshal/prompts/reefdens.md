You are the `reefdens` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the real Florida Keys. You
know nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-reefdens` (run every command from inside it)
- Branch: `lane/reefdens`
- Read first: `MARSHAL.md`, `.claude/agents/sea-life.md`, `.claude/agents/world-builder.md`,
  `.claude/agents/verifier.md`, and the skills in `.claude/skills/` (`build`, `scripted-run`, `perf-check`,
  `lane-status`, `marshal-request`).

## Goal

The reefs have fish, corals and sponges but nowhere for an animal to hide, and so none of the animals that live
in holes: no moray, octopus, lobster or squirrelfish, no nurse shark asleep under a ledge. Give the reefs hiding
places and the animals that live in them.

The user was told the reef has "no hiding places" and said: "create hidden holes then, even if i cant explore
them". The user's standing rule is "i just want to keep it realistic" (the real Florida Keys reef tract). Everything
else below is the marshal's reading. Say so in your status file and list anything you had to guess.

A den is a look and a home, not a cave to swim into: a dark opening, an undercut ledge, a crevice between coral
heads, a hole in the rock, a gap in a wreck's plating, with the animal's front end showing and room behind it
implied by darkness. The diver cannot go in and does not need to.

## What is already there

- Game coordinates are metres: x along the chain, y up, z south. `-KRClassicMap` runs the older invented map;
  reefs exist on both. `goto=reef` goes to Sombrero Reef; `tpcheck` lists destinations; named reefs on the real
  map include Looe Key, Molasses and Carysfort.
- Reef dressing is `KeysReef.cpp/.h`: deterministic (`ReefSeed`), 50 m chunks drawn within 110 m, kinds chosen
  by a suitability rule from depth and zone; code-built kinds (elkhorn, staghorn, brain, star, sea fan, sea
  plume, barrel, tube, encrusting, seagrass; `GeoFor()`), and sixteen modelled kinds loaded from
  `Content/KeysRun/Props/reef_<key>.krfish`. Solids use `M_KR_Reef` / `M_KR_VertexColor`; swaying cards
  `M_KR_ReefCard`. Instanced, three LODs.
- Wrecks are `KeysWrecks.cpp` (hub). The seafloor is `KeysSeafloor.*` / `KeysSea.*` (hub): a height mesh with
  vertex colours; it has no overhangs and cannot have holes cut in it.
- Fish seen from the boat and by the diver are `KeysLife` (hub; one procedural mesh rewritten every frame);
  schools are `KeysSchools` (hub; boids as instanced meshes with the `M_KR_Boid` material, roles Bait, Reef,
  Hole, Hanger, Flats, Hunter, Patrol; groupers already sit at the foot of the tallest coral head in a cell).
  A nurse shark, a generic stingray and groupers exist there.
- The mangrove ecosystem (`KeysMangroveEco*.cpp`) is the newest example of animals made by tile near the boat,
  with built shapes (`BuildShape`), instanced pools with cull distances, and state-driven behaviour with log
  lines. Read it as the pattern; do not edit it (another lane, `birdview`, is working in it).
- The diver is `AKeysDiver` (`KeysDiver.*`, hub): script commands `dive`, `sink=N`, `dv:chase`, `dv:mask`,
  `dv:depth=N`, `dv:hold=N`, `dv:look=yaw,pitch`, `dv:nearfish`; there is a dive light (T key; it comes on by
  itself below 14 m or at low sun). `own=dive.scuba.set` before `dive` gives a long dive in a scripted run.
  The game has day and sunset lighting only; "night" is the feeding clock's hour (`clock=HH:MM`).
- No animal in the game is skeletal; everything is built in C++ (`FKeysMesh`, `KeysMesh.h`, metres, vertex
  colours). Generated 3D models exist for some fish but cost credits: buy and generate nothing. Build your
  animals in code; say in the status file which would gain most from a model later.

## Owned paths

- `Source/KeysRun/KeysReef.cpp`, `KeysReef.h`
- new `Source/KeysRun/KeysReefDen*.cpp`, `KeysReefDen*.h`
- `.marshal/status/reefdens.md`, `.marshal/requests/reefdens/`

Everything else is read-only for you, including `KeysLife`, `KeysSchools`, `KeysData`, `KeysWrecks`, `KeysDiver`,
`KeysSea*`, `KeysSeafloor*`, `KeysMangroveEco*`, the game mode, materials (`Content/Python/init_unreal.py`) and
config. What you need from them (spawning your actor, a material parameter, a hook in the diver, dens in wrecks)
goes to the marshal as numbered patch requests (`marshal-request` skill): apply them in your worktree to test,
never commit them. Two other lanes are running (`birdview` in the mangrove ecosystem files, `canalways` in the
canal data): do not touch their files.

## Deliverables and acceptance criteria

1. **Dens that read as holes.** At least five kinds, each convincing from 1 m and from 10 m in the dive-mask
   and chase views, with a dark interior that never shows the seafloor, the sky or the back of a polygon from any
   angle a diver can reach: an undercut ledge along the base of a spur or a big coral head; a crevice between
   two heads; a round hole in reef rock; a low cave mouth under a ledge on the deep reef; an overhang of
   plate-like coral with shadow beneath. Built from rock and coral that match the reef around them, placed by a
   deterministic rule from depth, zone and the reef structure (denser on the fore reef, spur-and-groove and deep
   reef, sparse on patch reefs, none on sand, seagrass or in canals), on every reef of both maps. The diver and
   the spear cannot pass into them; fish do not swim through their rock.
   - Pass: mask-view and chase-view shots of each kind at Sombrero Reef, at one other named reef on the real map
     and on the classic map; dens per 50 m chunk by zone in a log line; a shot with the dive light on a den.
2. **Residents, real ones.** Each den may hold an animal, shown part out of the hole, sized and coloured as in
   life, built in code:
   - green moray (to 1.8 m, deep crevices and wrecks), spotted moray (to 0.9 m, small holes): head and a
     little body out, mouth opening and closing as it breathes, drawing back when the diver comes close or the
     light is on it, coming out again after;
   - Caribbean spiny lobster (body 25-45 cm): several to a ledge, long antennae out and waving, backing in when
     approached;
   - Caribbean reef octopus: a den with a midden of shells in front, an arm or an eye showing by day, pale
     blue-green; by the feeding clock's night it sits out at the mouth;
   - squirrelfish and blackbar soldierfish (20 cm, red, big-eyed): small groups hovering just inside a ledge's
     shadow by day;
   - glassy sweepers: a tight school inside a cave mouth;
   - nurse shark (2 m+): asleep on the sand under a ledge, tail out; and a goliath or black grouper filling a
     big hole now and then;
   - channel clinging crab or stone crab in a hole, banded coral shrimp at a hole's mouth (close up only).
   Most dens are empty or show only eyes: a diver should find something every few minutes, not at every hole.
   State the rule and the odds; same animals in the same dens every run.
   - Pass: a close mask-view shot of each resident, each opened and looked at; a before/after pair of a moray
     and a lobster withdrawing; counts near the diver and per reef in a log line.
3. **Day and night.** By the feeding clock's night the lobsters walk out on the sand near their ledge, the
   octopus and the morays are out and hunting within a few metres of home, the squirrelfish leave the ledge and
   the day fish that shelter (grunts, if a request lets you tell the schools) move in close. State what could
   not be done without night lighting.
   - Pass: a day and a night shot of the same ledge, and the log lines.
4. **They are part of the reef.** At least three links that can be watched, each driven by state: schools shy
   away from a moray's or an octopus's hole; a grouper and a moray hunt together now and then (they do in
   life); small fish gather at a hole with a cleaner shrimp and hold still (a simple cleaning station); a
   lobster flicks back when a triggerfish or the diver comes near. Hub parts as requests.
   - Pass: a log line stating cause and effect and a shot for each.
5. **Something for the diver.** A logbook or toast line the first time each resident kind is found ("Green
   moray - Sombrero Reef, 9 m"), as a request to the hub if it needs one. Spiny lobster may not be speared in
   life (they are caught by hand or net, in season): do not make them spearable; write up catching them by hand
   as a follow-up request, not built.
6. **Wrecks, as a request.** Dens and residents in the gaps of the wrecks (green morays, goliath, sweepers),
   delivered as a request with a patch built from a function in your own files.
7. **Realistic.** Only species of the Florida Keys reef tract, with correct common names, relative sizes,
   colours and habits; list each with a one-line source of your knowledge in the status file, and anything you
   were unsure of. No clownfish, giant clams or table corals.
8. **No slowdown.** Everything by chunk near the diver or boat and taken down behind; residents animate only
   within 25 m of the camera and draw only within 40 m (shrimp and crabs 8 m). `perf-check` on Medium at
   `goto=reef` from the boat and during a dive at 6 m, against `main`: game, draw and GPU within 3%. Triangles,
   instances and animals near the diver in a log line. No new frame over 50 ms when a reef chunk arrives.
9. **Nothing else moves.** The same corals in the same places as before (a pair of shots at `goto=reef`, same
   camera); `tpcheck` 0 bad on both maps; `dk:audit` zero overlaps; no dens in canals, on flats or in the
   mangroves; the spear test (`dv:aimfish`, `dv:spear`, `dv:haul=1`) still lands a fish.

## Agents and skills

- Do the animals as `sea-life`, the dens and placement as `world-builder`; check each criterion as `verifier`
  before you tick it. If the agent types are not available in your session, follow their briefs yourself.
- The first build in this worktree compiles everything (15 minutes or more, longer while other lanes build):
  start it in the background first. Check the build log for `Result: Succeeded` before any scripted run: a failed
  build followed by a run hangs the editor on a rebuild prompt and holds the game lock.
- Only one copy of the game runs at a time on this machine (`shoot.sh` takes a lock that expires after 20
  minutes), and two other lanes and the marshal run tests too: a scripted run may wait a long time. Batch your
  shots. End every script with a `quit` step, or the game runs for ever. Never kill a game process you did not
  start.
- The editor build is capped at 30 fps (33.3 ms); read game, draw and GPU times, not the frame time alone. Never
  teleport inside a `perf=` sample (5 s). Frame times taken while another lane is compiling are worthless.
- Do not make a packaged build. Delete scratch files you no longer need, and only your own: never `rm` with a
  wildcard in a shared directory.
- Open and look at every screenshot you cite. No real brands or logos.

## Dependencies

None. `main` holds everything merged so far, including scuba diving and the dive light.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never use a bare `git stash`, never
push anything, buy nothing, and send nothing about the user or the project to any outside service. Write your
status file and commit early and often: a usage limit can stop you at any moment, and what is committed is what
survives. End every commit message with these two lines:

Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_015oSiK1WwZ5JkZ11PikjADc

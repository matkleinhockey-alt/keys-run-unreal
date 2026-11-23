You are the `reefparrot` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the real Florida Keys. You
know nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-reefparrot` (run every command from inside it)
- Branch: `lane/reefparrot`
- Read first: `MARSHAL.md`, `.claude/agents/sea-life.md`, `.claude/agents/verifier.md`, and the skills in
  `.claude/skills/` (`build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`).

## Goal

The user approved a roster of about a hundred real Florida Keys reef species (fish, hiders, invertebrates,
visitors, corals) to turn the reefs into a real ecosystem, and asked that one creature be built end to end first,
the parrotfish, merged, and only then the other lanes opened "so they copy a working pattern". You are that
template lane. You deliver two things: the stoplight parrotfish, finished in every respect (definition,
placement, behaviour, look, animation, sound, tests), and the pattern itself: a small reef-life framework and a
written recipe that six later lanes will follow to add their own species in their own files without touching
yours or each other's.

The user's rules: real species with correct common names, sizes relative to each other, colours and behaviour;
"i just want to keep it realistic"; performance matters. The design below is the marshal's reading. Say so in
your status file and list anything you had to guess.

## What is already there

- Coordinates are metres: x along the chain, y up, z south. `goto=reef` is Sombrero Reef; the real map has named
  reefs (Looe Key, Molasses, Carysfort...); `-KRClassicMap` runs the older invented map, which has reefs too.
- `KeysSchools.cpp` (hub): boid schools as `UInstancedStaticMeshComponent` with the `M_KR_Boid` material (tail
  beat and body bend on the GPU from 8 per-instance floats), 26 m cells within 140 m from a fixed seed, roles
  Bait/Reef/Hole/Hanger/Flats/Hunter/Patrol, stepped every frame within 35 m, every 2nd within 80 m, every 4th
  beyond, three LODs, caps `MaxSchools 72`, `MaxFish 5200`. Meshes come from `KeysLife`'s `BuildSchoolMesh(key)`.
- `KeysLife.cpp` (hub): the 77 visible kinds (`FKeysVis`, `FKeysShape`), drawn by rewriting one procedural mesh
  every frame (expensive: do not add to it). It has a generic "parrotfish" and "angelfish" with no species.
- `KeysReef.cpp/.h`: corals and sponges by 50 m chunk within 110 m, fixed seed, suitability from depth and
  zone. Another lane, `reefdens`, is editing it now (dens and their residents, in `KeysReefDen*`): read
  `KeysReef.h` for where coral heads are, but do not edit it.
- `KeysMangroveEco*.cpp`: the newest example of animals made by chunk near the camera with instanced pools,
  cull distances, state-driven behaviour and cause-and-effect log lines. Read it as a pattern; do not edit it.
- The diver: script commands `dive`, `sink=N`, `dv:chase`, `dv:mask`, `dv:hold=N`, `dv:look=yaw,pitch`,
  `dv:aimfish`, `dv:spear`; `own=dive.scuba.set` before `dive` gives a long dive. Underwater sound goes through
  `AKeysAudio` (`Burst` is a public one-shot synth; everything under water is low-passed). `clock=HH:MM` sets the
  feeding clock; there is no night lighting.
- No animal is skeletal; bodies are built in C++ with `FKeysMesh` (`KeysMesh.h`, metres, vertex colours). Buy
  and generate nothing.

## Owned paths

- new `Source/KeysRun/KeysReefLife.h`, `KeysReefLife.cpp` (the framework)
- new `Source/KeysRun/KeysReefLifeParrot.cpp` (the species file) and any other new `KeysReefLife*` file
- `.marshal/status/reefparrot.md`, `.marshal/requests/reefparrot/`

Everything else is read-only, including `KeysSchools`, `KeysLife`, `KeysData`, `KeysReef`, `KeysDiver`,
`KeysAudio`, the game mode, `init_unreal.py`, `.claude/` and `MARSHAL.md`. What you need from them goes to the
marshal as numbered patch requests (`marshal-request` skill): apply them in your worktree to test, never commit
them. Three other lanes are running (`birdview`, `canalways`, `reefdens`): do not touch their files.

## Deliverables and acceptance criteria

1. **The framework (`KeysReefLife`).** One actor that owns reef animals near the camera, with:
   - a registry: a species file registers a definition (key, common name, scientific name, length range,
     colours and phases, body builder, role, group size, depth band, reef zones, day/night habit, shyness,
     budget weight) with one static registration call, so adding a species never edits the framework or
     another species' file;
   - placement by chunk near the diver or boat from a fixed seed (same animals in the same places each run),
     from reef habitat read through `KeysGeo`/`KeysReef` public functions (depth, zone, coral heads), released
     behind;
   - drawing as instanced meshes with the existing boid material, three LODs, per-species cull distance;
   - a small set of behaviour roles written once for all species: roving grazer group, pair, territory holder,
     hoverer above a head, bottom sitter, cruiser; each a function of state (hour, diver distance, neighbours),
     not a timer alone; fish steer round coral and never swim through rock or above the surface;
   - a budget: a hard cap on individuals alive (you propose it; the whole reef-life system must stay under
     900 individuals within 60 m once all lanes are in, and the game's total of schooling fish near the diver
     must not go up: write the request that lowers `KeysSchools`' reef mix to pay for what you add);
   - test commands (`reef:stats`, `reef:find <key>`, `reef:look <key>`, `reef:census`) as a request.
   - Pass: `reef:stats` log line (species, individuals, instances, triangles, cpu ms a frame); the header
     comment of `KeysReefLife.h` explains the registry in under 60 lines.
2. **Stoplight parrotfish, defined.** *Sparisoma viride*, both phases: the initial phase (20-30 cm, scaled
   grey-brown and white above, bright red belly and fins) and the terminal male (35-45 cm, emerald green,
   yellow spot at the tail base, yellow crescent on the tail, pink-edged gill cover). Blunt head, fused beak,
   large scales. Sized correctly against the game's other fish (state three comparisons).
   - Pass: mask-view shots of each phase within 2 m, side on, each opened and looked at, beside a reference
     description; triangle counts per LOD.
3. **Placed where it lives.** Reef top, crest, spur tops and patch reefs, 1-15 m, densest 3-8 m, none on sand
   flats, seagrass (adults), deep wall, canals or mangroves; on every reef of both maps; groups of one
   terminal male with three to six initial-phase fish, and some lone or paired initial-phase fish.
   - Pass: counts by depth band and zone in a log line at Sombrero, one other named reef and the classic map;
     top-down shot showing spacing.
4. **Behaves like one.** Swims with its pectoral fins (a rowing beat, tail held still except in a burst);
   roves in its group grazing: tips beak-down onto coral rock, bites, moves on; leaves a pale bite scar that
   fades; now and then lets go a puff of white sand; the male chases a rival male off; keeps about 2 m from a
   slow diver, more from a fast one, bursts away from a spear; by the feeding clock's night it is tucked
   against the reef, still, in a clear mucus cocoon, and out again at dawn.
   - Pass: shots of a bite, a sand puff, a chase and a sleeping fish; log lines stating cause and effect for
     flee, chase and sleep; a 20 s screen recording is not possible, so give a strip of six stills 0.5 s apart.
5. **Sounds like one.** The crunch of the beak on rock is heard under water at close range: synthesise it
   through the public audio one-shot; say exactly what is played. Nothing bought.
   - Pass: the log line each time it plays, with distance; a note that nobody has listened to it.
6. **Replaces the stand-in.** A request that retires the generic "parrotfish" from `KeysLife`'s reef table so
   there are not two, and adds a logbook or toast line the first time the diver sees one. It is not catchable
   and not a spearing target (taking parrotfish is prohibited in Florida): the spear test must not pick it.
7. **No slowdown.** `perf-check` on Medium at `goto=reef` from the boat and during a dive at 6 m, against
   `main`: game, draw and GPU within 3%; state individuals alive, the framework's cpu ms a frame, and the
   schooling-fish total before and after. No frame over 50 ms when a chunk arrives.
8. **Nothing else moves.** `tpcheck` 0 bad on both maps; `dk:audit` zero overlaps; the spear test lands a
   fish; nothing of yours away from reefs.
9. **The recipe.** A draft skill, written to `.marshal/requests/reefparrot/reef-creature-SKILL.md`, that a new
   lane follows to add a species: the registration call with every field explained, how to build a body and
   colour phases, how to pick or extend a role, how to place it, the test commands, the shots and log lines
   that prove it, the budget rule, and the mistakes you made. Under 150 lines. Prove it: add a second, simple
   species by following only the recipe (blue tang, *Acanthurus coeruleus*, 15-25 cm, schools of 10-40 grazing
   the reef top, yellow when small) in a second file, and say how long it took and what the recipe lacked.

## Agents and skills

- Work as `sea-life`; check each criterion as `verifier` before you tick it. If the agent types are not
  available in your session, follow their briefs yourself.
- The first build in this worktree compiles everything (15 minutes or more, longer while other lanes build):
  start it in the background first. Check the build log for `Result: Succeeded` before any scripted run.
- One copy of the game runs at a time on this machine (`shoot.sh` takes a lock that expires after 20 minutes),
  and three other lanes and the marshal test too: a run may wait a long time. Batch your shots. End every script
  with a `quit` step. Never kill a game process you did not start.
- The editor is capped at 30 fps (33.3 ms): read game, draw and GPU times. Never teleport inside a `perf=`
  sample (5 s). Frame times taken while another lane compiles are worthless: take them again.
- No packaged build. Delete only your own scratch files; never `rm` with a wildcard in a shared directory.
- Open and look at every screenshot you cite.

## Dependencies

None. `reefdens` will later give sleeping fish real holes; do not wait for it.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never use a bare `git stash`, never
push anything, and send nothing about the user or the project to any outside service. Commit your status file
early and often: a usage limit can stop you at any moment. End every commit message with these two lines:

Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_015oSiK1WwZ5JkZ11PikjADc

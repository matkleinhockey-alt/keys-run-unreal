You are the `mangrovelife` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the Florida Keys. You know
nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-mangrovelife` (run every command from inside it)
- Branch: `lane/mangrovelife`
- Read first: `MARSHAL.md` (the log at the end says what the map and the mangroves are today),
  `.claude/agents/sea-life.md`, `.claude/agents/world-builder.md`, `.claude/agents/verifier.md`, and the skills in
  `.claude/skills/` (`build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`).
- What the lane before you did: `git show lane/mangroves:.marshal/status/mangroves.md`.

## Goal

Make the mangroves a living ecosystem: animals that move, feed, hide and react, tied to the tide, the time of day
and each other, on every mangrove shore of the map, so idling a skiff along the roots is worth doing for its own
sake.

The user said only: "Make a new lane for the mangrove's ecosystem". Everything below is the marshal's reading of
that. Say so in your status file and list anything you had to guess.

## What is already there

- The map is real geography, 65.6 km long (x -30060..35500, z -13720..6200), generated in C++ at run time; there are
  no placed assets. Game coordinates are metres: x along the chain (east is +x), y up, z south (ocean side).
  `KeysGeo::ToUE(X, Y, Z)` converts to Unreal units. `-KRClassicMap` runs the older invented map.
- Mangrove cover is baked per map phase (`Content/KeysRun/Geo/*_mangrove.bin`, read through `KeysGeo::OnMangrove`,
  `SurfaceKind`). `KeysMangrove.cpp` builds the trees in 600 m tiles: prop roots (drawn to 700 m), canopy (4.5 km).
  `AKeysWorld::StreamMangroves` (hub) grows the tiles within 5 km of the boat one at a time off the game thread and
  takes them down past 6.2 km; roots are only set up within 1.15 km. Anything you add to a tile's build runs on a
  worker thread: no UObject work there.
- Wildlife today, all of it still: wading birds (great egret, great blue heron, little blue) standing off the roots
  and specks for crabs on the roots (`KeysMangrove.cpp`, baked into the tile's root mesh); perched pelicans and
  snags (`KeysMangroveLife`); osprey nests on channel marks. Moving: a manatee or two at three haunts in the
  Middle Keys creeks, asked for from `KeysLife`. The named creeks, the haunts and the roosts exist only in the
  Middle Keys (Phase 1); the other four fifths of the map has trees, still waders and nothing else.
- Fish the player can see and catch are `KeysLife` (hub; zone tables in `KeysData.cpp`, a "Creek" zone, the mix by
  tide, tarpon rolling at dawn and dusk, tails up on the flood). Bait schools are `KeysSchools` (hub). Seabirds over
  open water are in `KeysLife`. Read how each draws many animals cheaply before you write your own.
- Tide: `KeysOcean::SeaLevel()` (0.32 m either side). Time of day and night: see how `KeysDocks.cpp` and
  `KeysLife.cpp` read it. Residential canals (`KeysGeo::CanalDist`) are seawalls, not mangrove: nothing of yours
  there.

## Owned paths

- `Source/KeysRun/KeysMangroveLife.cpp`, `KeysMangroveLife.h`
- `Source/KeysRun/KeysMangrove.cpp`
- new `Source/KeysRun/KeysMangroveEco*.cpp`, `KeysMangroveEco*.h`
- `.marshal/status/mangrovelife.md`, `.marshal/requests/mangrovelife/`

Everything else is read-only for you, including `KeysLife`, `KeysSchools`, `KeysData`, `KeysWorld`, `KeysGeo`,
`KeysBoatPawn`, audio and config. What you need from them goes to the marshal as numbered patch requests
(`marshal-request` skill): apply them in your worktree to test, never commit them. Prefer doing it in your own
files; a request should be small and exact.

## Deliverables and acceptance criteria

1. **Birds that live there.** The waders stalk, stop, strike and now and then lift off and land further along; they
   flush when the boat comes close and fast, and stay if she idles. Add white ibis in small parties, a roseate
   spoonbill here and there, cormorants and anhingas on low limbs with wings held out to dry, a belted kingfisher or
   green heron on a root, a night heron after dark. A few rookery keys (small mangrove islands, chosen by rule from
   the map, at least one in each of the Lower, Middle and Upper Keys and Key Largo) hold many birds, with flights
   out at dawn and back at dusk. An osprey now and then hovers and dives.
   - Pass: chase-view shots of each kind; a pair of shots before and after a flush; a dawn or dusk shot of a flight
     at a rookery; the count of each kind map-wide and near the boat in a log line.
2. **Life under the roots.** In the shade of the prop roots: schools of small fish (silversides, pilchards,
   juvenile snappers and grunts) that hold among the roots and scatter and re-form when the boat or a predator
   comes; needlefish at the surface; mullet that jump; a small barracuda lying still; juvenile lemon and nurse
   sharks and a stingray on the shallow edge; upside-down jellyfish on the bottom of calm ponds. On the roots
   below the tide line: oysters, barnacles and sponges, showing more at low tide.
   - Pass: shots from the chase view and with the camera under the surface at two shores; a low-tide and a
     high-tide pair (`tide=-1`, `tide=1`) of the same roots.
3. **Crabs and the mud.** Mangrove tree crabs that climb and drop when the boat nears, fiddler crabs on mud exposed
   at low tide that vanish as you approach, horseshoe crabs on a shallow edge. Close-up detail only.
   - Pass: a close shot of each, and the draw distance of each in your status file.
4. **It is one system.** At least four links the player can watch, each driven by state and not a timer alone:
   predators (snook, jacks, tarpon, a small shark) rush the bait under the roots and it showers at the surface;
   birds gather and dive where bait is showering; waders fish where the small fish are thick and the water is
   shallow; on the ebb the small fish leave the roots for the creek mouths and the predators and birds follow, on
   the flood they go back in; at night the day birds roost and the night heron hunts.
   - Pass: for each link a log line that states the cause and the effect (for example "bait shower at x, z: 3 terns,
     1 pelican diverted"), and a screenshot of it. A scripted run at `tide=-1` and at `tide=1` logs where the small
     fish are, and the two differ as the rule says.
5. **Everywhere, by rule.** All of it appears on mangrove shores across the whole map, placed from the map data by
   a rule with a fixed seed (same animals in the same places each run), not from a list of Middle Keys coordinates.
   Manatee haunts and pelican roosts get the same treatment: found by rule in every part of the map.
   - Pass: shots off Big Pine or Sugarloaf (Lower Keys), in a Middle Keys creek, in Florida Bay off the Upper Keys
     and in Key Largo's sounds; per-region counts in a log line.
6. **Fishing agrees with what you see.** Where the player can see snook under the roots, `picks` favours snook
   there; where bait is thick the bite is better. This lives in hub files: deliver it as a request with the exact
   change, and say what you could do without it.
7. **Sound.** Audio is a hub matter: write a request listing the calls and splashes that would sell it (the sources
   must be invented or already in the project; there is a paid sound library, and spending needs the user's OK
   through the marshal, so list what you would want and its use, and buy nothing).
8. **The classic map is unharmed.** With `-KRClassicMap` nothing new of yours is built and the log has no new
   errors.
9. **No slowdown, measured.** Everything is made by tile near the boat and taken down behind her; nothing ticks
   more than 600 m from the camera; small animals draw only up close. `perf-check` on Medium at
   `tp=-1150,452,4.71`, at the busiest rookery and in a Middle Keys creek: within 3% of before (take "before" with
   a switch that leaves your work out, as `-KRNoDocks` does). No frame over 50 ms when a tile arrives (the log
   line `KRMangrove tile ... the frame after took N ms` is the measure). Report triangles, mesh count and animals
   alive near the boat. Start-up time to a running game does not grow by more than 0.5 s.
10. **Nothing else moves.** `tpcheck` reports 0 bad; `dk:audit` reports zero overlaps; the mangrove screenshot spot
    (`tp=-1150,452,4.71` with `cam=0,6,1.5`) shows the same trees.

## Agents and skills

- Do the animals as `sea-life`, the placement and tiles as `world-builder`; check each criterion as `verifier`
  before you tick it. If those agent types are not available in your session, follow their briefs yourself.
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first.
  Check the build log for `Result: Succeeded` before any scripted run: a failed build followed by a run hangs the
  editor on a rebuild prompt.
- Only one copy of the game runs at a time on this machine (`shoot.sh` takes a lock) and the marshal runs tests too:
  a scripted run may wait its turn. Never kill a game process you did not start.
- The editor build is capped at 30 fps (33.3 ms); read game, draw and GPU times, not the frame time alone.
- Disk space is tight: do not make a packaged build, and delete scratch files you no longer need.
- Open and look at every screenshot you cite. Screenshots can land a few seconds late; leave a gap before the next
  teleport.
- Species are real animals; that is fine. No real brands, products or logos anywhere.

## Dependencies

None on another lane. `main` holds everything merged so far, including map Phase 3 (the Lower Keys and Key Largo)
and scenery made by tile near the boat.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never use a bare `git stash` (it is
shared across worktrees), never push anything, and nothing sent to an outside service may carry anything about the
user or the project. End every commit message with these two lines:

Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_015oSiK1WwZ5JkZ11PikjADc

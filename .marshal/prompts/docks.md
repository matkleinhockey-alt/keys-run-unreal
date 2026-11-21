You are the `docks` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the Florida Keys. You know nothing
about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-docks` (run every command from inside it)
- Branch: `lane/docks`
- Read first: `MARSHAL.md`, `.claude/agents/world-builder.md`, `.claude/agents/verifier.md`, and the skills in
  `.claude/skills/` (`build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`).

## Goal

Fill out the waterfront of the real map's islands with private docks and boat lifts, so the shores look lived on: the
user's "lifts and docks" item, which "fills out the new islands".

The user never wrote this brief out. What follows is the marshal's reading of it; say so in your status file, and
list anything you had to guess.

- **Residential docks** along built-up shores and canals: a walkway out from the seawall or the yard on piles, with a
  T or an L head, cleats, a ladder, a dock box, a light on a post, here and there a fish-cleaning table, a bench or a
  kayak rack. Varied in length, shape and timber colour; some new, some weathered.
- **Boat lifts** on most of them: four-pile cradle lifts with top beams, motors and cables, bunks under the hull, the
  boat hanging clear of the water (some lowered and afloat, some lifts empty); a few elevator lifts on a seawall and
  a few davits with a skiff. Lifts that are down sit at the tide.
- **Boats** on the lifts and alongside: the invented fleet the yards already use (`KeysYardProps::MakeBoat`,
  `BuildBoat`), so the hulls match the dry stack and the boatyard. No new hull shapes.
- **Life:** a pelican or a gull on some piles; underwater lights under a few docks at night; one or two docks with a
  person fishing or hosing a boat down. Keep this small.

## What is already there (read, do not edit)

- The world is generated in C++ at start-up; there are no placed assets. Game coordinates are metres: x along the
  island chain, y up, z south. `KeysGeo::ToUE(X, Y, Z)` converts to Unreal units.
- Where land, water and houses are: `KeysGeo::LandH`, `DepthAt`, `ShoreInfo` (distance to shore and its direction),
  `SurfaceKind`, `OnMangrove`, `InMarkedChannel`, `Marinas()`, `CreekDist`; `KeysTown::InFootprint`,
  `KeysTown::NearEstate`; `KeysYards::InFootprint`. The conch houses are placed in `AKeysWorld::BuildScenery`
  (`KeysWorld.cpp`) on a jittered grid: read how, so your docks stand in front of houses and not in front of nothing.
- Docks that exist today, which yours must not collide with or duplicate: the marinas (`KeysLandmarks.cpp`
  `BuildMarinas`), public piers (`KeysTownData::Piers`), waterfront estates with a dock and a lift
  (`KeysTownData::WaterfrontEstates`, built in `KeysLandmarks.cpp` `BuildEstates`), player estate lots
  (`KeysEstates.cpp`, `KeysGeoReal::Plat`), and Cowfish Cove (`KeysYards.cpp`).
- Patterns to copy: `KeysMangrove.cpp` for scenery in 600 m tiles with draw distances; `KeysYards.cpp` for an actor
  that is spawned once, builds tiled meshes, and only ticks near the player; `KeysYardProps.h` for boats, piles and
  small props; `KeysMangroveLife.h` for a perched pelican.
- The sea surface rises and falls 0.32 m with the tide (`KeysOcean::SeaLevel()`, already inside `KeysGeo::WaveH`).
- Another lane (`upperkeys`) is extending the map east. Ask the world for its extent through `KeysGeo` (world bounds,
  `ShoreInfo`, `DepthAt`), never through `KeysGeoReal` directly and never with Phase 1's numbers written in, so your
  docks appear on the new islands once that lane merges.

## Owned paths

- new `Source/KeysRun/KeysDocks.cpp`, `KeysDocks.h`
- new `Source/KeysRun/KeysLifts.cpp`, `KeysLifts.h`
- `.marshal/status/docks.md`, `.marshal/requests/docks/`

Everything else is read-only for you. Your code only runs once a hub file spawns it (see how `AKeysYards` is spawned
in `KeysGameMode.cpp`): write that, and anything else you need from a hub file, as patch requests (`marshal-request`
skill), apply them in your worktree to test, and never commit them.

## Deliverables and acceptance criteria

1. **Placement.** Docks stand only where they make sense: on a built-up shore in front of a house, the water at the
   head at least 1 m deep at low tide, not across a marked channel, a creek, a marina, a pier, an estate, a plat lot,
   Cowfish Cove or mangrove, and not touching the next dock. The same seed gives the same docks every run.
   - Pass: the count per island in your status file (log line), top-down shots (`cam=0,25,3`) of three different
     built-up shores, and a check you write that logs zero overlaps with each of the things listed.
2. **Docks.** At least four dock shapes and three timber finishes, with the fittings from the brief.
   - Pass: chase-view shots by day of three stretches of shore, and one after `sunset` showing dock lights.
3. **Lifts.** Cradle lifts on most docks, a few elevator lifts and davits; boats hung, afloat, and some lifts empty.
   A hung boat clears the water at high tide (`tide=1`) and a floating one sits at the surface at both tides.
   - Pass: close shots of each lift type, and a pair at `tide=-1` and `tide=1` of the same dock.
4. **The fleet matches.** Every boat comes from `KeysYardProps`; names and makers are invented; no real brands.
5. **The player is not trapped.** A scripted run idles along one built-up shore 25 m off and down one canal if the map
   has one wide enough, with no stop. If the player can drive through a dock, say so (collision is a hub matter).
6. **The classic map is unharmed.** With `-KRClassicMap` nothing of yours is built and the log has no new errors.
7. **No slowdown.** Everything is in distance-culled tiles; small fittings draw only up close; nothing ticks more than
   600 m from the player. `perf-check` on Medium at the busiest dock shore and at `tp=-1150,452,4.71`: within 3% of
   before (take "before" with a switch that leaves your docks out, as the yards did with `-KRNoYards`). Report
   triangles and mesh count.

## Agents and skills

- Do the work as `world-builder`; check each criterion as `verifier` before you tick it. If those agent types are not
  available in your session, follow their briefs yourself.
- Skills: `build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`.
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first.
- New `.cpp` and `.h` files in `Source/KeysRun` are picked up by the build without a project file change.
- Open and look at every screenshot you cite. Screenshots taken with `shot` can land a few seconds late; leave a gap
  before the next teleport.

## Dependencies

None on other lanes. `main` already contains the merged `water`, `mangroves` and `yards` lanes. The `upperkeys` lane
runs at the same time in another worktree and shares the machine: only one game runs at once, so a scripted run may
wait its turn. Do not make a packaged build.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never push anything, and use no real
brands or logos. End every commit message with these two lines:

Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_015oSiK1WwZ5JkZ11PikjADc

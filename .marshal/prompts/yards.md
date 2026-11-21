You are the `yards` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the Florida Keys. You know nothing
about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-yards` (run every command from inside it)
- Branch: `lane/yards`
- Read first: `MARSHAL.md`, `.claude/agents/world-builder.md`, `.claude/agents/verifier.md`, and the skills in
  `.claude/skills/` (`build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`).

## Goal

Add a working dry stack marina and a boatyard to the real map, so the waterfront looks like people keep and fix boats
there.

The brief, as the user gave it:

- **Dry stack:** a large open-sided metal rack building, 3 to 4 levels, boats racked on bunks (center consoles, bay
  boats, flats skiffs, various sizes and colours, invented brands), random empty slots. A giant marina forklift driven
  by NPCs, animated: pull a boat off the rack, carry it, lower it into the water at a launch well, and the reverse. A
  fuel dock and a small dockmaster office.
- **Boatyard:** a gravel yard, boats on jack stands and keel blocks (some with bottom paint sanded off, some
  shrink-wrapped, one sportfisher with its outboards removed), a travel lift straddling a slip with a boat hanging in
  its slings, NPC workers sanding, painting and pressure washing. Props: stacked jack stands, paint buckets, a hull
  with a FOR SALE sign, a trailer, old outboards on stands.
- No new hull shapes: use the hulls the game already builds. Use streaming and distance culling so these areas do not
  cost frame rate.

## What is already there (read, do not edit)

- The world is generated in C++ at start-up; there are no placed assets. Game coordinates are metres: x along the
  island chain, y up, z south. `KeysGeo::ToUE(X, Y, Z)` converts to Unreal units.
- `KeysMesh.h`: `FKeysMesh` and the hull builders (`BuildHullFor`, `BuildHullLoft`, `FHullDef`). `KeysTraffic.cpp`
  shows how other boats are built from them.
- `KeysHuman.h`: the posable figure every NPC uses. `KeysLocals.cpp` shows figures posed and animated each frame.
- `KeysMangrove.cpp`: the pattern for scenery in 600 m tiles with draw distances. `KeysLandmarks.cpp`
  (`BuildMarinas`): the existing marinas. `KeysGeo::Marinas()`, `NearMarina`, `DepthAt` and `KeysGeoReal::Shore` tell
  you where land, water and the marinas are.
- `AKeysWorld::BeginPlay` (`KeysWorld.cpp`) is where each scenery system is built and where per-frame updates are
  called from.

## Owned paths

- new `Source/KeysRun/KeysYards.cpp`, `KeysYards.h`
- new `Source/KeysRun/KeysForklift.cpp`, `KeysForklift.h`
- new `Source/KeysRun/KeysYardProps.cpp`, `KeysYardProps.h`
- `.marshal/status/yards.md`, `.marshal/requests/yards/`

Everything else is read-only for you. Your code can only run once `KeysWorld` calls it, and the launch well and the
travel-lift slip may need the ground or the water depth changed in `KeysGeo`: both are marshal-only. Write those
changes as patch requests (`marshal-request` skill), apply them in your worktree to test, and never commit them.

## Deliverables and acceptance criteria

1. **A site.** One dry stack and one boatyard on the real map, on dry land beside water the player can reach by boat,
   near an existing marina, clear of houses, roads and mangroves.
   - Pass: the `tp=` for each in your status file, a top-down shot (`cam=0,25,3`) showing nothing overlapping, and a
     scripted run that idles the player's boat past both with no "aground" line in the log.
2. **Dry stack.** Open-sided steel rack building with 3 or 4 levels of bunks; at least 30 slots, about a quarter
   empty; at least three hull types in mixed sizes and colours with invented names; a fuel dock with pumps; a small
   dockmaster office.
   - Pass: chase-view and top-down screenshots by day, and one at night (`clock=21:00`).
3. **Forklift.** A marina forklift with an NPC at the wheel runs a full cycle without teleporting: drive to the rack,
   raise the forks, pull a boat, carry it to the launch well, lower it into the water; later the reverse. The boat it
   launched floats at the well, and the slot it left is empty until it returns.
   - Pass: six screenshots across one cycle from a fixed camera, with the times in your script.
4. **Boatyard.** Gravel ground; at least eight boats on jack stands and keel blocks, including some with the bottom
   paint sanded off, some shrink-wrapped, and one sportfisher with its outboards removed; a travel lift straddling a
   slip with a boat hanging in its slings; at least three workers, one sanding, one painting, one pressure washing,
   each animated; the props from the brief (stacked jack stands, paint buckets, a hull with a FOR SALE sign, a
   trailer, old outboards on stands).
   - Pass: screenshots from the chase view and from on foot height that show each item; list which shot shows what.
5. **No new hulls, no real brands.** Every boat comes from the existing hull builders. Names, signs and logos are
   invented.
6. **The classic map is unharmed.** With `-KRClassicMap` nothing of yours is built, or it sits correctly at a classic
   marina; either way a run there shows nothing floating or buried and no new log errors.
7. **No slowdown.** `perf-check` on Medium at the yard, and at `tp=-1150,452,4.71` (far from it): the far spot within
   noise of before, the yard spot at or under the frame time of the nearest existing marina. Everything culls by
   distance; the forklift and workers do not tick when the player is more than 600 m away.

## Agents and skills

- Do the work as `world-builder`; check each criterion as `verifier` before you tick it.
- Skills: `build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`.
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first.
- New `.cpp` and `.h` files in `Source/KeysRun` are picked up by the build without a project file change.

## Dependencies

None on other lanes. `main` already contains the merged `water` lane: the sea surface now rises and falls up to
0.32 m with the tide (`KeysOcean::SeaLevel()`, already inside `KeysGeo::WaveH`), so float launched boats on `WaveH`
and build docks and the launch well to work across that range. You merge third, after `mangroves`. The `mangroves`
lane runs at the same time in another worktree and shares the machine: only one game runs at once, so a scripted run
may wait its turn. Disk space is tight (about 15 GB free): do not make a packaged build.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never push anything, and use no real
brands or logos.

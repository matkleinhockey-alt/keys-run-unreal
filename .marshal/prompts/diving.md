You are the `diving` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the Florida Keys. You know
nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-diving` (run every command from inside it)
- Branch: `lane/diving`
- Read first: `MARSHAL.md`, `.claude/agents/sea-life.md`, `.claude/agents/world-builder.md`,
  `.claude/agents/verifier.md`, and the skills in `.claude/skills/` (`build`, `scripted-run`, `perf-check`,
  `lane-status`, `marshal-request`).

## Goal

Add scuba diving to the game: a tank and the gear that goes with it, so the player can stay down and look around
the reefs and wrecks instead of only holding a breath. And while you are in there, make diving as a whole better.

The user said only: "can you work on the diving aspect and add a scuba tank and gear". Everything below is the
marshal's reading of that. Say so in your status file and list anything you had to guess.

## What is already there

- Diving today is free-diving with a speargun. `KeysDive.h/.cpp` hold the rules (breath-hold air of 180 units
  burned by depth and exertion, buoyancy, narcosis, blackout, fin kicks, the band gun and the fight on the float
  line). `KeysDiver.h/.cpp` (`AKeysDiver`) own going over the side and climbing back aboard, the fixed-step sim,
  the dive-mask and chase cameras, the diver's model (wetsuit, head, mask, back plate, arms, fins), the gun, breath
  bubbles, marine snow, and the underwater post-process. Test hooks: `AKeysDiver::DebugCmd` (`depth=N`,
  `look=yaw,pitch`, `aimfish`, `spear`, `reload`, `air=frac`, `swim=0/1`, `haul=0/1`); find how a script goes over
  the side and comes back in `KeysBoatPawn.cpp`.
- Gear is bought at the dive shop in town, "Scuba Steve's" (`KeysTownData.cpp`, items with effects such as
  `dive.wetsuit`, `dive.fins`, `dive.mask`, `dive.tank`, and `dive.fill` counted fills). The only tank today is a
  slung pony bottle that multiplies the breath hold by 2.2 for one dive per fill. The boat sets the diver's numbers
  as she goes in (`KeysBoatPawn.cpp` near "sim/dive-gear.ts": `AirMul`, `SwimMul`, `ViewBonusM`); `KeysTags.cpp`
  spends the fill; `KeysHUD.cpp` draws the air bar, the tunnel vision and the ascent warning (`AirFrac`,
  `TunnelAmount`, `AscentWarning`); `KeysHUD2.cpp` shows the loadout; `KeysAudio` (`SetUnderwater`) does the
  muffling, bubble pops, breathing and heartbeat. All of those files are hub files.
- The world is generated in C++; game coordinates are metres: x along the chain, y up, z south. The real map has
  named reefs (Sombrero, Looe Key, Molasses, Carysfort and others, `goto=reef`, `tpcheck` lists destinations),
  wrecks, and an "Abyssal Garden" deep site. `-KRClassicMap` runs the older invented map; diving works on both.
- People are drawn with `FKeysHuman` (`KeysHuman.h`); meshes are built with `FKeysMesh` (`KeysMesh.h`), vertices in
  metres. Boats come from `KeysMesh`/`BuildBoat` (hub).

## Owned paths

- `Source/KeysRun/KeysDive.cpp`, `KeysDive.h`
- `Source/KeysRun/KeysDiver.cpp`, `KeysDiver.h`
- new `Source/KeysRun/KeysScuba*.cpp`, `KeysScuba*.h`
- `.marshal/status/diving.md`, `.marshal/requests/diving/`

Everything else is read-only for you, including `KeysBoatPawn`, `KeysTownData`, `KeysTags`, `KeysHUD*`,
`KeysAudio`, `KeysData`, the save, the boats and config. What you need from them goes to the marshal as numbered
patch requests (`marshal-request` skill): apply them in your worktree to test, never commit them. Design so the
hub changes are small: put the rules, the gear's meshes, the gauges' numbers and the text in your own files and
have the hub call them. Two other lanes are running (`water2` in `KeysSea*`, `KeysSeafloor*`, `KeysOcean*`,
`KeysSchools*`; `mangrovelife` in the mangrove files): do not touch their files.

## Deliverables and acceptance criteria

1. **The scuba set, worn.** A full set on the diver's model, each piece visible in the chase view: an aluminium
   cylinder on the back with its valve and a boot, a buoyancy jacket with straps and a waist band, a first stage
   on the valve with hoses to the regulator in the mouth, a spare (octopus) regulator, a gauge console and the
   jacket's inflator, a weight belt, a dive computer on the wrist, a mask and full-foot or strap fins. Without the
   set the diver looks as she does today. In the dive-mask view the gauge console or the wrist computer can be
   glanced at.
   - Pass: chase-view shots from the side, the back and three-quarters, with and without the set; one dive-mask
     shot showing the gauge.
2. **Scuba rules.** With the set on, the diver breathes from the cylinder: pressure in psi (or bar, one unit
   throughout) falling with depth (surface rate times absolute pressure) and with exertion, a normal cylinder
   giving roughly 45 to 60 minutes of game play near the surface and about a third of that at 30 m, scaled so a
   dive is a real stretch of play and not a chore (state your numbers and why). No breath-hold blackout while there
   is gas. The jacket holds her neutral at any depth, so she can hover (no sinking or floating up when the keys are
   let go); she swims slower than a freediver and cannot sprint for long. Low gas warns at a reserve; out of gas
   turns into today's breath-hold rules from that moment, so the ascent is still on her. A too-fast ascent warns
   and has a cost; a safety stop near 5 m is asked for after a deep dive and rewarded, never forced. Narcosis stays.
   Free-diving without the set plays exactly as it does today.
   - Pass: unit numbers in a log line from a scripted dive: gas used over two minutes at 5 m and at 25 m, hover
     drift over 20 s with no input (under 0.3 m), time to empty stated; a scripted free-dive whose air trace matches
     `main`'s (same script on both builds).
3. **Bubbles and sound.** On scuba, each breath out sends a burst of bubbles up from the regulator that grow and
   wobble as they rise and break the surface where the boat's crew could see them; no bubbles on the breath in.
   The sound of the regulator (the breath in, the bubbling breath out) is a hub matter: deliver it as a request
   that says exactly what to synthesise or which existing sounds to use; buy nothing.
   - Pass: a chase-view shot mid-exhale and a shot from the boat's side of the water showing the bubbles at the
     surface.
4. **Getting it and keeping it filled.** The set is bought at Scuba Steve's as pieces or a package (cylinder,
   jacket, regulator set, computer), with invented names and no real brands, priced against what is there. A
   cylinder is used up across dives and is filled at the shop for a small price; a second cylinder can be carried
   on the boat and swapped at the surface. The pony bottle keeps working as it does for free-diving, and on scuba
   it becomes the bail-out. The player chooses scuba or breath-hold before going over. All of this is hub work:
   deliver it as requests with patches (shop rows, the loadout as she goes in, the save, the HUD's gauge in place
   of the air bar, the loadout page).
   - Pass: with the requests applied, a scripted run that grants the gear (find or ask for a test command; never
     use `estbuy`, and do not write the user's save: use a test flag that keeps it in memory), dives, uses gas,
     comes back with the cylinder part used, and dives again from that pressure.
5. **On the boat.** While the set is owned, a cylinder or two stand in a rack or lie strapped in the cockpit, and
   the set is gone from the rack while the diver is down. A red-and-white diver-down flag flies while she is in
   the water. Boats are hub files: a request with a patch, built from a function in your own files.
   - Pass: shots of the cockpit with the rack before and during a dive, and the flag.
6. **Something to dive for.** With the long bottom time, the reefs and wrecks have to be worth it. At least three
   of these, in your own files or as requests: fish that let a slow scuba diver come closer than a freediver;
   a light for looking under ledges and into a wreck; lobster to find under ledges in season (catching them is a
   request to the hub); a logbook line for each dive (site, depth, time, gas used); a named dive site marker on
   the chart for each reef and wreck with a mooring ball to tie to.
   - Pass: a shot or log line for each one you did, and the request for each one you handed on.
7. **Free-diving and spearfishing unharmed.** The gun works on scuba and off; everything a free-dive did before
   it still does.
   - Pass: the scripted spear test (`aimfish`, `spear`, `haul=1`) lands a fish on both.
8. **Both maps.** Works on the real map and with `-KRClassicMap`, with no new log errors.
9. **No slowdown.** `perf-check` on Medium during a dive at `goto=reef` with the set on, against the same dive
   on `main`: game, draw and GPU times within 3%. The gear adds under 6,000 triangles to the diver; bubbles reuse
   the pool that is there or stay under 400 at once.
10. **Nothing else moves.** `tpcheck` reports 0 bad; the boat looks the same when no scuba gear is owned.

## Agents and skills

- Check each criterion as `verifier` before you tick it. If the agent types are not available in your session,
  follow their briefs yourself.
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first.
  Check the build log for `Result: Succeeded` before any scripted run: a failed build followed by a run hangs the
  editor on a rebuild prompt.
- Only one copy of the game runs at a time on this machine (`shoot.sh` takes a lock that expires after 20
  minutes), and two other lanes and the marshal run tests too: a scripted run may wait a long time for its turn.
  Keep runs short and batch your shots. Never kill a game process you did not start.
- The editor build is capped at 30 fps (33.3 ms); read game, draw and GPU times, not the frame time alone.
- Do not make a packaged build. Delete scratch files you no longer need.
- Open and look at every screenshot you cite.
- No real brands, makers or logos on any gear: invent the names.

## Dependencies

None. `main` holds everything merged so far.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never use a bare `git stash` (it is
shared across worktrees), never push anything, and send nothing about the user or the project to any outside
service. Write your status file and commit your requests early and often: a usage limit can stop you at any
moment, and what is committed is what survives. End every commit message with these two lines:

Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_015oSiK1WwZ5JkZ11PikjADc

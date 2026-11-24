# reeflinks

Status: IN PROGRESS
Updated: 2026-11-24 16:45
Branch: lane/reeflinks at a235d49

The user's words: "behaviour links" (after the marshal's list "cleaning stations, fish guarding territory, angelfish
nipping sponges, parrotfish sleeping in the new holes at night") and the standing rule "i just want to keep it
realistic". The nine criteria are the marshal's reading; what this lane had to guess is under "Guesses".

Evidence: `L` = `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-reeflinks/`.

## Acceptance criteria
- [ ] 1 Sleeping in the holes - built; evidence run pending
- [ ] 2 Territory - built; `L/log_day1.log`, unasked: "chase: sergeantmajorguard guard #157 (group 55) went for
  queenangel adult #155 (group 53) because it came within 2.3 m of his eggs on the rock ... effect: he charges at
  1.9 m/s, the intruder turns and runs", "chase over: ... saw the intruder off; effect: pursued it 6.0 m in 4.0 s,
  it is now 2.8 m away, he turns back to his eggs"; `reef:roles`: "hoverer: 1 groups, 40 animals ... mean height
  over the reef 1.90 m, furthest from home 1.3 m". Shots pending (egg patch not yet seen in a shot)
- [ ] 3 Cleaning stations - built; `L/log_day1.log`, all unasked but one `reef:clean`: 8 stations alive at
  Sombrero; "clean: stoplightparrot initial #246 ... makes for the cleaning station ... because it has gone 123 s
  since it was last cleaned", "queue: stoplightparrot terminal #243 ... but stoplightparrot initial #246 ... is being
  cleaned; effect: it waits its turn off the head, 1 in the queue", "clean: ... holds still head-down at 35
  degrees, rolled on its side, fins spread, for 11 s ... effect: the 2 neon gobies of the station come off the head
  and pick it over", "clean over: ... and stoplightparrot terminal #243 (group 63), next in the queue, comes in";
  24 cleanings and 24 waits in 200 s; clients seen: stoplight, rainbow and midnight parrotfish, queen, French and
  gray angelfish. Shots pending (day1's look camera was blocked by a coral head)
- [ ] 4 Angelfish nip sponges - built; `L/log_day1.log`: "nip: queenangel adult #31 (group 12) nipped at the wall
  of a tube sponge at -1220.5, -8.7, 2872.9 ... because angelfish feed on sponges"; 143 nips in 200 s. Shot pending
- [ ] 5 Hunters together - built (moray side in my files; the jacks need R2); evidence run pending
- [ ] 6 The faults - moray poses, octopus by day, nurse shark, ray in the drawn sand built; pale den floor and the
  turtle at the surface not started
- [ ] 7 The recipe - R1 written
- [ ] 8 No slowdown - before taken (`L/log_base1.log`): boat frame 33.34 game 11.93 draw 7.22 gpu 31.14; dive 6 m
  frame 33.55 game 18.01 draw 15.55 gpu 25.96; reef cpu 0.10-0.15 ms a frame, 254 animals
- [ ] 9 Nothing else moves - not run

## Done
- Framework (`KeysReefLife.h/.cpp`): link traits `bSleepsInHoles`, `bSponges`, `bClient`, `bCleaner`, `RestEvery`,
  `bEggs`, `bShadows`; states for cleaning (to station, waiting, being cleaned, cleaning); dens taken for sleep
  and rest (`KeysReefDen::DensOfChunk`); egg patch pool; `KeysReefLife::Alarm` and `DiggingRays` for the other
  systems; test hooks `reef:clean`, `reef:intrude`, `reef:do clean|rest`; a `links:` line in `reef:stats`.
- New species `neongoby` (Elacatinus oceanops), `KeysReefLifeGoby.cpp`: the cleaner at the stations.
- Dens (`KeysReefDenLife.cpp`): moray body along the floor of its hole; travelling wave when it swims; hunt with
  the grouper works the feet of coral heads and alarms reef fish; octopus back in the den by day; nurse shark
  broader and curved; shrimp station: one place, a queue, a grunt and a young black grouper.

## Requests for the marshal
- R1 (open): `.claude/skills/reef-creature/SKILL.md` - section 7 written (the reef lanes' mistakes), the new link
  traits in the field table, the new test hooks - patch: `.marshal/requests/reeflinks/R1.patch`
- R2 (open): `KeysSchools.cpp` - jacks and blue runners within 40 m of a southern stingray that is digging come
  in low over it (`KeysReefLife::DiggingRays`), logged as `KRSchools link shadow` - patch:
  `.marshal/requests/reeflinks/R2.patch`

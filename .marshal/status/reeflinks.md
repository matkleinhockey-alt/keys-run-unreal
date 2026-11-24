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
- [ ] 2 Territory - built; evidence run pending
- [ ] 3 Cleaning stations - built; evidence run pending
- [ ] 4 Angelfish nip sponges - built; evidence run pending
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

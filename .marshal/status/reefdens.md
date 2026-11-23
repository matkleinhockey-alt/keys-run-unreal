# reefdens

Status: IN PROGRESS
Updated: 2026-11-23 12:00
Branch: lane/reefdens

The user's own words for this lane are two: "create hidden holes then, even if i cant explore them" and the standing
rule "i just want to keep it realistic". Everything else in the brief (five den kinds, the list of residents, day and
night, the links, the logbook line, wrecks, the perf limits) is the marshal's reading of those words; what is listed
under "Guesses" is this lane's reading of the marshal's.

Evidence lives in `L` = `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-reefdens/`
(`L/shots/<tag>/KRScript_NN.png`, `L/log_<tag>.log`).

## Acceptance criteria
- [ ] 1 Dens that read as holes - built, being looked at in the game
- [ ] 2 Residents, real ones - built, not yet looked at
- [ ] 3 Day and night - built, not yet run
- [ ] 4 They are part of the reef - built, not yet run
- [ ] 5 Something for the diver - built (a toast and a log line), not yet run
- [ ] 6 Wrecks, as a request - R3 written, not yet run
- [ ] 7 Realistic - list below
- [ ] 8 No slowdown - baseline taken on the clean branch (`L/log_base_real.log`), not yet compared
- [ ] 9 Nothing else moves - baseline shots taken on the clean branch (`L/shots/base_real/`), not yet compared

## Done
- `Source/KeysRun/KeysReefDen.h`, `KeysReefDenInt.h`, `KeysReefDen.cpp`: the rock, the placement, the runtime, the
  hooks for the hub, the test command `kr.den` (a console command: `con=kr.den ...` in a script, no hub change).
- `Source/KeysRun/KeysReefDenLife.cpp`: the residents.
- `Source/KeysRun/KeysReef.cpp/.h`: deals each chunk its dens after its corals, skips them when drawing, owns the
  dens' components, exposes its ground rules to the dens (`KeysReefDen::GroundAt`).

## In progress
- Looking at every kind and every resident in the game and correcting what is wrong.

## Blocked
- Nothing. Scripted runs wait a long time behind two other lanes.

## Requests for the marshal
- See the end of this file once the lane is done.

## Notes for integration
- `-KRNoDens` on the command line gives the reef as it was (no dens, no residents).

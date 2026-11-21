# docks

Status: IN PROGRESS
Updated: 2026-11-21 16:35 EST
Branch: lane/docks (see `git log -1` on the branch)

The user never wrote this lane's brief. The acceptance criteria below are the marshal's reading of "lifts and docks";
what I had to guess on top of that is listed under "Guesses".

## Acceptance criteria
- [ ] 1. Placement - planner and audit written and running (`KRDocks audit:` line, all zeros so far); shots not taken yet
- [ ] 2. Docks - four shapes, four finishes, fittings built; shots not taken yet
- [ ] 3. Lifts - cradle, elevator, davits built in four states; tide pair not shot yet
- [ ] 4. The fleet matches - every boat is `KeysYardProps::MakeBoat` / `BuildBoat`; to be written up
- [ ] 5. The player is not trapped - idle run not made yet
- [ ] 6. The classic map is unharmed - not run yet
- [ ] 7. No slowdown - not measured yet

## Done
- `KeysDocks.cpp/.h`: where the docks go (from `KeysGeo` only), the docks and their fittings, tiles, lamps, people, test hooks.
- `KeysLifts.cpp/.h`: cradle lift, elevator lift, davits.

## In progress
- Acceptance runs.

## Blocked
- Nothing.

## Requests for the marshal
- R1 (open): `KeysGameMode.cpp`, `KeysBoatPawn.cpp` - spawn `AKeysDocks` and route `dk:` script commands to
  `AKeysDocks::DebugCmd`. Without it nothing of this lane runs. patch: .marshal/requests/docks/R1.patch
- R2 (open): `KeysWorld.cpp` - `BuildScenery` keeps its palms and shore bushes off the docks
  (`KeysDocks::InFootprint(JX, JZ, 4)`). patch: .marshal/requests/docks/R2.patch

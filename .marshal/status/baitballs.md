# baitballs

Status: IN PROGRESS
Updated: 2026-11-24
Branch: lane/baitballs at a5132d1

## Acceptance criteria
- [ ] 1. More bait balls out deep - census works (log_t1.log: "KRBait census: a ball within 300 m at 288 of 300 points in deep water (96%), 74 of 150 over the reef (49%)"; five offshore spots 1-3 balls each, Hawk Channel and a flat 0); budget line pending
- [ ] 2. Big fish working them - seen from the mask (shots/t3/KRScript_53.png ball with predators, KRScript_55.png two sailfish, sails up, herding); surface/birds from the boat not yet shown
- [ ] 3. It is fishing - picks beside a worked ball (log_t3.log: "KRPicks ... skipjack 24% blackfin 22% mahi 20% kingfish 13% sailfish 8%") with R1 applied locally; sounder (R2) not yet looked at
- [ ] 4. Proved - in progress
- [ ] 5. No slowdown - after-run pending
- [ ] 6. Nothing else moves - pending

## Done
- `KeysBait.h/.cpp` (new): the published ball list, hub hooks `PickAt`, `WorkedAt`, `SonarNear`; bodies for ballyhoo, sardine, scad, silky (repainted), tern and frigatebird (built).
- `KeysSchools.*`: 12 new species (4 bait, 8 predators, role `Worker`), deep sites dealt on a 170 m grid, put in the water within 165 m, predators circle and charge, ball rises with its work, showers, flying fish glides, nervous water and charge splashes through `AKeysLife::Splash`, birds (instanced), sounder samples, test commands deep, census, balls, ballhere, toball, lookpred.

## Baseline (main code, this worktree, Medium, editor, 1600x900; log_perf_before2.log)
- offshore trolling: frame 33.61 / 34.62 ms, game 16.82 / 25.79, draw 10.10 / 17.53, GPU 25.93 / 28.27 (noisy: two samples disagree by 50% on game)
- bait_ball (reef dive, silverside ball put ahead): frame 21.30 / 20.55, game 19.47 / 18.75, draw 19.40 / 19.68, GPU 19.75 / 19.54
- fish: offshore 13-15 schools, ~3040 fish (bait share full: herring/silverside); reef dive 60 schools, 3510 fish

## Blocked
- none

## Requests for the marshal
- R1 (open): `KeysLife.cpp` - include `KeysBait.h`; in `SpotAt` a worked ball counts as a hot spot (bFrenzy when crashed); in `PickSpecies` first ask `KeysBait::PickAt` (most bites at a worked ball are its predators) - patch: .marshal/requests/baitballs/R1.patch
- R2 (open): `KeysHUD.cpp` - include `KeysBait.h`; the sounder also takes `KeysBait::SonarNear` contacts - patch: .marshal/requests/baitballs/R2.patch

## Notes for integration
- The criteria are the marshal's reading of the user's words; guesses listed here as made.

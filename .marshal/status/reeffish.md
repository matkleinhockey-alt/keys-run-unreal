# reeffish

Status: IN PROGRESS
Updated: 2026-11-23 17:50 (interim: final proof runs and the repeat perf run are under way)
Branch: lane/reeffish at the commit that carries this file

The split of the user's reef list into the criteria below is the marshal's reading, not the user's own words.
The user's words: "Setting: the Florida Keys reef tract. Use only species found there." and "i just want to keep
it realistic". Anything this lane had to guess is listed under "Guesses".

Evidence lives under `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-reeffish/`
(`log_<tag>.log`, `shots/<tag>/`).

## Acceptance criteria (interim, none ticked yet)
- [ ] 1 rainbow and midnight parrotfish - built, shot, biting and puffing logged (log_sombrero.log); final shots pending
- [ ] 2 queen, French and gray angelfish - built and shot; trailing fins need request R1
- [ ] 3 sergeant major - groups and guarding male built and shot; the egg patch and an observed chase are missing
- [ ] 4 proof - Sombrero done; other reefs and the classic map being re-run after a density change
- [ ] 5 budget - 185 units of caps; perf A/B taken once, dive GPU reads over 3%, repeat under way
- [ ] 6 nothing else moves - tpcheck 0 bad on both maps, spear landed on the real map; classic spear missed, retry under way
- [ ] 7 report on the pattern - requests R1, R2 written as patches; the rest to write up

## Done
- `Source/KeysRun/KeysReefLifeFishParrots.cpp`: rainbowparrot, midnightparrot (Grazer)
- `Source/KeysRun/KeysReefLifeFishAngels.cpp`: queenangel, frenchangel, grayangel (Pair)
- `Source/KeysRun/KeysReefLifeFishSergeant.cpp`: sergeantmajor (Hoverer), sergeantmajorguard (Territory)
- `.marshal/requests/reeffish/R1.patch`, `R2.patch`

## In progress
- final proof runs (tags perf_after2, final_real, final_classic)

## Blocked
- nothing

## Requests for the marshal
- R1 (open): trailing fins for angelfish and a home on a coral head - patch: .marshal/requests/reeffish/R1.patch
- R2 (open): retire the generic "angelfish" from KeysLife's reef table - patch: .marshal/requests/reeffish/R2.patch
- more to come in the final version of this file

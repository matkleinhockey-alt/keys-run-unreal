# reefdens

Status: IN PROGRESS
Updated: 2026-11-24 00:50 (second session: the lane was stopped part-way and taken over)
Branch: lane/reefdens

The user's own words for this lane: "create hidden holes then, even if i cant explore them", the standing rule "i just
want to keep it realistic", and since then "the Florida Keys reef tract, only species found there". This lane's share
of that list: green moray, spotted moray, Caribbean reef octopus, Caribbean spiny lobster, stone crab, and a nurse
shark resting under a ledge. Everything else in the brief is the marshal's reading; what is under "Guesses" is this
lane's.

Evidence lives in `L` = `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-reefdens/`
(`L/shots/<tag>/KRScript_NN.png`, `L/log_<tag>.log`; below `tag/NN`). Every shot named below was opened and looked
at. Shots from tags not named here were not all opened.

Why this is not READY FOR MERGE (in order of weight):
1. The cave and plate dens show a pale, lit floor inside the mouth when looked at from a little above
   (`molasses/56`, `molasses/58`, `molasses/62`, `mol2/50`, `mol2/51`): not a dark interior. Cause not found (two
   guesses about the seafloor coming up inside were tried and ruled out; the placement code is as it was).
2. Wreck gaps (R3) do not read as holes (`wreck/50`, `wreck/52`): do not apply R3 as it stands.
3. Perf is not shown to be within 3% (the difference is smaller than the spread between two identical runs).
4. The moray-and-grouper hunt was never seen.
5. On the branch as committed (hub files restored, no R1-R3) two late runs read the residents' cost at 7.8 to 60.9
   ms a frame (`log_nopatch.log`, `log_nopatch2.log`) against 0.016 to 0.107 ms in every earlier run. A packaged
   copy of the game was running at about 190% CPU with the load average at 9 to 14 during both, and the second run
   never finished its `perf=` sample, so this may be the machine and not the code; it could not be told apart.
   Measure `kr.den stats` ("residents' cpu") on a quiet machine, with and without R2, before merging.

## The six the user asked for (done first)

| Animal | Close shot (mask view) | What it shows |
|---|---|---|
| Green moray | `molasses/60` (best), `six1/50` | head and neck out from under a ledge, mouth open; yellow-green |
| Spotted moray | `six2/52` | small pale head with dark spots at the rim of a round hole |
| Caribbean reef octopus | `six1/52` | blue-green mantle and eyes in a hole, arms over the rim |
| Caribbean spiny lobster | `resS/53` (two, close), `six1/53` (four along a ledge), `six1/60` (five, from 3 m) | brown bodies in the ledge's shadow, long antennae out |
| Stone crab | `six1/54`, `resS/62` | tan crab at the mouth of a ledge / hole, heavy claws with black tips |
| Nurse shark | `six2/58` (best), `six1/55`, `six2/60` | tan shark lying under a ledge, two dorsal fins set far back, long tail out on the sand |

What is wrong in those shots: the nurse shark's body is thin and stiff (it is the hub's `KeysLife` mesh; `main` has
since changed it); the green moray's body comes down out of the roof of a ledge rather than along its floor
(`six1/50`); the spotted moray is small and plain from 0.9 m; the octopus shows its whole head by day, more than "an
arm or an eye"; a green moray out at night lies as straight as a stick (`night/53`).

Withdraw pairs, same camera: the animals are first told to take no notice of the diver (`kr.den calm 1`), then to mind
her (`calm 0`); the camera follows the animal, so it shifts a little between the two.
- Green moray: `molasses/60` -> `molasses/61` (out past the ledge's edge and lit, then back under it and dark);
  `log_molasses.log`: "KRDen withdraw: the Green moray at 26060.3, -2969.2 draws back into its hole: the diver (1.0 m
  off)". By the dive light, log line only (the pair `molasses/62`, `63` shows the light on the cave but the moray is
  too small to see): "draws back into its hole: the dive light is on it (2.8 m off)".
- Spotted moray: `six2/52` -> `six2/53`; `log_six2.log`: "draws back into its hole: the diver (0.9 m off)".
- Spiny lobster: `six2/54` -> `six2/55` (the right-hand one gone back into the dark); "KRDen link flick: 1 of 2 spiny
  lobsters ... flick back under the ledge (cause: the diver within 1.6 m, 1.3 m from the ledge)". By approach:
  `six1/60` (3.2 m, five out) -> `six1/61` (0.9 m, all back in the dark), "5 of 5 spiny lobsters ... flick back".
- Octopus: log line only ("pulls its arm in: the diver"); its pair was shot (`six2/56`, `57`) but not opened.

Counts (`log_six1.log`, `kr.den census`: the 200 m square on each named reef's charted point, real map; the same
numbers came back in `log_mol2.log` and `log_mol3.log`): Sombrero 48 dens, 27 empty (green moray 1, spotted moray 2,
lobster 9, octopus 1, squirrelfish 19, clinging crab 2, shrimp 1, eyes 7); Looe Key 16; American Shoal 51; Maryland
Shoal 44; Eastern Sambo 49; Middle Sambo 49; Western Sambo 80; Sand Key 63; Molasses 39; French 54; Grecian Rocks 42;
Key Largo Dry Rocks 63; Elbow 45; Carysfort 64; Pickles 27; Delta Shoal 42; Coffins Patch 0. Classic map
(`log_classic.log`): Sombrero 62, Coffins Patch 5, Delta Shoal 25. Stone crabs and nurse sharks are few: none in
Sombrero's square (the nearest nurse shark is 177 m from the mooring, the nearest stone crab 128 m).
Near the diver (`log_six1.log`): "KRDen drawn: 95 den rocks, 62204 triangles; 15 dens within 60 m of the camera (4
within 25 m), 8 of them empty".

## Acceptance criteria
- [ ] 1 Dens that read as holes - ledge, crevice and hole read as holes; cave and plate do not from above (see the
  top). Sombrero, 10 m and 1.3 m, mask view: ledge `kS/50`, `52` (chase `kS/51`, `53`), crevice `kS/54`, `56`, hole
  `kS/58`, cave `kS/62`, `64`, plate `kS/66` (hidden by a coral), `68`. Molasses Reef, 4 m, mask: ledge `molasses/50`
  (good), crevice `52` (seen from behind: the opening is not in the shot), hole `54`, cave `56` and plate `58` (pale
  floor), chase `57`. Classic map, 4 m: ledge `classic/50`, crevice `52`, hole (chase) `55`, cave `56`, plate `58`
  (all dark there). Dive light on a den: `molasses/63`. Dens per chunk by zone, `log_molasses.log`: "crest and back
  reef 21 dens (7 chunks ..., 3.0 a chunk); fore reef 105 dens (19 chunks, 5.5 a chunk); spur and groove 62 dens (12
  chunks, 5.2 a chunk)"; classic: deep reef 4.0 a chunk; patch reef 0 a chunk wherever it was asked (see "Not
  verified"). The diver and the spear stopped by the rock: `log_kS.log` "KRDen solid: a body put in the middle of the
  hole ... was pushed out, to 0.83 m from its middle ...; a shaft fired at its opening from 4 m stops after 3.98 m of
  12" (through the test command; it needs R1 to act in play). Fish not swimming through the rock: not checked.
  Chase-view shots exist for every kind at the three places but only the ones listed were opened.
- [ ] 2 Residents, real ones - the six above are shown. Of the brief's others, from the earlier session's run `resS`
  (after its last code change) only the logbook lines were read, not the shots: squirrelfish, soldierfish, sweepers,
  goliath, black grouper, clinging crab (`kS/56` was opened: a clinging crab in a crevice), shrimp. Rule and odds:
  `KeysReefDen.cpp`, `OddsOf` (50 to 60 in 100 dens empty, 10 to 13 a pair of eyes; by kind of den) and
  `RollResident` (what the depth and the opening's width allow); from each den's own seed, so the same every run (the
  census gave the same numbers in three runs).
- [ ] 3 Day and night - built and run once: day `night/50` and night `night/51` of the same ledge (a lobster out on
  the sand; the camera moved with it), `night/53` a green moray out of its hole. `log_night.log`: "KRDen night: 2
  spiny lobsters at the ledge at -1859.1, 2845.0 walk out onto the sand (hour 22.0)", "the reef octopus at -1966.4,
  2960.9 leaves its den to hunt", "the Green moray at -1959.1, 2991.3 leaves its hole to hunt", and by day "8 Blackbar
  soldierfish ... go back into its shadow". Squirrelfish leaving, sweepers spreading, the nurse shark's round, grunts
  moving in: not looked at. Without night lighting the "night" shots are in daylight.
- [ ] 4 They are part of the reef - log lines for three links, shots for one: lobster flick (`six2/54` -> `55`, log
  above); schools keep off a hole (`log_six1.log` and others, "KRDen link shy: the Green moray ... is out (cause), so
  the schools are given a 1.6 m keep-off round it (effect): 0 school fish within 1.6 m of it, 26 within 5 m"; no shot;
  needs R2); cleaning station (`log_resS.log` "KRDen link clean: 1 grunts leave the cleaning station ... (cause: the
  diver came within 0.5 m)"; no shot opened). Grouper and moray hunt: asked for in `log_night.log`, did not happen
  in the 18 s before the run ended: never seen.
- [ ] 5 Something for the diver - a line is logged and, the code says, toasted the first time each kind is found:
  `log_resS.log` "KRDen found: Green moray - Sombrero Reef, 6 m (the first of its kind: a line on the screen; 1 of 13
  kinds found this session)"; the toast is on screen in `kS/56` ("Channel clinging crab - Sombrero Reef, 5 m"). No
  logbook page. The dens' animals are not among the spear's targets (`AKeysDiver::GetTargets` was not changed;
  not tried in play). Catching them by hand: a follow-up, not
  built, not written up beyond this line.
- [ ] 6 Wrecks, as a request - R3 places two gaps on the Harbor tug and deals them residents (`log_wreck.log`: glassy
  sweepers x34, goliath grouper x1), but the look is wrong: `wreck/50` no dark gap to be seen in the deckhouse wall,
  `wreck/52` a ragged pale frame with no dark behind it and the goliath drawn as a pale many-sided blob.
- [ ] 7 Realistic - list below. Not ticked because of the flaws listed under the six.
- [ ] 8 No slowdown - not shown. Medium, `goto=reef`, same script, dens / no dens (`-KRNoDens`, same binary with
  R1-R3 applied, not `main`), two runs each, no other game or build running (`log_pA1`, `pA2` dens; `pB1`, `pB2` no
  dens):

  | sample | game dens | game none | draw dens | draw none | GPU dens | GPU none | worst dens | worst none |
  |---|---|---|---|---|---|---|---|---|
  | boat, stopped | 12.33, 11.72 | 11.73, 6.35 | 7.56, 6.92 | 6.87, 4.94 | 28.49, 30.61 | 30.35, 30.05 | 42.3, 33.3 | 33.3, 33.3 |
  | boat, driving | 13.42, 12.11 | 13.03, 12.43 | 8.57, 6.88 | 6.88, 7.22 | 30.29, 29.35 | 29.29, 29.17 | 36.2, 33.3 | 188.1, 33.3 |
  | dive at 6 m | 20.31, 22.10 | 19.84, 21.14 | 13.22, 14.56 | 13.40, 14.26 | 24.79, 24.40 | 23.45, 24.76 | 78.5, 78.7 | 73.6, 48.6 |

  Means in the dive: game +3.5%, draw +0.4%, GPU +2.0%; but two identical runs differ by 6 to 9%, so the samples
  cannot tell 3% apart. One no-dens boat sample (6.35 / 4.94) is far off the other three and unexplained. A later
  run (`log_final.log`: dive frame 88 ms, worst 1412 ms) is worthless: a packaged copy of the game and another
  editor were found running on the machine at that moment (not this lane's; left alone). Whether either was
  already running during the four runs in the table is not known, so read the table with that doubt. Frames over
  50 ms in the dive happen with and without dens. The dens' own cost: "residents' cpu 0.018 ms a frame"; "dealing a
  chunk its dens: 0.08 ms mean, 7.90 ms worst over 155 chunks"; "67 den rocks, 38883 triangles"; "2 drawn (1 moving),
  24 instances in 3 instanced meshes, 3324 triangles" (`log_pA2.log`). Limits in code: households within 60 m, drawn
  within 40 m (crabs and shrimp 8 m), moving within 25 m (`KeysReefDenLife.cpp`).
- [x] 9 Nothing else moves - evidence: `kr.den.corals` (count and checksum of every coral in the 49 chunks round a
  point) is the same with dens and with `-KRNoDens` at five points on each map: `log_chkA.log` / `log_chkB.log` (real:
  Sombrero 19547 corals, checksum 982a7331; Molasses 17448, fa5f6b44; Looe Key 22579, b9b5c5ac; two more) and
  `log_chkCA.log` / `log_chkCB.log` (classic: 16063, cce340ef; 5496, b2e4369b; three more). `tpcheck`: "checked 48
  destinations, 0 bad" (real, `log_pA1.log`), "checked 19 destinations, 0 bad" (classic, `log_classic.log`).
  `dk:audit`: 545 docks, every "standing on" count 0 (`log_pA1.log`; the line's "another dock 0" is the overlap
  count). Spear: "KRDive speared mutton 13.6 lb at 7.4 m" (`log_pA1.log`). No dens under 2.6 m of water or in dredged
  water by rule (`HabitatOf`); not looked for in a canal or on a flat in the game. The shot pairs taken for this
  (`pA1` / `pB1` 50-55) do not have the same camera and prove nothing.

## Done
- `Source/KeysRun/KeysReefDen.h`, `KeysReefDenInt.h`, `KeysReefDen.cpp`: the rock, the placement, the runtime, the
  hooks for the hub, the test commands `kr.den` and `kr.den.corals` (console commands: `con=kr.den ...` in a script).
- `Source/KeysRun/KeysReefDenLife.cpp`: the residents.
- `Source/KeysRun/KeysReef.cpp/.h`: see "Notes for integration".
- This session: the earlier agent's uncommitted `goto` sight-line filter reviewed, built and kept (test command
  only); `kr.den.corals` added; evidence runs `six1`, `six2`, `pA1`, `pB1`, `pA2`, `pB2`, `molasses`, `classic`,
  `night`, `wreck`, `chkA`, `chkB`, `chkCA`, `chkCB`, `mol2`, `mol3`, `final`.

## In progress
- Nothing is running. Left to do: the cave and plate interiors; the wreck gaps; a perf comparison that can resolve
  3%; the hunt; the residents not looked at.

## Blocked
- Nothing.

## Requests for the marshal
- R1 (open): `KeysDive.cpp`, `KeysDiver.cpp` - the diver is pushed out of a den's rock and the spear stops at it
  (`KeysReefDen::PushOut`, `RayHit`). Without it she swims through the rocks. Tested only through `kr.den solid`,
  not by swimming or firing at a rock in play. Patch: `.marshal/requests/reefdens/R1.patch`
- R2 (open): `KeysSchools.cpp` - schools keep off a hole with a moray, octopus or big grouper at its door
  (`KeysReefDen::SchoolThreats`, threat kind 5, no fright), and lend a fish count to the dens' test lines. Patch:
  `R2.patch`
- R3 (open, NOT ready: do not apply): `KeysWrecks.cpp` - gaps in the wrecks' plating as dens
  (`KeysReefDen::AddWreckGap`). See criterion 6. Patch: `R3.patch`
- R4 (open, no patch): `KeysLife` - the dens' nurse shark is `AKeysLife::BuildSchoolMesh("nurse")`; `main` has
  changed that mesh since, so look at a den's nurse shark after the join (`kr.den goto nurse 0 2.4 -2.0 0.0 420`).
  `KeysLife` already has a nurse shark of its own and `main` now has reef-life species: check the two do not double
  up at a ledge.
- The three patch files are byte for byte what was applied in the worktree for the runs above (checked with `diff`).
  The hub files are restored on the branch; all runs above except the last had R1-R3 applied.

## Notes for integration
- `-KRNoDens` on the command line gives the reef as it was (no dens, no residents).
- Edits to `KeysReef.cpp` (all additive, none in the coral rules, the sea fans, barrel sponges or elkhorn colour):
  `#include "KeysReefDenInt.h"`; a block after the anonymous namespace (`KeysReefDen::GroundAt`, `ChunkHash`,
  `AKeysReef::IsSolidCoral`, `IsTallCoral`, `DenStandIn`); `Dens = KeysReefDen::Create(this)` at the end of
  `BeginPlay`; a new `EndPlay`; in `ChunkInstances` one call `KeysReefDen::OnChunk(Dens, CX, CZ, Out)` after the
  modelled species and `KeysReefDen::Forget` where the cache drops a chunk; in `Rebuild` one line `if (I.Den)
  continue;` at the top of the instance loop and one call `KeysReefDen::Rebuild` at the end; in `Tick` one call
  `KeysReefDen::Tick`. `IsSolidCoral` / `IsTallCoral` name the code-built kinds by enum and the modelled ones by
  index (0-12, 6 and 7 tall): if `main` reordered or added kinds, check them.
- Edits to `KeysReef.h`: a forward declaration; `EndPlay`; `FInst::Den`; the three static functions; `DenState()`;
  three `UPROPERTY` arrays (`DenPools`, `DenMeshes`, `DenMats`); the private `Dens` pointer.
- A den's rock stands in a chunk's instance list as a boulder star coral with `Den = 1` (so the schools' floor cache
  finds it): anything on `main` that reads `ChunkInstances` to draw or count corals must skip `Den`.
- Which animals would gain most from a model later: nurse shark, octopus, green moray, in that order.

## Species (criterion 7), each from general knowledge of the western Atlantic reef fauna, no source consulted
- Green moray, *Gymnothorax funebris*: to 1.8 m and more, uniformly green to brown-green, crevices, ledges, wrecks.
- Spotted moray, *Gymnothorax moringa*: to about 1 m, pale with dense dark spots, small holes on shallow reef.
- Caribbean reef octopus, *Octopus briareus*: blue-green, nocturnal, dens with a midden of shells.
- Caribbean spiny lobster, *Panulirus argus*: gregarious under ledges by day, long antennae, walks out at night.
- Florida stone crab, *Menippe mercenaria*: heavy claws with black tips, holes and ledges, mostly shallower water
  (the code keeps it above 14 m; more a hard-bottom and seagrass animal than a fore-reef one: unsure how common on
  the outer reef).
- Nurse shark, *Ginglymostoma cirratum*: 2 to 3 m, tan-brown, rests under ledges by day, hunts at night.
- Squirrelfish, blackbar soldierfish, glassy sweeper, goliath grouper, black grouper, channel clinging crab, banded
  coral shrimp: all of the Keys reef tract. Unsure: how often a goliath sits in a reef hole rather than on a wreck.

## Guesses
- "Convincing from 10 m" was taken to mean a dark mark that reads as an opening, not that the animal is visible.
- A den is a rock standing on the bottom, not a hole in an existing coral head; "an undercut ledge along the base of
  a spur" is therefore a ledge rock placed on a spur's flank.
- Patch reefs get dens only where the reef's own patch rule says there is a patch (see "Not verified").

## Not verified
- The branch without R1-R3: it builds (`Result: Succeeded`) and one smoke run found a ledge with five lobsters,
  dealt and drew dens and gave the same coral checksum (`log_nopatch.log`; its shot `nopatch/50` was not opened),
  but see point 5 at the top.
- Patch reefs: Coffins Patch has 0 dens in its square on the real map (5 on the classic map) and "patch reef 0 dens"
  at every place `kr.den stats` was run; no patch-reef den was ever looked at.
- Looe Key and Carysfort were only counted, never dived.
- Fish swimming through den rock; dens seen from the boat; the far level of detail; any shot at Low or High graphics.
- The earlier session's shots (`r1`, `r2`, `r3`, `dens`, `noden`, `base_real`) other than those named.
- Nothing was merged or rebased from `main`; none of this was run against the reef-life framework.

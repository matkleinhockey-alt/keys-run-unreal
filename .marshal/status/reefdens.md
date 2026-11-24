# reefdens

Status: IN PROGRESS
Updated: 2026-11-23 (second session, after the lane was stopped and taken over; evidence runs still being read)
Branch: lane/reefdens

The user's own words for this lane: "create hidden holes then, even if i cant explore them", the standing rule "i just
want to keep it realistic", and since then "the Florida Keys reef tract, only species found there". This lane's share
of that list: green moray, spotted moray, Caribbean reef octopus, Caribbean spiny lobster, stone crab, and a nurse
shark resting under a ledge. Everything else in the brief is the marshal's reading; what is under "Guesses" is this
lane's.

Evidence lives in `L` = `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-reefdens/`
(`L/shots/<tag>/KRScript_NN.png`, `L/log_<tag>.log`). Every shot named below was opened and looked at.

## The six the user asked for (done first)

| Animal | Close shot (mask view, Sombrero Reef) | What it shows |
|---|---|---|
| Green moray | `six1/50` | head and neck out from under a ledge, mouth open, teeth; pale green in the reef light |
| Spotted moray | `six2/52` | small pale head with dark spots at the rim of a round hole |
| Caribbean reef octopus | `six1/52` | blue-green mantle and eyes in a hole, arms over the rim |
| Caribbean spiny lobster | `resS/53` (two, close), `six1/53` (four along a ledge), `six1/60` (five, from 3 m) | brown bodies in the ledge's shadow, long antennae out |
| Stone crab | `six1/54`, `resS/62` | tan crab at the mouth of a ledge / hole, heavy claws with black tips |
| Nurse shark | `six2/58` (best), `six1/55`, `six2/60` | tan shark lying under a ledge, two dorsal fins set far back, long tail out on the sand |

Withdraw pairs (same camera, the animals first told to take no notice of the diver with `kr.den calm 1`, then
`calm 0`; the log line gives cause and distance):
- Spotted moray: `six2/52` -> `six2/53`; `log_six2.log`: "KRDen withdraw: the Spotted moray at -1753.9, 2763.3 draws
  back into its hole: the diver (0.9 m off)".
- Spiny lobster: `six2/54` -> `six2/55` (the right-hand one is gone back into the dark); "KRDen link flick: 1 of 2
  spiny lobsters ... flick back under the ledge (cause: the diver within 1.6 m, 1.3 m from the ledge)". Also by
  approach, `six1/60` (3.2 m, five out) -> `six1/61` (0.9 m, all back in the dark): "5 of 5 spiny lobsters ... flick
  back".
- Green moray: see "Not verified" until the `molasses` run is read.

Counts (`log_six1.log`, `kr.den census`: the 200 m square on each named reef's charted point, real map):
Sombrero 48 dens, 27 empty (green moray 1, spotted moray 2, lobster 9, octopus 1); Looe Key 16 / 11 empty; American
Shoal 51; Maryland Shoal 44; Eastern Sambo 49; Middle Sambo 49; Western Sambo 80; Sand Key 63; Molasses 39; French 54;
Grecian Rocks 42; Key Largo Dry Rocks 63; Elbow 45; Carysfort 64; Pickles 27; Delta Shoal 42; Coffins Patch 0 (see
"Not verified"). Near the diver: "KRDen drawn: 95 den rocks, 62204 triangles; 15 dens within 60 m of the camera (4
within 25 m), 8 of them empty".

## Acceptance criteria
(being filled in from the runs of this session; see the next commit)

## Requests for the marshal
- R1 (open): `KeysDive.cpp`, `KeysDiver.cpp` - the diver is pushed out of a den's rock and the spear stops at it
  (`KeysReefDen::PushOut`, `RayHit`) - patch: `.marshal/requests/reefdens/R1.patch`
- R2 (open): `KeysSchools.cpp` - schools keep off a hole with a moray, octopus or big grouper at its door
  (`KeysReefDen::SchoolThreats`, threat kind 5), and lend a fish count to the dens' test lines - patch: `R2.patch`
- R3 (open): `KeysWrecks.cpp` - gaps in the wrecks' plating become dens (`KeysReefDen::AddWreckGap`) - patch: `R3.patch`

## Notes for integration
- `-KRNoDens` on the command line gives the reef as it was (no dens, no residents).
- The lane's edits to `KeysReef.cpp` / `.h` are listed in the next commit of this file.

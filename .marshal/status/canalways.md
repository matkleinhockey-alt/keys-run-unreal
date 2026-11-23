# canalways

Status: IN PROGRESS
Updated: 2026-11-23 09:45 EST
Branch: lane/canalways

The user's words, as the marshal passed them on: "Open the 46 canals that have no way out to sea, mostly in the
Lower Keys and Key Largo." The eight criteria below are the marshal's reading of that
(`.marshal/prompts/canalways.md`), not the user's words.

Evidence is outside the repo in `KeysRun-devkit/lanes/KeysRun-canalways/` (`shots/<tag>/KRScript_NN.png`,
`log_<tag>.log`), written `<tag>` below.

## Acceptance criteria (interim: checks still running)
- [ ] 1. The list - table below (from the tool; matches `fish:canals` on the untouched branch, `log_base.log`: 195 canals, 46 shut)
- [ ] 2. Every canal has a way out - `log_ways10.log`: "KRCanal 217 canals, 0 with no 1.0 m water out to the open sea"; shots `ways10`
- [ ] 3. Made by the tool - `Tools/geo/canals.py p2|p3|p4 --check`: the same, byte for byte
- [ ] 4. A boat can make the trip - running
- [ ] 5. They look right - to do
- [ ] 6. Marked, as a request - R1 written, 46 pairs, to be shot
- [ ] 7. What moved - to do
- [ ] 8. Nothing else moves - to do

## The 46 (criterion 1)

| Canal (index on main) | Middle x, z | Part | Why it was shut | Nearest 1 m water joined to the sea, along the route | How it is opened |
|---|---|---|---|---|---|
| 10 | 12960, -1008 | Upper Keys (p2) | a pond on the grid | 108 m | mouth cut open: 70 m of canal line + 46 m of way, to 12765, -878 |
| 26 | 14530, -1648 | Upper Keys (p2) | a pond on the grid | 78 m | mouth cut open: 79 m of canal line + 8 m of way, to 14638, -1708 |
| 35 | 20115, -3402 | Upper Keys (p2) | a pinched mouth | 10 m | 33 m of way, to 20052, -3400 |
| 36 | 13300, -1168 | Upper Keys (p2) | a pinched mouth | 45 m | mouth cut open: 28 m of canal line + 33 m of way, to 13465, -1178 |
| 59 | -22232, -2428 | Lower Keys (p3) | a pinched mouth | 10 m | 33 m of way, to -22142, -2485 |
| 64 | -10880, -1560 | Lower Keys (p3) | a pond on the grid | 94 m | mouth cut open: 37 m of canal line + 93 m of way, to -10722, -1438 |
| 66 | -22352, -2470 | Lower Keys (p3) | a pond on the grid | 14 m | mouth cut open: 23 m of canal line + 5 m of way, to -22215, -2588 |
| 67 | -10845, -1670 | Lower Keys (p3) | a pond on the grid | 68 m | mouth cut open: 78 m of canal line, to -10878, -1572 |
| 68 | -16145, -2775 | Lower Keys (p3) | the flat outside is too shallow | 412 m | 403 m of way, to -15915, -2450 |
| 70 | -15800, -2098 | Lower Keys (p3) | the flat outside is too shallow | 52 m | 65 m of way, to -15932, -2448 |
| 72 | -10948, -1370 | Lower Keys (p3) | a pond on the grid | 38 m | mouth cut open: 45 m of canal line, to -10912, -1468 |
| 76 | -17495, -2440 | Lower Keys (p3) | the flat outside is too shallow | 195 m | 199 m of way, to -17392, -2395 |
| 86 | -19952, -2385 | Lower Keys (p3) | a pond on the grid | 84 m | mouth cut open: 94 m of canal line, to -19912, -2490 |
| 90 | -15508, -2038 | Lower Keys (p3) | a pond on the grid | 157 m | mouth cut open: 119 m of canal line + 109 m of way, to -15572, -2248 |
| 96 | -14968, -3280 | Lower Keys (p3) | the flat outside is too shallow | 152 m | 377 m of way, to -15298, -3112 |
| 97 | -8872, -312 | Lower Keys (p3) | a pond on the grid | 386 m | mouth cut open: 43 m of canal line + 381 m of way, to -9132, -565 |
| 98 | -12095, -1855 | Lower Keys (p3) | the flat outside is too shallow | 76 m | 84 m of way, to -12182, -1845 |
| 107 | -10188, -1472 | Lower Keys (p3) | a pond on the grid | 68 m | mouth cut open: 77 m of canal line, to -10162, -1548 |
| 110 | -10985, -1252 | Lower Keys (p3) | a pond on the grid | 57 m | mouth cut open: 63 m of canal line, to -10952, -1358 |
| 113 | -22205, -1735 | Lower Keys (p3) | a pinched mouth | 22 m | mouth cut open: 30 m of canal line + 11 m of way, to -22152, -1665 |
| 117 | -19900, -2155 | Lower Keys (p3) | the flat outside is too shallow | 360 m | 361 m of way, to -19928, -1752 |
| 118 | -14590, -3058 | Lower Keys (p3) | a pond on the grid | 117 m | mouth cut open: 113 m of canal line + 11 m of way, to -14462, -3038 |
| 119 | -10205, -2005 | Lower Keys (p3) | a pond on the grid | 143 m | mouth cut open: 135 m of canal line + 24 m of way, to -10022, -1958 |
| 120 | -15082, -3170 | Lower Keys (p3) | the flat outside is too shallow | 43 m | 53 m of way, to -15082, -3268 |
| 121 | -16042, -2690 | Lower Keys (p3) | the flat outside is too shallow | 14 m | 29 m of way, to -16088, -2662 |
| 122 | -16185, -2562 | Lower Keys (p3) | the flat outside is too shallow | 51 m | 56 m of way, to -16052, -2565 |
| 123 | -10085, -2060 | Lower Keys (p3) | joined to canal 119: shut with it | - | opened with canal 119 |
| 125 | -12932, -2425 | Lower Keys (p3) | the flat outside is too shallow | 59 m | 71 m of way, to -12865, -2385 |
| 126 | -19820, -1488 | Lower Keys (p3) | a pond on the grid | 91 m | mouth cut open: 98 m of canal line, to -19882, -1382 |
| 130 | -15952, -2565 | Lower Keys (p3) | the flat outside is too shallow | 16 m | 26 m of way, to -16018, -2542 |
| 134 | -22295, -2615 | Lower Keys (p3) | a pinched mouth | 41 m | mouth cut open: 53 m of canal line + 7 m of way, to -22265, -2718 |
| 137 | -19842, -2095 | Lower Keys (p3) | joined to canal 117: shut with it | - | opened with canal 117 |
| 138 | -13015, -2390 | Lower Keys (p3) | the flat outside is too shallow | 39 m | 46 m of way, to -12895, -2388 |
| 139 | -12975, -2310 | Lower Keys (p3) | joined to canal 138: shut with it | - | opened with canal 138 |
| 141 | -17252, -3298 | Lower Keys (p3) | the flat outside is too shallow | 177 m | 299 m of way, to -17538, -3172 |
| 143 | -15998, -2628 | Lower Keys (p3) | a pinched mouth | 17 m | 28 m of way, to -16012, -2592 |
| 144 | -23912, -2280 | Lower Keys (p3) | a pond on the grid | 27 m | mouth cut open: 38 m of canal line + 17 m of way, to -23992, -2358 |
| 145 | -13018, -2260 | Lower Keys (p3) | a pinched mouth | 34 m | 43 m of way, to -12958, -2298 |
| 146 | -22302, -2118 | Lower Keys (p3) | a pinched mouth | 20 m | 59 m of way, to -22245, -2118 |
| 155 | 26360, -6720 | Key Largo (p4) | a pinched mouth | 92 m | 101 m of way, to 26245, -6590 |
| 172 | 28002, -8778 | Key Largo (p4) | the flat outside is too shallow | 44 m | 63 m of way, to 28098, -8725 |
| 173 | 26495, -7222 | Key Largo (p4) | a pond on the grid | 94 m | mouth cut open: 99 m of canal line, to 26548, -7292 |
| 177 | 28145, -8358 | Key Largo (p4) | a pond on the grid | 23 m | mouth cut open: 27 m of canal line + 8 m of way, to 28142, -8388 |
| 179 | 28162, -8288 | Key Largo (p4) | a pond on the grid | 23 m | mouth cut open: 33 m of canal line, to 28138, -8348 |
| 183 | 27855, -8918 | Key Largo (p4) | a pond on the grid | 71 m | mouth cut open: 68 m of canal line + 31 m of way, to 27798, -8822 |
| 190 | 27082, -11065 | Key Largo (p4) | the flat outside is too shallow | 17 m | 78 m of way, to 26982, -11125 |
| Layton | 10062, -825 | Upper Keys (p2) | a pond on the grid | 116 m | mouth cut open: 120 m of canal line, to 10158, -848 (Layton: open on main by the hand-added line, which the tool now replaces) |

## Requests for the marshal
- R1 (open): `Source/KeysRun/KeysGeoReal.cpp` - a pair of daybeacons at the outer end of every way that crosses a flat
  (`MarkWays`, called at the end of `LoadAll`) - patch: `.marshal/requests/canalways/R1.patch`

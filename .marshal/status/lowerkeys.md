# lowerkeys

Status: IN PROGRESS
Updated: 2026-11-22 00:30 EST
Branch: lane/lowerkeys (see `git log`; this file is in the latest commit)

Read first:
- The user said only "start phase 3". The split (Part A the Lower Keys, Part B Key Largo), the order and the ten
  criteria are the marshal's reading of it, from the brief. Everything under "Decisions I made on my own" is mine.
- This lane runs overnight with nobody to ask. Where the brief left a choice I made it and wrote it down below.

Evidence paths: shots and logs are under `KeysRun-devkit/lanes/KeysRun-lowerkeys/` (`shots/<tag>/`, `log_<tag>.log`,
`rss_<tag>.txt` the peak memory of that run in KB); previews under `KeysRun-devkit/geo/`.

## Part A (the Lower Keys) - acceptance criteria
- [ ] 1 Data
- [ ] 2 No seam
- [ ] 3 The game loads all of it
- [ ] 4 A boat can make the trip
- [ ] 5 The chart covers it
- [ ] 6 Cost, measured and bounded
- [ ] 7 Requests written
- [ ] 8 Nothing else moves
- [ ] 9 The classic map is unharmed
- [ ] 10 No slowdown once running

## Part B (Key Largo) - acceptance criteria
- [ ] 1..10 not started

## Decisions I made on my own
- Phase names and rectangles: `p3` = the Lower Keys, x -30060..-8300, z -6920..6200 (the same north and south edges
  as p2, so Part A needs no bay strip of its own). Planned for Part B: `p4` = Key Largo, x 23500..35500,
  z -13720..6200, and `p4_bay` = the bay and Gulf north of everything else, x -30060..23500, z -13720..-6920.
- West edge -30060, not a rounder number: 21760 m west of Phase 1's edge is 16 steps of 1360 m, which keeps Phase 1
  and 2's palms (16 m grid), houses (34 m), terrain (8 m) and depth grid (10 m) on the same spots, as the Phase 2
  lane did going north. It takes in Key West harbor (x -24830) and Sand Key Light (x -28330) and stops short of
  Ballast Key and Woman Key (x -30300 and beyond). The Marquesas and the Dry Tortugas are not on the map.
- US-1's bridge bents are counted every 36 m from the map's west edge, and 21760 is not a multiple of 36: no west edge
  that keeps the map a sensible size is. So the request that stops the road at Key West also counts the bents from
  Phase 1's own corner (see Requests).

## Baseline (before any change, branch at bf2feac)
- (being measured)

## Done
- (see the criteria)

## In progress
- Part A data.

## Blocked
- Nothing.

## Requests for the marshal
- (none written yet)

## Notes for integration
- (none yet)

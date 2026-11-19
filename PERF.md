# Keys Run performance

Every graphics change re-runs the six bookmarks below, adds after-screenshots under `Docs/Screens/<tag>/`, and must keep
**Medium at 60 fps or above**. A change that drops Medium below 60 gets a scalability setting (off or reduced on Medium)
instead of shipping at full cost.

## How to measure

```
Tools/perf_bookmarks.sh <tag> ["path/to/Keys Run.app"] [one bookmark]
```

Runs the packaged Shipping app once per bookmark, 1920x1080 windowed. Each run sets the scene up, waits for it to settle,
takes a screenshot, then measures 5 s of frames on Medium, then again on High. The `perf=<label>` script command records
average frame time, game thread, draw (render) thread and GPU time (the numbers `stat unit` shows) and the worst frame.
The results go to `Docs/Screens/<tag>/perf.tsv`.

| Bookmark | Scene |
|---|---|
| marina | boat just off Boot Key Harbor City Marina, bow on to the docks |
| reef_underwater | diving on Sombrero Reef, 5 m down, looking along the reef |
| bait_ball | diving on the reef, 4 m, a silverside bait ball put a few metres ahead |
| offshore_trolling | Gulf Stream, throttle at 0.3 (trolling speed), chase camera |
| oil_rig | 140 m off the first oil rig, camera on it |
| rain_at_sea | Gulf Stream, a storm cell spawned overhead (rain, wind 41 kn) |

## Baseline: v1.36, 2026-11-17

Apple M4 (10-core GPU), macOS, 4K display, game window 1920x1080, Shipping build. Screenshots are in
`Docs/Screens/baseline/` (`<bookmark>_medium.png`, `<bookmark>_high.png`).

| Bookmark | Quality | fps | frame ms | game ms | draw ms | GPU ms | worst ms |
|---|---|---|---|---|---|---|---|
| marina | Medium | 29.6 | 33.7 | 18.0 | 5.7 | 33.9 | 35.8 |
| marina | High | 13.7 | 73.3 | 19.7 | 6.6 | 73.1 | 78.7 |
| reef_underwater | Medium | 30.0 | 33.3 | 20.6 | 9.1 | 29.9 | 33.4 |
| reef_underwater | High | 15.1 | 66.3 | 22.7 | 11.0 | 66.2 | 130.1 |
| bait_ball | Medium | 30.0 | 33.3 | 20.6 | 9.2 | 29.4 | 33.4 |
| bait_ball | High | 17.6 | 56.9 | 21.9 | 11.1 | 56.3 | 96.1 |
| offshore_trolling | Medium | 30.0 | 33.3 | 10.3 | 5.4 | 27.4 | 33.4 |
| offshore_trolling | High | 15.4 | 64.9 | 16.5 | 10.5 | 64.8 | 66.5 |
| oil_rig | Medium | 30.0 | 33.3 | 10.0 | 5.9 | 24.3 | 33.3 |
| oil_rig | High | 17.8 | 56.2 | 14.4 | 10.7 | 56.1 | 58.8 |
| rain_at_sea | Medium | 30.0 | 33.3 | 10.5 | 5.6 | 27.5 | 33.4 |
| rain_at_sea | High | 16.1 | 61.9 | 16.4 | 10.7 | 61.9 | 63.4 |

### What the baseline says

- **Medium does not reach 60 fps anywhere.** The GPU takes 24-34 ms per frame, over the 16.7 ms a 60 fps frame allows,
  so the display falls back to every other refresh and Medium is locked at 30 fps.
- **The game thread is also over budget in some places.** At the marina, on the reef and at the bait ball it takes
  18-21 ms (lighter offshore: 10 ms). At 60 fps it has to stay under about 14 ms.
- **High costs about twice Medium on the GPU** (55-73 ms), mostly from virtual shadow maps, finer Lumen and more
  cloud ray-march steps (see `KeysGfx.cpp`).
- Before any new graphics feature ships, Medium needs to be brought to 60: GPU under about 14 ms and game thread
  under about 14 ms at these bookmarks. Until then, new features go in only behind a scalability setting that is off
  on Medium.

## Changes

(Each graphics change adds a row here, with its tag under `Docs/Screens/`.)

| Date | Change | Tag | Medium worst-case fps | Notes |
|---|---|---|---|---|
| 2026-11-17 | Baseline (v1.36) | baseline | 29.6 | GPU bound 24-34 ms; game thread up to 21 ms |
| 2026-11-17 | Boids schooling (v1.37) | boids | 30.0 | GPU down underwater (reef 29.9 -> 24.8 ms, bait ball 29.4 -> 23.0 ms; High bait ball 56 -> 51 ms); game thread about the same (+1.4 ms marina, -1.9 ms reef); boids CPU ~0.8-1.0 ms for ~3,000 fish. No regression; Medium still GPU bound at 30 |
| 2026-11-18 | Town style kit, three restaurants, gloved rod hand (v1.39) | towncafe | 30.0 | Medium still GPU-locked at 30 everywhere; GPU about the same (marina 31.3 -> 32.1 ms). Game thread at the marina up 2.8 ms (19.4 -> 22.2), from about 20 more seated, animated people in town (rebuilt each frame). Elsewhere unchanged. To claw back when Medium-60 work starts: draw seated diners only within ~60 m |
| 2026-11-18 | Town reacts to your fishing: Wall of Fame, derby board, barks, iguanas, dock cat, chimes, string lights, bar music (v1.40) | townfame | 30.0 | No change: Medium GPU-locked at 30 everywhere (marina GPU 32.1 -> 32.6 ms, game 22.2 -> 21.2 ms); the critters mesh is rebuilt per frame only within ~70 m of the marina |

### Boids (v1.37) vs baseline, Medium

| Bookmark | fps | GPU ms (was) | game ms (was) |
|---|---|---|---|
| marina | 30.0 | 31.3 (33.9) | 19.4 (18.0) |
| reef_underwater | 30.0 | 24.8 (29.9) | 18.7 (20.6) |
| bait_ball | 32.8 | 23.0 (29.4) | 21.1 (20.6) |
| offshore_trolling | 30.0 | 27.1 (27.4) | 9.2 (10.3) |
| oil_rig | 30.0 | 24.3 (24.3) | 10.8 (10.0) |
| rain_at_sea | 30.0 | 27.3 (27.5) | 8.4 (10.5) |

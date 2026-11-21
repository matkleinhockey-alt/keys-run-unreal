---
name: scripted-run
description: Run Keys Run from a timed script to take screenshots and log lines. This is the project's test procedure; use it to check any change in the real game.
---

# Scripted run (the tests)

There is no test suite. A check is a scripted run of the game plus the screenshots and log lines it leaves.

```bash
"/Users/mat/Documents/Unreal Projects/KeysRun-devkit/shoot.sh" <tag> "<seconds:command;seconds:command;...>" [extra args]
```

- Run it from inside your worktree, after a successful `build`. It runs the project you are standing in.
- Output for a lane worktree named `KeysRun-<lane>` lands in
  `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-<lane>/`:
  `shots/<tag>/*.png`, `run_<tag>.log` (launcher output) and `log_<tag>.log` (the game log: grep this one).
- Only one game runs at a time on this machine. If another lane is running, your call waits its turn; that is not a hang.
- Always end the script with `quit`. `RX` and `RY` set the window size (default 1600 x 900).
- Read every screenshot you take.

## Commands (the useful ones)

| Command | What it does |
|---|---|
| `play` | leave the title screen (always first, around second 3) |
| `tp=x,z,heading` | put the boat at game metres x, z |
| `cam=yaw,pitch,zoom` | camera: pitch 25 is top-down, 0 is the chase view |
| `shot` / `shotclean` | screenshot with / without the HUD |
| `dive` | go underwater at the boat |
| `tide=L` | force the tide (-1 low .. 1 high); `tide=9` releases it |
| `clock=HH:MM` | fake the hour the fish feed by |
| `picks` | log `KRPicks`: what 400 casts here would catch |
| `fish:stats` | log school and fish counts and boids CPU time |
| `catch=key:lb` | land a fish of that species |
| `gfx=N` | graphics level (1 is Medium) |
| `perf=label` | log a frame-time sample under that label |
| `con=<console command>` | any Unreal console command |
| `throttle=`, `heading=`, `steer=` | drive the boat |
| `quit` | exit |

The full list is the `else if (Cmd ...)` chain in `Source/KeysRun/KeysBoatPawn.cpp` (read only: that file is
marshal-only).

## Rules

- Never use `estbuy`: it writes the user's real save.
- The real map is the default. Add `-KRClassicMap` as an extra arg to test the old map.
- Useful spots on the real map: mangrove flat off Boot Key `tp=-1150,452,4.71`; Bahia Honda bridge
  `tp=-2700,-560,2.6`; `goto=reef`, `goto=offshore`, `bookmark=marina`.

## Example

```bash
"/Users/mat/Documents/Unreal Projects/KeysRun-devkit/shoot.sh" flat "3:play;6:tp=-1150,452,4.71;7:cam=0,25,3;16:shotclean;17:picks;18:quit"
grep KRPicks "/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-<lane>/log_flat.log"
```

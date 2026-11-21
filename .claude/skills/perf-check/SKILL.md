---
name: perf-check
description: Measure what a Keys Run change costs in frame time. Use after any change that adds geometry, raises a fish or instance budget, or touches lighting or materials.
---

# Perf check

The project rule: graphics changes must not take Medium below 60 fps. A lane cannot run the packaged-build bookmarks
(that is the marshal's job at integration), so a lane measures before and after in the editor build.

1. Pick the spot your change affects and one spot it should not affect.
2. Run the same script before your change and after it. Take the "before" run first, on the clean branch, before you
   start editing. Never use `git stash`: the stash is shared with the other worktrees.

```bash
"/Users/mat/Documents/Unreal Projects/KeysRun-devkit/shoot.sh" perf_after "3:play;4:gfx=1;6:tp=<x>,<z>,<heading>;16:perf=after;24:quit"
grep KRPerf "/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-<lane>/log_perf_after.log" | tail -5
```

3. Let the scene settle for at least 8 seconds before the `perf=` sample.
4. Report both numbers and the difference in your status file. More than 5% slower at the affected spot needs a
   reason or a fix (tiles, draw distance, fewer triangles, instancing).

## Notes

- `shoot.sh` already makes sure no other lane's game is running during your run. A copy the user has open by hand
  still skews the numbers: if frame times look wrong, say so instead of reporting them.
- Editor-build numbers are slower than the shipped game. Compare before with after, never with a shipped number.
- `fish:stats` gives the boids' CPU time separately when the change is about fish counts.

You are the `water` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the Florida Keys. You know nothing
about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-water` (run every command from inside it)
- Branch: `lane/water`
- Read first: `MARSHAL.md`, `.claude/agents/sea-life.md`, `.claude/agents/verifier.md`, and the skills in
  `.claude/skills/` (`build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`).

## Goal

Polish the sea and the seafloor: remove a visible rendering seam on the flats, show the tide, and confirm the new
bait balls on screen.

## Owned paths

- `Source/KeysRun/KeysSeafloor.cpp`, `KeysSeafloor.h`
- `Source/KeysRun/KeysSea.cpp`, `KeysSea.h`
- `Source/KeysRun/KeysOcean.cpp`, `KeysOcean.h`
- `Source/KeysRun/KeysSchools.cpp`, `KeysSchools.h`
- `.marshal/status/water.md`, `.marshal/requests/water/`

Everything else is read-only for you.

## Deliverables and acceptance criteria

1. **The seafloor square is gone.** From above on the flats, the detailed seafloor chunks round the camera
   (`KeysSeafloor`: 64 m chunks out to 220 m) show as a brighter square against the far skirt.
   - Reproduce: `3:play;6:tp=-1150,452,4.71;7:cam=0,25,3;16:shotclean;17:quit`. Adding `-KRSeaDebug` as an extra
     argument paints near chunks red, mid chunks blue and the skirt green, which shows the square is exactly the chunk
     area.
   - Measured so far: at 1600 x 900 the red channel steps from about 87 to about 100 across x = 500..520 px on rows
     600..700.
   - Already ruled out (each tried, none changed the step): the skirt's depth offset (`FarSkirtYBias`), shadow
     casting on the chunks, the chunks' part in indirect lighting, and turning Lumen diffuse, virtual shadow maps or
     ambient occlusion off by console command. The two meshes use the same height and colour functions and the same
     material.
   - Not yet tried: hiding the chunks to see whether the skirt alone looks like the outside; comparing vertex normals
     and vertex colours of both meshes at the same point by logging them; the material's use of UVs or distance.
   - Pass: the same shot shows no step (under 2 levels in any channel across the old edge), and a `dive` shot at the
     same spot still looks right.
2. **The tide is visible.** The tide (`KeysGeo::TideLevel()`, plus or minus 0.32 m) changes depth and fishing but the
   water surface does not move. The surface mesh lives in `KeysWorld` and wave heights in `KeysGeo`, both marshal-only.
   - Deliver: a patch request (`marshal-request` skill) that makes the drawn water level follow the tide, plus any
     change your own files need so the seafloor, schools and shore foam agree with it.
   - Pass: with your patch applied locally, shots at `tide=-1` and `tide=1` from
     `tp=-1150,452,4.71` with `cam=0,6,1.5` show the shoreline in different places; the patch file is committed and
     the hub files are untouched in your commits.
3. **Bait balls confirmed on screen.** Bait was recently increased in every zone and added offshore (5 to 14 m deep),
   checked by counts only.
   - Pass: one underwater screenshot at the reef (`goto=reef`, `dive`) and one offshore (`goto=offshore`, `dive`)
     that each clearly show a bait ball, with the `fish:stats` line from the same run. If a ball is hard to find or
     looks wrong (for example bright green dots from above), fix it in `KeysSchools` and show before and after.
4. **No slowdown.** `perf-check` at the flat and at the reef: within 5% of the numbers before your changes.

## Agents and skills

- Do the work as `sea-life`; check each criterion as `verifier` before you tick it.
- Skills: `build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`.
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first.

## Dependencies

None. You merge first. The `mangroves` lane runs at the same time in another worktree and shares the machine: only
one game runs at once, so a scripted run may wait its turn.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), and never push anything.

You are the `reefreal` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the real Florida Keys. You
know nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-reefreal` (run every command from inside it)
- Branch: `lane/reefreal`
- Read first: `MARSHAL.md` (the last log entries), `.claude/agents/sea-life.md`, `.claude/agents/verifier.md`, and
  the skills in `.claude/skills/` (`build`, `scripted-run`, `perf-check`, `lane-status`, `marshal-request`,
  `reef-creature`).

## Goal

Take out of the game what does not live in the Florida Keys, and check the rest. The user asked for "a realism
cleanup but dont get rid of the black marlin". The user's standing rule is "i just want to keep it realistic"
(the real Florida Keys). The criteria are the marshal's reading: say so in your status file and list what you
had to guess.

## What is already there

- Catchable species: `KeysData.cpp` (`AllSpecies()`, 56 rows: key, name, weights, strength, score, colour,
  fact text; `ZoneTable()` weights per zone). Visible kinds: `KeysLife.cpp` (`LifeVisTable`, `LifeShapeOf`,
  `LifeZoneLife`, the model `Kin` map). Schools: `KeysSchools.cpp`. Reef life: `KeysReefLife*.cpp`. Mangrove
  animals: `KeysMangroveEco*.cpp`. The save keeps catch records by species key.
- Known not to belong: **Atlantic halibut** (`halibut`, a cold-water fish); **southern flounder** (`flounder`:
  its range ends north of the Keys; the gulf flounder, *Paralichthys albigutta*, is the local one); **kelp**
  in the "Abyssal Garden" dive site (`KeysGarden.*`), which does not grow in tropical water.
- **Black marlin stays**, at the user's word, although it is a Pacific fish: leave it exactly as it is and say
  so in the audit.
- The hogchoker sole, the great white, the whale shark, the mola, the humpback and sperm whales, the giant
  manta, the sawfish, the bluefin and the albacore are doubtful or seasonal: judge each (some do pass through
  the Keys) and say what you found; change only what is plainly wrong.

## Owned paths

- `Source/KeysRun/KeysGarden.cpp`, `KeysGarden.h`
- `.marshal/status/reefreal.md`, `.marshal/requests/reefreal/`

Everything else is read-only, including `KeysData`, `KeysLife`, `KeysSchools`, the save and the HUD: changes
there are numbered patch requests (apply locally to test, never commit). Other lanes are running in
`KeysReef*`, `KeysMangroveEco*` and new audio files: do not touch theirs.

## Deliverables and acceptance criteria

1. **The audit.** A table of every animal and plant kind in the game (all the systems above, and the birds,
   corals and sponges), each with: the name shown to the player, whether it lives in the Keys (yes / seasonal
   or rare / no), whether its size, colour and fact text are right, and how you know. No outside lookups are
   required; if you fetch anything, carry nothing about the user or the project and say what you read.
2. **Halibut retired.** As a patch: it can no longer be hooked, seen or listed; a save that holds halibut
   catches, records or a mounted one loads with no error and keeps its history (shown under its old name);
   every table that weighted it is re-balanced so the zone's total and the other fishes' shares stay as they
   were. Test with the game's own test save (`-KRTestSave`), never the user's.
3. **Flounder made local.** As a patch: the flounder becomes the gulf flounder (name, fact text, size range,
   the three eye-spots on its back), keeping its key so saves still match.
4. **No kelp in the Abyssal Garden.** In your own files: the kelp is replaced by what a deep Keys reef has
   (sea whips, black coral bushes, deep-water sea fans, large sponges), swaying as the kelp did, the site as
   dramatic as before and still 60 fps-safe. Before and after shots (`goto=garden` then `dive`).
5. **Other plain errors fixed**, each as a patch with a one-line reason; doubtful ones listed, not changed.
   The barracuda's catch card reads 32 in for 20 lb (another lane's note): check the length-for-weight of
   every catchable fish against life and fix the formula or the table where it is off.
6. **Nothing else moves.** `tpcheck` 0 bad on both maps; a fishing run in each zone still picks fish
   (`picks`); `perf-check` at the Abyssal Garden within 3% of before.

## Working conditions

- Check each criterion as `verifier` before you tick it (follow the agent briefs yourself if the agent types are
  not available).
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first.
  Check the build log for `Result: Succeeded` before any scripted run.
- One copy of the game runs at a time (`shoot.sh` takes a lock that expires after 20 minutes); other lanes test
  too, and the user may be playing a packaged copy of the game on this machine: a run may wait, and frame times
  may be disturbed (take them twice). Batch your shots. End every script with a `quit` step. Never kill a game
  process you did not start.
- The editor is capped at 30 fps (33.3 ms): read game, draw and GPU times. Never teleport inside a `perf=`
  sample (5 s).
- No packaged build. Buy and generate nothing. Delete only your own scratch files; never `rm` with a wildcard in
  a shared directory. Open and look at every screenshot you cite.
- Usage is limited and you can be cut off at any moment: keep runs few, commit early and often with the status
  file current.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never use a bare `git stash`, never
push anything, and send nothing about the user or the project to any outside service. End every commit message
with these two lines:

Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_015oSiK1WwZ5JkZ11PikjADc

---
name: marshal-request
description: Ask the marshal for a change to a marshal-only file in Keys Run (hub sources, build files, config, binary materials, shared pipeline code). Use instead of editing such a file yourself.
---

# Requesting a change to a marshal-only file

The marshal-only list is in `MARSHAL.md`. If your work needs one of those files changed:

1. Do not edit it, not even temporarily in a commit.
2. Add a numbered request under "Requests for the marshal" in `.marshal/status/<lane>.md`:
   the file, the exact change, and why your lane needs it.
3. When the change is more than a line or two, write it as a patch so the marshal can apply it as given:
   - make the edit in your working tree,
   - `git diff -- <file> > .marshal/requests/<lane>/R<n>.patch`,
   - `git checkout -- <file>` to put the file back,
   - commit the patch file and the status file.
4. Keep working on what does not depend on it. Design your side so the hook is one call: a public function in your own
   file that the hub calls (`AKeysWorld::BuildMangroves()` is the model), or a `DebugCmd(const FString&)` the test
   harness can route to.
5. If nothing else can move, set `Status: BLOCKED` and say so.

## What usually needs a request

- Spawning a new actor or calling a new builder at start-up (`KeysGameMode`, `KeysWorld`).
- A new test command (`KeysBoatPawn.cpp`).
- Ground, depth, channel or zone changes (`KeysGeo`).
- Chart or HUD text (`KeysHUD`), species and zone tables (`KeysData`), visible creatures (`KeysLife`).
- A material or map asset (`Content/KeysRun/Materials`, `Maps`): describe it; these are binary and cannot be merged.

Testing your side before the request lands: you may apply your own patch locally to run the game, as long as the
hub file is restored before you commit.

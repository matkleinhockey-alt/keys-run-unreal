---
name: build
description: Build the Keys Run editor target for the worktree you are in. Use after any C++ change and before every scripted run.
---

# Build

Run from anywhere inside your worktree:

```bash
"/Users/mat/Documents/Unreal Projects/KeysRun-devkit/build.sh"
```

- It builds the project you are standing in and exits non-zero on failure. It prints the errors and the result line.
- Always chain a run after it with `&&`. Launching the game on a failed build hangs on a rebuild prompt:

```bash
"/Users/mat/Documents/Unreal Projects/KeysRun-devkit/build.sh" && "/Users/mat/Documents/Unreal Projects/KeysRun-devkit/shoot.sh" mytag "3:play;10:shotclean;11:quit"
```

- The first build in a new worktree compiles everything and can take 15 minutes or more: run it in the background.
  Later builds take two to three minutes.
- Two builds at once queue up behind each other; that is expected.
- This is your lane's build only. The full integration build belongs to the marshal.

## Common errors

- `-Wshadow`: a local reuses the name of a field or outer variable. Rename the local.
- An incomplete type used by value in a header: hold it by pointer.
- A new `.cpp` is picked up automatically; there is no file list to edit. Do not touch `KeysRun.Build.cs`.

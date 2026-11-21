---
name: lane-status
description: The status and handoff file every Keys Run lane keeps on its branch. Use whenever you finish a piece of work, get blocked, need something from the marshal, or are ready for merge.
---

# Lane status file

Keep `.marshal/status/<lane>.md` on your branch and commit it with your work. The marshal reads it from your branch;
nobody reads your chat.

```markdown
# <lane>

Status: IN PROGRESS | BLOCKED | READY FOR MERGE
Updated: <date and time>
Branch: lane/<lane> at <short commit>

## Acceptance criteria
- [x] <criterion, as given in the kickoff prompt> - evidence: <screenshot path or log line>
- [ ] <criterion> - <what is missing>

## Done
- <what changed, in one line each, with the files>

## In progress
- <what you are on now>

## Blocked
- <what stops you and what would unblock it>

## Requests for the marshal
- R1 (open | done | declined): <file> - <the change you need and why> - patch: .marshal/requests/<lane>/R1.patch

## Notes for integration
- <anything the marshal must know: new test commands wanted, perf numbers, things you saw but did not own>
```

## Rules

- Tick a criterion only with evidence a person can open or grep.
- Report failures and unverified items as plainly as successes.
- Set `READY FOR MERGE` only when every criterion is ticked or explicitly handed to the marshal as a request, your
  build passes, and `git status` is clean. Then stop.
- Check before you stop: `git diff --name-only main...HEAD` lists only your owned paths.

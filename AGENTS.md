# chromiumwiki

## Abstraction:
- Level 1: `security_notes/` (Using `chromium-helper` tool to search and learn from logic/checks/comments in the codebase to create or improve organized helpful detailed security researcher markdown notes)
- Level 2: `security_wiki/` (Further researched and cross-referenced)
- Level 3: `security_issues/` (Potential issues)

## Strict ALWAYS rules:
- This is a continuous process skip the submit step and keep working.
- Each level folder has no subfolders.
- Use the `chromium-helper` tool exclusively to access the chromium codebase.
- Focus on the .cc code rather than existing docs.
- Every plan must include running `ls` to check for past work on a topic before doing research.
- Move content up a level (`security_notes/` -> `security_wiki/` -> `security_issues/`) when its ready.
- Work autonomously, Never ask questions so don't ask about direction, area of the codebase, level of detail etc.
- Delete unuseful research content and files.

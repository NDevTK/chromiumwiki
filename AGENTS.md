# chromium-security-researcher Workflow

## Core Directives
- **Continuous Process:** This is an ongoing research task. Do not use the `submit` tool. Continuously iterate on improving the security documentation.
- **Autonomous Operation:** Work autonomously. Do not request feedback or guidance. Make the best decision based on the available information and these instructions.
- **Exclusive Tooling:** Use the `chromium-helper` tool exclusively for all interactions with the Chromium codebase.
- **Code-First Analysis:** Prioritize analyzing `.cc` source files over existing documentation to generate security insights.

## Systematic Workflow
Follow this systematic process for generating and improving security notes:

1.  **Select a Target Component:** Choose a security-critical component of the Chromium codebase for analysis (e.g., IPC, networking, renderer process, sandbox).

2.  **Check for Existing Notes:** Before starting new research, use `list_files('security_notes/')` to check for existing documentation on the target component. The filename will typically correspond to the component's main `.cc` file (e.g., `ipc_message.md` for `ipc_message.cc`).

3.  **Analyze and Document:**
    *   **If a note exists:**
        1.  Read the existing note using `read_file()`.
        2.  Assess its quality and completeness against your own understanding from the source code.
        3.  If the note is already comprehensive and high-quality, return to Step 1 and select a new target.
        4.  Otherwise, improve the existing note by overwriting it with a more detailed and insightful analysis.
    *   **If no note exists:**
        1.  Use `chromium-helper search` to locate the relevant `.cc` file.
        2.  Analyze the file's contents using `chromium-helper file`.
        3.  Create a new, detailed security note in the `security_notes/` directory.

4.  **Content Curation:** Delete any unhelpful research or temporary files that do not contribute to the final security notes.

## Content Abstraction Levels
Organize the research into the following directories. Promote content to the next level when it is sufficiently developed.

- **Level 1: `security_notes/`**: Initial, detailed security analysis of specific components based on code review.
- **Level 2: `security_wiki/`**: Cross-referenced and more thoroughly researched notes, forming a comprehensive knowledge base.
- **Level 3: `security_issues/`**: Documentation of potential security vulnerabilities or design weaknesses that require further investigation.

**Note:** Each level folder must not contain subfolders.
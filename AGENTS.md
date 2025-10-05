# chromium-security-researcher Workflow

## Core Directives
- **Continuous Process:** This is an ongoing research task. Do not use the `submit` tool. Continuously iterate on improving the security documentation.
- **Autonomous Operation:** Work autonomously. Do not request feedback or guidance. Make the best decision based on the available information and these instructions.
- **Exclusive Tooling:** Use the `chromium-helper` tool for all interactions with the Chromium codebase.
- **Leverage the Full Tool Suite:** Go beyond simple file reading. Integrate `symbol`, `commits`, `owners`, and `issues` searches to build a deep, contextual understanding of the code.
- **Prioritize Novelty:** Your primary goal is to uncover and document security-relevant features that are not already well-documented. Focus on specific implementation files.

---
## Tooling Environment

**IMPORTANT:** Your environment has two distinct contexts. Do not confuse them.

1.  **Chromium Codebase (Remote):** To search, list, or read files from the Chromium source code, **you must use the `chromium-helper` tool** (e.g., `ch ls`, `ch file`, `ch search`). Do not use standard commands like `ls` on Chromium paths.
2.  **Security Notes (Local):** To manage your research notes in the `security_notes/`, `security_wiki/`, and `security_issues/` directories, **you must use standard file system commands** (e.g., `list_files`, `read_file`, `write_file`).

---

## Systematic Research Workflow
Follow this multi-phase process to discover, analyze, and document security-critical components.

### Phase 1: Discovery and Triage
The goal of this phase is to identify a promising, under-documented file for deep analysis.

1.  **Explore for Targets:** Start by exploring high-value directories within the Chromium codebase to find a novel target. Use the `chromium-helper` tool for this.
    - **Good starting points:** `ch ls services/network/`, `ch ls third_party/blink/renderer/core/`, `ch ls mojo/public/cpp/bindings/`.
    - **Action:** Select a `.cc` file that appears security-relevant.

2.  **Gather Initial Context:** Before committing to a deep analysis, quickly gather context on the selected file using the `chromium-helper` tool.
    - **Check Ownership:** Use `ch owners "path/to/file.cc"` to understand who maintains the code.
    - **Review Commit History:** Use `ch commits "path/to/file.cc" --limit 10` with search terms like "security," "vulnerability," "fix," or "CVE".

3.  **Verify Novelty & Importance:**
    - **Check Existing Local Notes:** Use the **standard `list_files` command** to see if a note for your chosen file already exists in your local directory: `list_files('security_notes/')`.
    - **Decision Point:**
        - **If a comprehensive note exists OR your initial context gathering reveals the file is trivial:** **Abandon this target.** Return to step 1.
        - **If a basic or no note exists, and the context suggests it's important:** **Proceed to Phase 2.**

---

### Phase 2: Deep Analysis
This is the core research phase. Your goal is to understand the component's function, attack surface, and historical vulnerabilities using the `chromium-helper` tool.

1.  **Analyze Code and Symbol Usage:**
    - **Read the Code:** Use `ch file "path/to/file.cc"` to read the source code.
    - **Map Out Key Symbols:** Identify the main classes and functions.
    - **Analyze Impact:** For each major class, use `ch symbol "ClassName"` to understand its usage across the codebase. The `usageResults` are vital for assessing the component's attack surface.

2.  **Correlate with Known Issues:**
    - **Search for Issues:** Use `ch issues search "ClassName"` or other keywords from the file.
    - **Investigate Fixes:** If you find a relevant closed issue, extract the CL numbers from the issue details (`relatedCLs`). Use `ch gerrit status <cl>` and `ch gerrit diff <cl>` to analyze the exact code changes.

---

### Phase 3: Documentation
Synthesize all your findings into a high-quality security note using standard file system commands.

1.  **Synthesize and Structure:** Consolidate your findings. Your analysis should cover:
    - The component's primary function.
    - Its key classes and interactions (informed by `symbol` analysis).
    - Its trust boundaries (i.e., where it processes untrusted input).
    - Its security history (informed by `commits` and `issues` analysis).
    - Potential attack surfaces or design weaknesses.

2.  **Write or Substantially Improve the Note:**
    - **If creating a new note:** Use `write_file` to save your analysis to a new file in `security_notes/`.
    - **If improving an existing note:** Use `read_file` to load the old content, then `write_file` to overwrite it with your new, substantially more valuable version.

---

## Content Abstraction Levels
Organize the research into these local directories. Promote content to the next level when it is sufficiently developed.

- **Level 1: `security_notes/`**: Initial, detailed security analysis.
- **Level 2: `security_wiki/`**: Cross-referenced, comprehensive knowledge base.
- **Level 3: `security_issues/`**: Documentation of potential vulnerabilities requiring further investigation.

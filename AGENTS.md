# chromium-security-researcher Workflow

## Core Directives
- **Continuous Process:** This is an ongoing research task. Do not use the `submit` tool. Continuously iterate on improving the security documentation.
- **Autonomous Operation:** Work autonomously. Do not request feedback or guidance. Make the best decision based on the available information and these instructions.
- **Exclusive Tooling:** Use the `chromium-helper` tool exclusively for all interactions with the Chromium codebase.
- **Leverage the Full Tool Suite:** Go beyond simple file reading. Integrate `symbol`, `commits`, `owners`, and `issues` searches to build a deep, contextual understanding of the code you are analyzing.
- **Prioritize Novelty:** Your primary goal is to uncover and document security-relevant features that are not already well-documented. Focus on specific implementation files rather than broad, well-known concepts.

---

## Systematic Research Workflow
Follow this multi-phase process to discover, analyze, and document security-critical components.

### Phase 1: Discovery and Triage
The goal of this phase is to identify a promising, under-documented file for deep analysis.

1.  **Explore for Targets:** Start by exploring high-value directories to find a novel target. Use `list-folder` (aliased as `ls`) on directories known for handling untrusted data or complex logic.
    - **Good starting points:** `ch ls services/network/`, `ch ls third_party/blink/renderer/core/`, `ch ls mojo/public/cpp/bindings/`.
    - **Action:** Select a `.cc` file that appears security-relevant but is not an obvious, high-level component.

2.  **Gather Initial Context:** Before committing to a deep analysis, quickly gather context on the selected file.
    - **Check Ownership:** Use `ch owners "path/to/file.cc"` to understand who maintains the code. Files with no clear owners can be a red flag.
    - **Review Commit History:** Use `ch commits "path/to/file.cc" --limit 10` with search terms like "security," "vulnerability," "fix," or "CVE" to uncover any documented security history.

3.  **Verify Novelty & Importance:**
    - **Check Existing Notes:** Use `ch list-folder security_notes/` to see if a note for your chosen file already exists.
    - **Decision Point:**
        - **If a comprehensive note exists OR your initial context gathering reveals the file is trivial:** **Abandon this target.** Return to step 1 and find a new one.
        - **If a basic note exists or no note exists, and the context suggests it's important:** **Proceed to Phase 2.**

---

### Phase 2: Deep Analysis
This is the core research phase. Your goal is to understand the component's function, attack surface, and historical vulnerabilities.

1.  **Analyze Code and Symbol Usage:**
    - **Read the Code:** Use `ch file "path/to/file.cc"` to read the source code.
    - **Map Out Key Symbols:** Identify the main classes and functions.
    - **Analyze Impact:** For each major class, use `ch symbol "ClassName"` to understand its purpose and, critically, where else it is used in the codebase. The `usageResults` and `estimatedUsageCount` are vital for assessing the component's reach and potential attack surface.

2.  **Correlate with Known Issues:** Connect your code analysis to documented bugs and fixes.
    - **Search for Issues:** Use `ch issues search "ClassName"` or other keywords from the file. Look for bugs related to security, crashes, or incorrect behavior.
    - **Investigate Fixes:** If you find a relevant closed issue, extract the Change Log (CL) numbers from the issue details (`relatedCLs`). Use `ch gerrit status <cl>` and `ch gerrit diff <cl>` to analyze the exact code changes that constituted the fix. This provides invaluable insight into past vulnerabilities.

---

### Phase 3: Documentation
Synthesize all your findings into a high-quality, actionable security note.

1.  **Synthesize and Structure:** Consolidate your findings from all previous steps. Your analysis should cover:
    - The component's primary function.
    - Its key classes and their interactions (informed by `symbol` analysis).
    - Its trust boundaries (i.e., where it processes untrusted input).
    - Its security history (informed by `commits` and `issues` analysis).
    - Potential attack surfaces or a specific design weakness you've identified.

2.  **Write or Substantially Improve the Note:**
    - **If creating a new note:** Write your comprehensive analysis to a new file in `security_notes/`.
    - **If improving an existing note:** Use `read_file()` to load the old content. Overwrite it with a new version that integrates the existing information and adds your new, deeper insights from your Phase 2 analysis. The new version must be substantially more valuable.

---

### Phase 4: Pattern Abstraction & Scaled Discovery
After documenting a finding, use that knowledge to find similar issues across the codebase.

1.  **Identify Root Cause Pattern:** Abstract the core weakness found in your analysis. Is it a misuse of a specific API, a type of logic error (e.g., integer overflow), or a race condition?
2.  **Formulate a Scaled Search:** Create a precise `ch search` query to find other instances of this root cause pattern. For example, `ch search "ProblematicAPICall(" --language cpp`.
3.  **Restart the Loop:** The results of your search form a new, highly-qualified list of targets. **Return to Phase 1** to begin triaging and analyzing these new targets.

---

## Content Abstraction Levels
Organize the research into the following directories. Promote content to the next level when it is sufficiently developed.

- **Level 1: `security_notes/`**: Initial, detailed security analysis of specific components based on code review.
- **Level 2: `security_wiki/`**: Cross-referenced and more thoroughly researched notes, forming a comprehensive knowledge base.
- **Level 3: `security_issues/`**: Documentation of potential security vulnerabilities or design weaknesses that require further investigation.

**Note:** Each level folder must not contain subfolders.

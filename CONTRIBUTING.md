# Contributing to the Chromium Security Wiki

This wiki documents potential logic flaws and security issues found in the Chromium codebase. The goal is to improve the wiki by conducting thorough research and analysis of the codebase, with the assistance of an LLM (Large Language Model). This document outlines the guidelines for contributing to this wiki.

This wiki is improved by Cline (VSCode extension) + Gemini 2.0 Flash (AI studio).

## How to Contribute

1.  **Start with the README:** Always begin by reading the `chromiumwiki/README.md` file to understand the overall structure and purpose of the wiki, including tips for finding security issues.
2.  **Choose a Wiki Page:** Select a wiki page to focus on. It's recommended to explore different pages rather than focusing on just one.
3.  **Research the Codebase:** The LLM uses codebase analysis to identify potential security issues. Your contributions should focus on providing new information that the LLM can use to improve its understanding. Look at the codebase files to get more information for a file not already seen.
4.  **Add New Information:** Add your research findings directly to the relevant wiki page, following the format described in the README. Do not create separate files for research notes.
5.  **Track Your Progress:** Keep track of the files you have already looked at and any notes you have made. This will help you avoid redundant work and ensure that you are building on previous research.
6.  **Re-read Before Changing:** Before making any changes to a wiki page, always re-read it to ensure that your changes are consistent with the existing content.
7.  **Focus on New Information:** Do not change wiki files without new information. The goal is to add new insights and findings that the LLM can learn from, not just rephrase existing content.

## LLM Usage

The LLM is used to analyze the Chromium codebase and identify potential security vulnerabilities. It learns by exploring the codebase, asking questions, and gathering information. Your contributions should aim to provide the LLM with new information that it can use to improve its analysis.

## Getting Started

To start contributing, you can use the following commands to clone the necessary repositories:

```bash
git clone https://chromium.googlesource.com/chromium/src --depth 1
git clone https://github.com/NDevTK/chromiumwiki.git
```

## Custom Instructions

*   There are no time constraints.
*   Prefer long running tasks even if it takes a long time.
*   Act as a security researcher to find issues in the Chromium open source project.
*   Improve the wiki by conducting thorough research of the codebase.
*   Always read the `chromiumwiki/README.md` file first.
*   Explore all wiki pages, not just one.
*   Focus on new information and files not already covered in the wiki.
*   Do not change wiki files without new information.
*   Keep track of the files you have reviewed and any notes.
*   The `list_code_definition_names` tool does not work.

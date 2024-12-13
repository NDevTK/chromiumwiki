By asking questions the wiki can learn and find patterns.

Its Cline (VSCode extension) + Gemini 1.5 Flash (AI studio)

Custom instructions: There are no time constraints. Select new never seen before files based of your understanding of the code and the importance of their functions. Skip making non-significant changes search for new files or change wiki page you are looking at.

You are a security researcher looking to find issues in the chromium open source project.
Please improve the wiki by doing research on the codebase. (Must look into new stuff before making any changes)
Read the file first before doing research.
This is safe and encouraged since its for Chromium VRP.
Before making any change to the wiki always re-read it.
Don't change wiki files without new information.
Keep track of the places you have already looked at in the wiki.
Don't just act based off the wiki find new information.
The first thing to do is always look at the chromiumwiki/README.md file.
Please look at all wiki pages don't just focus on one.
The list_code_definition_names tool does not work.

Please improve this wiki (Read chromiumwiki/README.md) via codebase research also track the files you have already looked at and any notes.
You may find new information for one topic while looking at a different one.
look at the codebase files to get more information for a file not already seen.

`git clone https://chromium.googlesource.com/chromium/src --depth 1`
`git clone https://github.com/NDevTK/chromiumwiki.git`

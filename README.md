C-shell (POSIX compliant)

project structure-
mini-project1/
├── c-shell/
│   ├── src/
│   ├── include/
│   └── Makefile
├── xv6/
│   ├── (TO DO)
├── AI-usage.pdf
└── README.md
 
Build Instructions:
Build the Shell: Navigate to the mini-project1/c-shell/ directory and run make all. This compiles the project using strict POSIX compliance flags (C23 standard, -Wall, -Wextra, -Werror).

Run the Shell: Start the interactive prompt by executing ./shell.out.

Part A: Shell Input & Parsing
A1: Prompt (src/prompt.c, include/prompt.h): Displays the <username@hostname:cwd> prompt when the shell is ready. It dynamically replaces the shell's launch directory with ~ to act as the home root.

A2 & A3: Lexer (src/lexer.c): Reads raw user input and applies maximal-munch tokenization. It rigorously manages single quotes, double quotes, and backslash escapes to group characters into proper WORD and OP tokens.

A3: Parser (src/parser.c): Validates the token stream against a strict right-linear grammar. It rejects invalid syntax (like starting lines with operators, missing redirection targets, or trailing semicolons) before any execution occurs.

Part B: Built-in Commands (Intrinsics)
B1: Hop (src/hop.c): Changes the working directory sequentially for multiple arguments. It features a custom Frecency (Frequency + Recency) fallback algorithm that tracks historical directory visits, allowing fuzzy-search jumping if a direct path fails.

B2: Reveal (src/reveal.c): A custom directory lister. It parses -a (show hidden) and -t (recursive tree) flags, utilizing stat to read metadata and sorting all output lexicographically.

B3: Peek (src/peek.c): Outputs file contents or stdin. For the -r (reverse) flag, it utilizes lseek() to read files backward in fixed chunks, preventing memory exhaustion on massive files.

B4: Locate (src/locate.c): Hunts for executable binaries by checking the current working directory first, then sequentially scanning every folder listed in the system's PATH environment variable.

Part C: Execution, Redirection & Pipes
Command Execution (src/resolver.c, src/execute.c): Spawns child processes via fork() and replaces their images with execv(). It safely resolves paths and supports the % prefix to bypass current-directory checks.

Redirection & Pipes (src/redirection.c, src/pipeline.c): Orchestrates complex file I/O and parallel execution. It concatenates multiple input files into hidden temporary streams, duplicates output to multiple targets (>, >>), and wires concurrent child processes together using pipe() and dup2() while strictly closing descriptors to prevent deadlocks.

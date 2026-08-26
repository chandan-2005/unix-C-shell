C-shell (POSIX compliant)
Project structure-
mini-project1/
├── c-shell/
│   ├── src/
│   ├── include/
│   └── Makefile
├── xv6/ (TO DO)
│   ├── (TO DO)
├── AI-usage.pdf
└── README.md
A Unix shell written from scratch in C, as part of an OS & Networks mini-project
 Every time you press Enter: tokenizing your input, checking it makes grammatical sense, then wiring together fork, exec, pipe, and dup2 to actually run something.
Build Instructions:
Build the Shell: Navigate to the mini-project1/c-shell/ directory and run make all. This compiles the project using strict POSIX compliance flags (C23 standard, -Wall, -Wextra, -Werror).

Run the Shell: Start the interactive prompt by executing ./shell.out.

Part A: Shell Input & Parsing
A1: Prompt (src/prompt.c, include/prompt.h): Displays the <username@hostname:cwd> prompt when the shell is ready. It dynamically replaces the shell's launch directory with ~ to act as the home root.

A2 & A3: Lexer (src/lexer.c): Reads raw user input and applies maximal-munch tokenization. It rigorously manages single quotes, double quotes, and backslash escapes to group characters into proper WORD and OP tokens.

A3: Parser (src/parser.c): Validates the token stream against a strict right-linear grammar. It rejects invalid syntax (like starting lines with operators, missing redirection targets, or trailing semicolons) before any execution occurs.

Part B: Built-in Commands (Intrinsics)
* **`reveal`**: My custom take on `ls`. It reads directory streams and pulls detailed file metadata (permissions, links, sizes) using `stat`. It gracefully handles hidden files and directory traversal.
* **`peek`**: A highly robust file reader (similar to `cat` or `tac`). 
  * Prints file contents forward, with an optional `-n` flag for line numbering (ignoring empty lines).
  * **The `-r` flag (Reverse):** This was one of the hardest parts. Instead of cheating by loading the whole file into an array, it uses `lseek()` to jump to the end of the file and reads it *backwards* in 4KB chunks, scanning for newlines. For non-seekable input (like pipes), it safely buffers in memory.
* **`locate`**: Similar to `which -a`. It checks the current working directory first, then iterates through every directory in the system's `PATH` environment variable to find exactly where an executable lives.
B1: Hop (src/hop.c): Changes the working directory sequentially for multiple arguments. It features a custom Frecency (Frequency + Recency) fallback algorithm that tracks historical directory visits, allowing fuzzy-search jumping if a direct path fails.

B2: Reveal (src/reveal.c): A custom directory lister. It parses -a (show hidden) and -t (recursive tree) flags, utilizing stat to read metadata and sorting all output lexicographically.

B3: Peek (src/peek.c): Outputs file contents or stdin. For the -r (reverse) flag, it utilizes lseek() to read files backward in fixed chunks, preventing memory exhaustion on massive files.

B4: Locate (src/locate.c): Hunts for executable binaries by checking the current working directory first, then sequentially scanning every folder listed in the system's PATH environment variable.

Part C: Execution, Redirection & Pipes
Command Execution (src/resolver.c, src/execute.c): Spawns child processes via fork() and replaces their images with execv(). It safely resolves paths and supports the % prefix to bypass current-directory checks.

Redirection & Pipes (src/redirection.c, src/pipeline.c): Orchestrates complex file I/O and parallel execution. It concatenates multiple input files into hidden temporary streams, duplicates output to multiple targets (>, >>), and wires concurrent child processes together using pipe() and dup2() while strictly closing descriptors to prevent deadlocks.

How the code is organized:

File    What it's responsible for
main.c	The read–lex–parse–execute loop
prompt.c / prompt.h	Building and printing the user@host:path prompt, tracking the home directory
lexer.c / lexer.h	Turning a raw input line into a linked list of tokens, handling quotes and escapes
parser.c / parser.h	Checking the token list against the shell's grammar before anything runs
execute.c / execute.h	Looking at the first token and deciding: builtin, or hand off to external execution?
hop.c / hop.h	The hop builtin, including the persistent frecency database
reveal.c / reveal.h	The reveal builtin
peek.c / peek.h	The peek builtin
locate.c / locate.h	The locate builtin
resolver.c / resolver.h	Turning a bare command name into a real, executable path (checking /, cwd, %, and PATH)
pipeline.c / pipeline.h	Splitting a command line on `
redirection.c / redirection.h	Opening the right files in the right mode for <, >, and >>, including fan-in/fan-out for multiple files
external.c / external.h	Thin entry point that hands external commands off to the pipeline machinery
# cshell

## What it does
type a line → lex it into tokens → validate the grammar → run it

- **Lexing**: turns raw text into a stream of tokens (`WORD`, `|`, `&`, `;`, `<`, `>`, `>>`), correctly handling single quotes, double quotes, and backslash escapes the way a POSIX shell would.
- **Parsing**: checks the token stream against a small right-linear grammar before anything is allowed to run, so `cat |` or `echo hi ;` get rejected with a clear error instead of doing something weird.
- **Execution**: dispatches to one of four built-in commands, or — if it's not a builtin — resolves it against `PATH` (or the current directory), forks, sets up any pipes/redirections, and executes it.

## Building it

```bash
cd c-shell
make all
```

This produces `shell.out` in the same directory. `make clean` removes it along with all the intermediate `.o` files.

The build is intentionally strict — it compiles with:

```
-std=c23 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm
```
## Running it

```bash
./shell.out
```
You'll get a prompt like:

```
<yourname@yourhost:~>
```

The directory you launch `shell.out` from becomes its "home" (`~`). Wander into subdirectories and the prompt will show `~/wherever/you/are`; wander outside your home entirely and it falls back to showing the full absolute path.

Exit with `Ctrl+D` (EOF).

## The built-in commands

### `hop` — smarter directory navigation

```
hop                  # go home
hop ~                # go home
hop .                # do nothing (stay put)
hop ..                # go up one level
hop -                # go back to wherever you were before the last hop
hop some/relative/path
hop /an/absolute/path
```

### `reveal` — list directory contents

```
reveal            # list the current directory
reveal ~          # list home
reveal -a         # include hidden (dotfile) entries
reveal -t         # recurse into subdirectories
reveal -ta        # both, combined
reveal somedir    # list a specific directory
```

Flags can be smashed together or repeated in any combination (`-ta`, `-ttaaaa`, whatever) — it just cares whether `a` or `t` ever appeared. Output is always sorted, plain ASCII/lexicographic order, same as you'd expect from `ls`. With `-t`, each subdirectory's contents are printed immediately after the subdirectory itself (not batched at the end), and nested entries are shown as paths relative to whatever you asked to reveal — so digging into `include/` shows up as `include/parser.h`, not just `parser.h`.

### `peek` — read file contents, forwards or backwards

```
peek file.txt              # print it, same order as it's stored
peek -n file.txt           # prefix each non-empty line with its line number
peek -r file.txt           # print it in reverse line order
peek -rn file.txt          # both together
peek file1.txt file2.txt   # concatenate multiple files
peek                       # read from stdin
peek -                     # also reads from stdin, explicitly
```

The reverse mode is the neat bit: for a real file, it doesn't load the whole thing into memory and reverse it — it seeks backward through the file in fixed-size chunks, scanning for line breaks as it goes. For a pipe or stdin (which you can't seek backward through), it falls back to buffering the input first, since there's no other way to know where the end is.

### `locate` — find where a command actually lives

```
locate python       # prints every matching executable, cwd first, then each PATH directory in order
locate python nada   # handles multiple names, one per line of output
```

This is basically "show me what `which` would find, but check every match instead of stopping at the first one, and check the current directory too." If a name doesn't resolve to anything runnable, it prints `locate: command not found (name)` and keeps going with whatever else you asked for.
```

### Redirection

```bash
cat < input.txt                    # read stdin from a file
cat < a.txt < b.txt                # concatenate a.txt then b.txt as one input stream
echo hi > out.txt                  # overwrite out.txt
echo hi >> log.txt                 # append to log.txt
echo hi > out1.txt >> out2.txt     # write the same output to both, independently
```

Sequencing (`;`) and backgrounding (`&`) are recognized by the grammar and get validated correctly, but only the first command in the line is currently executed — running everything after a `;` or launching a true background job is on the roadmap, not yet wired up.

## How the code is organized
| File | What it's responsible for |
|---|---|
| `main.c` | The read–lex–parse–execute loop |
| `prompt.c` / `prompt.h` | Building and printing the `user@host:path` prompt, tracking the home directory |
| `lexer.c` / `lexer.h` | Turning a raw input line into a linked list of tokens, handling quotes and escapes |
| `parser.c` / `parser.h` | Checking the token list against the shell's grammar before anything runs |
| `execute.c` / `execute.h` | Looking at the first token and deciding: builtin, or hand off to external execution? |
| `hop.c` / `hop.h` | The `hop` builtin, including the persistent frecency database |
| `reveal.c` / `reveal.h` | The `reveal` builtin |
| `peek.c` / `peek.h` | The `peek` builtin |
| `locate.c` / `locate.h` | The `locate` builtin |
| `resolver.c` / `resolver.h` | Turning a bare command name into a real, executable path (checking `/`, cwd, `%`, and `PATH`) |
| `pipeline.c` / `pipeline.h` | Splitting a command line on `|`, wiring up pipes between stages, forking each one |
| `redirection.c` / `redirection.h` | Opening the right files in the right mode for `<`, `>`, and `>>`, including fan-in/fan-out for multiple files |
| `external.c` / `external.h` | Thin entry point that hands external commands off to the pipeline machinery |

The token stream itself is a simple singly-linked list (`Token *`) rather than a full parse tree — the grammar is small enough that a flat list carries all the information execution needs.

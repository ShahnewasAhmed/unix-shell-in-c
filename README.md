# CSH — A Small Unix Shell in C

CSH is a Unix command-line shell written from scratch in C11 against the POSIX API. It reads a line, splits it into tokens, parses the tokens into an abstract syntax tree (AST), and runs that tree using `fork`, `execvp`, `pipe`, `dup2` and `waitpid`.

It supports pipelines, conditional chaining (`&&`, `||`), command sequencing (`;`), background jobs (`&`), I/O redirection (stdin, stdout, stderr, both truncate and append), quoted arguments, and a set of built-in commands.

```
$ ./csh
Welcome to CSH!
CSH> echo hello | tr a-z A-Z
HELLO
CSH> false || echo "fallback ran"
fallback ran
CSH> sleep 5 &
[1] 48213
CSH> jobs
[1] 48213
CSH> exit
```

---

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Building](#building)
- [Running](#running)
- [Command Syntax](#command-syntax)
  - [Simple commands and quoting](#simple-commands-and-quoting)
  - [Operators](#operators)
  - [Redirections](#redirections)
  - [Grammar](#grammar)
  - [Exit status](#exit-status)
- [Built-in Commands](#built-in-commands)
- [Background Jobs](#background-jobs)
- [Architecture](#architecture)
  - [Processing pipeline](#processing-pipeline)
  - [Lexer](#lexer)
  - [Parser and AST](#parser-and-ast)
  - [Executor](#executor)
  - [Signals](#signals)
- [Project Structure](#project-structure)
- [Public API Reference](#public-api-reference)
- [Testing](#testing)
- [Debugging and Memory Checking](#debugging-and-memory-checking)
- [Known Limitations](#known-limitations)
- [Development History](#development-history)

---

## Features

| Feature | Syntax | Notes |
|---|---|---|
| External commands | `ls -l /tmp` | Resolved through `PATH` with `execvp` |
| Pipelines | `a \| b \| c` | Any number of stages; status is that of the last stage |
| Logical AND | `a && b` | `b` runs only if `a` exits with 0 |
| Logical OR | `a \|\| b` | `b` runs only if `a` exits non-zero |
| Sequencing | `a ; b` | Runs `a`, then `b` |
| Background jobs | `a &` | Prints `[job_id] pid`, does not wait |
| Input redirection | `cmd < file` | |
| Output redirection | `cmd > file`, `cmd >> file` | Truncate / append, created with mode `0644` |
| Stderr redirection | `cmd 2> file`, `cmd 2>> file` | Truncate / append |
| Quoting | `"two words"`, `'two words'` | Keeps whitespace and operators inside one argument |
| Built-ins | `cd`, `pwd`, `export`, `unset`, `jobs`, `exit` | Run inside the shell process; redirections work on them |
| Job reaping | — | Finished background children are reaped via `SIGCHLD`, so no zombies are left behind |

---

## Requirements

- A POSIX system (developed on Linux)
- `gcc` (or another C11 compiler; change `CC` in the `Makefile`)
- `make`
- Optional: `valgrind`, `strace`, and a compiler with AddressSanitizer support for debugging

---

## Building

```sh
make          # build ./csh
make clean    # remove object files, dependency files and binaries
make re       # clean, then build again
make debug    # build ./csh-debug with AddressSanitizer + UndefinedBehaviorSanitizer
```

Compiler flags (from the `Makefile`):

```
-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -g
```

`-Werror` is on, so any warning fails the build. The Makefile finds every `.c` file under `src/` automatically and generates header dependency files (`*.d`) with `-MMD -MP`, so a header change rebuilds only the files that include it.

---

## Running

```sh
./csh
```

The shell prints `Welcome to CSH!` and then shows the `CSH> ` prompt. It reads one line at a time from standard input until:

- you run `exit`, or
- standard input reaches end-of-file (press **Ctrl-D**).

Empty lines are ignored. Because CSH reads from stdin, you can also pipe a script into it:

```sh
printf 'echo one\necho two | rev\n' | ./csh
```

---

## Command Syntax

### Simple commands and quoting

A command is a program name followed by its arguments, separated by whitespace (spaces, tabs, newlines):

```
CSH> ls -la /usr/bin
```

A word wrapped in double quotes `"..."` or single quotes `'...'` becomes one argument with the quotes removed. Anything inside the quotes is kept as-is, including spaces and operator characters:

```
CSH> echo "a | b ; c"
a | b ; c
CSH> grep 'hello world' notes.txt
```

Both kinds of quote behave the same way: there is no variable expansion and no escape characters inside either one. See [Known Limitations](#known-limitations) for quoting rules.

### Operators

| Operator | Name | Meaning |
|---|---|---|
| `\|` | Pipe | Connects the stdout of the left command to the stdin of the right command |
| `&&` | AND | Runs the right side only if the left side succeeded (status 0) |
| `\|\|` | OR | Runs the right side only if the left side failed (status ≠ 0) |
| `;` | Sequence | Runs the left side, then the right side, whatever the result |
| `&` | Background | Runs the preceding command in the background |

**Precedence**, from tightest to loosest:

1. Redirections (bind to a single command)
2. `|`
3. `&&` and `||` (same level, left-associative)
4. `;` and `&`

For example:

```
a | b && c || d ; e
```

is parsed as:

```
SEQUENCE
├── OR
│   ├── AND
│   │   ├── PIPELINE(a, b)
│   │   └── c
│   └── d
└── e
```

### Redirections

| Syntax | File descriptor | Open flags |
|---|---|---|
| `< file` | stdin (0) | `O_RDONLY` |
| `> file` | stdout (1) | `O_WRONLY \| O_CREAT \| O_TRUNC` |
| `>> file` | stdout (1) | `O_WRONLY \| O_CREAT \| O_APPEND` |
| `2> file` | stderr (2) | `O_WRONLY \| O_CREAT \| O_TRUNC` |
| `2>> file` | stderr (2) | `O_WRONLY \| O_CREAT \| O_APPEND` |

New files are created with mode `0644`. A redirection can go anywhere in a command, and a command can have more than one. They are applied left to right:

```
CSH> sort < input.txt > output.txt 2> errors.log
CSH> > out.txt echo hi            # also valid
```

`2>` is only treated as a stderr redirection when `2` starts a new word. `echo 12>x` passes the argument `12` and redirects stdout to `x`.

In a pipeline, each stage can have its own redirections. They are applied after the pipe is connected, so an explicit redirection wins over the pipe:

```
CSH> cat < in.txt | sort | uniq > out.txt
```

### Grammar

The parser is recursive descent and accepts this grammar:

```
line        := list EOF
list        := and_or ( (';' | '&') and_or )* [ ';' | '&' ]
and_or      := pipeline ( ('&&' | '||') pipeline )*
pipeline    := command ( '|' command )*
command     := ( WORD | redirection )+          -- needs at least one WORD
redirection := ( '<' | '>' | '>>' | '2>' | '2>>' ) WORD
```

Input that does not match the grammar (for example `| ls`, `ls &&`, `ls >`, `a ;; b`, or a redirection with no command) is rejected, and the line is skipped.

### Exit status

Every node in the AST returns an integer status:

| Situation | Status |
|---|---|
| Program exits normally | its exit code |
| Program killed by signal *N* | `128 + N` |
| Command not found / `execvp` failed | `127` |
| Redirection could not be opened | `1` |
| Pipeline | status of the **last** stage |
| `a && b`, `a \|\| b` | status of the last command that ran |
| `a ; b` | status of `b` |
| `a &` | `0` once the job has started |
| Built-in | `0` on success, `1` on error |

The status is used internally for `&&` and `||`. There is no `$?` variable.

---

## Built-in Commands

Built-ins run inside the shell process, so they can change the shell's own state (working directory, environment). Redirections work on them: the shell saves stdin/stdout/stderr with `dup`, applies the redirections, runs the built-in, then restores the original descriptors.

When a built-in is used as a pipeline stage or in the background, it runs in a child process. This means `cd /tmp | cat` or `cd /tmp &` does **not** change the shell's directory, which is the same as in other shells.

| Command | Usage | Description |
|---|---|---|
| `cd` | `cd [dir]` | Change the working directory. More than one argument is an error. |
| `pwd` | `pwd` | Print the current working directory. Takes no arguments. |
| `export` | `export NAME=value [NAME2=value2 ...]` | Set one or more environment variables, which child processes inherit. Needs at least one `NAME=value`. |
| `unset` | `unset NAME [NAME2 ...]` | Remove one or more environment variables. |
| `jobs` | `jobs` | List background jobs that are still tracked, as `[job_id] pid`. |
| `exit` | `exit` | Exit the shell with status 0. Takes no arguments. |

**Variable names** for `export` and `unset` must start with a letter or `_`, and contain only letters, digits and `_`. Anything else is rejected:

```
CSH> export GREETING=hello
CSH> env | grep GREETING
GREETING=hello
CSH> export 1BAD=x
export: invalid variable name: 1BAD
CSH> unset GREETING
```

---

## Background Jobs

Ending a command with `&` starts it in a child process and returns to the prompt right away:

```
CSH> sleep 30 &
[1] 51234
CSH> sleep 60 | cat &
[2] 51240
CSH> jobs
[2] 51240
[1] 51234
```

How it works:

1. `execute_background` forks. The child runs the wrapped sub-tree (which may be a pipeline or an `&&`/`||` chain) and exits with its status.
2. The parent records the child in a linked list of `BackgroundProcess { pid, job_id, next }`, prints `[job_id] pid`, and returns `0`.
3. Job IDs start at 1 and keep counting up for the life of the shell. They are not reused.
4. When a child exits, the `SIGCHLD` handler sets a flag. Before each prompt, the shell loop sees the flag and calls `reap_background_processes()`. That function calls `waitpid(pid, ..., WNOHANG)` for each tracked job and removes the ones that have finished.

Finished jobs are removed quietly. No "Done" message is printed. `jobs` lists the newest job first.

---

## Architecture

### Processing pipeline

```
          ┌──────────┐   char*   ┌─────────┐ TokenList ┌─────────┐ ASTNode* ┌──────────┐
 stdin ──▶│  shell   │──────────▶│  lexer  │──────────▶│ parser  │─────────▶│ executor │──▶ fork/exec
          │  loop    │           └─────────┘           └─────────┘          └──────────┘
          └──────────┘                                                           │
               ▲                                                                 ▼
               └──────────── reap background jobs (on SIGCHLD flag) ◀───── builtins
```

`shell_loop()` in `src/shell.c` does this for every line:

1. If `child_exited` is set, clear it and reap finished background jobs.
2. Print `CSH> ` and read a line with `getline`.
3. `lexer_tokenize(line)` → `TokenList*` (or `NULL` on a lexing error).
4. `parser_parse(tokens)` → `ASTNode*` (or `NULL` on a syntax error).
5. `execute_node(root)`.
6. Free the AST and the token list.

### Lexer

**Files:** `src/lexer/lexer.c`, `src/lexer/lexer_utils.c`, `include/lexer.h`

The lexer reads the input from left to right and builds a growable `TokenList` (capacity starts at 2 and doubles when full). Every token's `value` is a copy made with `strdup`. The list always ends with a `TOKEN_EOF`.

| Token | Text |
|---|---|
| `TOKEN_WORD` | any run of characters that are not whitespace or operators, or a quoted string |
| `TOKEN_PIPE` | `\|` |
| `TOKEN_OR` | `\|\|` |
| `TOKEN_AMPERSAND` | `&` |
| `TOKEN_AND` | `&&` |
| `TOKEN_SEMICOLON` | `;` |
| `TOKEN_INPUT` | `<` |
| `TOKEN_OUTPUT` | `>` |
| `TOKEN_APPEND` | `>>` |
| `TOKEN_ERROR` | `2>` (stderr redirection) |
| `TOKEN_ERROR_APPEND` | `2>>` |
| `TOKEN_EOF` | end of input |
| `TOKEN_ERROR_TYPE` | used internally to report an invalid token |

Operators are matched longest-first (`||` before `|`, `>>` before `>`, `2>>` before `2>`). `lexer_tokenize` returns `NULL` (after freeing everything it allocated) if:

- a quote is never closed (`echo "abc`),
- a quote appears in the middle of an unquoted word (`ab"cd"`), or
- memory allocation fails.

Helpers in `lexer_utils.c`: `is_operator(c)` (true for `| ; & < >`) and `skip_whitespace(input, i)`.

### Parser and AST

**Files:** `src/parser/parser.c`, `src/parser/ast.c`, `src/parser/ast_print.c`, `include/parse.h`, `include/ast.h`

The parser is a recursive-descent parser with one function per grammar rule. Each one takes the token list and an `int *position` cursor:

| Function | Rule | Produces |
|---|---|---|
| `parser_parse` | `line` | the root node; fails if there are tokens left before `EOF` |
| `parse_list` | `list` | `NODE_SEQUENCE`, `NODE_BACKGROUND` |
| `parse_and_or` | `and_or` | `NODE_AND`, `NODE_OR` (left-associative) |
| `parse_pipeline` | `pipeline` | `NODE_PIPELINE` (only when there is at least one `\|`) |
| `parse_command` | `command` | `NODE_COMMAND` |

The AST is a tagged union:

```c
typedef enum {
    NODE_COMMAND,     // data.command    : argv[], argc, redirections[], redirection_count
    NODE_PIPELINE,    // data.pipeline   : commands[], count
    NODE_AND,         // data.binary     : left, right
    NODE_OR,          // data.binary     : left, right
    NODE_SEQUENCE,    // data.binary     : left, right
    NODE_BACKGROUND   // data.background : command
} NodeType;

typedef struct {
    TokenType type;   // TOKEN_INPUT / OUTPUT / APPEND / ERROR / ERROR_APPEND
    char *filename;
} Redirection;
```

`argv` always ends with a `NULL` entry, so it can be passed straight to `execvp`. The AST owns all of its strings (copied from the tokens), so the token list can be freed separately.

- `ast_free(node)` frees a whole tree recursively.
- `ast_print(root)` prints an indented view of the tree, which helps with debugging:

```
SEQUENCE
    PIPELINE
        COMMAND
            argc: 1
            argv:
                ls
        COMMAND
            argc: 2
            argv:
                grep
                c
            OUTPUT: out.txt
    COMMAND
    ...
```

If the parser fails at any point, it frees everything it has built so far and returns `NULL`, so a bad line does not leak memory.

### Executor

**Files:** `src/executor/executor.c`, `src/builtins/builtin.c`, `include/executor.h`, `include/builtin.h`

`execute_node` walks the AST and decides what to do based on the node type:

| Node | What happens |
|---|---|
| `NODE_COMMAND` (built-in) | `execute_builtin_with_redirections`: save fds 0/1/2 with `dup`, apply redirections, run the built-in, `fflush`, restore the fds |
| `NODE_COMMAND` (external) | `execute_external`: `fork`; the child applies redirections and calls `execvp`; the parent calls `waitpid` (and retries on `EINTR`) |
| `NODE_PIPELINE` | `execute_pipeline`: create *n−1* pipes, fork *n* children, connect each child's stdin/stdout with `dup2`, close every pipe end in every process, wait for all children, return the last stage's status |
| `NODE_AND` / `NODE_OR` | short-circuit evaluation |
| `NODE_SEQUENCE` | run left, then right |
| `NODE_BACKGROUND` | `execute_background`: fork, the child runs the sub-tree, the parent records the job |

Robustness details:

- If a `fork` fails partway through starting a pipeline, the children already started get `SIGTERM`, all pipes are closed, and those children are waited on. Nothing is left running.
- Children exit with `_exit` (not `exit`) so they do not flush stdio buffers they share with the parent. Built-ins that run in a child call `fflush(NULL)` first.
- Every pipe end is closed in every child and in the parent. If any process kept a write end open, the reader would never see EOF and the pipeline would hang.

### Signals

**Files:** `src/signals/signals.c`, `include/signals.h`

`setup_signal_handlers()` installs a `SIGCHLD` handler with `SA_RESTART | SA_NOCLDSTOP`. The handler only sets `volatile sig_atomic_t child_exited = 1`, which is safe to do inside a signal handler. The actual reaping happens later in the main loop. `SA_RESTART` lets the blocking `getline` keep going when a background child exits.

CSH does not handle any other signals. **Ctrl-C** (`SIGINT`) ends the shell itself, as well as the foreground child.

---

## Project Structure

```
csh/
├── Makefile                 # build, clean, re, debug, test targets
├── README.md
├── include/                 # public headers (added with -Iinclude)
│   ├── ast.h                #   AST node types, ast_free, ast_print
│   ├── builtin.h            #   is_builtin, execute_builtin
│   ├── executor.h           #   execute_node, job helpers
│   ├── lexer.h              #   TokenType, Token, TokenList, lexer API
│   ├── parse.h              #   parser entry points
│   └── signals.h            #   child_exited flag, setup_signal_handlers
├── src/
│   ├── main.c               # entry point: banner, signals, shell loop
│   ├── shell.c              # read → lex → parse → execute loop
│   ├── lexer/
│   │   ├── lexer.c          # tokenizer
│   │   └── lexer_utils.c    # character helpers
│   ├── parser/
│   │   ├── parser.c         # recursive-descent parser
│   │   ├── ast.c            # AST deallocation
│   │   └── ast_print.c      # AST pretty-printer (debugging)
│   ├── executor/
│   │   └── executor.c       # process creation, pipes, redirections, jobs
│   ├── builtins/
│   │   └── builtin.c        # cd, pwd, export, unset, jobs, exit
│   └── signals/
│       └── signals.c        # SIGCHLD handler
└── tests/
    └── unit/
        ├── test_lexer.c     # 12 lexer tests
        ├── test_parser.c    # 21 parser tests
        └── test_executor.c  # 37 executor / integration tests
```

---

## Public API Reference

### `lexer.h`

| Function | Description |
|---|---|
| `TokenList *lexer_tokenize(const char *input)` | Turns `input` into tokens. Returns `NULL` on error. The caller frees the result with `token_list_free`. |
| `void token_list_free(TokenList *list)` | Frees the list and every token value. Safe to call with `NULL`. |
| `int is_operator(char c)` | Returns non-zero for `\| ; & < >`. |
| `int skip_whitespace(const char *input, int i)` | Returns the index of the first non-whitespace character at or after `i`. |

### `parse.h`

| Function | Description |
|---|---|
| `ASTNode *parser_parse(TokenList *tokens)` | Parses a whole line. Returns `NULL` on a syntax error or if `tokens` is `NULL`. The caller frees the result with `ast_free`. |
| `parse_list`, `parse_and_or`, `parse_pipeline`, `parse_command` | One function per grammar rule. They move `*position` forward. |

### `ast.h`

| Function | Description |
|---|---|
| `void ast_free(ASTNode *node)` | Frees a tree recursively. Safe to call with `NULL`. |
| `void ast_print(ASTNode *root)` | Prints the tree to stdout. |

### `executor.h`

| Function | Description |
|---|---|
| `int execute_node(ASTNode *node)` | Runs a tree and returns its exit status. Returns `1` for `NULL`. |
| `void reap_background_processes(void)` | Reaps finished background jobs without blocking. |
| `void print_background_jobs(void)` | Prints the tracked jobs (used by `jobs`). |

### `builtin.h`

| Function | Description |
|---|---|
| `int is_builtin(const char *name)` | True for `cd`, `exit`, `pwd`, `export`, `unset`, `jobs`. |
| `int execute_builtin(int argc, char **argv)` | Runs the built-in in the current process. Returns `0` or `1`. |

### `signals.h`

| Symbol | Description |
|---|---|
| `extern volatile sig_atomic_t child_exited` | Set to `1` by the `SIGCHLD` handler. |
| `void setup_signal_handlers(void)` | Installs the `SIGCHLD` handler. |

---

## Testing

The unit tests are standalone C programs in `tests/unit/`. Each one prints a result per test case and a summary:

```
Passed: 37
Failed: 0
Total:  37
```

| Suite | Covers |
|---|---|
| `test_lexer.c` | every operator, quoted words, mixed input, unterminated and misplaced quotes |
| `test_parser.c` | root node types, argv contents, pipeline shape, AND/OR/SEQUENCE structure, redirections, background, invalid input, `NULL` tokens |
| `test_executor.c` | exit statuses, `NULL` nodes, all five redirections, `cd`, short-circuit `&&`/`\|\|`, nested operators, 3- and 4-stage pipelines, large data through pipes, consumer exiting early, pipeline failures, single and multiple background jobs, job listing, reaping, redirected and piped background jobs |

Build and run them from the project root:

```sh
CFLAGS="-std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -Wall -Wextra -g -Iinclude"

gcc $CFLAGS tests/unit/test_lexer.c src/lexer/*.c -o test_lexer && ./test_lexer

gcc $CFLAGS tests/unit/test_parser.c src/lexer/*.c src/parser/*.c -o test_parser && ./test_parser

gcc $CFLAGS tests/unit/test_executor.c \
    src/lexer/*.c src/parser/*.c src/executor/*.c src/builtins/*.c src/signals/*.c \
    -o test_executor && ./test_executor
```

`-D_DEFAULT_SOURCE` is needed for `usleep`, which the executor tests call. The executor tests create and delete temporary files in the current directory, so run them from the project root.

> **Note:** `make test` runs `$(MAKE) -C tests`, but there is no `tests/Makefile` yet, so that target fails for now. Use the commands above until one is added.

---

## Debugging and Memory Checking

**Sanitizers:**

```sh
make debug
./csh-debug
```

This builds with `-fsanitize=address,undefined -fno-omit-frame-pointer`, which reports memory errors, leaks and undefined behaviour while the shell runs.

**Valgrind** (check the test binaries or the shell itself for leaks):

```sh
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes ./test_executor
```

`--track-fds=yes` is useful for confirming that no pipe ends or saved descriptors are left open.

**strace** (watch the process and pipe system calls):

```sh
strace -f -e trace=fork,clone,execve,pipe,pipe2,dup2,wait4,close ./csh
```

**AST dump:** call `ast_print(root)` in `shell_loop` after `parser_parse` to see how a line was parsed.

---

## Known Limitations

CSH is a teaching-scale shell. These features are **not** supported:

- **No expansions:** no `$VAR`, `$?`, `~`, globbing (`*.c`), command substitution, or arithmetic.
- **Limited quoting:** a quote must start a word, and the whole word must be inside the quotes. `"abc"def` and `ab"cd"` are rejected. There are no escape characters (`\`), and single and double quotes behave the same.
- **No job control:** no `fg`, `bg`, `kill %n`, process groups, or `SIGTSTP` handling. **Ctrl-C** ends the shell.
- **No "Done" messages:** finished background jobs are removed from `jobs` without a message.
- **Background scope:** `&` wraps everything to its left in the same list. So `a ; b &` runs *both* `a` and `b` together in the background, while bash would background only `b`.
- **No fd duplication:** `2>&1`, `>&`, here-documents (`<<`) and here-strings are not recognised.
- **Silent syntax errors:** a line that fails to lex or parse is skipped without an error message.
- **`exit`** takes no status argument and always exits with 0.
- **`cd` with no arguments** looks up the environment variable `Home` instead of `HOME`. Unless `Home` is set, it prints `cd: HOME not set`. Changing `getenv("Home")` to `getenv("HOME")` in `src/builtins/builtin.c` fixes this.
- **No subshells or grouping:** `( ... )` and `{ ...; }` are not supported.
- **No history or line editing:** input is read with plain `getline`.

---

## Development History

| Commit | Milestone |
|---|---|
| `c2b4a4f` | Initialize CSH project |
| `42933cb` | Add lexer and token types |
| `6cdad15` | Implement parser and AST |
| `395e607` | Remove compiled binaries from tracking |
| `7d12802` | Basic executor and first two built-ins (`cd`, `exit`) |
| `ca89c5f` | Built-ins `pwd`, `export`, `unset` |
| `45e69ee` | Redirections on built-ins |
| `3eba324` | Background job IDs and `jobs` |
| `afd8e39` | Unit tests |
| `e53e8b5` | Makefile |

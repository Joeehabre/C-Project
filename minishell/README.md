# minishell

A Unix-like shell written in C from scratch.

## Features

- **Builtins:** `cd`, `pwd`, `history`, `exit [code]`, `env`
- **Pipelines:** `cmd1 | cmd2 | cmd3`
- **Redirection:** `< infile`, `> outfile`, `>> append`
- **Background jobs:** `cmd &` (with automatic zombie reaping via `SIGCHLD`)
- **Signal handling:** Ctrl-C interrupts the current input line without killing the shell
- **Exit status:** `$?`-style tracking; `exit` returns the last command's code
- **History:** last 100 commands, printed with `history`

## Build

```bash
make        # produces ./minishell
make clean
```

## Usage

```
joe:/home/joe$ ls -la | grep '^d'
joe:/home/joe$ cat < input.txt | sort | uniq > output.txt
joe:/home/joe$ sleep 10 &
[bg] pid 4321
joe:/home/joe$ history
joe:/home/joe$ cd /tmp
joe:/tmp$ exit
```

## Implementation Notes

- Single source file: `shell.c`
- `split_pipeline` splits on `|` using `strtok_r`
- `tokenize` handles single/double quotes, `<`, `>`, `>>`, and `&`
- Builtins run in the parent process for correct state changes (e.g. `cd`)
- Each stage of a pipeline runs in a child with `dup2` to connect pipes

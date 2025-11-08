# minishell

A tiny Unix shell written in C.

## Features
- Built-ins: `cd`, `pwd`, `history`, `exit`
- External commands with args
- Pipelines: `cmd1 | cmd2 | cmd3`
- Redirection: `cmd < in.txt > out.txt` and `>>` append
- Background: `cmd &`
- Simple in-memory history (last 100 commands)

## Build & Run
```bash
make
./minishell

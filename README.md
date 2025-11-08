# C-Systems-Projects

A curated set of **systems programming** projects in **C** by Joe Habre (AUB).  
Focus areas: processes, pipes, files, sockets, and minimal OS/networking primitives.

<p align="left">
  <img alt="MIT" src="https://img.shields.io/badge/License-MIT-green">
  <img alt="Language" src="https://img.shields.io/badge/C-std%2Fc11-blue">
  <img alt="Platform" src="https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-lightgrey">
</p>

## 🔧 Projects
| Project | What it is | Concepts |
|---|---|---|
| **minishell** | Tiny Unix-like shell with `cd`, `pwd`, `history`, `exit`, pipes `|`, redirection `< > >>`, background `&` | `fork/exec`, `pipe`, `dup2`, signals, parsing |
| **http_server** | Minimal HTTP/1.0 static file server serving `./www` | TCP sockets, request parsing, MIME, I/O |

## 🚀 Quick Start
```bash
# minishell
cd minishell && make
./minishell
# examples
ls -la | grep '^d' > dirs.txt
cat < input.txt | wc -l
sleep 5 &

# http_server
cd ../http_server && make
mkdir -p www && echo "hello from joe" > www/index.html
./http_server 8080
# open http://localhost:8080

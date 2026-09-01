# C Systems Projects

![Build](https://github.com/Joeehabre/C-Projects/actions/workflows/build.yml/badge.svg)

A collection of systems programming projects in C by Joe Habre (AUB).  
Each project targets a different area of low-level programming: processes, pipes, sockets, and binary I/O.

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
![Language](https://img.shields.io/badge/C-C11-blue)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-lightgrey)

---

## Projects

| Project | Description | Key Concepts |
|---|---|---|
| [minishell](minishell/) | Unix shell with pipes, redirection, background jobs, and builtins | `fork/exec`, `pipe`, `dup2`, signals |
| [http_server](http_server/) | Static file HTTP/1.1 server with URL decoding and path safety | TCP sockets, MIME types, request parsing |
| [rle_compressor](rle_compressor/) | Run-Length Encoding compressor/decompressor | Binary I/O, streaming, encoding |
| [wc_clone](wc_clone/) | Reimplementation of Unix `wc` with flag support | Text processing, stdin/file I/O |

---

## Quick Start

```bash
# Clone
git clone https://github.com/Joeehabre/C-Projects.git
cd C-Projects

# Build any project
cd minishell && make && ./minishell
cd http_server && make && mkdir -p www && echo "<h1>Hello</h1>" > www/index.html && ./http_server 8080
cd wc_clone && make && echo "hello world" | ./wc_clone
cd rle_compressor && make && ./rle c input.bin output.rle
```

---

## Requirements

- GCC (or Clang) with C11 support
- POSIX-compliant OS (Linux or macOS)
- GNU Make

---

## License

[MIT](LICENSE), Joe Habre

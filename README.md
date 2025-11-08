# C-Systems-Projects

Systems programming projects in C by **Joe Habre** (AUB).  
Focus: processes, files, networking, and OS concepts.

## Projects
- **minishell/** — tiny Unix shell supporting `cd`, `pwd`, `history`, `exit`, pipes `|`, redirection `< > >>`, and background `&`
- **http_server/** — minimal single-process HTTP/1.0 static file server (serves files from `./www`)

## Build
Each project has its own `Makefile`:
```bash
cd minishell && make && ./minishell
cd http_server && make && ./http_server 8080

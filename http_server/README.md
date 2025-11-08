# http_server

A tiny single-process HTTP/1.0 static file server.

- Serves files from `./www`
- Handles `GET /path`
- Content-Type guessing for a few common types
- Simple 404/400 handling

## Build & Run
```bash
make
mkdir -p www
echo "hello from joe" > www/index.html
./http_server 8080
# open http://localhost:8080/

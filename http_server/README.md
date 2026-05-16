# http_server

A minimal HTTP/1.1 static file server written in C.  
Serves files from a `./www` document root.

## Features

- Serves static files (`GET`)
- URL decoding (`%XX`) with path traversal prevention
- Path normalization (rejects `/../` escapes)
- MIME type detection: `.html`, `.css`, `.js`, `.json`, `.png`, `.jpg`, `.gif`, `.svg`, `.ico`, `.txt`, `.xml`, `.pdf`
- Request logging with ISO-8601 timestamps
- Proper error responses: 400, 403, 404, 405, 414
- Full request buffering (reads until `\r\n\r\n`)
- Reliable `write_all` to avoid partial writes

## Build

```bash
make        # produces ./http_server
make clean
```

## Usage

```bash
mkdir -p www
echo "<h1>Hello from Joe</h1>" > www/index.html

./http_server 8080
# open http://localhost:8080/
```

Example log output:
```
http_server listening on http://localhost:8080  (www/ is document root)
[2025-05-16T18:30:01Z] GET / -> 200
[2025-05-16T18:30:02Z] GET /style.css -> 200
[2025-05-16T18:30:03Z] GET /missing.html -> 404
```

## Implementation Notes

- Single source file: `http_server.c`
- Sequential (one connection at a time); suitable for development/learning
- `url_decode()` decodes percent-encoded characters in-place
- `normalize_path()` collapses `.` and `..` segments; returns 403 if path escapes root
- `serve_file()` uses `fstat` to confirm the target is a regular file before sending

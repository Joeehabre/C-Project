#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* ── helpers ─────────────────────────────────────────────────────────── */

static int write_all(int fd, const void *buf, size_t n) {
    const char *p = buf;
    while (n) {
        ssize_t r = write(fd, p, n);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        p += r; n -= (size_t)r;
    }
    return 0;
}

/* Decode %XX sequences in-place. Returns 0 on success, -1 on bad encoding. */
static int url_decode(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (*r == '%') {
            if (!r[1] || !r[2]) return -1;
            char hex[3] = { r[1], r[2], '\0' };
            char *end;
            long v = strtol(hex, &end, 16);
            if (end != hex + 2 || v < 0 || v > 255) return -1;
            *w++ = (char)v;
            r += 3;
        } else if (*r == '+') {
            *w++ = ' '; r++;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
    return 0;
}

/* Normalize path: collapse //, /./, and resolve /../ segments.
   Returns -1 if the result escapes the root (/../ at top). */
static int normalize_path(char *path) {
    /* work on a temporary copy */
    char tmp[4096];
    if (strlen(path) >= sizeof(tmp)) return -1;
    strcpy(tmp, path);

    char *segs[512];
    int depth = 0;
    char *p = tmp;

    /* skip leading slash */
    if (*p == '/') p++;

    while (*p) {
        char *start = p;
        while (*p && *p != '/') p++;
        if (*p == '/') *p++ = '\0';

        if (strcmp(start, "..") == 0) {
            if (depth == 0) return -1; /* escape above root */
            depth--;
        } else if (strcmp(start, ".") == 0 || *start == '\0') {
            /* skip */
        } else {
            segs[depth++] = start;
        }
    }

    /* rebuild path */
    path[0] = '\0';
    for (int i = 0; i < depth; i++) {
        strcat(path, "/");
        strcat(path, segs[i]);
    }
    if (depth == 0) strcpy(path, "/");
    return 0;
}

/* ── MIME types ──────────────────────────────────────────────────────── */

static const char *guess_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (!strcmp(dot, ".html") || !strcmp(dot, ".htm")) return "text/html; charset=utf-8";
    if (!strcmp(dot, ".css"))  return "text/css; charset=utf-8";
    if (!strcmp(dot, ".js"))   return "application/javascript; charset=utf-8";
    if (!strcmp(dot, ".json")) return "application/json";
    if (!strcmp(dot, ".png"))  return "image/png";
    if (!strcmp(dot, ".jpg") || !strcmp(dot, ".jpeg")) return "image/jpeg";
    if (!strcmp(dot, ".gif"))  return "image/gif";
    if (!strcmp(dot, ".svg"))  return "image/svg+xml";
    if (!strcmp(dot, ".ico"))  return "image/x-icon";
    if (!strcmp(dot, ".txt"))  return "text/plain; charset=utf-8";
    if (!strcmp(dot, ".xml"))  return "application/xml";
    if (!strcmp(dot, ".pdf"))  return "application/pdf";
    return "application/octet-stream";
}

/* ── response helpers ────────────────────────────────────────────────── */

static void send_response(int fd, int code, const char *status,
                          const char *ctype, const void *body, size_t blen) {
    char hdr[2048];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\n"
        "Server: joe-http/2\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        code, status, ctype ? ctype : "text/plain; charset=utf-8", blen);
    write_all(fd, hdr, (size_t)n);
    if (body && blen) write_all(fd, body, blen);
}

static void send_error(int fd, int code, const char *status) {
    char body[256];
    int n = snprintf(body, sizeof(body),
        "<!DOCTYPE html><html><body><h1>%d %s</h1></body></html>", code, status);
    send_response(fd, code, status, "text/html; charset=utf-8", body, (size_t)n);
}

/* ── file serving ────────────────────────────────────────────────────── */

static void serve_file(int cfd, const char *fs_path) {
    int fd = open(fs_path, O_RDONLY);
    if (fd < 0) { send_error(cfd, 404, "Not Found"); return; }

    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        send_error(cfd, 403, "Forbidden");
        close(fd);
        return;
    }

    const char *ctype = guess_type(fs_path);
    char hdr[2048];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Server: joe-http/2\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        ctype, (size_t)st.st_size);
    write_all(cfd, hdr, (size_t)n);

    char buf[65536];
    ssize_t r;
    while ((r = read(fd, buf, sizeof(buf))) > 0)
        write_all(cfd, buf, (size_t)r);
    close(fd);
}

/* ── request reading ─────────────────────────────────────────────────── */

/* Read until we have the full header section (ends with \r\n\r\n). */
static ssize_t read_request(int fd, char *buf, size_t cap) {
    size_t total = 0;
    while (total < cap - 1) {
        ssize_t r = read(fd, buf + total, cap - 1 - total);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) break;
        total += (size_t)r;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) break;
    }
    buf[total] = '\0';
    return (ssize_t)total;
}

/* ── logging ─────────────────────────────────────────────────────────── */

static void log_request(const char *method, const char *path, int code) {
    time_t t = time(NULL);
    struct tm *tm = gmtime(&t);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);
    printf("[%s] %s %s -> %d\n", ts, method, path, code);
}

/* ── client handler ──────────────────────────────────────────────────── */

static void handle_client(int cfd) {
    char req[8192];
    if (read_request(cfd, req, sizeof(req)) <= 0) return;

    char method[16], raw_path[2048], version[16];
    if (sscanf(req, "%15s %2047s %15s", method, raw_path, version) != 3) {
        send_error(cfd, 400, "Bad Request");
        log_request("?", "?", 400);
        return;
    }

    /* strip query string */
    char *qs = strchr(raw_path, '?');
    if (qs) *qs = '\0';

    /* URL decode */
    if (url_decode(raw_path) < 0) {
        send_error(cfd, 400, "Bad Request");
        log_request(method, raw_path, 400);
        return;
    }

    /* normalize and check for path traversal */
    if (normalize_path(raw_path) < 0) {
        send_error(cfd, 403, "Forbidden");
        log_request(method, raw_path, 403);
        return;
    }

    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
        send_error(cfd, 405, "Method Not Allowed");
        log_request(method, raw_path, 405);
        return;
    }

    char fs_path[4096];
    int n = snprintf(fs_path, sizeof(fs_path), "www%s",
                     strcmp(raw_path, "/") == 0 ? "/index.html" : raw_path);
    if (n < 0 || (size_t)n >= sizeof(fs_path)) {
        send_error(cfd, 414, "URI Too Long");
        log_request(method, raw_path, 414);
        return;
    }

    serve_file(cfd, fs_path);
    log_request(method, raw_path, 200);
}

/* ── main ────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "invalid port: %s\n", argv[1]);
        return 1;
    }

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(sfd, 64) < 0) { perror("listen"); return 1; }

    printf("http_server listening on http://localhost:%d  (www/ is document root)\n", port);
    fflush(stdout);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int cfd = accept(sfd, (struct sockaddr *)&client_addr, &client_len);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }
        handle_client(cfd);
        close(cfd);
    }
    return 0;
}

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
#include <unistd.h>

static const char *guess_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (!strcmp(dot, ".html")) return "text/html; charset=utf-8";
    if (!strcmp(dot, ".htm"))  return "text/html; charset=utf-8";
    if (!strcmp(dot, ".css"))  return "text/css; charset=utf-8";
    if (!strcmp(dot, ".js"))   return "application/javascript";
    if (!strcmp(dot, ".png"))  return "image/png";
    if (!strcmp(dot, ".jpg") || !strcmp(dot, ".jpeg")) return "image/jpeg";
    if (!strcmp(dot, ".gif"))  return "image/gif";
    if (!strcmp(dot, ".txt"))  return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

static void send_response(int fd, int code, const char *status, const char *ctype, const void *body, size_t blen) {
    char hdr[1024];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 %d %s\r\n"
        "Server: joe-http\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        code, status, ctype ? ctype : "text/plain", blen);
    write(fd, hdr, n);
    if (body && blen) write(fd, body, blen);
}

static void serve_file(int cfd, const char *fs_path) {
    int fd = open(fs_path, O_RDONLY);
    if (fd < 0) {
        const char *msg = "<h1>404 Not Found</h1>";
        send_response(cfd, 404, "Not Found", "text/html; charset=utf-8", msg, strlen(msg));
        return;
    }
    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        const char *msg = "<h1>403 Forbidden</h1>";
        send_response(cfd, 403, "Forbidden", "text/html; charset=utf-8", msg, strlen(msg));
        close(fd);
        return;
    }

    const char *ctype = guess_type(fs_path);
    char hdr[1024];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 200 OK\r\n"
        "Server: joe-http\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        ctype, (size_t)st.st_size);
    write(cfd, hdr, n);

    char buf[8192];
    ssize_t r;
    while ((r = read(fd, buf, sizeof(buf))) > 0) {
        write(cfd, buf, (size_t)r);
    }
    close(fd);
}

static void handle_client(int cfd) {
    char req[4096];
    ssize_t n = read(cfd, req, sizeof(req)-1);
    if (n <= 0) return;
    req[n] = '\0';

    char method[16], path[2048], version[16];
    if (sscanf(req, "%15s %2047s %15s", method, path, version) != 3) {
        const char *msg = "Bad Request\n";
        send_response(cfd, 400, "Bad Request", "text/plain", msg, strlen(msg));
        return;
    }
    if (strcmp(method, "GET") != 0) {
        const char *msg = "Method Not Allowed\n";
        send_response(cfd, 405, "Method Not Allowed", "text/plain", msg, strlen(msg));
        return;
    }


    if (strstr(path, "..")) {
        const char *msg = "Forbidden\n";
        send_response(cfd, 403, "Forbidden", "text/plain", msg, strlen(msg));
        return;
    }


    char file_path[4096] = "www";
    if (strcmp(path, "/") == 0) {
        strcat(file_path, "/index.html");
    } else {
        strcat(file_path, path);
    }

    serve_file(cfd, file_path);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(sfd, 16) < 0) { perror("listen"); return 1; }

    printf("http_server listening on http://localhost:%d\n", port);

    for (;;) {
        int cfd = accept(sfd, NULL, NULL);
        if (cfd < 0) { perror("accept"); continue; }
        handle_client(cfd);
        close(cfd);
    }
    return 0;
}

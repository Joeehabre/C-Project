#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define MAX_LINE    4096
#define MAX_TOKS    256
#define MAX_CMDS    64
#define MAX_HISTORY 100

static char *history[MAX_HISTORY];
static int   history_count = 0;
static int   last_exit     = 0;

/* ── history ─────────────────────────────────────────────────────────── */

static void push_history(const char *line) {
    if (!line || !*line) return;
    if (history_count == MAX_HISTORY) {
        free(history[0]);
        memmove(history, history + 1, sizeof(char *) * (MAX_HISTORY - 1));
        history_count--;
    }
    history[history_count++] = strdup(line);
}

static void print_history(void) {
    for (int i = 0; i < history_count; i++)
        printf("%d  %s\n", i + 1, history[i]);
}

/* ── helpers ─────────────────────────────────────────────────────────── */

static int is_whitespace_only(const char *s) {
    for (; *s; s++)
        if (*s != ' ' && *s != '\t' && *s != '\n') return 0;
    return 1;
}

/* ── redirection descriptor ──────────────────────────────────────────── */

struct redir {
    char *infile;
    char *outfile;
    int   append;
    int   background;
};

/* ── pipeline splitting ──────────────────────────────────────────────── */

static int split_pipeline(char *line, char *cmds[], int max_cmds) {
    int n = 0;
    char *saveptr = NULL;
    for (char *p = strtok_r(line, "|", &saveptr);
         p && n < max_cmds;
         p = strtok_r(NULL, "|", &saveptr)) {
        while (*p == ' ' || *p == '\t') p++;
        char *end = p + strlen(p) - 1;
        while (end >= p && (*end == ' ' || *end == '\t' || *end == '\n'))
            *end-- = '\0';
        cmds[n++] = p;
    }
    return n;
}

/* ── tokenizer ───────────────────────────────────────────────────────── */

static int tokenize(char *cmd, char *argv[], int max_toks, struct redir *rd) {
    rd->infile = rd->outfile = NULL;
    rd->append = rd->background = 0;

    int argc = 0;
    char *p = cmd;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        if (*p == '&') { rd->background = 1; p++; continue; }

        if (*p == '<') {
            p++;
            while (*p == ' ' || *p == '\t') p++;
            char *start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '>' && *p != '<' && *p != '&') p++;
            if (p == start) { fprintf(stderr, "syntax error: expected infile\n"); return -1; }
            free(rd->infile);
            rd->infile = strndup(start, (size_t)(p - start));
            continue;
        }

        if (*p == '>') {
            p++;
            if (*p == '>') { rd->append = 1; p++; }
            while (*p == ' ' || *p == '\t') p++;
            char *start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '>' && *p != '<' && *p != '&') p++;
            if (p == start) { fprintf(stderr, "syntax error: expected outfile\n"); return -1; }
            free(rd->outfile);
            rd->outfile = strndup(start, (size_t)(p - start));
            continue;
        }

        char *tok;
        if (*p == '"' || *p == '\'') {
            char q = *p++;
            char *start = p;
            while (*p && *p != q) p++;
            if (!*p) { fprintf(stderr, "syntax error: unmatched quote\n"); return -1; }
            tok = strndup(start, (size_t)(p - start));
            p++;
        } else {
            char *start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '<' && *p != '>' && *p != '&') p++;
            tok = strndup(start, (size_t)(p - start));
        }

        if (argc >= max_toks - 1) {
            fprintf(stderr, "too many arguments\n");
            free(tok);
            return -1;
        }
        argv[argc++] = tok;
    }
    argv[argc] = NULL;
    return argc;
}

static void free_tokens(char *argv[], struct redir *rd) {
    for (int k = 0; argv[k]; k++) free(argv[k]);
    free(rd->infile);
    free(rd->outfile);
    rd->infile = rd->outfile = NULL;
}

/* ── builtins ────────────────────────────────────────────────────────── */

static int is_builtin(char **argv) {
    if (!argv[0]) return 0;
    return strcmp(argv[0], "cd")      == 0 ||
           strcmp(argv[0], "pwd")     == 0 ||
           strcmp(argv[0], "exit")    == 0 ||
           strcmp(argv[0], "history") == 0 ||
           strcmp(argv[0], "env")     == 0;
}

static int run_builtin(char **argv) {
    if (strcmp(argv[0], "cd") == 0) {
        const char *target = argv[1] ? argv[1] : getenv("HOME");
        if (!target) target = ".";
        if (chdir(target) != 0) { perror("cd"); return 1; }
        return 0;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        char buf[MAX_LINE];
        if (getcwd(buf, sizeof(buf))) puts(buf); else { perror("pwd"); return 1; }
        return 0;
    }
    if (strcmp(argv[0], "history") == 0) {
        print_history();
        return 0;
    }
    if (strcmp(argv[0], "env") == 0) {
        extern char **environ;
        for (char **e = environ; *e; e++) puts(*e);
        return 0;
    }
    if (strcmp(argv[0], "exit") == 0) {
        int code = argv[1] ? atoi(argv[1]) : last_exit;
        for (int i = 0; i < history_count; i++) free(history[i]);
        exit(code);
    }
    return 0;
}

/* ── redirections (in child only) ────────────────────────────────────── */

static void setup_redirections(struct redir *rd) {
    if (rd->infile) {
        int fd = open(rd->infile, O_RDONLY);
        if (fd < 0) { perror(rd->infile); _exit(1); }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (rd->outfile) {
        int flags = O_WRONLY | O_CREAT | (rd->append ? O_APPEND : O_TRUNC);
        int fd = open(rd->outfile, flags, 0644);
        if (fd < 0) { perror(rd->outfile); _exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

/* ── pipeline runner ─────────────────────────────────────────────────── */

static int run_pipeline(char *cmds[], int n) {
    int prev_read = -1;
    pid_t pids[MAX_CMDS];
    int background = 0;

    for (int i = 0; i < n; i++) {
        int pipe_fd[2] = {-1, -1};
        if (i < n - 1 && pipe(pipe_fd) < 0) { perror("pipe"); return 1; }

        struct redir rd;
        char *argv[MAX_TOKS];
        int argc = tokenize(cmds[i], argv, MAX_TOKS, &rd);
        if (argc < 0) {
            if (prev_read != -1) close(prev_read);
            if (pipe_fd[0] != -1) { close(pipe_fd[0]); close(pipe_fd[1]); }
            return 1;
        }

        if (i == n - 1) background = rd.background;

        /* run builtins directly only for single non-background commands */
        if (n == 1 && !background && is_builtin(argv)) {
            int r = run_builtin(argv);
            free_tokens(argv, &rd);
            return r;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            free_tokens(argv, &rd);
            if (prev_read != -1) close(prev_read);
            if (pipe_fd[0] != -1) { close(pipe_fd[0]); close(pipe_fd[1]); }
            return 1;
        }

        if (pid == 0) {
            /* restore default SIGINT in child */
            signal(SIGINT, SIG_DFL);

            if (prev_read != -1) {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (pipe_fd[1] != -1) {
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
            }
            if (pipe_fd[0] != -1) close(pipe_fd[0]);

            setup_redirections(&rd);

            if (!argv[0]) _exit(0);
            if (is_builtin(argv)) { _exit(run_builtin(argv)); }
            execvp(argv[0], argv);
            perror(argv[0]);
            _exit(127);
        }

        pids[i] = pid;

        /* parent: close ends we no longer need */
        if (prev_read != -1) close(prev_read);
        if (pipe_fd[1]  != -1) close(pipe_fd[1]);
        prev_read = pipe_fd[0];

        free_tokens(argv, &rd);
    }

    if (background) {
        printf("[bg] pid %d\n", pids[n - 1]);
        return 0;
    }

    int final_status = 0;
    for (int i = 0; i < n; i++) {
        int status;
        if (waitpid(pids[i], &status, 0) > 0 && i == n - 1) {
            if (WIFEXITED(status))   final_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status)) final_status = 128 + WTERMSIG(status);
        }
    }
    return final_status;
}

/* ── signal setup ────────────────────────────────────────────────────── */

static void sigint_handler(int sig) {
    (void)sig;
    /* just interrupt the read; the main loop reprints the prompt */
    write(STDOUT_FILENO, "\n", 1);
}

static void sigchld_handler(int sig) {
    (void)sig;
    /* reap background children without blocking */
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* ── main ────────────────────────────────────────────────────────────── */

int main(void) {
    struct sigaction sa_int  = { .sa_handler = sigint_handler,  .sa_flags = SA_RESTART };
    struct sigaction sa_chld = { .sa_handler = sigchld_handler, .sa_flags = SA_RESTART | SA_NOCLDSTOP };
    sigemptyset(&sa_int.sa_mask);
    sigemptyset(&sa_chld.sa_mask);
    sigaction(SIGINT,  &sa_int,  NULL);
    sigaction(SIGCHLD, &sa_chld, NULL);

    char line[MAX_LINE];

    for (;;) {
        char cwd[MAX_LINE];
        const char *dir = getcwd(cwd, sizeof(cwd)) ? cwd : "?";
        printf("joe:%s$ ", dir);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            /* EOF (Ctrl-D) */
            putchar('\n');
            break;
        }
        if (is_whitespace_only(line)) continue;

        /* strip trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        push_history(line);

        char line_copy[MAX_LINE];
        memcpy(line_copy, line, sizeof(line_copy));

        char *cmds[MAX_CMDS];
        int n = split_pipeline(line_copy, cmds, MAX_CMDS);
        if (n <= 0) continue;

        last_exit = run_pipeline(cmds, n);
    }

    for (int i = 0; i < history_count; i++) free(history[i]);
    return last_exit;
}

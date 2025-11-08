#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_LINE 4096
#define MAX_TOKS 256
#define MAX_CMDS 64
#define MAX_HISTORY 100

static char *history[MAX_HISTORY];
static int history_count = 0;

static void push_history(const char *line) {
    if (!line || !*line) return;
    if (history_count == MAX_HISTORY) {
        free(history[0]);
        memmove(history, history+1, sizeof(char*)*(MAX_HISTORY-1));
        history_count--;
    }
    history[history_count++] = strdup(line);
}

static void print_history(void) {
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i+1, history[i]);
    }
}

static int is_whitespace_only(const char *s) {
    for (; *s; s++) if (!(*s==' '||*s=='\t'||*s=='\n')) return 0;
    return 1;
}

struct redir {
    char *infile;
    char *outfile;
    int append;
    int background;
};

static int split_pipeline(char *line, char *cmds[], int max_cmds) {
    int n = 0;
    char *saveptr = NULL;
    for (char *p = strtok_r(line, "|", &saveptr); p && n < max_cmds; p = strtok_r(NULL, "|", &saveptr)) {
        // trim spaces
        while (*p==' '||*p=='\t') p++;
        char *end = p + strlen(p) - 1;
        while (end >= p && (*end==' '||*end=='\t'||*end=='\n')) *end-- = '\0';
        cmds[n++] = p;
    }
    return n;
}

static int tokenize(char *cmd, char *argv[], int max_toks, struct redir *rd) {
    rd->infile = rd->outfile = NULL;
    rd->append = 0;
    rd->background = 0;

    int argc = 0;
    char *tok = NULL;
    char *p = cmd;

    while (*p) {

        while (*p==' '||*p=='\t') p++;
        if (!*p) break;


        if (*p == '&') { rd->background = 1; p++; continue; }


        if (*p == '<') {
            p++;
            while (*p==' '||*p=='\t') p++;
            char *start = p;
            while (*p && *p!=' ' && *p!='\t' && *p!='>' && *p!='<' && *p!='&') p++;
            if (p == start) { fprintf(stderr, "syntax error: expected infile\n"); return -1; }
            tok = strndup(start, p-start);
            rd->infile = tok;
            continue;
        }
        if (*p == '>') {
            p++;
            if (*p == '>') { rd->append = 1; p++; }
            while (*p==' '||*p=='\t') p++;
            char *start = p;
            while (*p && *p!=' ' && *p!='\t' && *p!='>' && *p!='<' && *p!='&') p++;
            if (p == start) { fprintf(stderr, "syntax error: expected outfile\n"); return -1; }
            tok = strndup(start, p-start);
            rd->outfile = tok;
            continue;
        }
      
        if (*p=='"' || *p=='\'') {
            char q = *p++;
            char *start = p;
            while (*p && *p != q) p++;
            if (!*p) { fprintf(stderr, "syntax error: unmatched quote\n"); return -1; }
            tok = strndup(start, p-start);
            p++;
        } else {
            char *start = p;
            while (*p && *p!=' ' && *p!='\t' && *p!='<' && *p!='>' && *p!='&') p++;
            tok = strndup(start, p-start);
        }
        if (argc < max_toks-1) argv[argc++] = tok; else { fprintf(stderr,"too many args\n"); return -1; }
    }
    argv[argc] = NULL;
    return argc;
}

static int is_builtin(char **argv) {
    if (!argv[0]) return 0;
    return strcmp(argv[0],"cd")==0 || strcmp(argv[0],"pwd")==0 ||
           strcmp(argv[0],"exit")==0 || strcmp(argv[0],"history")==0;
}

static int run_builtin(char **argv) {
    if (strcmp(argv[0],"cd")==0) {
        const char *target = argv[1] ? argv[1] : getenv("HOME");
        if (!target) target = ".";
        if (chdir(target) != 0) perror("cd");
        return 1;
    }
    if (strcmp(argv[0],"pwd")==0) {
        char buf[4096];
        if (getcwd(buf, sizeof(buf))) puts(buf); else perror("pwd");
        return 1;
    }
    if (strcmp(argv[0],"history")==0) {
        print_history();
        return 1;
    }
    if (strcmp(argv[0],"exit")==0) {
        exit(0);
    }
    return 0;
}

static void setup_redirections(struct redir *rd) {
    if (rd->infile) {
        int fd = open(rd->infile, O_RDONLY);
        if (fd < 0) { perror("open infile"); _exit(1); }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (rd->outfile) {
        int flags = O_WRONLY | O_CREAT | (rd->append ? O_APPEND : O_TRUNC);
        int fd = open(rd->outfile, flags, 0644);
        if (fd < 0) { perror("open outfile"); _exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

static void exec_single(char **argv, struct redir *rd, int bg) {
    if (!argv[0]) _exit(0);
    if (is_builtin(argv)) { run_builtin(argv); _exit(0); }

    setup_redirections(rd);
    execvp(argv[0], argv);
    perror("execvp");
    _exit(127);
}

static void run_pipeline(char *cmds[], int n) {
    int i;
    int prev_fd[2] = {-1,-1};
    pid_t pids[MAX_CMDS];

    for (i = 0; i < n; i++) {
        int pipe_fd[2] = {-1,-1};
        if (i < n-1) {
            if (pipe(pipe_fd) < 0) { perror("pipe"); return; }
        }

        struct redir rd;
        char *argv[MAX_TOKS];
        int bg = 0;
        char *segment = cmds[i];

        int argc = tokenize(segment, argv, MAX_TOKS, &rd);
        if (argc < 0) return;
        if (i == n-1) bg = rd.background; 
      
        if (n == 1 && !bg && is_builtin(argv)) {
            run_builtin(argv);

            for (int k=0;k<argc;k++) free(argv[k]);
            free(rd.infile); free(rd.outfile);
            return;
        }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return; }
        if (pid == 0) {

            if (prev_fd[0] != -1) {
                dup2(prev_fd[0], STDIN_FILENO);
            }
            if (pipe_fd[1] != -1) {
                dup2(pipe_fd[1], STDOUT_FILENO);
            }

            if (prev_fd[0]!=-1){ close(prev_fd[0]); }
            if (prev_fd[1]!=-1){ close(prev_fd[1]); }
            if (pipe_fd[0]!=-1){ close(pipe_fd[0]); }
            if (pipe_fd[1]!=-1){ close(pipe_fd[1]); }

            exec_single(argv, &rd, bg);
        } else {
            pids[i] = pid;

            if (prev_fd[0]!=-1) close(prev_fd[0]);
            if (prev_fd[1]!=-1) close(prev_fd[1]);
            if (pipe_fd[1]!=-1) close(pipe_fd[1]);

            prev_fd[0] = pipe_fd[0];
            prev_fd[1] = pipe_fd[1];

            for (int k=0;k<argc;k++) free(argv[k]);
            free(rd.infile); free(rd.outfile);
        }
    }

    struct redir last_rd; char *argv_last[MAX_TOKS];
    tokenize(cmds[0], argv_last, MAX_TOKS, &last_rd);
    tokenize(cmds[n-1], argv_last, MAX_TOKS, &last_rd);
    int background = last_rd.background;
    for (int k=0; argv_last[k]; k++) free(argv_last[k]);
    free(last_rd.infile); free(last_rd.outfile);

    if (!background) {
        for (i = 0; i < n; i++) {
            int status;
            (void)waitpid(pids[i], &status, 0);
        }
    } else {
        printf("[bg] pipeline started (pid %d ...)\n", pids[n-1]);
    }
}

int main(void) {
    char line[MAX_LINE];

    for (;;) {
        // prompt
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) {
            printf("joe:%s$ ", cwd);
        } else {
            printf("joe$ ");
        }
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            putchar('\n'); break;
        }
        if (is_whitespace_only(line)) continue;


        char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
        push_history(line);

        char line_copy[MAX_LINE];
        strncpy(line_copy, line, sizeof(line_copy));
        char *cmds[MAX_CMDS];
        int n = split_pipeline(line_copy, cmds, MAX_CMDS);
        if (n <= 0) continue;

        run_pipeline(cmds, n);
    }

    for (int i=0;i<history_count;i++) free(history[i]);
    return 0;
}

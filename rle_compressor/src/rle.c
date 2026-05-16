#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static void die(const char *msg) { perror(msg); exit(1); }

/* Open a file or return stdin/stdout for "-". */
static FILE *open_file(const char *name, const char *mode) {
    if (strcmp(name, "-") == 0)
        return (mode[0] == 'r') ? stdin : stdout;
    FILE *f = fopen(name, mode);
    if (!f) die(name);
    return f;
}

static void close_file(FILE *f) {
    if (f != stdin && f != stdout) fclose(f);
}

/* ── compress ────────────────────────────────────────────────────────── */

static void compress(FILE *in, FILE *out, long *in_bytes, long *out_bytes) {
    *in_bytes = *out_bytes = 0;

    int prev = fgetc(in);
    if (prev == EOF) return;
    (*in_bytes)++;

    uint8_t run = 1;
    for (;;) {
        int cur = fgetc(in);
        if (cur == EOF) {
            if (fputc(run,  out) == EOF) die("write");
            if (fputc(prev, out) == EOF) die("write");
            *out_bytes += 2;
            break;
        }
        (*in_bytes)++;

        if (cur == prev && run < 255) {
            run++;
        } else {
            if (fputc(run,  out) == EOF) die("write");
            if (fputc(prev, out) == EOF) die("write");
            *out_bytes += 2;
            run = 1;
        }
        prev = cur;
    }
}

/* ── decompress ──────────────────────────────────────────────────────── */

static void decompress(FILE *in, FILE *out, long *in_bytes, long *out_bytes) {
    *in_bytes = *out_bytes = 0;

    int run_byte;
    while ((run_byte = fgetc(in)) != EOF) {
        (*in_bytes)++;
        int byte = fgetc(in);
        if (byte == EOF) { fprintf(stderr, "rle: corrupt input (odd byte count)\n"); exit(1); }
        (*in_bytes)++;

        unsigned int run = (unsigned int)(uint8_t)run_byte;
        if (run == 0) {
            /* run of 0 is invalid in our format */
            fprintf(stderr, "rle: corrupt input (run length of 0)\n"); exit(1);
        }

        for (unsigned int i = 0; i < run; i++) {
            if (fputc(byte, out) == EOF) die("write");
            (*out_bytes)++;
        }
    }
}

/* ── main ────────────────────────────────────────────────────────────── */

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s c|d <input> <output>\n", prog);
    fprintf(stderr, "       Use '-' for stdin/stdout.\n");
    fprintf(stderr, "examples:\n");
    fprintf(stderr, "  %s c input.bin output.rle\n", prog);
    fprintf(stderr, "  %s d output.rle restored.bin\n", prog);
    fprintf(stderr, "  cat file | %s c - - > file.rle\n", prog);
    exit(1);
}

int main(int argc, char **argv) {
    if (argc != 4 || (argv[1][0] != 'c' && argv[1][0] != 'd'))
        usage(argv[0]);

    FILE *in  = open_file(argv[2], "rb");
    FILE *out = open_file(argv[3], "wb");

    long in_bytes = 0, out_bytes = 0;
    int compress_mode = (argv[1][0] == 'c');

    if (compress_mode)
        compress(in, out, &in_bytes, &out_bytes);
    else
        decompress(in, out, &in_bytes, &out_bytes);

    close_file(in);
    if (fflush(out) != 0) die("flush");
    close_file(out);

    /* print stats to stderr so stdout stays clean when piping */
    if (compress_mode) {
        double ratio = (in_bytes > 0)
            ? (100.0 * out_bytes / in_bytes)
            : 100.0;
        fprintf(stderr, "compressed %ld -> %ld bytes (%.1f%%)\n",
                in_bytes, out_bytes, ratio);
    } else {
        fprintf(stderr, "decompressed %ld -> %ld bytes\n", in_bytes, out_bytes);
    }

    return 0;
}

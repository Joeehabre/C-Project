#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    unsigned long lines, words, bytes;
} Counts;

static void count_file(FILE *f, Counts *c) {
    c->lines = c->words = c->bytes = 0;
    int ch, in_word = 0;
    while ((ch = fgetc(f)) != EOF) {
        c->bytes++;
        if (ch == '\n') c->lines++;
        if (isspace((unsigned char)ch)) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            c->words++;
        }
    }
}

static void print_counts(const Counts *c, int show_l, int show_w, int show_c, const char *name) {
    if (show_l) printf(" %7lu", c->lines);
    if (show_w) printf(" %7lu", c->words);
    if (show_c) printf(" %7lu", c->bytes);
    if (name)   printf(" %s", name);
    putchar('\n');
}

int main(int argc, char **argv) {
    int show_l = 0, show_w = 0, show_c = 0;
    int first_file = 1;

    /* parse flags */
    int i;
    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++) {
        for (const char *p = argv[i] + 1; *p; p++) {
            switch (*p) {
                case 'l': show_l = 1; break;
                case 'w': show_w = 1; break;
                case 'c': show_c = 1; break;
                default:
                    fprintf(stderr, "wc: unknown flag -%c\n", *p);
                    fprintf(stderr, "usage: wc [-lwc] [file ...]\n");
                    return 1;
            }
        }
        first_file = i + 1;
    }

    /* default: show all */
    if (!show_l && !show_w && !show_c)
        show_l = show_w = show_c = 1;

    int rc = 0;
    Counts total = {0, 0, 0};
    int file_count = 0;

    if (first_file >= argc) {
        /* read from stdin */
        Counts c;
        count_file(stdin, &c);
        print_counts(&c, show_l, show_w, show_c, NULL);
        return 0;
    }

    for (i = first_file; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (!f) {
            perror(argv[i]);
            rc = 1;
            continue;
        }
        Counts c;
        count_file(f, &c);
        fclose(f);

        print_counts(&c, show_l, show_w, show_c, argv[i]);

        total.lines += c.lines;
        total.words += c.words;
        total.bytes += c.bytes;
        file_count++;
    }

    if (file_count > 1)
        print_counts(&total, show_l, show_w, show_c, "total");

    return rc;
}
